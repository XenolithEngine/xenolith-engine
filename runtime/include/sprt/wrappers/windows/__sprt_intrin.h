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

__SPRT_C_FUNC long _InterlockedOr(long volatile *_Value, long _Mask);
__SPRT_C_FUNC long _InterlockedExchangeAdd(long volatile *_Addend, long _Value);
__SPRT_C_FUNC void *_InterlockedExchangePointer(void *volatile *_Target, void *_Value);
__SPRT_C_FUNC long _InterlockedCompareExchange(long volatile *_Destination, long _Exchange,
		long _Comparand);
__SPRT_C_FUNC long _InterlockedExchange(long volatile *_Target, long _Value);
__SPRT_C_FUNC __int64 _InterlockedExchange64(__int64 volatile *_Target, __int64 _Value);
__SPRT_C_FUNC __int64 _InterlockedOr64(__int64 volatile *_Value, __int64 _Mask);
__SPRT_C_FUNC __int64 _InterlockedAnd64(__int64 volatile *, __int64);
__SPRT_C_FUNC void __faststorefence(void);
__SPRT_C_FUNC unsigned __int64 __readgsqword(unsigned long);
__SPRT_C_FUNC unsigned short __readgsword(unsigned long);
__SPRT_C_FUNC unsigned long _exception_code(void);
__SPRT_C_FUNC void *_exception_info(void);
__SPRT_C_FUNC int _abnormal_termination(void);
__SPRT_C_FUNC void _mm_pause(void);

__SPRT_C_FUNC SPRT_FORCEINLINE __int64 _InterlockedAdd64(__int64 volatile *target, __int64 value) {
	return __atomic_fetch_add(target, value, __ATOMIC_ACQ_REL);
}

#pragma intrinsic(_InterlockedOr)
#pragma intrinsic(_InterlockedExchangeAdd)
#pragma intrinsic(_InterlockedExchangePointer)
#pragma intrinsic(_InterlockedCompareExchange)
#pragma intrinsic(_InterlockedExchange)
#pragma intrinsic(_InterlockedExchange64)
#pragma intrinsic(_InterlockedOr64)
#pragma intrinsic(_InterlockedAnd64)
#pragma intrinsic(__faststorefence)
#pragma intrinsic(__readgsqword)
#pragma intrinsic(__readgsword)
#pragma intrinsic(_exception_code)
#pragma intrinsic(_exception_info)
#pragma intrinsic(_abnormal_termination)
#pragma intrinsic(_mm_pause)

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

#endif // SPRT_WRAPPERS_WINDOWS___SPRT_INTRIN_H_
