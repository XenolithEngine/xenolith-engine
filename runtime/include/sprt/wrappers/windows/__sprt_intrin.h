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

#ifndef SPRT_WRAPPERS_WINDOWS___SPRT_INTRIN_H_
#define SPRT_WRAPPERS_WINDOWS___SPRT_INTRIN_H_

#include <sprt/c/bits/__sprt_def.h>

#if defined(_WIN32)

// Interlocked / exception intrinsics available on every Windows architecture
__SPRT_C_FUNC long _InterlockedOr(long volatile *_Value, long _Mask);
__SPRT_C_FUNC long _InterlockedExchangeAdd(long volatile *_Addend, long _Value);
__SPRT_C_FUNC void *_InterlockedExchangePointer(void *volatile *_Target, void *_Value);
__SPRT_C_FUNC long _InterlockedCompareExchange(long volatile *_Destination, long _Exchange,
		long _Comparand);
__SPRT_C_FUNC long _InterlockedExchange(long volatile *_Target, long _Value);
__SPRT_C_FUNC __int64 _InterlockedExchange64(__int64 volatile *_Target, __int64 _Value);
__SPRT_C_FUNC __int64 _InterlockedOr64(__int64 volatile *_Value, __int64 _Mask);
__SPRT_C_FUNC __int64 _InterlockedAnd64(__int64 volatile *, __int64);
__SPRT_C_FUNC unsigned long _exception_code(void);
__SPRT_C_FUNC void *_exception_info(void);
__SPRT_C_FUNC int _abnormal_termination(void);

#pragma intrinsic(_InterlockedOr)
#pragma intrinsic(_InterlockedExchangeAdd)
#pragma intrinsic(_InterlockedExchangePointer)
#pragma intrinsic(_InterlockedCompareExchange)
#pragma intrinsic(_InterlockedExchange)
#pragma intrinsic(_InterlockedExchange64)
#pragma intrinsic(_InterlockedOr64)
#pragma intrinsic(_InterlockedAnd64)
#pragma intrinsic(_exception_code)
#pragma intrinsic(_exception_info)
#pragma intrinsic(_abnormal_termination)

#if __SPRT_ARCH_ID == __SPRT_ARCH_ID_AARCH64

// On ARM64 _InterlockedAdd64 is a true compiler intrinsic; the TEB lives in x18.

__SPRT_BEGIN_DECL

__int64 _InterlockedAdd64(__int64 volatile *, __int64);
unsigned __int64 __readx18qword(unsigned long);
void __yield(void);

void __dmb(unsigned int _Type);
void __dsb(unsigned int _Type);
void __isb(unsigned int _Type);
void __sb(void);

void __ld64b(const void *_addr, unsigned __int64 _value[8]);
void __st64b(void *_addr, unsigned __int64 _value[8]);
unsigned __int64 __st64bv(void *_addr, unsigned __int64 _value[8]);
unsigned __int64 __st64bv0(void *_addr, unsigned __int64 _value[8]);

// clang/gcc with a *-pc-windows-msvc target do NOT provide the LDAR/STLR
// load-acquire / store-release intrinsics as builtins (only __dmb, __yield,
// __iso_volatile_* and the _Interlocked* family are backed) -- a bare prototype
// would tail-call an undefined __ldar64 symbol. mimalloc's MSVC atomic path
// (mimalloc/atomic.h) needs __ldar64/__stlr64, so emit the instruction directly;
// the "memory" clobber gives the acquire (LDAR) / release (STLR) ordering the
// intrinsic guarantees. NB: the neighbouring __ldxr/__ldaxr/__stxr/__stlxr,
// __ldapr, __load_acquire and __ldtr prototypes are unbacked too, but no current
// consumer needs them, so they are left as-is.
#if defined(__clang__) || defined(__GNUC__)
#define __SPRT_DEF_LDAR(_W, _T, _SFX, _REG) \
	SPRT_FORCEINLINE _T __ldar##_W(const volatile _T *_Target) { \
		_T _Value; \
		__asm__ __volatile__("ldar" _SFX " " _REG "0, [%1]" : "=r"(_Value) : "r"(_Target) \
				: "memory"); \
		return _Value; \
	}
__SPRT_DEF_LDAR(8, unsigned __int8, "b", "%w")
__SPRT_DEF_LDAR(16, unsigned __int16, "h", "%w")
__SPRT_DEF_LDAR(32, unsigned __int32, "", "%w")
__SPRT_DEF_LDAR(64, unsigned __int64, "", "%x")
#undef __SPRT_DEF_LDAR
#else
unsigned __int8 __ldar8(const volatile unsigned __int8 *_Target);
unsigned __int16 __ldar16(const volatile unsigned __int16 *_Target);
unsigned __int32 __ldar32(const volatile unsigned __int32 *_Target);
unsigned __int64 __ldar64(const volatile unsigned __int64 *_Target);
#endif

unsigned __int8 __ldapr8(const volatile unsigned __int8 *_Target);
unsigned __int16 __ldapr16(const volatile unsigned __int16 *_Target);
unsigned __int32 __ldapr32(const volatile unsigned __int32 *_Target);
unsigned __int64 __ldapr64(const volatile unsigned __int64 *_Target);

unsigned __int8 __load_acquire8(const volatile unsigned __int8 *_Target);
unsigned __int16 __load_acquire16(const volatile unsigned __int16 *_Target);
unsigned __int32 __load_acquire32(const volatile unsigned __int32 *_Target);
unsigned __int64 __load_acquire64(const volatile unsigned __int64 *_Target);

unsigned __int8 __ldxr8(const volatile unsigned __int8 *_Target);
unsigned __int16 __ldxr16(const volatile unsigned __int16 *_Target);
unsigned __int32 __ldxr32(const volatile unsigned __int32 *_Target);
unsigned __int64 __ldxr64(const volatile unsigned __int64 *_Target);
unsigned __int8 __ldaxr8(const volatile unsigned __int8 *_Target);
unsigned __int16 __ldaxr16(const volatile unsigned __int16 *_Target);
unsigned __int32 __ldaxr32(const volatile unsigned __int32 *_Target);
unsigned __int64 __ldaxr64(const volatile unsigned __int64 *_Target);

unsigned __int8 __stxr8(volatile unsigned __int8 *_Target, unsigned __int8 _Value);
unsigned __int8 __stxr16(volatile unsigned __int16 *_Target, unsigned __int16 _Value);
unsigned __int8 __stxr32(volatile unsigned __int32 *_Target, unsigned __int32 _Value);
unsigned __int8 __stxr64(volatile unsigned __int64 *_Target, unsigned __int64 _Value);
unsigned __int8 __stlxr8(volatile unsigned __int8 *_Target, unsigned __int8 _Value);
unsigned __int8 __stlxr16(volatile unsigned __int16 *_Target, unsigned __int16 _Value);
unsigned __int8 __stlxr32(volatile unsigned __int32 *_Target, unsigned __int32 _Value);
unsigned __int8 __stlxr64(volatile unsigned __int64 *_Target, unsigned __int64 _Value);

void __clrex(unsigned __int8 crm);

// Store-release register (STLR) -- see the __ldar note above; not a clang/gcc builtin.
#if defined(__clang__) || defined(__GNUC__)
#define __SPRT_DEF_STLR(_W, _T, _SFX, _REG) \
	SPRT_FORCEINLINE void __stlr##_W(volatile _T *_Target, _T _Value) { \
		__asm__ __volatile__("stlr" _SFX " " _REG "1, [%0]" : : "r"(_Target), "r"(_Value) \
				: "memory"); \
	}
__SPRT_DEF_STLR(8, unsigned __int8, "b", "%w")
__SPRT_DEF_STLR(16, unsigned __int16, "h", "%w")
__SPRT_DEF_STLR(32, unsigned __int32, "", "%w")
__SPRT_DEF_STLR(64, unsigned __int64, "", "%x")
#undef __SPRT_DEF_STLR
#else
void __stlr8(volatile unsigned __int8 *_Target, unsigned __int8 _Value);
void __stlr16(volatile unsigned __int16 *_Target, unsigned __int16 _Value);
void __stlr32(volatile unsigned __int32 *_Target, unsigned __int32 _Value);
void __stlr64(volatile unsigned __int64 *_Target, unsigned __int64 _Value);
#endif

// load/store unprivileged
unsigned __int8 __ldtr8(const volatile unsigned __int8 *_Target);
unsigned __int16 __ldtr16(const volatile unsigned __int16 *_Target);
unsigned __int32 __ldtr32(const volatile unsigned __int32 *_Target);
unsigned __int64 __ldtr64(const volatile unsigned __int64 *_Target);

signed __int8 __ldtrs8(const volatile __int8 *_Target);
signed __int16 __ldtrs16(const volatile __int16 *_Target);
signed __int32 __ldtrs32(const volatile __int32 *_Target);

void __sttr8(volatile unsigned __int8 *_Target, unsigned __int8 _Value);
void __sttr16(volatile unsigned __int16 *_Target, unsigned __int16 _Value);
void __sttr32(volatile unsigned __int32 *_Target, unsigned __int32 _Value);
void __sttr64(volatile unsigned __int64 *_Target, unsigned __int64 _Value);

unsigned __int8 __swp8(unsigned __int8 volatile *_Target, unsigned __int8 _Value);
unsigned __int16 __swp16(unsigned __int16 volatile *_Target, unsigned __int16 _Value);
unsigned __int32 __swp32(unsigned __int32 volatile *_Target, unsigned __int32 _Value);
unsigned __int64 __swp64(unsigned __int64 volatile *_Target, unsigned __int64 _Value);
unsigned __int8 __swpa8(unsigned __int8 volatile *_Target, unsigned __int8 _Value);
unsigned __int16 __swpa16(unsigned __int16 volatile *_Target, unsigned __int16 _Value);
unsigned __int32 __swpa32(unsigned __int32 volatile *_Target, unsigned __int32 _Value);
unsigned __int64 __swpa64(unsigned __int64 volatile *_Target, unsigned __int64 _Value);
unsigned __int8 __swpl8(unsigned __int8 volatile *_Target, unsigned __int8 _Value);
unsigned __int16 __swpl16(unsigned __int16 volatile *_Target, unsigned __int16 _Value);
unsigned __int32 __swpl32(unsigned __int32 volatile *_Target, unsigned __int32 _Value);
unsigned __int64 __swpl64(unsigned __int64 volatile *_Target, unsigned __int64 _Value);
unsigned __int8 __swpal8(unsigned __int8 volatile *_Target, unsigned __int8 _Value);
unsigned __int16 __swpal16(unsigned __int16 volatile *_Target, unsigned __int16 _Value);
unsigned __int32 __swpal32(unsigned __int32 volatile *_Target, unsigned __int32 _Value);
unsigned __int64 __swpal64(unsigned __int64 volatile *_Target, unsigned __int64 _Value);

unsigned __int8 __cas8(unsigned __int8 volatile *_Target, unsigned __int8 _Comp,
		unsigned __int8 _Value);
unsigned __int16 __cas16(unsigned __int16 volatile *_Target, unsigned __int16 _Comp,
		unsigned __int16 _Value);
unsigned __int32 __cas32(unsigned __int32 volatile *_Target, unsigned __int32 _Comp,
		unsigned __int32 _Value);
unsigned __int64 __cas64(unsigned __int64 volatile *_Target, unsigned __int64 _Comp,
		unsigned __int64 _Value);
unsigned __int8 __casa8(unsigned __int8 volatile *_Target, unsigned __int8 _Comp,
		unsigned __int8 _Value);
unsigned __int16 __casa16(unsigned __int16 volatile *_Target, unsigned __int16 _Comp,
		unsigned __int16 _Value);
unsigned __int32 __casa32(unsigned __int32 volatile *_Target, unsigned __int32 _Comp,
		unsigned __int32 _Value);
unsigned __int64 __casa64(unsigned __int64 volatile *_Target, unsigned __int64 _Comp,
		unsigned __int64 _Value);
unsigned __int8 __casl8(unsigned __int8 volatile *_Target, unsigned __int8 _Comp,
		unsigned __int8 _Value);
unsigned __int16 __casl16(unsigned __int16 volatile *_Target, unsigned __int16 _Comp,
		unsigned __int16 _Value);
unsigned __int32 __casl32(unsigned __int32 volatile *_Target, unsigned __int32 _Comp,
		unsigned __int32 _Value);
unsigned __int64 __casl64(unsigned __int64 volatile *_Target, unsigned __int64 _Comp,
		unsigned __int64 _Value);
unsigned __int8 __casal8(unsigned __int8 volatile *_Target, unsigned __int8 _Comp,
		unsigned __int8 _Value);
unsigned __int16 __casal16(unsigned __int16 volatile *_Target, unsigned __int16 _Comp,
		unsigned __int16 _Value);
unsigned __int32 __casal32(unsigned __int32 volatile *_Target, unsigned __int32 _Comp,
		unsigned __int32 _Value);
unsigned __int64 __casal64(unsigned __int64 volatile *_Target, unsigned __int64 _Comp,
		unsigned __int64 _Value);

void *__xpaci(void *_Pointer);

unsigned __int32 __rbit(unsigned __int32 _Value);
unsigned long __rbitl(unsigned long _Value);
unsigned __int64 __rbitll(unsigned __int64 _Value);

__SPRT_END_DECL

#pragma intrinsic(_InterlockedAdd64)

#else

// x86_64 has no single-instruction atomic add-and-fetch; emulate it.
__SPRT_C_FUNC void __faststorefence(void);
__SPRT_C_FUNC unsigned __int64 __readgsqword(unsigned long);
__SPRT_C_FUNC unsigned short __readgsword(unsigned long);
__SPRT_C_FUNC void _mm_pause(void);

__SPRT_C_FUNC SPRT_FORCEINLINE __int64 _InterlockedAdd64(__int64 volatile *target, __int64 value) {
	return __atomic_fetch_add(target, value, __ATOMIC_ACQ_REL);
}

#pragma intrinsic(__faststorefence)
#pragma intrinsic(__readgsqword)
#pragma intrinsic(__readgsword)
#pragma intrinsic(_mm_pause)

#endif

#define InterlockedOr _InterlockedOr
#define InterlockedExchange64 _InterlockedExchange64
#define InterlockedOr64 _InterlockedOr64
#define InterlockedExchangeAdd _InterlockedExchangeAdd
#define InterlockedExchangePointer _InterlockedExchangePointer
#define InterlockedCompareExchange _InterlockedCompareExchange
#define InterlockedExchange _InterlockedExchange
#define InterlockedAdd64 _InterlockedAdd64
#define InterlockedAnd64 _InterlockedAnd64
#define GetExceptionCode            _exception_code
#define exception_code              _exception_code
#define GetExceptionInformation()   ((struct _EXCEPTION_POINTERS *)_exception_info())
#define exception_info()            ((struct _EXCEPTION_POINTERS *)_exception_info())
#define AbnormalTermination         _abnormal_termination
#define abnormal_termination        _abnormal_termination

#endif

#endif // SPRT_WRAPPERS_WINDOWS___SPRT_INTRIN_H_
