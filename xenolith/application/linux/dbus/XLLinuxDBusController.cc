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

#include "XLLinuxDBusController.h"
#include "SPBytesReader.h"
#include "SPMemInterface.h"
#include "SPStatus.h"
#include "SPString.h"
#include "SPEventLooper.h"
#include "SPEventPollHandle.h"
#include "SPEventTimerHandle.h"
#include "dbus/dbus.h"
#include "linux/dbus/XLLinuxDBusLibrary.h"
#include "linux/XLLinuxContextController.h"
#include "XLLinuxDBusGnome.h"
#include "XLLinuxDBusKde.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform::dbus {

SP_UNUSED static constexpr auto NM_SERVICE_NAME = "org.freedesktop.NetworkManager";
SP_UNUSED static constexpr auto NM_SERVICE_CONNECTION_NAME =
		"org.freedesktop.NetworkManager.Connection.Active";
SP_UNUSED static constexpr auto NM_SERVICE_VPN_NAME = "org.freedesktop.NetworkManager.VPN.Plugin";
SP_UNUSED static constexpr auto NM_SERVICE_FILTER =
		"type='signal',path='/org/freedesktop/"
		"NetworkManager',interface='org.freedesktop.DBus.Properties'";
SP_UNUSED static constexpr auto NM_SERVICE_CONNECTION_FILTER =
		"type='signal',interface='org.freedesktop.NetworkManager.Connection.Active'";
SP_UNUSED static constexpr auto NM_SERVICE_VPN_FILTER =
		"type='signal',interface='org.freedesktop.NetworkManager.VPN.Plugin'";
SP_UNUSED static constexpr auto NM_SERVICE_PATH = "/org/freedesktop/NetworkManager";
SP_UNUSED static constexpr auto NM_SIGNAL_STATE_CHANGED = "StateChanged";
SP_UNUSED static constexpr auto NM_SIGNAL_PROPERTIES_CHANGED = "PropertiesChanged";

SP_UNUSED static constexpr auto DESKTOP_PORTAL_SERVICE_NAME = "org.freedesktop.portal.Desktop";
SP_UNUSED static constexpr auto DESKTOP_PORTAL_SERVICE_PATH = "/org/freedesktop/portal/desktop";
SP_UNUSED static constexpr auto DESKTOP_PORTAL_SETTINGS_INTERFACE =
		"org.freedesktop.portal.Settings";
SP_UNUSED static constexpr auto DESKTOP_PORTAL_SERVICE_FILTER =
		"type='signal',interface='org.freedesktop.portal.Settings'";

SP_UNUSED static constexpr auto GNOME_DISPLAY_CONFIG_SERVICE = "org.gnome.Mutter.DisplayConfig";

SP_UNUSED static constexpr auto KSCREEN_SERVICE = "org.kde.KScreen";

Controller::Controller(NotNull< Library> dbus, NotNull<event::Looper> looper,
		NotNull<LinuxContextController> c) {
	_dbus = dbus;
	_looper = looper;
	_controller = c;

	_sessionBus = Rc<dbus::Connection>::alloc(_dbus,
			[this](dbus::Connection *c, const dbus::Event &ev) -> dbus_bool_t {
		return handleDbusEvent(c, ev);
	}, DBUS_BUS_SESSION);

	_systemBus = Rc<dbus::Connection>::alloc(_dbus,
			[this](dbus::Connection *c, const dbus::Event &ev) -> dbus_bool_t {
		return handleDbusEvent(c, ev);
	}, DBUS_BUS_SYSTEM);
}

bool Controller::setup() {
	_sessionBus->dispatchAll();
	_systemBus->dispatchAll();

	_sessionBus->setup();
	_systemBus->setup();

	return true;
}

void Controller::cancel() {
	_networkConnectionFilter = nullptr;
	_networkPropertiesFilter = nullptr;
	_settingsFilter = nullptr;

	if (_sessionBus) {
		_sessionBus->close();
		_sessionBus = nullptr;
	}

	if (_systemBus) {
		_systemBus->close();
		_systemBus = nullptr;
	}

	_controller = nullptr;
}

bool Controller::isConnectied() const {
	return (_sessionBus->connected || _sessionBus->failed)
			&& (_systemBus->connected || _systemBus->failed);
}

Rc<DisplayConfigManager> Controller::makeDisplayConfigManager(
		Function<void(NotNull<DisplayConfigManager>)> &&cb) {

	if (_sessionBus->services.find(GNOME_DISPLAY_CONFIG_SERVICE) != _sessionBus->services.end()) {
		return Rc<GnomeDisplayConfigManager>::create(this, sp::move(cb));
	}

	if (_sessionBus->services.find(KSCREEN_SERVICE) != _sessionBus->services.end()) {
		return Rc<KdeDisplayConfigManager>::create(this, sp::move(cb));
	}

	return nullptr;
}

dbus_bool_t Controller::handleDbusEvent(dbus::Connection *c, const dbus::Event &ev) {
	dbus_bool_t result = 0;
	auto isBusy = c->busy;
	c->busy = true;
	switch (ev.type) {
	case dbus::Event::None: break;
	case dbus::Event::AddWatch: {
		if (auto d = _dbus->dbus_watch_get_data(ev.watch)) {
			auto handle = reinterpret_cast<event::PollHandle *>(d);
			handle->reset(dbus::getPollFlags(_dbus->dbus_watch_get_flags(ev.watch)));
			return 1;
		}

		auto handle = _looper->listenPollableHandle(_dbus->dbus_watch_get_unix_fd(ev.watch),
				dbus::getPollFlags(_dbus->dbus_watch_get_flags(ev.watch)),
				event::CompletionHandle<event::PollHandle>::create<DBusWatch>(ev.watch,
						[](DBusWatch *watch, event::PollHandle *handle, uint32_t flags,
								Status status) {
			if (sprt::status::isErrno(status)) {
				return;
			}

			auto c = static_cast<dbus::Connection *>(handle->getUserdata());
			if (!c) {
				return;
			}

			if (!c->handle(handle, dbus::Event{dbus::Event::TriggerWatch, {watch}},
						event::PollFlags(flags))) {
				handle->cancel();
			}
		}),
				c);

		handle->retain(0);
		_dbus->dbus_watch_set_data(ev.watch, handle.get(), [](void *ptr) {
			auto handle = reinterpret_cast<event::PollHandle *>(ptr);
			handle->cancel(Status::ErrorCancelled);
			handle->setUserdata(nullptr);
			handle->release(0);
		});

		if (!_dbus->dbus_watch_get_enabled(ev.watch)) {
			handle->pause();
		}

		result = 1;
		break;
	}
	case dbus::Event::ToggleWatch: {
		auto handle = reinterpret_cast<event::PollHandle *>(_dbus->dbus_watch_get_data(ev.watch));
		if (!_dbus->dbus_watch_get_enabled(ev.watch)) {
			if (handle->getStatus() != Status::Declined) {
				handle->pause();
			}
		} else {
			if (handle->getStatus() == Status::Declined) {
				handle->reset(dbus::getPollFlags(_dbus->dbus_watch_get_flags(ev.watch)));
				handle->resume();
			}
		}
		result = 1;
		break;
	}
	case dbus::Event::RemoveWatch: {
		auto handle = reinterpret_cast<event::PollHandle *>(_dbus->dbus_watch_get_data(ev.watch));
		handle->cancel(Status::Done);
		handle->setUserdata(nullptr);
		_dbus->dbus_watch_set_data(ev.watch, nullptr, nullptr);
		result = 1;
		break;
	}
	case dbus::Event::TriggerWatch: break;
	case dbus::Event::AddTimeout:
		if (auto d = _dbus->dbus_timeout_get_data(ev.timeout)) {
			auto handle = reinterpret_cast<event::TimerHandle *>(d);
			handle->reset(event::TimerInfo{
				.timeout = TimeInterval::milliseconds(_dbus->dbus_timeout_get_interval(ev.timeout)),
				.count = 1,
			});
			result = 1;
		} else {
			auto handle = _looper->scheduleTimer(
					event::TimerInfo{
						.completion =
								event::CompletionHandle<event::TimerHandle>::create<DBusTimeout>(
										ev.timeout,
										[](DBusTimeout *timeout, event::TimerHandle *handle,
												uint32_t flags, Status status) {
				if (sprt::status::isErrno(status)) {
					return;
				}

				auto c = static_cast<dbus::Connection *>(handle->getUserdata());
				if (!c) {
					return;
				}

				if (!c->handle(handle,
							dbus::Event{dbus::Event::TriggerTimeout, {.timeout = timeout}},
							event::PollFlags(flags))) {
					handle->cancel();
				} else if (c->lib->dbus_timeout_get_enabled(timeout)) {
					auto ival =
							TimeInterval::milliseconds(c->lib->dbus_timeout_get_interval(timeout));
					if (ival) {
						handle->reset(event::TimerInfo{
							.timeout = TimeInterval::milliseconds(
									c->lib->dbus_timeout_get_interval(timeout)),
							.count = 1,
						});
					}
				}
			}),
						.timeout = TimeInterval::milliseconds(
								_dbus->dbus_timeout_get_interval(ev.timeout)),
						.count = 1,
					},
					c);

			handle->retain(0);
			_dbus->dbus_timeout_set_data(ev.timeout, handle.get(), [](void *ptr) {
				auto handle = reinterpret_cast<event::TimerHandle *>(ptr);
				handle->cancel(Status::ErrorCancelled);
				handle->setUserdata(nullptr);
				handle->release(0);
			});

			if (!_dbus->dbus_timeout_get_enabled(ev.timeout)) {
				handle->pause();
			}
			result = 1;
		}
		break;
	case dbus::Event::ToggleTimeout: {
		auto handle =
				reinterpret_cast<event::TimerHandle *>(_dbus->dbus_timeout_get_data(ev.timeout));
		if (!_dbus->dbus_timeout_get_enabled(ev.timeout)) {
			if (handle->getStatus() != Status::Declined) {
				handle->pause();
			}
		} else {
			if (handle->getStatus() == Status::Declined) {
				handle->reset(event::TimerInfo{
					.timeout = TimeInterval::milliseconds(
							_dbus->dbus_timeout_get_interval(ev.timeout)),
					.count = 1,
				});
				handle->resume();
			}
		}
		result = 1;
		break;
	}
	case dbus::Event::RemoveTimeout: {
		auto handle =
				reinterpret_cast<event::TimerHandle *>(_dbus->dbus_timeout_get_data(ev.timeout));
		handle->cancel(Status::ErrorCancelled);
		handle->setUserdata(nullptr);
		_dbus->dbus_timeout_set_data(ev.timeout, nullptr, nullptr);
		result = 1;
		break;
	}
	case dbus::Event::TriggerTimeout: break;
	case dbus::Event::Dispatch:
		_looper->performOnThread([c]() { c->dispatchAll(); }, c, false);
		break;
	case dbus::Event::Wakeup:
		/*_looper->performOnThread([c]() {
			//c->flush();
		}, c, true);*/
		break;
	case dbus::Event::Connected:
		if (c == _systemBus) {
			handleSystemConnected(c);
		} else if (c == _sessionBus) {
			handleSessionConnected(c);
		}
		if (isConnectied()) {
			_controller->tryStart();
		}
		result = 1;
		break;
	case dbus::Event::Message: {
		if (StringView(_dbus->dbus_message_get_interface(ev.message)) == "org.freedesktop.DBus"
				&& StringView(_dbus->dbus_message_get_sender(ev.message)) == "org.freedesktop.DBus"
				&& StringView(_dbus->dbus_message_get_member(ev.message)) == "NameAcquired") {
			ReadIterator iter(_dbus, ev.message);
			c->name = iter.getString().str<Interface>();
			result = DBUS_HANDLER_RESULT_HANDLED;
		} else {
			dbus::describe(_dbus, ev.message, [](StringView str) { std::cout << str; });
			result = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}
		break;
	}
	case dbus::Event::Failed:
		if (c == _systemBus) {
			handleSystemConnected(c);
		} else if (c == _sessionBus) {
			handleSessionConnected(c);
		}
		if (isConnectied()) {
			_controller->tryStart();
		}
		result = 1;
	}
	c->busy = isBusy;
	return result;
}

void Controller::updateNetworkState() {
	_systemBus->callMethod(NM_SERVICE_NAME, NM_SERVICE_PATH, "org.freedesktop.DBus.Properties",
			"GetAll", [](WriteIterator &iter) { iter.add(StringView(NM_SERVICE_NAME)); },
			[this](NotNull<dbus::Connection> c, DBusMessage *reply) {
		_networkState = NetworkState(_dbus, reply);

		if (_controller) {
			_controller->handleNetworkStateChanged(_networkState.getFlags());
		}
	}, this);
}

void Controller::updateInterfaceTheme() {
	_sessionBus->callMethod(DESKTOP_PORTAL_SERVICE_NAME, DESKTOP_PORTAL_SERVICE_PATH,
			DESKTOP_PORTAL_SETTINGS_INTERFACE, "ReadAll", [](WriteIterator &iter) {
		StringView array[] = {"org.gnome.desktop.interface", "org.gnome.desktop.peripherals.mouse"};
		iter.add(makeSpanView(array));
	}, [this](NotNull<dbus::Connection> c, DBusMessage *reply) {
		auto newThemeInfo = readThemeInfo(_dbus, reply);

		if (_controller) {
			_controller->handleThemeInfoChanged(sp::move(newThemeInfo));
		}
	});
}

void Controller::handleSessionConnected(NotNull<dbus::Connection> c) {
	if (c->connected) {
		if (c->services.find(DESKTOP_PORTAL_SERVICE_NAME) != c->services.end()) {
			_settingsFilter = Rc<dbus::BusFilter>::alloc(c, DESKTOP_PORTAL_SERVICE_FILTER,
					DESKTOP_PORTAL_SETTINGS_INTERFACE, "SettingChanged",
					[this](NotNull<const BusFilter>, NotNull<DBusMessage>) -> uint32_t {
				updateInterfaceTheme();
				return DBUS_HANDLER_RESULT_HANDLED;
			});
			updateInterfaceTheme();
		}
		log::source().debug("dbus::Controller", "Session bus connected");
	}
}

void Controller::handleSystemConnected(NotNull<dbus::Connection> c) {
	if (c->connected) {
		if (c->services.find(NM_SERVICE_NAME) != c->services.end()) {
			_networkConnectionFilter = Rc<dbus::BusFilter>::alloc(c, NM_SERVICE_CONNECTION_FILTER,
					NM_SERVICE_CONNECTION_NAME, "StateChanged",
					[this](NotNull<const BusFilter>, NotNull<DBusMessage> message) -> uint32_t {
				updateNetworkState();
				return DBUS_HANDLER_RESULT_HANDLED;
			});

			_networkPropertiesFilter = Rc<dbus::BusFilter>::alloc(c, NM_SERVICE_FILTER,
					NM_SERVICE_NAME, "PropertiesChanged",
					[this](NotNull<const BusFilter>, NotNull<DBusMessage> message) -> uint32_t {
				updateNetworkState();
				return DBUS_HANDLER_RESULT_HANDLED;
			});

			updateNetworkState();
		}
		log::source().debug("dbus::Controller", "System bus connected");
	}
}

struct MessageNetworkStateParser {
	bool onArrayBegin(Type type) { return true; }
	bool onArrayEnd() { return true; }

	bool onDictEntry(const BasicValue &val, NotNull<DBusMessageIter> entry) {
		if (val.type == Type::String) {
			StringView prop(val.value.str);
			if (prop == "NetworkingEnabled") {
				uint32_t value = 0;
				if (MessagePropertyParser::parse(lib, entry, value)) {
					target->networkingEnabled = value ? true : false;
				}
			} else if (prop == "WirelessEnabled") {
				uint32_t value = 0;
				if (MessagePropertyParser::parse(lib, entry, value)) {
					target->wirelessEnabled = value ? true : false;
				}
			} else if (prop == "WwanEnabled") {
				uint32_t value = 0;
				if (MessagePropertyParser::parse(lib, entry, value)) {
					target->wwanEnabled = value ? true : false;
				}
			} else if (prop == "WimaxEnabled") {
				uint32_t value = 0;
				if (MessagePropertyParser::parse(lib, entry, value)) {
					target->wimaxEnabled = value ? true : false;
				}
			} else if (prop == "PrimaryConnectionType") {
				const char *value = nullptr;
				if (MessagePropertyParser::parse(lib, entry, value)) {
					target->primaryConnectionType = String(value);
				}
			} else if (prop == "Metered") {
				uint32_t value = 0;
				if (MessagePropertyParser::parse(lib, entry, value)) {
					target->metered = NMMetered(value);
				}
			} else if (prop == "State") {
				uint32_t value = 0;
				if (MessagePropertyParser::parse(lib, entry, value)) {
					target->state = NMState(value);
				}
			} else if (prop == "Connectivity") {
				uint32_t value = 0;
				if (MessagePropertyParser::parse(lib, entry, value)) {
					target->connectivity = NMConnectivityState(value);
				}
			} else if (prop == "Capabilities") {
				Vector<uint32_t> value;
				if (MessagePropertyParser::parse(lib, entry, value)) {
					target->capabilities = sp::move(value);
				}
			}
		}
		return true;
	}

	Library *lib = nullptr;
	NetworkState *target = nullptr;
};

struct MessageSettingsInfoParser {
	bool onArrayBegin(Type type) { return true; }
	bool onArrayEnd() { return true; }

	bool onDictEntry(const BasicValue &val, NotNull<DBusMessageIter> entry) {
		if (val.type == Type::String) {
			StringView prop(val.value.str);
			if (prop == "org.gnome.desktop.interface") {
				lib->parseMessage(entry, *this);
			} else if (prop == "org.gnome.desktop.peripherals.mouse") {
				lib->parseMessage(entry, *this);
			} else if (prop == "document-font-name") {
				const char *value = nullptr;
				if (MessagePropertyParser::parse(lib, entry, value)) {
					//target->documentFontName = String(value);
				}
			} else if (prop == "icon-theme") {
				const char *value = nullptr;
				if (MessagePropertyParser::parse(lib, entry, value)) {
					//target->iconTheme = String(value);
				}
			} else if (prop == "scaling-factor") {
				uint32_t value = 0;
				if (MessagePropertyParser::parse(lib, entry, value)) {
					target->cursorScaling = value;
				}
			} else if (prop == "cursor-size") {
				int32_t value = 0;
				if (MessagePropertyParser::parse(lib, entry, value)) {
					target->cursorSize = value;
				}
			} else if (prop == "monospace-font-name") {
				const char *value = nullptr;
				if (MessagePropertyParser::parse(lib, entry, value)) {
					//target->monospaceFontName = String(value);
				}
			} else if (prop == "color-scheme") {
				const char *value = nullptr;
				if (MessagePropertyParser::parse(lib, entry, value)) {
					target->colorScheme = String(value);
				}
			} else if (prop == "cursor-theme") {
				const char *value = nullptr;
				if (MessagePropertyParser::parse(lib, entry, value)) {
					target->systemTheme = String(value);
				}
			} else if (prop == "text-scaling-factor") {
				float value = 1.0f;
				if (MessagePropertyParser::parse(lib, entry, value)) {
					target->textScaling = value;
				}
			} else if (prop == "font-name") {
				const char *value = nullptr;
				if (MessagePropertyParser::parse(lib, entry, value)) {
					target->systemFontName = String(value);
				}
			} else if (prop == "left-handed") {
				bool value = false;
				if (MessagePropertyParser::parse(lib, entry, value)) {
					target->leftHandedMouse = value;
				}
			} else if (prop == "double-click") {
				int32_t value = 0;
				if (MessagePropertyParser::parse(lib, entry, value)) {
					target->doubleClickInterval = value * 1'000;
				}
			} else {
				//log::source().info("DBus", "Unknown settings key:", prop);
			}
		}
		return true;
	}

	Library *lib = nullptr;
	ThemeInfo *target = nullptr;
};

NetworkState::NetworkState(NotNull<dbus::Library> lib, NotNull<DBusMessage> message) {
	dbus::MessageNetworkStateParser parser;
	parser.lib = lib;
	parser.target = this;

	lib->parseMessage(message, parser);
}

void NetworkState::description(const CallbackStream &out) const {
	out << primaryConnectionType << ": ( ";
	if (networkingEnabled) {
		out << "networking ";
	}
	if (wirelessEnabled) {
		out << "wireless ";
	}
	if (wwanEnabled) {
		out << "wwan ";
	}
	if (wimaxEnabled) {
		out << "wimax ";
	}
	out << ")";

	switch (connectivity) {
	case NM_CONNECTIVITY_UNKNOWN: out << " NM_CONNECTIVITY_UNKNOWN"; break;
	case NM_CONNECTIVITY_NONE: out << " NM_CONNECTIVITY_NONE"; break;
	case NM_CONNECTIVITY_PORTAL: out << " NM_CONNECTIVITY_PORTAL"; break;
	case NM_CONNECTIVITY_LIMITED: out << " NM_CONNECTIVITY_LIMITED"; break;
	case NM_CONNECTIVITY_FULL: out << " NM_CONNECTIVITY_FULL"; break;
	}

	switch (state) {
	case NM_STATE_UNKNOWN: out << " NM_STATE_UNKNOWN"; break;
	case NM_STATE_ASLEEP: out << " NM_STATE_ASLEEP"; break;
	case NM_STATE_DISCONNECTED: out << " NM_STATE_DISCONNECTED"; break;
	case NM_STATE_DISCONNECTING: out << " NM_STATE_DISCONNECTING"; break;
	case NM_STATE_CONNECTING: out << " NM_STATE_CONNECTING"; break;
	case NM_STATE_CONNECTED_LOCAL: out << " NM_STATE_CONNECTED_LOCAL"; break;
	case NM_STATE_CONNECTED_SITE: out << " NM_STATE_CONNECTED_SITE"; break;
	case NM_STATE_CONNECTED_GLOBAL: out << " NM_STATE_CONNECTED_GLOBAL"; break;
	}

	switch (metered) {
	case NM_METERED_UNKNOWN: out << " NM_METERED_UNKNOWN"; break;
	case NM_METERED_YES: out << " NM_METERED_YES"; break;
	case NM_METERED_GUESS_YES: out << " NM_METERED_GUESS_YES"; break;
	case NM_METERED_NO: out << " NM_METERED_NO"; break;
	case NM_METERED_GUESS_NO: out << " NM_METERED_GUESS_NO"; break;
	}

	if (!capabilities.empty()) {
		out << " ( ";
		for (auto &it : capabilities) { out << it << " "; }
		out << ")";
	}
}

NetworkFlags NetworkState::getFlags() const {
	NetworkFlags ret = NetworkFlags::None;

	if (primaryConnectionType == "vpn") {
		ret |= NetworkFlags::Vpn;
	} else if (primaryConnectionType == "802-3-ethernet") {
		ret |= NetworkFlags::Wired;
	} else if (primaryConnectionType == "802-11-wireless") {
		ret |= NetworkFlags::WLAN;
	}

	switch (connectivity) {
	case NM_CONNECTIVITY_UNKNOWN:
	case NM_CONNECTIVITY_NONE: ret |= NetworkFlags::Restricted; break;
	case NM_CONNECTIVITY_PORTAL:
		ret |= NetworkFlags::Internet | NetworkFlags::CaptivePortal | NetworkFlags::Restricted;
		break;
	case NM_CONNECTIVITY_LIMITED: ret |= NetworkFlags::Internet; break;
	case NM_CONNECTIVITY_FULL: ret |= NetworkFlags::Internet | NetworkFlags::Validated; break;
	}

	switch (state) {
	case NM_STATE_UNKNOWN: break;
	case NM_STATE_ASLEEP: ret |= NetworkFlags::Suspended; break;
	case NM_STATE_DISCONNECTED: ret |= NetworkFlags::Suspended; break;
	case NM_STATE_DISCONNECTING: ret |= NetworkFlags::Suspended; break;
	case NM_STATE_CONNECTING: ret |= NetworkFlags::Suspended; break;
	case NM_STATE_CONNECTED_LOCAL: ret |= NetworkFlags::Restricted | NetworkFlags::Local; break;
	case NM_STATE_CONNECTED_SITE: ret |= NetworkFlags::Restricted; break;
	case NM_STATE_CONNECTED_GLOBAL: break;
	}

	switch (metered) {
	case NM_METERED_UNKNOWN: break;
	case NM_METERED_YES:
	case NM_METERED_GUESS_YES: ret |= NetworkFlags::Metered; break;
	case NM_METERED_NO:
	case NM_METERED_GUESS_NO: break;
	}

	return ret;
}

ThemeInfo Controller::readThemeInfo(NotNull<dbus::Library> lib, NotNull<DBusMessage> message) {
	ThemeInfo ret;
	dbus::MessageSettingsInfoParser parser;
	parser.lib = lib;
	parser.target = &ret;

	lib->parseMessage(message, parser);
	return ret;
}

} // namespace stappler::xenolith::platform::dbus
