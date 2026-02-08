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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_PLATFORM_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_PLATFORM_H_

#include <sprt/runtime/int.h>
#include <sprt/runtime/stringview.h>
#include <sprt/c/bits/__sprt_def.h>

#if SPRT_ANDROID
namespace sprt::jni {

struct ApplicationInfo;

}
#endif

namespace sprt::platform {

enum class ClockType {
	Default,
	Monotonic,
	Realtime,
	Process,
	Thread,

	// hardware clock tick counter with unknown monotonic resolution;
	// see `rdtsc`
	Hardware
};

// current time in microseconds (with except for Hardware, see above)
SPRT_API uint64_t clock(ClockType = ClockType::Default);

// current time in nanoseconds
SPRT_API uint64_t nanoclock(ClockType = ClockType::Default);

// sleep for the microseconds
SPRT_API void sleep(uint64_t microseconds);

SPRT_API uint32_t getMemoryPageSize();

SPRT_API StringView getOsLocale();

// Relatively unique id for current device
SPRT_API StringView getUniqueDeviceId();

// Path to running executable
SPRT_API StringView getExecPath();

// User shared home path (/home/<user> or С:\users\<user>)
SPRT_API StringView getHomePath();

} // namespace sprt::platform


namespace sprt {

/**
	Method for determining standard paths to application files.

	ExecutableRelative - search for files and resources in directories
	located next to the executable file

	SystemRelative - use standard common system paths (XDG on Linux,
	AppData on Windows)

	ContainerRelative - use paths inside the application container. If the application
	is not in a container, simulate the container environment if possible

	ForceContainer - if possible, pack the application into a container at startup.
	If not possible, it works similarly to ContainerRelative

	If the application is running in a container and it knows about it,
	the ContainerRelative scheme will be used. Note, that some container engines
	are invisible for applications, in this cases you should use ExecutableRelative
	or SystemRelative schemes.

	Default scheme defined with APPCONFIG_APP_PATH_COMMON when building application.
*/
enum class AppLocationScheme {
	ExecutableRelative = 0,
	SystemRelative = 1,
	ContainerRelative = 2,
	ForceContainer = 3,
};

struct AppConfig {
	StringView bundleName;
	StringView appName;
	StringView bundlePath;

	uint32_t versionCode = 0;
	AppLocationScheme pathScheme = AppLocationScheme::ExecutableRelative;
};

// Static-safe check
SPRT_API bool isInitialized();

SPRT_API bool initialize(AppConfig &&, int &resultCode);

SPRT_API void terminate();

SPRT_API const AppConfig &getAppConfig();

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_PLATFORM_H_
