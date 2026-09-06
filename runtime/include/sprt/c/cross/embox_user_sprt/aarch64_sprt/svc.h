// The raw aarch64 `svc` sequence for the Embox EL0 boundary.
//
// Not pulled in by any cross/__sprt_*.h dispatcher: this is the calling
// convention, not a set of values, so it is included explicitly by the two
// places that speak it (runtime/core/include/__el0_syscall.h and, through it,
// libc_impl's embox_user backend). Nothing else should issue an svc.
//
// The convention (ABI doc section 1.1): x8 carries the number, x0-x5 the
// arguments, x0 the result. A failure comes back as the NEGATED errno in x0,
// which is why the return type is signed and why every caller has to go through
// __el0_ret() rather than testing for -1: -1 is EPERM, and a syscall that
// legitimately returns a large unsigned value (mmap's address, brk's break) must
// not have it read as an error. The kernel's error range is [-4095, -1].
//
// The register-asm form and the "0"(x0) tie for the first argument are musl's
// (musl-libc/arch/aarch64/syscall_arch.h). They are not stylistic: the compiler
// has to be told that x0 is both an input and the output, or it is free to
// reload x0 after the svc.

#ifndef CORE_RUNTIME_INCLUDE_C_CROSS_EMBOX_USER_SPRT_AARCH64_SVC_H_
#define CORE_RUNTIME_INCLUDE_C_CROSS_EMBOX_USER_SPRT_AARCH64_SVC_H_

#include <sprt/c/bits/__sprt_def.h>

#if !SPRT_EMBOX_USER
#error "svc.h is the Embox EL0 boundary; it has no meaning on another platform"
#endif

// "memory" tells the compiler the kernel may read or write anything reachable,
// which is what makes it safe to pass a pointer to a stack object. "cc" covers
// the condition flags, which the svc handler does not preserve.
#define __SPRT_SVC_BODY(...) \
	__asm__ __volatile__("svc #0" : "=r"(__x0) : __VA_ARGS__ : "memory", "cc"); \
	return __x0

__SPRT_BEGIN_DECL

SPRT_FORCEINLINE long __sprt_svc0(long __nr) {
	register long __x8 __asm__("x8") = __nr;
	register long __x0 __asm__("x0");
	__SPRT_SVC_BODY("r"(__x8));
}

SPRT_FORCEINLINE long __sprt_svc1(long __nr, long __a) {
	register long __x8 __asm__("x8") = __nr;
	register long __x0 __asm__("x0") = __a;
	__SPRT_SVC_BODY("r"(__x8), "0"(__x0));
}

SPRT_FORCEINLINE long __sprt_svc2(long __nr, long __a, long __b) {
	register long __x8 __asm__("x8") = __nr;
	register long __x0 __asm__("x0") = __a;
	register long __x1 __asm__("x1") = __b;
	__SPRT_SVC_BODY("r"(__x8), "0"(__x0), "r"(__x1));
}

SPRT_FORCEINLINE long __sprt_svc3(long __nr, long __a, long __b, long __c) {
	register long __x8 __asm__("x8") = __nr;
	register long __x0 __asm__("x0") = __a;
	register long __x1 __asm__("x1") = __b;
	register long __x2 __asm__("x2") = __c;
	__SPRT_SVC_BODY("r"(__x8), "0"(__x0), "r"(__x1), "r"(__x2));
}

SPRT_FORCEINLINE long __sprt_svc4(long __nr, long __a, long __b, long __c, long __d) {
	register long __x8 __asm__("x8") = __nr;
	register long __x0 __asm__("x0") = __a;
	register long __x1 __asm__("x1") = __b;
	register long __x2 __asm__("x2") = __c;
	register long __x3 __asm__("x3") = __d;
	__SPRT_SVC_BODY("r"(__x8), "0"(__x0), "r"(__x1), "r"(__x2), "r"(__x3));
}

SPRT_FORCEINLINE long __sprt_svc5(long __nr, long __a, long __b, long __c, long __d, long __e) {
	register long __x8 __asm__("x8") = __nr;
	register long __x0 __asm__("x0") = __a;
	register long __x1 __asm__("x1") = __b;
	register long __x2 __asm__("x2") = __c;
	register long __x3 __asm__("x3") = __d;
	register long __x4 __asm__("x4") = __e;
	__SPRT_SVC_BODY("r"(__x8), "0"(__x0), "r"(__x1), "r"(__x2), "r"(__x3), "r"(__x4));
}

SPRT_FORCEINLINE long __sprt_svc6(long __nr, long __a, long __b, long __c, long __d, long __e,
		long __f) {
	register long __x8 __asm__("x8") = __nr;
	register long __x0 __asm__("x0") = __a;
	register long __x1 __asm__("x1") = __b;
	register long __x2 __asm__("x2") = __c;
	register long __x3 __asm__("x3") = __d;
	register long __x4 __asm__("x4") = __e;
	register long __x5 __asm__("x5") = __f;
	__SPRT_SVC_BODY("r"(__x8), "0"(__x0), "r"(__x1), "r"(__x2), "r"(__x3), "r"(__x4), "r"(__x5));
}

__SPRT_END_DECL

#undef __SPRT_SVC_BODY

#endif // CORE_RUNTIME_INCLUDE_C_CROSS_EMBOX_USER_SPRT_AARCH64_SVC_H_
