/**
Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>

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

#include "SPCommon.h"

#include "SPThread.cc"
#include "platform/SPThreads-android.cc"
#include "platform/SPThreads-linux.cc"
#include "platform/SPThreads-win32.cc"
#include "platform/SPThreads-macos.cc"
#include "SPThreadTask.cc"
#include "SPThreadPool.cc"
#include "SPThreadTaskQueue.cc"

#include "SPSharedModule.h"
#include "stappler-buildconfig.h"

namespace STAPPLER_VERSIONIZED stappler::thread {

static SharedSymbol s_threadSharedSymbols[] = {
	SharedSymbol{"ThreadInfo::setThreadInfo", &ThreadInfo::setThreadInfo},
	SharedSymbol{"ThreadInfo::setThreadPool", &ThreadInfo::setThreadPool},
};

SP_USED static SharedModule s_threadSharedModule(buildconfig::MODULE_STAPPLER_THREADS_NAME,
		s_threadSharedSymbols, sizeof(s_threadSharedSymbols) / sizeof(SharedSymbol));

struct ThreadInitializer {
	static void init(void *ptr) {
		auto pool = memory::pool::acquire();

		ThreadInfo::setThreadInfo("Main");
		ThreadInfo::setThreadPool(pool);
	}

	static void term(void *ptr) { }

	ThreadInitializer() { addInitializer(this, &init, &term); }
};

static ThreadInitializer s_threadInit;

} // namespace stappler::thread
