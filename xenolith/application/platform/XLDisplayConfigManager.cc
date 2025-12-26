/**
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 **/

#include "XLDisplayConfigManager.h"
#include "XlCoreMonitorInfo.h"
#include "SPStatus.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

const DisplayMode *PhysicalDisplay::getMode(const core::ModeInfo &m) const {
	if (m == core::ModeInfo::Current) {
		return &getCurrent();
	} else if (m == core::ModeInfo::Preferred) {
		for (auto &it : modes) {
			if (it.preferred) {
				return &it;
			}
		}
		return &modes.front();
	} else {
		for (auto &it : modes) {
			if (it.mode == m) {
				return &it;
			}
		}
		return nullptr;
	}
}

DisplayMode DisplayMode::None{};

const DisplayMode &PhysicalDisplay::getCurrent() const {
	for (auto &it : modes) {
		if (it.current) {
			return it;
		}
	}
	for (auto &it : modes) {
		if (it.preferred) {
			return it;
		}
	}
	if (!modes.empty()) {
		return modes.front();
	} else {
		return DisplayMode::None;
	}
}

bool LogicalDisplay::hasMonitor(const MonitorId &id) const {
	for (auto &it : monitors) {
		if (it == id) {
			return true;
		}
	}
	return false;
}

const PhysicalDisplay *DisplayConfig::getMonitor(const MonitorId &id) const {
	if (id == MonitorId::Primary) {
		for (auto &it : logical) {
			if (it.primary) {
				return getMonitor(it.monitors.front());
			}
		}
	}
	for (auto &it : monitors) {
		if (it.id == id) {
			return &it;
		}
	}
	return nullptr;
}

const LogicalDisplay *DisplayConfig::getLogical(const MonitorId &id) const {
	for (auto &it : logical) {
		if (!it.monitors.empty() && it.monitors.front() == id) {
			return &it;
		}
	}
	for (auto &it : logical) {
		if (std::find(it.monitors.begin(), it.monitors.end(), id) != it.monitors.end()) {
			return &it;
		}
	}
	return nullptr;
}

const LogicalDisplay *DisplayConfig::getLogical(const NativeId &id) const {
	for (auto &it : logical) {
		if (it.xid == id) {
			return &it;
		}
	}
	return nullptr;
}

bool DisplayConfig::isEqual(const DisplayConfig *cfg) const {
	return serial == cfg->serial && monitors == cfg->monitors && logical == cfg->logical;
}

Extent2 DisplayConfig::getSize() const {
	Extent2 ret;
	if (desktopRect.width > 0 || desktopRect.height > 0) {
		return Extent2(desktopRect.width, desktopRect.height);
	}

	for (auto &it : logical) {
		if (it.rect.width > 0 && it.rect.height > 0) {
			ret.width = std::max(ret.width, it.rect.x + it.rect.width);
			ret.height = std::max(ret.height, it.rect.y + it.rect.height);
		} else {
			for (auto &mId : it.monitors) {
				if (auto m = getMonitor(mId)) {
					auto &cMode = m->getCurrent();
					ret.width = std::max(ret.width, it.rect.x + cMode.mode.width);
					ret.height = std::max(ret.height, it.rect.y + cMode.mode.height);
				}
			}
		}
	}
	return ret;
}

Extent2 DisplayConfig::getSizeMM() const {
	Extent2 ret = getSize();

	float scale = 0.01f;
	for (auto &it : monitors) {
		auto m = it.getMode(core::ModeInfo::Preferred);
		if (m) {
			scale = std::max(scale, float(it.mm.width) / float(m->mode.width));
			scale = std::max(scale, float(it.mm.height) / float(m->mode.height));
		}
	}
	return Extent2(ret.width * scale, ret.height * scale);
}

bool DisplayConfigManager::init(Function<void(NotNull<DisplayConfigManager>)> &&cb) {
	_onConfigChanged = sp::move(cb);
	return true;
}

void DisplayConfigManager::invalidate() {
	_onConfigChanged = nullptr;
	_waitForConfigNotification.clear();
}

void DisplayConfigManager::exportScreenInfo(NotNull<core::ScreenInfo> info) const {
	if (!_currentConfig) {
		return;
	}

	for (auto &it : _currentConfig->monitors) {
		xenolith::MonitorInfo m;
		m.name = it.id.name;
		m.edid = it.id.edid;

		for (auto &mit : it.modes) {
			xenolith::ModeInfo mode = mit.mode;
			if (mit.preferred) {
				m.preferredMode = m.modes.size();
			}
			if (mit.current) {
				m.currentMode = m.modes.size();
			}
			m.modes.emplace_back(mode);
		}

		info->monitors.emplace_back(move(m));
	}

	for (auto &it : _currentConfig->logical) {
		if (it.primary) {
			for (auto &mIt : it.monitors) {
				uint32_t offset = 0;
				for (auto &tIt : info->monitors) {
					if (tIt == mIt) {
						info->primaryMonitor = offset;
						break;
					}
					++offset;
				}
			}
		}
	}
}

void DisplayConfigManager::setModeExclusive(core::MonitorId targetId, core::ModeInfo targetMode,
		Function<void(Status)> &&cb, Ref *ref) {
	prepareDisplayConfigUpdate([this, targetId, targetMode, cb = sp::move(cb), ref = Rc<Ref>(ref)](
									   DisplayConfig *data) mutable {
		if (!data) {
			cb(Status::ErrorNotImplemented);
			return;
		}
		auto current = extractCurrentConfig(data);
		if (!_savedConfig) {
			_savedConfig = current;
		}

		Status status = Status::ErrorNotImplemented;

		auto targetConfig = Rc<DisplayConfig>::create();
		targetConfig->native = data->native;
		targetConfig->serial = data->serial;

		// build new monitor info
		for (auto &it : data->monitors) {
			auto &m = targetConfig->monitors.emplace_back(PhysicalDisplay{
				it.xid,
				it.index,
				it.id,
				it.mm,
			});
			if (it.id == targetId) {
				// target monitor: set target mode
				auto tMode = it.getMode(targetMode);
				if (tMode) {
					m.modes.emplace_back(*tMode);
					status = Status::Ok;
				} else {
					m.modes.emplace_back(it.getCurrent());
				}
			} else {
				// others - set default captured mode
				bool found = false;
				for (auto &dmIt : _savedConfig->monitors) {
					if (dmIt.id == it.id) {
						auto &targetMode = dmIt.modes.front();
						auto tMode = it.getMode(targetMode.mode);
						if (tMode) {
							m.modes.emplace_back(*tMode);
						} else {
							status = Status::ErrorNotImplemented;
							m.modes.emplace_back(it.getCurrent());
						}
						found = true;
						break;
					}
				}
				if (!found) {
					// no saved data - use current
					for (auto &dmIt : current->monitors) {
						if (dmIt.id == it.id) {
							auto &targetMode = dmIt.modes.front();
							auto tMode = it.getMode(targetMode.mode);
							if (tMode) {
								m.modes.emplace_back(*tMode);
							} else {
								status = Status::ErrorNotImplemented;
								m.modes.emplace_back(it.getCurrent());
							}
							found = true;
							break;
						}
					}
				}
			}
		}

		// build new logical monitors info
		for (auto &it : data->logical) {
			if (it.hasMonitor(targetId)) {
				auto &m = targetConfig->logical.emplace_back(LogicalDisplay{
					it.xid,
					it.rect,
					it.scale,
					it.transform,
					it.primary,
				});
				m.monitors.emplace_back(targetId);
			} else {
				targetConfig->logical.emplace_back(it);
			}
		}

		if (status == Status::Ok) {
			adjustDisplay(targetConfig);
			applyDisplayConfig(targetConfig, sp::move(cb));
		} else {
			if (cb) {
				cb(status);
			}
		}
	});
}

void DisplayConfigManager::setMode(xenolith::MonitorId, xenolith::ModeInfo,
		Function<void(Status)> &&, Ref *) { }

void DisplayConfigManager::restoreMode(Function<void(Status)> &&cb, Ref *ref) {
	if (!_savedConfig) {
		if (cb) {
			cb(Status::ErrorInvalidArguemnt);
		}
		return;
	}

	prepareDisplayConfigUpdate(
			[this, cb = sp::move(cb), ref = Rc<Ref>(ref)](DisplayConfig *data) mutable {
		if (!data) {
			if (cb) {
				cb(Status::ErrorNotImplemented);
			}
			return;
		}

		Status status = Status::ErrorInvalidArguemnt;

		auto targetConfig = Rc<DisplayConfig>::create();
		targetConfig->native = data->native;
		targetConfig->serial = data->serial;

		bool restored = true;
		// build new monitor info
		for (auto &it : data->monitors) {
			auto &m = targetConfig->monitors.emplace_back(PhysicalDisplay{
				it.xid,
				it.index,
				it.id,
				it.mm,
			});
			auto sourceMon = _savedConfig->getMonitor(m.id);
			if (sourceMon && !sourceMon->modes.empty()) {
				auto mode = it.getMode(sourceMon->getCurrent().mode);
				if (mode) {
					m.modes.emplace_back(*mode);
				} else {
					restored = false;
					m.modes.emplace_back(it.getCurrent());
				}
			} else {
				restored = false;
				m.modes.emplace_back(it.getCurrent());
			}
		}

		if (restored) {
			targetConfig->logical = _savedConfig->logical;
			_savedConfig = nullptr;
			adjustDisplay(targetConfig);
			applyDisplayConfig(targetConfig, sp::move(cb));
		} else {
			_savedConfig = nullptr;
			if (cb) {
				cb(status);
			}
		}
	});
}

Rc<DisplayConfig> DisplayConfigManager::extractCurrentConfig(
		NotNull<const DisplayConfig> config) const {
	auto ret = Rc<DisplayConfig>::create();
	for (auto &it : config->monitors) {
		auto &mon = ret->monitors.emplace_back(PhysicalDisplay{
			it.xid,
			it.index,
			it.id,
			it.mm,
		});

		for (auto &mIt : it.modes) {
			if (mIt.current) {
				mon.modes.emplace_back(mIt);
				break;
			}
		}
	}

	for (auto &it : config->logical) {
		ret->logical.emplace_back(LogicalDisplay{
			it.xid,
			it.rect,
			it.scale,
			it.transform,
			it.primary,
			it.monitors,
		});
	}
	ret->native = config->native;
	return ret;
}

void DisplayConfigManager::adjustDisplay(NotNull<DisplayConfig> config) const {
	auto getLogicalMonitorSize = [&](const LogicalDisplay &mon) {
		auto hMon = config->getMonitor(mon.monitors.front());
		auto mode = hMon->getCurrent();
		Extent2 size{mode.mode.width, mode.mode.height};

		switch (_scalingMode) {
		case PostScaling:
			size.width = static_cast<uint32_t>(
					std::round(size.width * std::ceil(mon.scale) / mon.scale));
			size.height = static_cast<uint32_t>(
					std::round(size.height * std::ceil(mon.scale) / mon.scale));
			break;
		case DirectScaling:
			size.width = static_cast<uint32_t>(std::round(size.width * mon.scale));
			size.height = static_cast<uint32_t>(std::round(size.height * mon.scale));
			break;
		}
		return size;
	};

	Vector<LogicalDisplay *> data;
	for (auto &it : config->logical) {
		auto size = getLogicalMonitorSize(it);
		it.rect.width = size.width;
		it.rect.height = size.height;
		data.emplace_back(&it);
	}

	// Build ordered lists

	Vector<LogicalDisplay *> listX;
	Vector<LogicalDisplay *> listY;

	for (auto &it : data) {
		if (listX.empty()) {
			listX.emplace_back(it);
		} else {
			auto lIt = listX.begin();
			while (lIt != listX.end() && (*lIt)->rect.x < it->rect.x) { ++lIt; }
			listX.emplace(lIt, it);
		}
		if (listY.empty()) {
			listY.emplace_back(it);
		} else {
			auto lIt = listY.begin();
			while (lIt != listY.end() && (*lIt)->rect.y < it->rect.y) { ++lIt; }
			listY.emplace(lIt, it);
		}
	}

	// Adjust by X
	auto listItX = listX.begin();
	while (listItX != listX.end()) {
		int32_t front = (*listItX)->rect.y;
		int32_t back = (*listItX)->rect.y + (*listItX)->rect.height;
		int32_t nextX = (*listItX)->rect.x + (*listItX)->rect.width;

		// find next monitor by X;
		auto listNextIt = listItX + 1;
		while (listNextIt != listX.end()) {
			int32_t nextFront = (*listNextIt)->rect.y;
			int32_t nextBack = (*listNextIt)->rect.y + (*listNextIt)->rect.height;

			if (nextBack > front && nextFront < back) {
				// intersection, do adjustment
				(*listNextIt)->rect.x = nextX;
				break;
			} else {
				++listNextIt;
			}
		}

		++listItX;
	}

	// Adjust by Y
	auto listItY = listY.begin();
	while (listItY != listY.end()) {
		int32_t front = (*listItY)->rect.x;
		int32_t back = (*listItY)->rect.x + (*listItY)->rect.width;
		int32_t nextY = (*listItY)->rect.y + (*listItY)->rect.height;

		// find next monitor by X;
		auto listNextIt = listItY + 1;
		while (listNextIt != listY.end()) {
			int32_t nextFront = (*listNextIt)->rect.x;
			int32_t nextBack = (*listNextIt)->rect.x + (*listNextIt)->rect.width;

			if (nextBack > front && nextFront < back) {
				// intersection, do adjustment
				(*listNextIt)->rect.y = nextY;
				break;
			} else {
				++listNextIt;
			}
		}

		++listItY;
	}
}

void DisplayConfigManager::handleConfigChanged(NotNull<DisplayConfig> cfg) {
	bool configUpdated = false;
	if (!_currentConfig || (!cfg->isEqual(_currentConfig) && cfg->time > _currentConfig->time)) {
		configUpdated = true;
		_currentConfig = cfg;
		if (_onConfigChanged) {
			_onConfigChanged(this);
		}
	} else {
		// set new config, just in case of WM error in serial
		if (cfg->time > _currentConfig->time) {
			configUpdated = true;
			_currentConfig = cfg;
		}
	}

	if (configUpdated) {
		auto tmp = sp::move(_waitForConfigNotification);
		_waitForConfigNotification.clear();

		for (auto &it : tmp) { it(); }
	}
}

void DisplayConfigManager::prepareDisplayConfigUpdate(Function<void(DisplayConfig *)> &&cb) {
	cb(nullptr);
}

void DisplayConfigManager::applyDisplayConfig(NotNull<DisplayConfig>, Function<void(Status)> &&cb) {
	cb(Status::ErrorNotImplemented);
}

} // namespace stappler::xenolith::platform
