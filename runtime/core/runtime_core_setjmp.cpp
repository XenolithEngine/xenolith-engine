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

#define __SPRT_BUILD 1

#include <sprt/c/__sprt_setjmp.h>
#include <sprt/c/__sprt_string.h>
#include <sprt/c/__sprt_pthread.h>

#if SPRT_WINDOWS

#include <sprt/wrappers/windows/context_api.h>

#else

#include <stdlib.h>
#include <stdio.h>
#include <setjmp.h>
#include <unwind.h>

#include "private/SPRTThread.h"

#endif

//static_assert(sizeof(jmp_buf) == sizeof(__sprt_native_jmp_buf));

#include <sprt/cxx/detail/ctypes.h>

namespace sprt {

__SPRT_ID(setjmp_fn) get_setjmp_fn();

__SPRT_C_FUNC __SPRT_ID(setjmp_fn) __SPRT_ID(get_setjmp_fn)() {
#if SPRT_LINUX || SPRT_ANDROID || SPRT_NMACOS
	return &setjmp;
#elif SPRT_WINDOWS
	return get_setjmp_fn();
#else
#error Not implemented
#endif
}

__SPRT_C_FUNC int __SPRT_ID(cfa_setjmp)(int arg, __SPRT_ID(jmp_buf) buf) {
	if (arg != 0) {
		return arg;
	}

	struct CFALookup {
		int offset = 1; // lookup for an address for frame directly above us
		uintptr_t result = 0;
	} lookup;

#if SPRT_LINUX || SPRT_ANDROID || SPRT_NMACOS
	_Unwind_Backtrace([](struct _Unwind_Context *ctx, void *l) {
		CFALookup *lookup = (CFALookup *)l;
		if (--lookup->offset > 0) {
			return _URC_NO_REASON;
		}
		lookup->result = _Unwind_GetCFA(ctx);
		return _URC_END_OF_STACK;
	}, &lookup);
#endif
	buf->__cfa = lookup.result;

	return 0;
}

__SPRT_C_FUNC __SPRT_NORETURN void __SPRT_ID(longjmp)(__SPRT_ID(jmp_buf) buf, int ret) {
#if SPRT_LINUX || SPRT_ANDROID || SPRT_MACOS
	using jmp_buf_t = decltype(buf);
	// TODO: Maybe, add some additional info for unwinder?

	// Preserve result on jmp_buf.
	// It's safe to know that we will not use anything from stack before jmp_buf
	buf->__result = ret;

	auto code = _Unwind_ForcedUnwind(&_thread::thread_t::self()->unwinder.excpt,
			[](int version, _Unwind_Action actions, _Unwind_Exception_Class exceptionClass,
					_Unwind_Exception *exceptionObject, struct _Unwind_Context *context,
					void *stop_parameter) {
		auto buf = reinterpret_cast<jmp_buf_t>(stop_parameter);
		if (actions & _UA_END_OF_STACK) {
			fprintf(stderr, "%s",
					"End of stack is reached in longjmp; It means that jmp_buf pointing to invalid "
					"location, that was not found on current thread's stack; aborting;");
			abort();
		} else if (buf->__cfa == _Unwind_GetCFA(context)) {
#if SPRT_LINUX
			longjmp(reinterpret_cast<struct __jmp_buf_tag *>(buf->__native), buf->__result);
#else
			longjmp(buf->__native, buf->__result);
#endif
		}
		return _URC_NO_REASON;
	},
			buf);
	sprt_passert(code, "__sprt_longjmp: _Unwind_ForcedUnwind failed");
	abort();
#elif SPRT_WINDOWS
	// On windows, longjmp is already an SPRT wrapper (see libc_impl/src/windows/except.cc)
	longjmp((_JUMP_BUFFER *)buf->__native, ret);
#else
#error Not implemented
#endif
}

} // namespace sprt
