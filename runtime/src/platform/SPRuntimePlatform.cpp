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

#include <sprt/runtime/string.h>
#include <sprt/runtime/detail/new.h>
#include "private/SPRTPrivate.h"

#if SPRT_LINUX || SPRT_ANDROID || SPRT_MACOS
#include "SPRuntimePlatform-posix.cc"
#endif

#if SPRT_LINUX
#include "SPRuntimePlatform-linux.cc"
#endif

#if SPRT_ANDROID
#include "SPRuntimePlatform-android.cc"
#include "SPRuntimeJni.cc"
#endif

#include <locale.h>

namespace sprt {

const nothrow_t nothrow;

bool initialize(AppConfig &&cfg, int &resultCode) {
#if SPRT_WINDOWS
	// force Windows to use UTF-8
	::setlocale(LC_ALL, "*.UTF8");
#endif

	memory::pool::initialize();
	if (platform::initialize(sprt::move(cfg), resultCode)) {
		backtrace::initialize();
		filesystem::initialize();
		return true;
	}
	memory::pool::terminate();
	return false;
}

void terminate() {
	filesystem::terminate();
	backtrace::terminate();
	memory::pool::terminate();
	platform::terminate();
}

} // namespace sprt
