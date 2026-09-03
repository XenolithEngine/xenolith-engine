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

#elif SPRT_WASM

// Freestanding wasm has no system <setjmp.h>/<unwind.h>. A working setjmp/longjmp
// requires the wasm exception-handling lowering (-fwasm-exceptions), which is a
// later milestone (wasm-port-draft.adoc §7); the skeleton only needs to compile,
// so the entry points below are stubs.

#elif SPRT_EMBOX_USER

// setjmp/longjmp ARE real here -- musl's aarch64 pair, built into
// runtime_musl_libc -- so this target belongs with the platform-libc branches
// below, not with the wasm stubs. What it cannot do is reach them through
// <setjmp.h>: with -nostdinc the umbrella forwards to a system header that does
// not exist, and runtime_core is compiled hosted (__STDC_HOSTED__ == 1) so the
// umbrella takes that fork. Declare the four entries against the sprt native
// types instead, the way core/embox_user/libc.h does for the rest of the libc.
//
// <unwind.h> resolves: it is a compiler header, and target.mk points
// -resource-dir at this sysroot's lib/clang.
#include <unwind.h>

#include <sprt/c/cross/__sprt_file_ptr.h>
#include <sprt/c/cross/__sprt_setjmp.h>

extern "C" {
int setjmp(__SPRT_ID(native_jmp_buf)) __SPRT_NOEXCEPT;
__SPRT_NORETURN void longjmp(__SPRT_ID(native_jmp_buf), int) __SPRT_NOEXCEPT;
__SPRT_NORETURN void siglongjmp(__SPRT_ID(native_sigjmp_buf), int) __SPRT_NOEXCEPT;

// The diagnostic path below reports an unrecoverable longjmp before aborting.
extern __SPRT_ID(FILE) * stderr;
int fprintf(__SPRT_ID(FILE) * __SPRT_RESTRICT, const char *__SPRT_RESTRICT, ...) __SPRT_NOEXCEPT;
__SPRT_NORETURN void abort(void) __SPRT_NOEXCEPT;
}

#else

#include <stdlib.h>
#include <stdio.h>
#include <setjmp.h>
#include <unwind.h>

#endif

// Only where jmp_buf IS the platform's. On the targets whose libc is ours,
// <setjmp.h>'s jmp_buf is the sprt WRAPPER (__sprt___ext_jmp_buf: the native
// buffer plus the CFA and the pending result), which is deliberately larger than
// the native one -- so the comparison would be between two different things.
#if !defined(SPRT_WINDOWS) && !defined(SPRT_WASM) && !defined(SPRT_EMBOX_USER)
static_assert(sizeof(jmp_buf) == sizeof(__sprt_native_jmp_buf));
static_assert(sizeof(sigjmp_buf) == sizeof(__sprt_native_sigjmp_buf));
#endif

#include <sprt/cxx/detail/ctypes.h>

#include "pthread/pthread_thread_t.h"

namespace sprt {

#if SPRT_WINDOWS
__SPRT_ID(setjmp_fn) get_setjmp_fn();
__SPRT_ID(sigsetjmp_fn) get_sigsetjmp_fn();
#endif

#if defined(__SPRT_UNWIND_DLOPEN) && __SPRT_UNWIND_DLOPEN
// Defined in the broker layer below. The unwinder is borrowed and may not be
// there at all — longjmp has to survive that rather than die.
bool __unwinder_available();
#else
// The unwinder is linked in, so it is always there.
SPRT_UNUSED static inline bool __unwinder_available() { return true; }
#endif

#if SPRT_WASM
// wasm has no stack-switching setjmp/longjmp yet (it needs the -fwasm-exceptions
// lowering, a later milestone). Per the runtime contract, setjmp is a no-op that
// returns 0 — the ordinary "first return" — and longjmp traps (below): a jump
// into an already-dead frame cannot be honoured. These shims give
// get_*setjmp_fn() a callable no-op to return instead of a null pointer, which
// the __sprt_setjmp macro would otherwise call and crash on.
static int __wasm_setjmp_noop(__SPRT_ID(native_jmp_buf)) { return 0; }
static int __wasm_sigsetjmp_noop(__SPRT_ID(native_sigjmp_buf), int) { return 0; }
#endif

#if SPRT_EMBOX
// Embox's longjmp does not restore x29 (the frame pointer).
//
// src/arch/aarch64/lib/setjmp.S saves lr, sp and x19-x28 — twelve slots, which
// is the whole of its jmp_buf — and stops there. x29 is callee-saved under
// AAPCS64 too, and clang addresses the locals of the frame that called setjmp
// through it (`stur w8, [x29, #-0xc]`), so returning into that frame with a
// stale x29 makes every local read garbage AND makes every store land in dead
// stack below sp. The symptom is quiet rather than loud: a value written and
// read back through the same stale pointer still matches, and only something
// stored BEFORE the setjmp shows the corruption.
//
// The buffer cannot simply be widened to hold x29 (and d8-d15, which Embox
// drops as well). sizeof(__sprt_jmp_buf) is ABI: every third-party library on
// this target that embeds a jmp_buf — FreeType's FT_ValidatorRec, libpng's
// png_jmpbuf — has the old size compiled into its own structs, and a wider
// buffer overruns them. Verified the hard way: it lands squarely on
// tt_face_build_cmaps' locals.
//
// So the value is not saved, it is RECOVERED. Every longjmp here goes through
// _Unwind_ForcedUnwind, and by the time the stop function recognises the target
// frame the unwinder has already restored that frame's register set inside the
// _Unwind_Context — including x29, which _Unwind_GetGR(context, 29) hands over.
// The restorer below takes it as a third argument; everything else comes out of
// Embox's own buffer layout, unchanged:
//     [0] x30  [8] sp  [16] x19 x20  [32] x21 x22 ... [80] x27 x28
//
// d8-d15 stay unrestored, exactly as under Embox's own longjmp — the public
// _Unwind_ API exposes no FP registers, and matching the platform's existing
// behaviour is not a regression.
#if !defined(__aarch64__)
#error sprt longjmp for Embox is implemented for aarch64 only
#endif

extern "C" __SPRT_NORETURN void __sprt_embox_longjmp_fp(__SPRT_ID(native_jmp_buf), int,
		__SPRT_ID(uintptr_t));

// clang-format off
__asm__(
	".text\n"
	".globl __sprt_embox_longjmp_fp\n"
	".hidden __sprt_embox_longjmp_fp\n"
	".type __sprt_embox_longjmp_fp, %function\n"
"__sprt_embox_longjmp_fp:\n"
	"ldp x30, x3, [x0]\n"
	"mov sp, x3\n"
	"ldp x19, x20, [x0, #16]\n"
	"ldp x21, x22, [x0, #32]\n"
	"ldp x23, x24, [x0, #48]\n"
	"ldp x25, x26, [x0, #64]\n"
	"ldp x27, x28, [x0, #80]\n"
	"mov x29, x2\n"
	// The 0 -> 1 conversion ISO C requires already happened in __sprt_longjmp,
	// the only caller; pass the value through unchanged.
	"mov w0, w1\n"
	"ret\n"
	".size __sprt_embox_longjmp_fp, .-__sprt_embox_longjmp_fp\n"
);
// clang-format on
#endif

__SPRT_C_FUNC __SPRT_ID(setjmp_fn) __SPRT_ID(get_setjmp_fn)() {
#if SPRT_LINUX || SPRT_ANDROID || SPRT_APPLE || SPRT_NUTTX || SPRT_EMBOX || SPRT_EMBOX_USER
	return reinterpret_cast<__SPRT_ID(setjmp_fn)>(&setjmp);
#elif SPRT_WINDOWS
	return get_setjmp_fn();
#elif SPRT_WASM
	// No-op setjmp (returns 0); see __wasm_setjmp_noop above.
	return reinterpret_cast<__SPRT_ID(setjmp_fn)>(&__wasm_setjmp_noop);
#else
#error Not implemented
#endif
}

__SPRT_C_FUNC __SPRT_ID(sigsetjmp_fn) __SPRT_ID(get_sigsetjmp_fn)() {
#if SPRT_LINUX
#ifdef __GLIBC__
	// glibc exposes `sigsetjmp` only as a macro forwarding to `__sigsetjmp`
	return reinterpret_cast<__SPRT_ID(sigsetjmp_fn)>(&__sigsetjmp);
#else
	// musl declares `sigsetjmp` as a real function and has no `__sigsetjmp`
	return reinterpret_cast<__SPRT_ID(sigsetjmp_fn)>(&sigsetjmp);
#endif
#elif SPRT_ANDROID || SPRT_APPLE
	return reinterpret_cast<__SPRT_ID(sigsetjmp_fn)>(&sigsetjmp);
#elif SPRT_WINDOWS
	return get_sigsetjmp_fn();
#elif SPRT_WASM
	// No-op sigsetjmp (returns 0); see __wasm_sigsetjmp_noop above.
	return reinterpret_cast<__SPRT_ID(sigsetjmp_fn)>(&__wasm_sigsetjmp_noop);
#elif SPRT_EMBOX
	// Embox DECLARES sigsetjmp/siglongjmp in <setjmp.h> (marked "stubs" there)
	// and defines neither; it has no sigprocmask either, so there is no signal
	// mask to save in the first place. sigjmp_buf and jmp_buf are the same type
	// here, so the plain saver fills the same buffer and the savemask argument is
	// simply ignored (aarch64 drops the extra register). Same shape as the NuttX
	// branch below.
	return reinterpret_cast<__SPRT_ID(sigsetjmp_fn)>(&setjmp);
#elif SPRT_NUTTX || SPRT_EMBOX_USER
	// Same as the Embox branch above: sigjmp_buf and jmp_buf are one type, and
	// there is no signal mask to save until K8, so the plain saver fills the same
	// buffer and savemask is ignored (aarch64 drops the extra argument).
	return reinterpret_cast<__SPRT_ID(sigsetjmp_fn)>(&setjmp);
#else
#error Not implemented
#endif
}

__SPRT_C_FUNC int __SPRT_ID(cfa_setjmp)(int arg, __SPRT_ID(jmp_buf) buf) {
	if (arg != 0) {
		return arg;
	}

	struct CFALookup {
		// Capture the CFA of the frame ONE ABOVE us — the function that called setjmp,
		// whose frame the jmp_buf belongs to and which longjmp's forced-unwind must find.
		// _Unwind_Backtrace visits this cfa_setjmp frame first, so with the pre-decrement
		// (`--offset > 0`) below we need offset=2 to skip our own frame and land on the
		// caller.
		int offset = 2;
		uintptr_t result = 0;
	} lookup;

#if SPRT_LINUX || SPRT_ANDROID || SPRT_APPLE || SPRT_NUTTX || SPRT_EMBOX
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

__SPRT_C_FUNC int __SPRT_ID(cfa_sigsetjmp)(int arg, __SPRT_ID(sigjmp_buf) buf, int savemask) {
	if (arg != 0) {
		return arg;
	}

	struct CFALookup {
		// Capture the CFA of the frame ONE ABOVE us — the function that called setjmp,
		// whose frame the jmp_buf belongs to and which longjmp's forced-unwind must find.
		// _Unwind_Backtrace visits this cfa_setjmp frame first, so with the pre-decrement
		// (`--offset > 0`) below we need offset=2 to skip our own frame and land on the
		// caller.
		int offset = 2;
		uintptr_t result = 0;
	} lookup;

#if SPRT_LINUX || SPRT_ANDROID || SPRT_APPLE || SPRT_NUTTX || SPRT_EMBOX
	_Unwind_Backtrace([](struct _Unwind_Context *ctx, void *l) {
		CFALookup *lookup = (CFALookup *)l;
		if (--lookup->offset > 0) {
			return _URC_NO_REASON;
		}
		lookup->result = _Unwind_GetCFA(ctx);
		return _URC_END_OF_STACK;
	}, &lookup);
#elif SPRT_WINDOWS
	if (savemask) {
		__sprt_sigset_t current;
		__sprt_sigset_t empty;
		__sprt_sigemptyset(&empty);
		__sprt_sigprocmask(__SPRT_SIG_BLOCK, &empty, &current);
		lookup.result = current.__bits[0];
	} else {
		lookup.result = Max<uintptr_t>;
	}
#endif

#if SPRT_NUTTX
	buf->__native->savemask = savemask;
	if (savemask) {
		::sigprocmask(0, nullptr, &buf->__native->sigmask);
	}
#endif

	buf->__cfa = lookup.result;

	return 0;
}

__SPRT_C_FUNC __SPRT_NORETURN void __SPRT_ID(longjmp)(__SPRT_ID(jmp_buf) buf, int ret) {
#if SPRT_LINUX || SPRT_ANDROID || SPRT_APPLE || SPRT_NUTTX || SPRT_EMBOX || SPRT_EMBOX_USER
	using jmp_buf_t = decltype(buf);
	// TODO: Maybe, add some additional info for unwinder?

	// Preserve result on jmp_buf.
	// It's safe to know that we will not use anything from stack before jmp_buf
#if SPRT_EMBOX
	// ISO C 7.13.2.1: longjmp(buf, 0) must make setjmp return 1. Every other
	// platform's libc does that conversion inside longjmp; Embox's aarch64 asm
	// (src/arch/aarch64/lib/setjmp.S) returns its second argument verbatim, so
	// it has to happen here — this is the only place that value passes through.
	buf->__result = ret ? ret : 1;
#else
	buf->__result = ret;
#endif

	// The jump must happen even without an unwinder: longjmp is what C requires,
	// running destructors on the way is our addition on top of it. We lose the
	// addition, not the function. The broker layer prints the warning, once per
	// process.
	if (!__unwinder_available()) {
#if SPRT_LINUX
		longjmp(reinterpret_cast<struct __jmp_buf_tag *>(buf->__native), buf->__result);
#else
		longjmp(buf->__native, buf->__result);
#endif
	}

	[[maybe_unused]]
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
#elif SPRT_EMBOX
			// x29 comes out of the unwind context; see __sprt_embox_longjmp_fp.
			__sprt_embox_longjmp_fp(buf->__native, buf->__result, _Unwind_GetGR(context, 29));
#else
			longjmp(buf->__native, buf->__result);
#endif
		}
		return _URC_NO_REASON;
	},
			buf);
	sprt_passert(code, "__sprt_longjmp: _Unwind_ForcedUnwind failed");
	abort();
	// Embox declares abort() without _Noreturn, so without this the compiler
	// thinks this __SPRT_NORETURN function can fall off its end.
	__builtin_unreachable();
#elif SPRT_WINDOWS
	// On windows, longjmp is already an SPRT wrapper (see libc_impl/src/windows/except.cc)
	longjmp((_JUMP_BUFFER *)buf->__native, ret);
#elif SPRT_WASM
	// TODO(wasm-eh): unwind via wasm exceptions. Until then a longjmp cannot be
	// honored; trap rather than silently returning into a dead frame.
	(void)buf;
	(void)ret;
	__builtin_trap();
#else
#error Not implemented
#endif
}

__SPRT_C_FUNC __SPRT_NORETURN void __SPRT_ID(siglongjmp)(__SPRT_ID(sigjmp_buf) buf, int ret) {
#if SPRT_LINUX || SPRT_ANDROID || SPRT_APPLE || SPRT_NUTTX || SPRT_EMBOX || SPRT_EMBOX_USER
	using jmp_buf_t = decltype(buf);
	// TODO: Maybe, add some additional info for unwinder?

	// Preserve result on jmp_buf.
	// It's safe to know that we will not use anything from stack before jmp_buf
#if SPRT_EMBOX
	// ISO C 7.13.2.1: longjmp(buf, 0) must make setjmp return 1. Every other
	// platform's libc does that conversion inside longjmp; Embox's aarch64 asm
	// (src/arch/aarch64/lib/setjmp.S) returns its second argument verbatim, so
	// it has to happen here — this is the only place that value passes through.
	buf->__result = ret ? ret : 1;
#else
	buf->__result = ret;
#endif

	// See __sprt_longjmp: with no unwinder we jump without running destructors.
	if (!__unwinder_available()) {
#if SPRT_LINUX
		siglongjmp(reinterpret_cast<struct __jmp_buf_tag *>(buf->__native), buf->__result);
#elif SPRT_EMBOX
		// Embox defines no siglongjmp (see get_sigsetjmp_fn) and has no signal
		// mask to restore, so the plain jump IS the whole of the contract here.
		longjmp(buf->__native, buf->__result);
#else
		siglongjmp(buf->__native, buf->__result);
#endif
	}

	[[maybe_unused]]
	auto code = _Unwind_ForcedUnwind(&_thread::thread_t::self()->unwinder.excpt,
			[](int version, _Unwind_Action actions, _Unwind_Exception_Class exceptionClass,
					_Unwind_Exception *exceptionObject, struct _Unwind_Context *context,
					void *stop_parameter) {
		auto buf = reinterpret_cast<jmp_buf_t>(stop_parameter);
		if (actions & _UA_END_OF_STACK) {
			fprintf(stderr, "%s",
					"End of stack is reached in siglongjmp; It means that sigjmp_buf pointing to "
					"invalid " "location, that was not found on current thread's stack; aborting;");
			abort();
		} else if (buf->__cfa == _Unwind_GetCFA(context)) {
#if SPRT_LINUX
			siglongjmp(reinterpret_cast<struct __jmp_buf_tag *>(buf->__native), buf->__result);
#elif SPRT_EMBOX
			// See the no-unwinder branch above; x29 comes out of the unwind
			// context, as in __sprt_longjmp.
			__sprt_embox_longjmp_fp(buf->__native, buf->__result, _Unwind_GetGR(context, 29));
#else
			siglongjmp(buf->__native, buf->__result);
#endif
		}
		return _URC_NO_REASON;
	},
			buf);
	sprt_passert(code, "__sprt_siglongjmp: _Unwind_ForcedUnwind failed");
	abort();
	// Embox declares abort() without _Noreturn, so without this the compiler
	// thinks this __SPRT_NORETURN function can fall off its end.
	__builtin_unreachable();
#elif SPRT_WINDOWS
	// On windows, longjmp is already an SPRT wrapper (see libc_impl/src/windows/except.cc)
	if (buf->__cfa != Max<uintptr_t>) {
		__sprt_sigset_t restoreMask;
		__sprt_sigemptyset(&restoreMask);
		restoreMask.__bits[0] = buf->__cfa;
		__sprt_sigprocmask(__SPRT_SIG_SETMASK, &restoreMask, nullptr);
	}
	longjmp((_JUMP_BUFFER *)buf->__native, ret);
#elif SPRT_WASM
	// TODO(wasm-eh): see __sprt_longjmp above.
	(void)buf;
	(void)ret;
	__builtin_trap();
#else
#error Not implemented
#endif
}

} // namespace sprt


// The unwinder as a BORROWED resource rather than a linked-in one.
//
// Why. There must be exactly one unwinder per process: its mutable state is the
// FDE table, into which a JIT (a shader compiler inside a GPU driver, say)
// registers frames through __register_frame. A second copy cannot see that
// table, and an unwind dies at the first JIT frame. On top of that
// _Unwind_Context is opaque: a foreign personality routine reads it only through
// _Unwind_Get*/_Unwind_Set*, and those accessors must come from the same copy
// that built the context.
//
// Why dlopen and not -lgcc_s / -lunwind. The runtime ships neither to its
// consumer, so putting them on the link line would force a dependency the
// consumer may not have. dlopen creates no such dependency.
//
// Why libgcc_s.so.1 first. That is the very library glibc itself loads
// (misc/unwind-link.c) for pthread_exit, pthread_cancel and backtrace(), and it
// loads it with the same dlopen on the same SONAME. So we end up with pointers
// into THE SAME loaded object and our copy coincides with glibc's. On a glibc
// system libgcc_s.so.1 is always present: without it pthread_exit already dies
// today, and sprt calls it on every thread teardown. In the Xenolith OS image
// libgcc_s.so.1 is our own shim, which defines no entry points itself — but
// dlsym on its handle searches dependencies too, and it declares NEEDED on
// libunwind.so.1.
//
// Resolution order: libgcc_s.so.1 -> libunwind.so.1 -> nothing. The second step
// covers environments with no gcc runtime at all but an LLVM one; the third is
// not fatal, see unwinderUnavailable() below.

#if defined(__SPRT_UNWIND_DLOPEN) && __SPRT_UNWIND_DLOPEN

#include <sprt/c/__sprt_dlfcn.h>

#include <stdio.h>
#include <stdlib.h>
#include <unwind.h>

namespace sprt {
namespace {

// In order of preference, see the note above.
constexpr const char *s_unwinderNames[] = {"libgcc_s.so.1", "libunwind.so.1"};

struct UnwindLink {
	__typeof(_Unwind_RaiseException) *RaiseException;
	__typeof(_Unwind_Resume) *Resume;
	__typeof(_Unwind_Resume_or_Rethrow) *Resume_or_Rethrow;
	__typeof(_Unwind_DeleteException) *DeleteException;
	__typeof(_Unwind_ForcedUnwind) *ForcedUnwind;
	__typeof(_Unwind_Backtrace) *Backtrace;
	__typeof(_Unwind_GetGR) *GetGR;
	__typeof(_Unwind_SetGR) *SetGR;
	__typeof(_Unwind_GetIP) *GetIP;
	__typeof(_Unwind_SetIP) *SetIP;
	__typeof(_Unwind_GetIPInfo) *GetIPInfo;
	__typeof(_Unwind_GetCFA) *GetCFA;
	__typeof(_Unwind_GetLanguageSpecificData) *GetLanguageSpecificData;
	__typeof(_Unwind_GetRegionStart) *GetRegionStart;
	__typeof(_Unwind_GetDataRelBase) *GetDataRelBase;
	__typeof(_Unwind_GetTextRelBase) *GetTextRelBase;
	__typeof(_Unwind_FindEnclosingFunction) *FindEnclosingFunction;
	__typeof(__register_frame) *register_frame;
	__typeof(__deregister_frame) *deregister_frame;
};

UnwindLink s_link;

// 0 — not resolved yet, 1 — done. A race here is benign: dlopen is thread-safe
// and hands out one handle per SONAME, so two threads would write the very same
// values into s_link. The only thing that has to be ordered is publishing the
// flag.
int s_ready = 0;

// A missing unwinder is no reason to die. It buys us the extras — destructors
// running on longjmp, a meaningful backtrace — not the functions themselves, so
// every broker returns a state from which the caller sees "nothing was unwound"
// and carries on: END_OF_STACK for the walkers, zero for the getters, a no-op
// for the setters. Reported once per process; inside a loop this would
// otherwise flood the output.
int s_reported = 0;

void unwinderUnavailable(const char *what) {
	if (__atomic_exchange_n(&s_reported, 1, __ATOMIC_RELAXED) != 0) {
		return;
	}
	fprintf(stderr,
			"sprt: no unwinder available (neither libgcc_s.so.1 nor libunwind.so.1 could be "
			"loaded); %s and any further unwinder call degrades to a no-op. C++ destructors will "
			"not run on longjmp and backtraces will be empty.\n",
			what);
}

void *resolveAll() {
	void *handle = nullptr;
	for (auto name : s_unwinderNames) {
		handle = ::__sprt_dlopen(name, __SPRT_RTLD_LAZY);
		if (handle) {
			// One probe is enough: if the object is there but carries no entry
			// points, the next candidate may well be a complete one.
			if (::__sprt_dlsym(handle, "_Unwind_ForcedUnwind")) {
				break;
			}
			::__sprt_dlclose(handle);
			handle = nullptr;
		}
	}
	return handle;
}

// Exactly one attempt: not found means not there, and re-scanning on every call
// would only burn time. s_ready is raised either way; null pointers left in
// s_link are handled by the brokers one by one.
void initLink() {
	if (__atomic_load_n(&s_ready, __ATOMIC_ACQUIRE)) {
		return;
	}

	void *h = resolveAll();
	if (!h) {
		__atomic_store_n(&s_ready, 1, __ATOMIC_RELEASE);
		return;
	}

#define SPRT_UNWIND_BIND(field, name) \
	s_link.field = (__typeof(s_link.field))::__sprt_dlsym(h, name)

	SPRT_UNWIND_BIND(RaiseException, "_Unwind_RaiseException");
	SPRT_UNWIND_BIND(Resume, "_Unwind_Resume");
	SPRT_UNWIND_BIND(Resume_or_Rethrow, "_Unwind_Resume_or_Rethrow");
	SPRT_UNWIND_BIND(DeleteException, "_Unwind_DeleteException");
	SPRT_UNWIND_BIND(ForcedUnwind, "_Unwind_ForcedUnwind");
	SPRT_UNWIND_BIND(Backtrace, "_Unwind_Backtrace");
	SPRT_UNWIND_BIND(GetGR, "_Unwind_GetGR");
	SPRT_UNWIND_BIND(SetGR, "_Unwind_SetGR");
	SPRT_UNWIND_BIND(GetIP, "_Unwind_GetIP");
	SPRT_UNWIND_BIND(SetIP, "_Unwind_SetIP");
	SPRT_UNWIND_BIND(GetIPInfo, "_Unwind_GetIPInfo");
	SPRT_UNWIND_BIND(GetCFA, "_Unwind_GetCFA");
	SPRT_UNWIND_BIND(GetLanguageSpecificData, "_Unwind_GetLanguageSpecificData");
	SPRT_UNWIND_BIND(GetRegionStart, "_Unwind_GetRegionStart");
	SPRT_UNWIND_BIND(GetDataRelBase, "_Unwind_GetDataRelBase");
	SPRT_UNWIND_BIND(GetTextRelBase, "_Unwind_GetTextRelBase");
	SPRT_UNWIND_BIND(FindEnclosingFunction, "_Unwind_FindEnclosingFunction");
	SPRT_UNWIND_BIND(register_frame, "__register_frame");
	SPRT_UNWIND_BIND(deregister_frame, "__deregister_frame");

#undef SPRT_UNWIND_BIND

	__atomic_store_n(&s_ready, 1, __ATOMIC_RELEASE);
}

// Resolve at startup rather than on first use: dlopen/dlsym take the ld.so load
// lock, and taking it in the middle of a forced unwind (thread cancellation, in
// particular) is a bad idea. Exactly the discipline glibc follows — it too
// resolves the whole set up front, in one go.
//
// The lazy path is kept in case something reaches us before this TU's static
// initialisation, and it is safe on its own: our first touch of the unwinder is
// either __sprt_cfa_setjmp (thread start or setjmp) or a backtrace, and neither
// happens inside an unwind.
struct UnwindLinkInit {
	UnwindLinkInit() { initLink(); }
};

[[maybe_unused]]
UnwindLinkInit s_init;

inline const UnwindLink *link() {
	if (!__atomic_load_n(&s_ready, __ATOMIC_ACQUIRE)) {
		initLink();
	}
	return &s_link;
}

} // namespace

// For __sprt_longjmp/__sprt_siglongjmp earlier in this file: they have to work
// without an unwinder too, just without running destructors.
bool __unwinder_available() { return link()->ForcedUnwind != nullptr; }

} // namespace sprt

// The brokers.
//
// musttail here is a correctness requirement, not an optimisation:
// _Unwind_Backtrace, _Unwind_ForcedUnwind, _Unwind_RaiseException and
// _Unwind_Resume capture the context of THEIR OWN caller. An ordinary call would
// leave the broker's frame behind and the unwind would start from it — and
// __sprt_cfa_setjmp, which counts exactly two frames up to the CFA it wants,
// would record the wrong one. musttail is a compile error when a tail call is
// impossible, so this cannot degrade silently.
//
// Applied to all of them, not just the four context-sensitive ones: uniformity
// is cheaper than remembering which may be called the ordinary way.

// visibility("default") is mandatory: the runtime is built with
// -fvisibility=hidden, and the linker turns a hidden symbol local before
// --export-dynamic-symbol from make/os/linux.mk ever gets to it. Without the
// attribute the brokers would only work inside the executable itself, leaving a
// plugin on its own copy of the unwinder — the whole point lost, and lost
// silently.
// The last parameter is the state a broker returns when the entry point is
// absent. It has to be one from which a caller expecting a real unwind sees
// "nothing was unwound" and carries on instead of dying:
//
//   stack walkers  END_OF_STACK — zero frames visited, stop/trace never called;
//   RaiseException FATAL_PHASE1_ERROR — nothing was thrown (we never get here
//                  anyway: the runtime is built -fno-exceptions);
//   getters        0. A context can only come out of a real unwind, so this is
//                  unreachable — but let it be defined;
//   setters,       no-op. With no unwinder there is nowhere for
//   __*register_frame  __register_frame to register anything.
#define SPRT_UNWIND_FWD(ret, name, params, args, field, absent) \
	extern "C" __attribute__((visibility("default"))) ret name params { \
		auto l = ::sprt::link(); \
		if (!l->field) { \
			::sprt::unwinderUnavailable(#name); \
			return absent; \
		} \
		__attribute__((musttail)) return l->field args; \
	}

SPRT_UNWIND_FWD(_Unwind_Reason_Code, _Unwind_RaiseException, (_Unwind_Exception * e), (e),
		RaiseException, _URC_FATAL_PHASE1_ERROR)
SPRT_UNWIND_FWD(void, _Unwind_Resume, (_Unwind_Exception * e), (e), Resume, void())
SPRT_UNWIND_FWD(_Unwind_Reason_Code, _Unwind_Resume_or_Rethrow, (_Unwind_Exception * e), (e),
		Resume_or_Rethrow, _URC_FATAL_PHASE1_ERROR)
SPRT_UNWIND_FWD(void, _Unwind_DeleteException, (_Unwind_Exception * e), (e), DeleteException,
		void())
SPRT_UNWIND_FWD(_Unwind_Reason_Code, _Unwind_ForcedUnwind,
		(_Unwind_Exception * e, _Unwind_Stop_Fn s, void *a), (e, s, a), ForcedUnwind,
		_URC_END_OF_STACK)
SPRT_UNWIND_FWD(_Unwind_Reason_Code, _Unwind_Backtrace, (_Unwind_Trace_Fn f, void *a), (f, a),
		Backtrace, _URC_END_OF_STACK)
SPRT_UNWIND_FWD(uintptr_t, _Unwind_GetGR, (struct _Unwind_Context * c, int i), (c, i), GetGR, 0)
SPRT_UNWIND_FWD(void, _Unwind_SetGR, (struct _Unwind_Context * c, int i, uintptr_t v), (c, i, v),
		SetGR, void())
SPRT_UNWIND_FWD(uintptr_t, _Unwind_GetIP, (struct _Unwind_Context * c), (c), GetIP, 0)
SPRT_UNWIND_FWD(void, _Unwind_SetIP, (struct _Unwind_Context * c, uintptr_t v), (c, v), SetIP,
		void())
SPRT_UNWIND_FWD(uintptr_t, _Unwind_GetIPInfo, (struct _Unwind_Context * c, int *b), (c, b),
		GetIPInfo, 0)
SPRT_UNWIND_FWD(uintptr_t, _Unwind_GetCFA, (struct _Unwind_Context * c), (c), GetCFA, 0)
SPRT_UNWIND_FWD(uintptr_t, _Unwind_GetLanguageSpecificData, (struct _Unwind_Context * c), (c),
		GetLanguageSpecificData, 0)
SPRT_UNWIND_FWD(uintptr_t, _Unwind_GetRegionStart, (struct _Unwind_Context * c), (c),
		GetRegionStart, 0)
SPRT_UNWIND_FWD(uintptr_t, _Unwind_GetDataRelBase, (struct _Unwind_Context * c), (c),
		GetDataRelBase, 0)
SPRT_UNWIND_FWD(uintptr_t, _Unwind_GetTextRelBase, (struct _Unwind_Context * c), (c),
		GetTextRelBase, 0)
SPRT_UNWIND_FWD(void *, _Unwind_FindEnclosingFunction, (void *pc), (pc), FindEnclosingFunction,
		nullptr)
SPRT_UNWIND_FWD(void, __register_frame, (const void *fde), (fde), register_frame, void())
SPRT_UNWIND_FWD(void, __deregister_frame, (const void *fde), (fde), deregister_frame, void())

#undef SPRT_UNWIND_FWD

#endif // __SPRT_UNWIND_DLOPEN
