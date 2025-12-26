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

#ifndef XENOLITH_APPLICATION_LINUX_DBUS_XLLINUXDBUSCONTROLLER_H_
#define XENOLITH_APPLICATION_LINUX_DBUS_XLLINUXDBUSCONTROLLER_H_

#include "SPEventLooper.h"
#include "XLCommon.h"
#include "XlCoreMonitorInfo.h"

#if LINUX

#include "XLLinuxDBusLibrary.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

class LinuxContextController;
class DisplayConfigManager;

} // namespace stappler::xenolith::platform

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform::dbus {

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

struct SP_PUBLIC NetworkState {
	bool networkingEnabled = false;
	bool wirelessEnabled = false;
	bool wwanEnabled = false;
	bool wimaxEnabled = false;
	NMMetered metered = NMMetered::NM_METERED_UNKNOWN;
	NMState state = NMState::NM_STATE_UNKNOWN;
	NMConnectivityState connectivity = NMConnectivityState::NM_CONNECTIVITY_UNKNOWN;
	String primaryConnectionType;
	Vector<uint32_t> capabilities;

	NetworkState() = default;
	NetworkState(NotNull<dbus::Library>, NotNull<DBusMessage>);

	void description(const CallbackStream &out) const;

	NetworkFlags getFlags() const;

	bool operator==(const NetworkState &) const = default;
	bool operator!=(const NetworkState &) const = default;
};

class SP_PUBLIC Controller : public Ref {
public:
	static ThemeInfo readThemeInfo(NotNull<dbus::Library>, NotNull<DBusMessage>);

	virtual ~Controller() = default;

	Controller(NotNull< Library>, NotNull<event::Looper>, NotNull<LinuxContextController>);

	bool setup();
	void cancel();

	Connection *getSessionBus() const { return _sessionBus; }
	Connection *getSystemBus() const { return _systemBus; }
	Library *getLibrary() const { return _dbus; }

	bool isConnectied() const;

	// available only if  `isConnectied`
	Rc<DisplayConfigManager> makeDisplayConfigManager(
			Function<void(NotNull<DisplayConfigManager>)> &&);

protected:
	dbus_bool_t handleDbusEvent(dbus::Connection *c, const dbus::Event &);

	void updateNetworkState();
	void updateInterfaceTheme();

	void handleSessionConnected(NotNull<dbus::Connection>);
	void handleSystemConnected(NotNull<dbus::Connection>);

	Rc<Library> _dbus;
	Rc<event::Looper> _looper;
	LinuxContextController *_controller = nullptr;

	Rc<Connection> _sessionBus;
	Rc<Connection> _systemBus;
	Rc<BusFilter> _networkConnectionFilter;
	Rc<BusFilter> _networkPropertiesFilter;
	Rc<BusFilter> _settingsFilter;

	dbus::NetworkState _networkState;
};

} // namespace stappler::xenolith::platform::dbus

#endif

#endif // XENOLITH_APPLICATION_LINUX_DBUS_XLLINUXDBUSCONTROLLER_H_
