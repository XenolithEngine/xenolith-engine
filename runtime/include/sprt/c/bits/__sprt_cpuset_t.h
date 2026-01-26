/**
Copyright (c) 2026 Xenolith Team <admin@senolith.studio>

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

#ifndef CORE_RUNTIME_INCLUDE_C_BITS___SPRT_CPUSET_T_H_
#define CORE_RUNTIME_INCLUDE_C_BITS___SPRT_CPUSET_T_H_

#include <sprt/c/bits/__sprt_def.h>

#define __SPRT_CPU_SETSIZE 1'024

typedef struct {
	unsigned long __bits[__SPRT_CPU_SETSIZE / sizeof(long)];
} __SPRT_ID(cpu_set_t);

#define __SPRT_CPU_op_S(i, size, set, op) ( (i)/8U >= (size) ? 0 : \
	(((unsigned long *)(set))[(i)/8/sizeof(long)] op (1UL<<((i)%(8*sizeof(long))))) )

#define __SPRT_CPU_SET_S(i, size, set) __SPRT_CPU_op_S(i, size, set, |=)
#define __SPRT_CPU_CLR_S(i, size, set) __SPRT_CPU_op_S(i, size, set, &=~)
#define __SPRT_CPU_ISSET_S(i, size, set) __SPRT_CPU_op_S(i, size, set, &)

#define __SPRT_CPU_op_func_S(func, op) \
static __inline void __S_SPRT_CPU_##func##_S(size_t __size, __SPRT_ID(cpu_set_t) *__dest, \
	const __SPRT_ID(cpu_set_t) *__src1, const __SPRT_ID(cpu_set_t) *__src2) \
{ \
	size_t __i; \
	for (__i=0; __i<__size/sizeof(long); __i++) \
		((unsigned long *)__dest)[__i] = ((unsigned long *)__src1)[__i] \
			op ((unsigned long *)__src2)[__i] ; \
}

__SPRT_CPU_op_func_S(AND, &) __SPRT_CPU_op_func_S(OR, |) __SPRT_CPU_op_func_S(XOR, ^)

#define __SPRT_CPU_AND_S(a, b, c, d) __S_SPRT_CPU_AND_S(a,b,c,d)
#define __SPRT_CPU_OR_S(a, b, c, d) __S_SPRT_CPU_OR_S(a,b,c,d)
#define __SPRT_CPU_XOR_S(a, b, c, d) __S_SPRT_CPU_XOR_S(a,b,c,d)

#define __SPRT_CPU_COUNT_S(size, set) __SPRT_ID(__sched_cpucount)(size,set)
#define __SPRT_CPU_ZERO_S(size, set) __SPRT_ID(memset)(set,0,size)
#define __SPRT_CPU_EQUAL_S(size, set1, set2) (!__SPRT_ID(memcmp)(set1,set2,size))

#define __SPRT_CPU_ALLOC_SIZE(n) (sizeof(long) * ( (n)/(8*sizeof(long)) \
	+ ((n)%(8*sizeof(long)) + 8*sizeof(long)-1)/(8*sizeof(long)) ) )
#define __SPRT_CPU_ALLOC(n) ((cpu_set_t *)__SPRT_ID(calloc)(1,__SPRT_CPU_ALLOC_SIZE(n)))
#define __SPRT_CPU_FREE(set) __SPRT_ID(free)(set)

#define __SPRT_CPU_SET(i, set) __SPRT_CPU_SET_S(i,sizeof(cpu_set_t),set)
#define __SPRT_CPU_CLR(i, set) __SPRT_CPU_CLR_S(i,sizeof(cpu_set_t),set)
#define __SPRT_CPU_ISSET(i, set) __SPRT_CPU_ISSET_S(i,sizeof(cpu_set_t),set)
#define __SPRT_CPU_AND(d, s1, s2) __SPRT_CPU_AND_S(sizeof(cpu_set_t),d,s1,s2)
#define __SPRT_CPU_OR(d, s1, s2) __SPRT_CPU_OR_S(sizeof(cpu_set_t),d,s1,s2)
#define __SPRT_CPU_XOR(d, s1, s2) __SPRT_CPU_XOR_S(sizeof(cpu_set_t),d,s1,s2)
#define __SPRT_CPU_COUNT(set) __SPRT_CPU_COUNT_S(sizeof(cpu_set_t),set)
#define __SPRT_CPU_ZERO(set) __SPRT_CPU_ZERO_S(sizeof(cpu_set_t),set)
#define __SPRT_CPU_EQUAL(s1, s2) __SPRT_CPU_EQUAL_S(sizeof(cpu_set_t),s1,s2)

#endif // CORE_RUNTIME_INCLUDE_C_BITS___SPRT_CPUSET_T_H_
