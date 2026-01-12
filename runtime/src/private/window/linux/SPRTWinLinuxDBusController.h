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

#ifndef CORE_RUNTIME_PRIVATE_WINDOW_LINUX_SPRTWINLINUXDBUSCONTROLLER_H_
#define CORE_RUNTIME_PRIVATE_WINDOW_LINUX_SPRTWINLINUXDBUSCONTROLLER_H_

#include <sprt/runtime/init.h>

#if SPRT_LINUX

#include <sprt/runtime/window/notifications.h>
#include <sprt/runtime/window/theme_info.h>
#include <sprt/runtime/window/interface.h>

#include "private/window/linux/SPRTWinLinuxDBusLibrary.h"

namespace sprt::window {

class LinuxContextController;
class DisplayConfigManager;

} // namespace sprt::window

namespace sprt::window::dbus {

enum NMState {
	NM_STATE_UNKNOWN = 0, // networking state is unknown
	NM_STATE_ASLEEP = 10, // networking is not enabled
	NM_STATE_DISCONNECTED = 20, // there is no active network connection
	NM_STATE_DISCONNECTING = 30, // network connections are being cleaned up
	NM_STATE_CONNECTING = 40, // a network connection is being started
	NM_STATE_CONNECTED_LOCAL = 50, // there is only local IPv4 and/or IPv6 connectivity
	NM_STATE_CONNECTED_SITE = 60, // there is only site-wide IPv4 and/or IPv6 connectivity
	NM_STATE_CONNECTED_GLOBAL = 70, // there is global IPv4 and/or IPv6 Internet connectivity
};

enum NMConnectivityState {
	NM_CONNECTIVITY_UNKNOWN = 1, // Network connectivity is unknown.
	NM_CONNECTIVITY_NONE = 2, // The host is not connected to any network.
	NM_CONNECTIVITY_PORTAL =
			3, // The host is behind a captive portal and cannot reach the full Internet.
	NM_CONNECTIVITY_LIMITED =
			4, // The host is connected to a network, but does not appear to be able to reach the full Internet.
	NM_CONNECTIVITY_FULL =
			5, // The host is connected to a network, and appears to be able to reach the full Internet.
};

enum NMMetered {
	NM_METERED_UNKNOWN = 0, // The metered status is unknown
	NM_METERED_YES = 1, // Metered, the value was statically set
	NM_METERED_NO = 2, // Not metered, the value was statically set
	NM_METERED_GUESS_YES = 3, // Metered, the value was guessed
	NM_METERED_GUESS_NO = 4, // Not metered, the value was guessed
};

struct SPRT_API NetworkState {
	bool networkingEnabled = false;
	bool wirelessEnabled = false;
	bool wwanEnabled = false;
	bool wimaxEnabled = false;
	NMMetered metered = NMMetered::NM_METERED_UNKNOWN;
	NMState state = NMState::NM_STATE_UNKNOWN;
	NMConnectivityState connectivity = NMConnectivityState::NM_CONNECTIVITY_UNKNOWN;
	memory::dynstring primaryConnectionType;
	memory::dynvector<uint32_t> capabilities;

	NetworkState() = default;
	NetworkState(NotNull<dbus::Library>, NotNull<DBusMessage>);

	void description(const callback<void(StringView)> &out) const;

	NetworkFlags getFlags() const;

	bool operator==(const NetworkState &) const = default;
	bool operator!=(const NetworkState &) const = default;
};

class SPRT_API Controller : public Ref {
public:
	static ThemeInfo readThemeInfo(NotNull<dbus::Library>, NotNull<DBusMessage>);

	virtual ~Controller() = default;

	Controller(NotNull<Library>, NotNull<LooperAdapter>, NotNull<LinuxContextController>);

	bool setup();
	void cancel();

	Connection *getSessionBus() const { return _sessionBus; }
	Connection *getSystemBus() const { return _systemBus; }
	Library *getLibrary() const { return _dbus; }

	bool isConnectied() const;

	// available only if  `isConnectied`
	Rc<DisplayConfigManager> makeDisplayConfigManager(
			memory::dynfunction<void(NotNull<DisplayConfigManager>)> &&);

protected:
	dbus_bool_t handleDbusEvent(dbus::Connection *c, const dbus::Event &);

	void updateNetworkState();
	void updateInterfaceTheme();

	void handleSessionConnected(NotNull<dbus::Connection>);
	void handleSystemConnected(NotNull<dbus::Connection>);

	Rc<Library> _dbus;
	Rc<LooperAdapter> _looper;
	LinuxContextController *_controller = nullptr;

	Rc<Connection> _sessionBus;
	Rc<Connection> _systemBus;
	Rc<BusFilter> _networkConnectionFilter;
	Rc<BusFilter> _networkPropertiesFilter;
	Rc<BusFilter> _settingsFilter;

	dbus::NetworkState _networkState;
};

} // namespace sprt::window::dbus

#endif

#endif // CORE_RUNTIME_PRIVATE_WINDOW_LINUX_SPRTWINLINUXDBUSCONTROLLER_H_
