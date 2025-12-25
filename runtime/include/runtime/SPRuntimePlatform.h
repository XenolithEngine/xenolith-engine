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

#ifndef CORE_RUNTIME_INCLUDE_SPRUNTIMEPLATFORM_H_
#define CORE_RUNTIME_INCLUDE_SPRUNTIMEPLATFORM_H_

#include "SPRuntimeInt.h"
#include "SPRuntimeString.h"

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

	// hardware clock tick counter with unknown monotonic resolution
	// see `rdtsc`
	Hardware
};


SPRT_API size_t makeRandomBytes(uint8_t *buf, size_t count);

// current time in microseconds
SPRT_API uint64_t clock(ClockType = ClockType::Default);

// current time in nanoseconds
SPRT_API uint64_t nanoclock(ClockType = ClockType::Default);

// sleep for the microseconds
SPRT_API void sleep(uint64_t microseconds);

SPRT_API uint32_t getMemoryPageSize();

SPRT_API StringView getOsLocale();

} // namespace sprt::platform


namespace sprt {

SPRT_API bool initialize(int &resultCode);

SPRT_API void terminate();

} // namespace sprt

#endif // CORE_RUNTIME_INCLUDE_SPRUNTIMEPLATFORM_H_
