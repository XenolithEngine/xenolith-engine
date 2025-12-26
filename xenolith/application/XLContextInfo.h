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

#ifndef XENOLITH_APPLICATION_XLCONTEXTINFO_H_
#define XENOLITH_APPLICATION_XLCONTEXTINFO_H_

#include "XLWindowInfo.h"
#include "XLCoreLoop.h" // IWYU pragma: keep
#include "XLCoreInstance.h"
#include "SPCommandLineParser.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class Context;
class AppThread;

#if ANDROID
using NativeContextHandle = sprt::jni::ApplicationInfo;
#else
using NativeContextHandle = void;
#endif

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

SP_DEFINE_ENUM_AS_MASK(NetworkFlags)

struct SP_PUBLIC UpdateTime final {
	uint64_t global = 0; // global OS timer in microseconds
	uint64_t app = 0; // microseconds since application was started
	uint64_t delta = 0; // microseconds since last update
	float dt = 0.0f; // seconds since last update
};

enum class ContextFlags : uint32_t {
	None = 0,

	Headless = 1 << 0,

	// Application shold be terminated when all it's windows were closed
	DestroyWhenAllWindowsClosed = 1 << 1,
};

SP_DEFINE_ENUM_AS_MASK(ContextFlags)

struct SP_PUBLIC ContextInfo final : public Ref {
	String bundleName = "org.stappler.xenolith.test"; // application reverce-domain name
	String appName = "Xenolith"; // application human-readable name
	String appVersion = "0.0.1"; // application human-readable version
	String userLanguage = ""; //"ru-ru"; // current locale name
	String userAgent = "XenolithApp"; // networking user agent
	String launchUrl; // initial launch URL (deep link)

	uint32_t appVersionCode = 0; // version code in Vulkan format (see SP_MAKE_API_VERSION)

	// Application event update interval (NOT screen update interval)
	TimeInterval appUpdateInterval = TimeInterval::seconds(1);

	uint16_t mainThreadsCount = config::getDefaultMainThreads(); // threads for general and gl tasks
	uint16_t appThreadsCount = config::getDefaultAppThreads(); // threads for app-level tasks

	ContextFlags flags = ContextFlags::None;

	Value extra;

	Value encode() const;
};

enum class CommonFlags : uint32_t {
	None = 0,
	Help = 1 << 0,
	Verbose = 1 << 1,
	Quiet = 1 << 2,
};

SP_DEFINE_ENUM_AS_MASK(CommonFlags)

struct ContextConfig final {
	static CommandLineParser<ContextConfig> getCommandLineParser();

	static bool readFromCommandLine(ContextConfig &, int argc, const char *argv[],
			const Callback<void(StringView)> &cb = nullptr);

	CommonFlags flags = CommonFlags::None;
	NativeContextHandle *native = nullptr;

	Rc<ContextInfo> context;
	Rc<WindowInfo> window;
	Rc<core::InstanceInfo> instance;
	Rc<core::LoopInfo> loop;

	ContextConfig(int argc, const char *argv[]);
	ContextConfig(NativeContextHandle *);

	Value encode() const;

private:
	ContextConfig();
};

/** DecorationInfo - Data for user-space decoration drawing

On some platforms, application should assist for WM to draw correct rounded corners and shadows

if resizeInset > 0, resizing layers (when WindowState::AllowedResize is set) should be placed inside user window space with
specified inset (and no other controls under this insets will receive events)

If borderRadius > 0, ronded coners should be drawn by application, actial radius is borderRadius * constraints.surfaceDensity

if userShadows is true, shadows should be drawn under rounded corners, using shadowWidth, shadowCurrentValue and shadowOffset
*/
struct SP_PUBLIC DecorationInfo {
	float resizeInset = 0.0f;
	float borderRadius = 0.0f;
	float shadowWidth = 0.0f;
	float shadowMinValue = 0.1f;
	float shadowMaxValue = 0.25f;
	Vec2 shadowOffset;

	void decode(const Value &);
	Value encode() const;

	// check if shadows drawen in user space
	bool hasShadows() const { return borderRadius > 0.0f || shadowWidth > 0.0f; }

	bool operator==(const DecorationInfo &) const = default;
	bool operator!=(const DecorationInfo &) const = default;
};

struct ThemeInfo {
	static constexpr StringView SchemePreferDark = StringView("prefer-dark");
	static constexpr StringView SchemePreferLight = StringView("prefer-light");
	static constexpr StringView SchemeDefault = StringView("default");

	String colorScheme;
	String systemTheme;
	String systemFontName;
	int32_t cursorSize = 0;
	float cursorScaling = 1.0f;
	float textScaling = 1.0f;
	float scrollModifier = 1.0f;
	bool leftHandedMouse = false;
	uint32_t doubleClickInterval = 500'000; // in microseconds

	DecorationInfo decorations;

	void decode(const Value &);
	Value encode() const;

	bool operator==(const ThemeInfo &) const = default;
	bool operator!=(const ThemeInfo &) const = default;
};

using OpacityValue = ValueWrapper<uint8_t, class OpacityTag>;
using ZOrder = ValueWrapper<int16_t, class ZOrderTag>;

SP_PUBLIC const CallbackStream &operator<<(const CallbackStream &stream, NetworkFlags t);
SP_PUBLIC std::ostream &operator<<(std::ostream &stream, NetworkFlags t);

} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_XLCONTEXTINFO_H_ */
