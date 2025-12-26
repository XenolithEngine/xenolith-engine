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

#include "XLLinuxDBusKde.h"
#include "SPGeometry.h"
#include "SPMemory.h"
#include "SPStatus.h"
#include "SPString.h"
#include "XlCoreMonitorInfo.h"
#include "dbus/dbus.h"
#include "linux/dbus/XLLinuxDBusLibrary.h"
#include "platform/XLDisplayConfigManager.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform::dbus {

static constexpr auto KSCREEN_BUS_NAME = "org.kde.KScreen";
static constexpr auto KSCREEN_BACKEND_INTERFACE = "org.kde.kscreen.Backend";
static constexpr auto KSCREEN_BACKEND_PATH = "/backend";
static constexpr auto KSCREEN_FILTER = "type='signal',interface='org.kde.kscreen.Backend'";

enum class KdeScreenFeature {
	None = 0,
	PrimaryDisplay = 1 << 0,
	Writable = 1 << 1,
	PerOutputScaling = 1 << 2,
	OutputReplication = 1 << 3,
	AutoRotation = 1 << 4,
	TabletMode = 1 << 5,
	SynchronousOutputChanges = 1 << 6,
	XwaylandScales = 1 << 7,
};

struct KdeDisplayMode {
	String id;
	String name;
	double refreshRate;
	Extent2 size;

	operator DisplayMode() const {
		DisplayMode ret;
		if (auto xid = StringView(id).readInteger(10).get()) {
			ret.xid = static_cast<uint32_t>(xid);
		}
		ret.id = id;
		ret.name = name;

		ret.mode.width = size.width;
		ret.mode.height = size.height;
		ret.mode.rate = static_cast<uint32_t>(refreshRate * 1000.0);

		if (ret.name.empty()) {
			ret.name = toString(ret.mode.width, "x", ret.mode.height, "@", ret.mode.rate);
		}
		return ret;
	}
};

struct KdeOutputInfo {
	PhysicalDisplay physical;
	LogicalDisplay logical;
	Vector<int64_t> clones;
	Vector<KdeDisplayMode> modes;
	Vector<String> preferredModes;
	String currentModeId;
	String icon;
	int64_t source = 0;
	int64_t type = 0;
	bool connected = false;
	bool enabled = false;
	bool followPreferredMode = false;
};

struct KdeScreenInfo : Ref {
	uint32_t id = 0;
	int64_t features = 0;
	int64_t maxActiveOutputsCount = 0;
	Extent2 currentSize;
	Extent2 maxSize;
	Extent2 minSize;
	Vector<KdeOutputInfo> outputs;
	uint32_t requestsInQueue = 0;
	bool tabletModeAvailable = false;
	bool tabletModeEngaged = false;

	Function<void(DisplayConfig *)> callback;

	Rc<DisplayConfig> exportConfig();
};

static Extent2 readSize(const ReadIterator &field) {
	Extent2 ret;
	field.foreachDictEntry([&](StringView key, const ReadIterator &valueIt) {
		if (key == "width") {
			ret.width = valueIt.getI32();
		} else if (key == "height") {
			ret.height = valueIt.getI32();
		}
	});
	return ret;
}

static KdeDisplayMode readKdeDisplayMode(const ReadIterator &modeIt) {
	KdeDisplayMode mode;
	modeIt.foreachDictEntry([&](StringView key, const ReadIterator &fieldIt) {
		if (key == "id") {
			mode.id = fieldIt.getString().str<Interface>();
		} else if (key == "name") {
			mode.name = fieldIt.getString().str<Interface>();
		} else if (key == "refreshRate") {
			mode.refreshRate = fieldIt.getDouble();
		} else if (key == "size") {
			mode.size = readSize(fieldIt);
		}
	});

	return mode;
}

static KdeOutputInfo readKdeDisplayOutput(const ReadIterator &outputIt) {
	KdeOutputInfo info;

	outputIt.foreachDictEntry([&](StringView key, const ReadIterator &fieldIt) {
		if (key == "connected") {
			info.connected = fieldIt.getBool();
		} else if (key == "currentModeId") {
			info.currentModeId = fieldIt.getString().str<Interface>();
		} else if (key == "clones") {
			fieldIt.foreach (
					[&](const ReadIterator &it) { info.clones.emplace_back(it.getU32()); });
		} else if (key == "enabled") {
			info.enabled = fieldIt.getBool();
		} else if (key == "followPreferredMode") {
			info.followPreferredMode = fieldIt.getBool();
		} else if (key == "id") {
			info.physical.xid = fieldIt.getU32();
		} else if (key == "icon") {
			info.icon = fieldIt.getString().str<Interface>();
		} else if (key == "name") {
			info.physical.id.name = fieldIt.getString().str<Interface>();
		} else if (key == "pos") {
			fieldIt.foreachDictEntry([&](StringView key, const ReadIterator &valueIt) {
				if (key == "x") {
					info.logical.rect.x = valueIt.getI32();
				} else if (key == "y") {
					info.logical.rect.y = valueIt.getI32();
				}
			});
		} else if (key == "priority") {
			info.physical.index = fieldIt.getU32();
		} else if (key == "replicationSource") {
			info.source = fieldIt.getI64();
		} else if (key == "preferredModes") {
			fieldIt.foreach ([&](const ReadIterator &it) {
				info.preferredModes.emplace_back(it.getString().str<Interface>());
			});
		} else if (key == "rotation") {
			info.logical.transform = fieldIt.getU32();
		} else if (key == "scale") {
			info.logical.scale = fieldIt.getFloat();
		} else if (key == "size") {
			fieldIt.foreachDictEntry([&](StringView key, const ReadIterator &valueIt) {
				if (key == "width") {
					info.logical.rect.width = valueIt.getI32();
				} else if (key == "height") {
					info.logical.rect.height = valueIt.getI32();
				}
			});
		} else if (key == "sizeMM") {
			info.physical.mm = readSize(fieldIt);
		} else if (key == "modes") {
			fieldIt.foreach ([&](const ReadIterator &it) {
				auto mode = readKdeDisplayMode(it);
				info.modes.emplace_back(mode);
				if (!mode.id.empty()) {
					info.physical.modes.emplace_back(mode);
				}
			});
		} else if (key == "type") {
			info.type = fieldIt.getI64();
		}
	});

	if (info.connected) {
		for (auto &it : info.physical.modes) {
			if (it.id == info.currentModeId) {
				it.current = true;
			}
			if (std::find(info.preferredModes.begin(), info.preferredModes.end(), it.id)
					!= info.preferredModes.end()) {
				it.preferred = true;
			}
		}
	}

	return info;
}

static Rc<KdeScreenInfo> readKdeDisplayConfig(ReadIterator &iter) {
	Rc<KdeScreenInfo> ret = Rc<KdeScreenInfo>::create();

	iter.foreachDictEntry([&](StringView key, const ReadIterator &field) {
		if (key == "features") {
			ret->features = field.getI64();
		} else if (key == "outputs") {
			field.foreach ([&](const ReadIterator &outputIt) {
				auto out = readKdeDisplayOutput(outputIt);
				ret->outputs.emplace_back(move(out));
			});
		} else if (key == "screen") {
			field.foreachDictEntry([&](StringView key, const ReadIterator &field) {
				if (key == "id") {
					ret->id = field.getU32();
				} else if (key == "currentSize") {
					ret->currentSize = readSize(field);
				} else if (key == "maxActiveOutputsCount") {
					ret->maxActiveOutputsCount = field.getI64();
				} else if (key == "minSize") {
					ret->minSize = readSize(field);
				} else if (key == "maxSize") {
					ret->maxSize = readSize(field);
				}
			});
		} else if (key == "tabletModeAvailable") {
			ret->tabletModeAvailable = field.getBool();
		} else if (key == "tabletModeEngaged") {
			ret->tabletModeEngaged = field.getBool();
		}
	});

	return ret;
}

static void writeSize(int64_t width, int64_t height, WriteIterator &iter) {
	iter.addVariant(StringView("height"), BasicValue(int64_t(height)));
	iter.addVariant(StringView("width"), BasicValue(int64_t(width)));
}

static void writeSize(Extent2 size, WriteIterator &iter) {
	writeSize(size.width, size.height, iter);
}

static void writeKdeMode(NotNull<DisplayConfig> data, KdeDisplayMode &mode, WriteIterator &iter) {
	iter.addVariant(StringView("id"), BasicValue(StringView(mode.id)));
	iter.addVariant(StringView("name"), BasicValue(StringView(mode.name)));
	iter.addVariant(StringView("refreshRate"), BasicValue(double(mode.refreshRate)));
	iter.addVariantMap(StringView("size"),
			[&](WriteIterator &iter) { writeSize(mode.size, iter); });
}

static void writeKdeOutput(NotNull<DisplayConfig> data, KdeOutputInfo &out, PhysicalDisplay &disp,
		WriteIterator &iter, bool updated) {
	auto &currentMode = disp.getCurrent();
	auto logical = data->getLogical(disp.id);
	if (!logical) {
		logical = &out.logical;
	}

	iter.addVariantArray(StringView("clones"), [&](WriteIterator &iter) {
		for (auto &it : out.clones) { iter.addVariant(BasicValue(it)); }
	});
	iter.addVariant(StringView("connected"), BasicValue(out.connected));

	if (updated) {
		iter.addVariant(StringView("currentModeId"), BasicValue(currentMode.id));
	} else {
		iter.addVariant(StringView("currentModeId"), BasicValue(out.currentModeId));
	}

	iter.addVariant(StringView("enabled"), BasicValue(bool(out.enabled)));
	iter.addVariant(StringView("followPreferredMode"), BasicValue(bool(out.followPreferredMode)));
	iter.addVariant(StringView("icon"), BasicValue(StringView(out.icon)));
	iter.addVariant(StringView("id"), BasicValue(int64_t(disp.xid)));
	iter.addVariantArray(StringView("modes"), [&](WriteIterator &iter) {
		for (auto &modeIt : out.modes) {
			iter.addVariantMap([&](WriteIterator &iter) { writeKdeMode(data, modeIt, iter); });
		}
	});
	iter.addVariant(StringView("name"), BasicValue(disp.id.name));
	iter.addVariantMap(StringView("pos"), [&](WriteIterator &iter) {
		iter.addVariant(StringView("x"), BasicValue(int64_t(logical->rect.x)));
		iter.addVariant(StringView("y"), BasicValue(int64_t(logical->rect.y)));
	});
	iter.addVariantArray(StringView("preferredModes"), [&](WriteIterator &iter) {
		for (auto &it : out.preferredModes) { iter.addVariant(BasicValue(it)); }
	});

	iter.addVariant(StringView("priority"), BasicValue(int64_t(disp.index)));
	iter.addVariant(StringView("replicationSource"), BasicValue(int64_t(out.source)));
	iter.addVariant(StringView("rotation"), BasicValue(int64_t(logical->transform)));
	iter.addVariant(StringView("scale"), BasicValue(int64_t(logical->scale)));
	iter.addVariantMap(StringView("size"), [&](WriteIterator &iter) {
		if (currentMode == DisplayMode::None) {
			writeSize(-1, -1, iter);
		} else {
			writeSize(Extent2(currentMode.mode.width, currentMode.mode.height), iter);
		}
	});
	iter.addVariantMap(StringView("sizeMM"),
			[&](WriteIterator &iter) { writeSize(Extent2(disp.mm), iter); });
	iter.addVariant(StringView("type"), BasicValue(int64_t(out.type)));
}

static void writeKdeDisplayConfig(NotNull<DisplayConfig> data, WriteIterator &iter) {
	auto native = data->native.get_cast<KdeScreenInfo>();
	iter.addMap([&](WriteIterator &iter) {
		iter.addVariant(StringView("features"), BasicValue(int64_t(native->features)));
		iter.addVariantArray("outputs", [&](WriteIterator &iter) {
			for (auto &kIt : native->outputs) {
				bool found = false;
				for (auto &pIt : data->monitors) {
					if (pIt.xid == kIt.physical.xid) {
						iter.addVariantMap([&](WriteIterator &iter) {
							writeKdeOutput(data, kIt, pIt, iter, true);
						});
						found = true;
						break;
					}
				}
				if (!found) {
					iter.addVariantMap([&](WriteIterator &iter) {
						writeKdeOutput(data, kIt, kIt.physical, iter, false);
					});
				}
			}
		});
		iter.addVariantMap("screen", [&](WriteIterator &iter) {
			iter.addVariantMap(StringView("currentSize"),
					[&](WriteIterator &iter) { writeSize(native->currentSize, iter); });
			iter.addVariant(StringView("id"), BasicValue(int64_t(native->id)));
			iter.addVariant(StringView("maxActiveOutputsCount"),
					BasicValue(native->maxActiveOutputsCount));
			iter.addVariantMap(StringView("maxSize"),
					[&](WriteIterator &iter) { writeSize(native->maxSize, iter); });
			iter.addVariantMap(StringView("minSize"),
					[&](WriteIterator &iter) { writeSize(native->minSize, iter); });
		});
		iter.addVariant(StringView("tabletModeAvailable"), BasicValue(native->tabletModeAvailable));
		iter.addVariant(StringView("tabletModeEngaged"), BasicValue(native->tabletModeEngaged));
	});
}

Rc<DisplayConfig> KdeScreenInfo::exportConfig() {
	auto ret = Rc<DisplayConfig>::create();

	for (auto &it : outputs) {
		if (it.source == 0 && it.connected) {
			// create logical monitor for this output
			auto &m = ret->logical.emplace_back(sp::move(it.logical));
			m.monitors.emplace_back(it.physical.id);
			for (auto &cloneId : it.clones) {
				for (auto &cIt : outputs) {
					if (cIt.physical.xid == uintptr_t(cloneId)) {
						m.monitors.emplace_back(cIt.physical.id);
					}
				}
			}
		}
	}

	for (auto &it : outputs) {
		if (it.connected) {
			ret->monitors.emplace_back(sp::move(it.physical));
		}
	}

	ret->native = this;

	return ret;
}

bool KdeDisplayConfigManager::init(NotNull<Controller> c,
		Function<void(NotNull<DisplayConfigManager>)> &&cb) {
	if (!DisplayConfigManager::init(sp::move(cb))) {
		return false;
	}

	_dbus = c;
	_configFilter = Rc<dbus::BusFilter>::alloc(_dbus->getSessionBus(), KSCREEN_FILTER,
			KSCREEN_BACKEND_INTERFACE, "configChanged",
			[this](NotNull<const BusFilter> f, NotNull<DBusMessage> msg) -> uint32_t {
		readDisplayConfig(msg, nullptr);
		return DBUS_HANDLER_RESULT_HANDLED;
	});

	updateDisplayConfig();

	return true;
}

void KdeDisplayConfigManager::invalidate() {
	DisplayConfigManager::invalidate();

	_configFilter = nullptr;
	_dbus = nullptr;
}

void KdeDisplayConfigManager::updateDisplayConfig(Function<void(DisplayConfig *)> &&fn) {
	_dbus->getSessionBus()->callMethod(KSCREEN_BUS_NAME, KSCREEN_BACKEND_PATH,
			KSCREEN_BACKEND_INTERFACE, "getConfig",
			[guard = Rc<KdeDisplayConfigManager>(this),
					fn = sp::move(fn)](NotNull<dbus::Connection> c, DBusMessage *reply) mutable {
		if (guard->_dbus) {
			guard->readDisplayConfig(reply, sp::move(fn));
		} else {
			fn(nullptr);
		}
	});
}

void KdeDisplayConfigManager::readDisplayConfig(NotNull<DBusMessage> reply,
		Function<void(DisplayConfig *)> &&fn) {
	// describe(_dbus->getLibrary(), reply, makeCallback(std::cout));
	ReadIterator iter(_dbus->getLibrary(), reply);
	auto info = readKdeDisplayConfig(iter);
	info->callback = sp::move(fn);
	uint32_t idx = 0;
	for (auto &it : info->outputs) {
		if (!it.connected) {
			++idx;
			continue;
		}
		auto eIt = _edidCache.find(toString(it.physical.xid.xid, ":", it.physical.id.name));
		if (eIt != _edidCache.end()) {
			it.physical.id.edid = eIt->second;
		} else {
			_dbus->getSessionBus()->callMethod(KSCREEN_BUS_NAME, KSCREEN_BACKEND_PATH,
					KSCREEN_BACKEND_INTERFACE, "getEdid", [&](WriteIterator &iter) {
				iter.add(int32_t(it.physical.xid));
			}, [this, info, idx](NotNull<Connection>, DBusMessage *reply) {
				ReadIterator iter(_dbus->getLibrary(), reply);
				auto edid = core::EdidInfo::parse(iter.getBytes());
				auto &mon = info->outputs[idx].physical;

				_edidCache.emplace(toString(mon.xid.xid, ":", mon.id.name), edid);
				info->outputs[idx].physical.id.edid = edid;
				if (--info->requestsInQueue == 0) {
					auto d = info->exportConfig();
					if (info->callback) {
						info->callback(d);
						info->callback = nullptr;
					}
					handleConfigChanged(d);
				}
			});
			++info->requestsInQueue;
		}
		++idx;
	}
	if (info->requestsInQueue == 0) {
		auto d = info->exportConfig();
		if (info->callback) {
			info->callback(d);
			info->callback = nullptr;
		}
		handleConfigChanged(d);
	}
}

void KdeDisplayConfigManager::prepareDisplayConfigUpdate(Function<void(DisplayConfig *)> &&fn) {
	updateDisplayConfig(sp::move(fn));
}

void KdeDisplayConfigManager::applyDisplayConfig(NotNull<DisplayConfig> data,
		Function<void(Status)> &&cb) {
	_dbus->getSessionBus()->callMethod(KSCREEN_BUS_NAME, KSCREEN_BACKEND_PATH,
			KSCREEN_BACKEND_INTERFACE, "setConfig", [&](WriteIterator &req) {
		// write request
		writeKdeDisplayConfig(data, req);
	}, [cb = sp::move(cb)](NotNull<dbus::Connection> c, DBusMessage *reply) {
		//ReadIterator iter(_dbus->getLibrary(), reply);
		// describe(_dbus->getLibrary(), reply, makeCallback(std::cout));
		if (cb) {
			cb(Status::Ok);
		}
	});
}

} // namespace stappler::xenolith::platform::dbus
