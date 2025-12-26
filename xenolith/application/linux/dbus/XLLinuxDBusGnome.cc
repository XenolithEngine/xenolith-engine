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

#include "XLLinuxDBusGnome.h"
#include "SPMemory.h"
#include "SPString.h"
#include "dbus/dbus.h"
#include "linux/dbus/XLLinuxDBusLibrary.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform::dbus {

static constexpr auto GNOME_DISPLAY_CONFIG_NAME = "org.gnome.Mutter.DisplayConfig";
static constexpr auto GNOME_DISPLAY_CONFIG_PATH = "/org/gnome/Mutter/DisplayConfig";
static constexpr auto GNOME_DISPLAY_CONFIG_INTERFACE = "org.gnome.Mutter.DisplayConfig";
static constexpr auto GNOME_DISPLAY_CONFIG_FILTER =
		"type='signal',interface='org.gnome.Mutter.DisplayConfig'";

static void Controller_readGnomeDisplayMonitorName(MonitorId &id, const ReadIterator &it) {
	it.foreach ([&](const ReadIterator &name) {
		switch (name.getIndex()) {
		case 0: id.name = name.getString().str<Interface>(); break;
		case 1:
			id.edid.vendorId = name.getString().str<Interface>();
			id.edid.vendor = core::EdidInfo::getVendorName(id.edid.vendorId);
			if (id.edid.vendor.empty()) {
				id.edid.vendor = id.edid.vendorId;
			}
			break;
		case 2: id.edid.model = name.getString().str<Interface>(); break;
		case 3: id.edid.serial = name.getString().str<Interface>(); break;
		default: break;
		}
	});
}

static void Controller_readGnomeDisplayConfigMonitor(PhysicalDisplay &mon, const ReadIterator &it) {
	it.foreach ([&](const ReadIterator &val) {
		switch (val.getIndex()) {
		case 0: // names
			Controller_readGnomeDisplayMonitorName(mon.id, val);
			break;
		case 1: // modes
			val.foreach ([&](const ReadIterator &m) {
				auto &mode = mon.modes.emplace_back();
				m.foreach ([&](const ReadIterator &field) {
					switch (field.getIndex()) {
					case 0: mode.name = field.getString().str<Interface>(); break;
					case 1: mode.mode.width = field.getI32(); break;
					case 2: mode.mode.height = field.getI32(); break;
					case 3:
						mode.mode.rate =
								static_cast<uint32_t>(std::ceil(field.getDouble() * 1'000));
						break;
					case 4: mode.mode.scale = field.getFloat(); break;
					case 5:
						field.foreach ([&](const ReadIterator &scale) {
							mode.scales.emplace_back(scale.getFloat());
						});
						break;
					case 6:
						field.foreachDictEntry([&](StringView name, const ReadIterator &f) {
							if (name == "is-current") {
								if (f.getBool()) {
									mode.current = true;
								}
							} else if (name == "is-preferred") {
								if (f.getBool()) {
									mode.preferred = true;
								}
							}
						});
						break;
					}
				});
			});
			break;
		default: break;
		}
	});
}

static Rc<DisplayConfig> readGnomeDisplayConfig(ReadIterator &iter) {
	Rc<DisplayConfig> ret = Rc<DisplayConfig>::create();

	while (iter) {
		switch (iter.getIndex()) {
		case 0:
			// serial
			ret->serial = iter.getU32();
			break;
		case 1:
			// monitors
			iter.foreach ([&](const ReadIterator &it) {
				auto &mon = ret->monitors.emplace_back();
				mon.index = it.getIndex();
				Controller_readGnomeDisplayConfigMonitor(mon, it);
			});
			break;
		case 2:
			// logical_monitors
			iter.foreach ([&](const ReadIterator &it) {
				auto &mon = ret->logical.emplace_back();
				it.foreach ([&](const ReadIterator &field) {
					switch (field.getIndex()) {
					case 0: mon.rect.x = field.getI32(); break;
					case 1: mon.rect.y = field.getI32(); break;
					case 2: mon.scale = field.getFloat(); break;
					case 3: mon.transform = field.getU32(); break;
					case 4: mon.primary = field.getBool(); break;
					case 5:
						field.foreach ([&](const ReadIterator &names) {
							MonitorId id;
							Controller_readGnomeDisplayMonitorName(id, names);
							mon.monitors.emplace_back(id);
							auto monIndex = it.getIndex();
							if (auto m = ret->getMonitor(id)) {
								const_cast<PhysicalDisplay *>(m)->index = monIndex;
							}
						});
						break;
					default: break;
					}
				});
			});
			break;
		case 3: // properties
			iter.foreachDictEntry([&](StringView key, const ReadIterator &value) {
				if (key == "max-screen-size") {
					value.foreach ([&](const ReadIterator &val) {
						switch (val.getIndex()) {
						case 0: ret->desktopRect.width = val.getI32(); break;
						case 1: ret->desktopRect.height = val.getI32(); break;
						}
					});
				}
			});
		default: break;
		}

		iter.next();
	}

	return ret;
}

static void readGnomeDisplayResources(ReadIterator &iter, DisplayConfig *info) {
	while (iter) {
		switch (iter.getIndex()) {
		case 0:
			// serial
			break;
		case 1:
			// crtcs
			break;
		case 2:
			// outputs
			break;
		case 3:
			// modes
			break;
		case 4:
			// max_screen_width
			info->desktopRect.width = iter.getU32();
			break;
		case 5:
			// max_screen_height
			info->desktopRect.height = iter.getU32();
			break;
		default: break;
		}

		iter.next();
	}
}

static void sanitizaDisplayConfig(DisplayConfig *info) {
	for (auto &it : info->logical) {
		if (it.rect.width == 9 || it.rect.height == 0) {
			for (auto &mId : it.monitors) {
				if (auto m = info->getMonitor(mId)) {
					auto &cMode = m->getCurrent();
					it.rect.width = cMode.mode.width;
					it.rect.height = cMode.mode.height;
					break;
				}
			}
		}
	}

	// fix <any> placeholder with actual value
	if (info->desktopRect.width == 32'767 || info->desktopRect.height == 32'767) {
		info->desktopRect.width = 0;
		info->desktopRect.height = 0;
		auto size = info->getSize();
		info->desktopRect.width = size.width;
		info->desktopRect.height = size.height;
	}
}

bool GnomeDisplayConfigManager::init(NotNull<Controller> c,
		Function<void(NotNull<DisplayConfigManager>)> &&cb) {
	if (!DisplayConfigManager::init(sp::move(cb))) {
		return false;
	}

	_dbus = c;
	_configFilter = Rc<dbus::BusFilter>::alloc(_dbus->getSessionBus(), GNOME_DISPLAY_CONFIG_FILTER,
			GNOME_DISPLAY_CONFIG_INTERFACE, "MonitorsChanged",
			[this](NotNull<const BusFilter> f, NotNull<DBusMessage> msg) -> uint32_t {
		updateDisplayConfig();
		return DBUS_HANDLER_RESULT_HANDLED;
	});

	updateDisplayConfig();

	return true;
}

void GnomeDisplayConfigManager::invalidate() {
	DisplayConfigManager::invalidate();

	_configFilter = nullptr;
	_dbus = nullptr;
}

void GnomeDisplayConfigManager::updateDisplayConfig(Function<void(DisplayConfig *)> &&fn) {
	_dbus->getSessionBus()->callMethod(GNOME_DISPLAY_CONFIG_NAME, GNOME_DISPLAY_CONFIG_PATH,
			GNOME_DISPLAY_CONFIG_INTERFACE, "GetCurrentState",
			[guard = Rc<GnomeDisplayConfigManager>(this),
					fn = sp::move(fn)](NotNull<dbus::Connection> c, DBusMessage *reply) mutable {
		if (guard->_dbus) {

			//dbus::describe(guard->_dbus->getLibrary(), reply, memory::makeCallback(std::cout));

			ReadIterator iter(guard->_dbus->getLibrary(), reply);
			auto info = readGnomeDisplayConfig(iter);
			if (fn) {
				fn(info);
			}

			// check if desktopRect was read from properties
			// If not - read from resources
			if (info->desktopRect.width == 0 || info->desktopRect.height == 0) {
				guard->_dbus->getSessionBus()->callMethod(GNOME_DISPLAY_CONFIG_NAME,
						GNOME_DISPLAY_CONFIG_PATH, GNOME_DISPLAY_CONFIG_INTERFACE, "GetResources",
						[guard = sp::move(guard), fn = sp::move(fn), info = sp::move(info)](
								NotNull<dbus::Connection> c, DBusMessage *reply) {
					if (guard->_dbus) {
						dbus::describe(guard->_dbus->getLibrary(), reply,
								memory::makeCallback(std::cout));
						ReadIterator iter(guard->_dbus->getLibrary(), reply);
						// send notifications
						readGnomeDisplayResources(iter, info);
						sanitizaDisplayConfig(info);
						guard->handleConfigChanged(info);
					}
				});
			} else {
				guard->handleConfigChanged(info);
			}
		}
	});
}

void GnomeDisplayConfigManager::prepareDisplayConfigUpdate(Function<void(DisplayConfig *)> &&fn) {
	updateDisplayConfig(sp::move(fn));
}

void GnomeDisplayConfigManager::applyDisplayConfig(NotNull<DisplayConfig> data,
		Function<void(Status)> &&cb) {
	_dbus->getSessionBus()->callMethod(GNOME_DISPLAY_CONFIG_NAME, GNOME_DISPLAY_CONFIG_PATH,
			GNOME_DISPLAY_CONFIG_INTERFACE, "ApplyMonitorsConfig", [&](WriteIterator &req) {
		// write request
		req.add(BasicValue(uint32_t(data->serial)));
		req.add(BasicValue(uint32_t(1)));
		req.addArray("(iiduba(ssa{sv}))", [&](WriteIterator &logicalMonitors) {
			for (auto &lmIt : data->logical) {
				logicalMonitors.addStruct([&](WriteIterator &monData) {
					monData.add(lmIt.rect.x);
					monData.add(lmIt.rect.y);
					monData.add(lmIt.scale);
					monData.add(lmIt.transform);
					monData.add(lmIt.primary);
					monData.addArray("(ssa{sv})", [&](WriteIterator &monitors) {
						for (auto &it : lmIt.monitors) {
							auto mPtr = data->getMonitor(it);
							if (mPtr) {
								monitors.addStruct([&](WriteIterator &monInfo) {
									monInfo.add(StringView(mPtr->id.name));
									monInfo.add(StringView(mPtr->modes.front().name));
									monInfo.addArray("{sv}", [&](WriteIterator &monInfo) { });
								});
							}
						}
					});
				});
			}
		});
		req.addArray("{sv}", [&](WriteIterator &monInfo) { });
	}, [this, cb = sp::move(cb)](NotNull<dbus::Connection> c, DBusMessage *reply) {
		ReadIterator iter(_dbus->getLibrary(), reply);
		if (iter && iter.getType() == Type::String) {
			auto str = iter.getString();
			if (str == "Logical monitors not adjacent") {
				if (cb) {
					cb(Status::ErrorInvalidArguemnt);
				}
				return;
			}
		}
		describe(_dbus->getLibrary(), reply, makeCallback(std::cout));
		if (cb) {
			cb(Status::Ok);
		}
	});
}

} // namespace stappler::xenolith::platform::dbus
