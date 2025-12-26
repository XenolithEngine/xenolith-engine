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
#include "SPThreadTaskQueue.h"
#include "SPThread.h"

#if (MACOS)

#include <objc/runtime.h>
#include <objc/message.h>

namespace STAPPLER_VERSIONIZED stappler::thread {

struct ThreadCallbacks;

using AutoreleasePool_new_type = id (*)(Class, SEL);
using AutoreleasePool_drain_type = void (*)(id, SEL);

using NSThread_currentThread_type = id (*)(Class, SEL);
using NSString_stringWithUTF8String_type = id (*)(Class, SEL, const char *);
using NSThread_setName_type = id (*)(id, SEL, id);

static void ThreadCallbacks_init(const ThreadCallbacks &, Thread *tm);
static bool ThreadCallbacks_worker(const ThreadCallbacks &, Thread *tm);
static void ThreadCallbacks_dispose(const ThreadCallbacks &, Thread *tm);

template <typename Callback>
static void ThreadCallbacks_performInAutorelease(Callback &&cb) {
	id pool = ((AutoreleasePool_new_type)&objc_msgSend)(objc_getClass("NSAutoreleasePool"),
			sel_getUid("new"));
	cb();
	((AutoreleasePool_drain_type)&objc_msgSend)(pool, sel_getUid("drain"));
}

SP_LOCAL static void _setThreadName(StringView name) {
	id thread = ((NSThread_currentThread_type)&objc_msgSend)(objc_getClass("NSThread"),
			sel_getUid("currentThread"));
	id string = ((NSString_stringWithUTF8String_type)&objc_msgSend)(objc_getClass("NSString"),
			sel_getUid("stringWithUTF8String:"),
			name.terminated() ? name.data() : name.str<memory::StandartInterface>().data());

	((NSThread_setName_type)&objc_msgSend)(thread, sel_getUid("setName:"), string);
}

SP_LOCAL static void _workerThread(const ThreadCallbacks &cb, Thread *tm) {
	ThreadCallbacks_performInAutorelease([&] { ThreadCallbacks_init(cb, tm); });

	bool ret = true;
	while (ret) {
		ThreadCallbacks_performInAutorelease([&] { ret = ThreadCallbacks_worker(cb, tm); });
	}

	ThreadCallbacks_performInAutorelease([&] { ThreadCallbacks_dispose(cb, tm); });
}

} // namespace stappler::thread

#endif
