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

#include "SPRuntimeString.h"
#include "private/SPRTPrivate.h"
#include <locale.h>

#if SPRT_LINUX || SPRT_ANDROID || SPRT_MACOS
#include "core/SPRuntimePlatform-posix.cc"
#endif

#if SPRT_LINUX
#include "core/SPRuntimePlatform-linux.cc"
#endif

#if SPRT_ANDROID
#include "core/SPRuntimePlatform-android.cc"
#include "core/SPRuntimeJni.cc"
#endif

namespace sprt {

bool initialize(int &resultCode) {
#if SPRT_WINDOWS
	// force Windows to use UTF-8
	::setlocale(LC_ALL, ".UTF8");
#endif
	if (platform::initialize(resultCode)) {
		backtrace::initialize();
		return true;
	}
	return false;
}

void terminate() {
	backtrace::terminate();
	platform::terminate();
}

} // namespace sprt
