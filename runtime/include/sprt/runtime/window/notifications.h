/**
 Copyright (c) 2026 Xenolith Team <admin@stappler.org>

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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_WINDOW_NOTIFICATIONS_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_WINDOW_NOTIFICATIONS_H_

#include <sprt/runtime/int.h>

namespace sprt::window {

enum class SystemNotification : uint32_t {
	LowMemory,
	LowPower,
	QuerySuspend,
	Suspending,
	Resume,
	DisplayChanged,
	ConfigurationChanged,
	ClipboardChanged,
	LiveReloadPerformed,
};

enum class NetworkFlags : uint32_t {
	None,
	Internet = 1 << 0,
	Congested = 1 << 1,
	Metered = 1 << 2,
	Restricted = 1 << 3,
	Roaming = 1 << 4,
	Suspended = 1 << 5,
	Vpn = 1 << 6,
	PrioritizeBandwidth = 1 << 7,
	PrioritizeLatency = 1 << 8,
	TemporarilyNotMetered = 1 << 9,
	Trusted = 1 << 10,
	Validated = 1 << 11,
	WifiP2P = 1 << 12,
	CaptivePortal = 1 << 13,
	Local = 1 << 14,
	Wired = 1 << 15,
	WLAN = 1 << 16, // WIFI
	WWAN = 1 << 17, // mobile connection
};

SPRT_DEFINE_ENUM_AS_MASK(NetworkFlags)

} // namespace sprt::window

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_WINDOW_NOTIFICATIONS_H_
