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
#include <sprt/cxx/new>

#include "private/SPRTPrivate.h"

#if SPRT_LINUX || SPRT_ANDROID || SPRT_APPLE || SPRT_WINDOWS || SPRT_NUTTX || SPRT_EMBOX
#include "SPRuntimePlatform-posix.cc"
#endif

#if SPRT_LINUX
#include "SPRuntimePlatform-linux.cc"
#endif

#if SPRT_WINDOWS
#include "SPRuntimePlatform-windows.cc"
#endif

#if SPRT_WASM
#include "SPRuntimePlatform-wasm.cc"
#endif

#if SPRT_NUTTX
#include "SPRuntimePlatform-nuttx.cc"
#endif

#if SPRT_EMBOX
#include "SPRuntimePlatform-embox.cc"
#endif

#include <locale.h>

namespace sprt::platform {

// Shared by every platform's initialize()/terminate(); see the comment on _pool
// in private/SPRTPrivate.h for why the pool is not a static-lifetime object.
void GlobalConfig::init() {
	if (!_pool) {
		_pool = memory::pool::create(memory::self_contained_allocator);
	}
}

void GlobalConfig::term() {
	// Every view below points into _pool and dies with it. Clearing them is the
	// load-bearing half: a StringView left pointing at freed pool memory still
	// looks valid, so the next initialize() would read it instead of failing.
	// infoMutex is deliberately not touched — it is live state, not pool data.
	uniqueIdBuf = StringView();
	execPathBuf = StringView();
	homePathBuf = StringView();
	locale = StringView(localeBuf);
	config = AppConfig();
	current = filesystem::LocationInfo();

	if (_pool) {
		memory::pool::destroy(_pool);
		_pool = nullptr;
	}
}

} // namespace sprt::platform

namespace sprt {

static int s_isInitialized = 0;

bool isInitialized() { return s_isInitialized == 1; }

bool initialize(AppConfig &&cfg, int &resultCode) {
	memory::pool::initialize();
	if (platform::initialize(sprt::move(cfg), resultCode)) {
		backtrace::initialize();
		filesystem::initialize();
		s_isInitialized = 1;
		return true;
	}
	// platform::initialize() may already have taken the config pool before it
	// failed, so it gets torn down here too.
	platform::terminate();
	memory::pool::terminate();
	return false;
}

void terminate() {
	s_isInitialized = 0;
	// Exact reverse of initialize(). platform::terminate() destroys the config
	// pool, which everything above allocates out of, so it has to run before
	// memory::pool::terminate() takes the pool subsystem down — the old order
	// had it last, which only ever worked because it did nothing.
	filesystem::terminate();
	backtrace::terminate();
	platform::terminate();
	memory::pool::terminate();
}

} // namespace sprt
