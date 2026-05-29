/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

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

#include <sprt/runtime/enum.h>
#include <sprt/runtime/stringview.h>

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

namespace sprt {

template <typename T>
struct io_traits;

template <>
struct io_traits<sprt::window::NetworkFlags> {
	using NetworkFlags = sprt::window::NetworkFlags;

	template <typename Callback>
	static void __envode(const Callback &stream, const NetworkFlags &value) {
		for (auto it : flags(value)) {
			stream << " ";
			switch (it) {
			case NetworkFlags::None: break;
			case NetworkFlags::Internet: stream << "NetworkFlags::Internet"; break;
			case NetworkFlags::Congested: stream << "NetworkFlags::Congested"; break;
			case NetworkFlags::Metered: stream << "NetworkFlags::Metered"; break;
			case NetworkFlags::Restricted: stream << "NetworkFlags::Restricted"; break;
			case NetworkFlags::Roaming: stream << "NetworkFlags::Roaming"; break;
			case NetworkFlags::Suspended: stream << "NetworkFlags::Suspended"; break;
			case NetworkFlags::Vpn: stream << "NetworkFlags::Vpn"; break;
			case NetworkFlags::PrioritizeBandwidth:
				stream << "NetworkFlags::PrioritizeBandwidth";
				break;
			case NetworkFlags::PrioritizeLatency:
				stream << "NetworkFlags::PrioritizeLatency";
				break;
			case NetworkFlags::TemporarilyNotMetered:
				stream << "NetworkFlags::TemporarilyNotMetered";
				break;
			case NetworkFlags::Trusted: stream << "NetworkFlags::Trusted"; break;
			case NetworkFlags::Validated: stream << "NetworkFlags::Validated"; break;
			case NetworkFlags::WifiP2P: stream << "NetworkFlags::WifiP2P"; break;
			case NetworkFlags::CaptivePortal: stream << "NetworkFlags::CaptivePortal"; break;
			case NetworkFlags::Local: stream << "NetworkFlags::Local"; break;
			case NetworkFlags::Wired: stream << "NetworkFlags::Wired"; break;
			case NetworkFlags::WLAN: stream << "NetworkFlags::WLAN"; break;
			case NetworkFlags::WWAN: stream << "NetworkFlags::WWAN"; break;
			}
		}
	}

	template <io_character CharType>
	static void encode(const callback<void(StringViewBase<CharType>)> &cb,
			const NetworkFlags &value) {
		__envode(cb, value);
	}

	static void encode(const callback<void(StringViewUtf8)> &cb, const NetworkFlags &value) {
		__envode(cb, value);
	}
};

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_WINDOW_NOTIFICATIONS_H_
