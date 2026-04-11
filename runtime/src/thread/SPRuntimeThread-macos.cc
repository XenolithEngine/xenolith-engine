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

#include "private/SPRTThread.h"

#if SPRT_MACOS

typedef struct objc_object *id;
typedef struct objc_class *Class;
typedef struct objc_selector *SEL;

extern "C" void objc_msgSend(void);
extern "C" Class objc_getClass(const char *name);
extern "C" SEL sel_getUid(const char *str);

namespace sprt::_thread {

using AutoreleasePool_new_type = id (*)(Class, SEL);
using AutoreleasePool_drain_type = void (*)(id, SEL);

template <typename Callback>
static void ThreadCallbacks_performInAutorelease(Callback &&cb) {
	id pool = ((AutoreleasePool_new_type)&objc_msgSend)(objc_getClass("NSAutoreleasePool"),
			sel_getUid("new"));
	cb();
	((AutoreleasePool_drain_type)&objc_msgSend)(pool, sel_getUid("drain"));
}

void _entry_platform(const callbacks &cb, NotNull<Ref> tm) {
	ThreadCallbacks_performInAutorelease([&] { _init(cb, tm); });

	bool ret = true;
	while (ret) {
		ThreadCallbacks_performInAutorelease([&] { ret = _worker(cb, tm); });
	}

	ThreadCallbacks_performInAutorelease([&] { _dispose(cb, tm); });
}

} // namespace sprt::_thread

#endif
