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

#ifndef RUNTIME_SRC_PRIVATE_SPRTATOMICS_H_
#define RUNTIME_SRC_PRIVATE_SPRTATOMICS_H_

#include <sprt/runtime/math.h>

namespace sprt {

template <typename Value>
static inline Value atomicLoadSeq(volatile Value *ptr) {
	return __atomic_load_n(ptr, __ATOMIC_SEQ_CST);
}

template <typename Value>
static inline Value atomicLoadRel(volatile Value *ptr) {
	return __atomic_load_n(ptr, __ATOMIC_RELAXED);
}

template <typename Value>
static inline void atomicStoreSeq(volatile Value *ptr, Value value) {
	__atomic_store_n(ptr, value, __ATOMIC_SEQ_CST);
}

template <typename Value>
static inline Value atomicFetchOr(volatile Value *ptr, Value value) {
	return __atomic_fetch_or(ptr, value, __ATOMIC_SEQ_CST);
}

template <typename Value>
static inline Value atomicFetchAnd(volatile Value *ptr, Value value) {
	return __atomic_fetch_and(ptr, value, __ATOMIC_SEQ_CST);
}

template <typename Value>
static inline Value atomicFetchAdd(volatile Value *ptr, Value value) {
	return __atomic_fetch_add(ptr, value, __ATOMIC_SEQ_CST);
}

template <typename Value>
static inline Value atomicFetchSub(volatile Value *ptr, Value value) {
	return __atomic_fetch_sub(ptr, value, __ATOMIC_SEQ_CST);
}

template <typename Value>
static inline Value atomicExchange(volatile Value *ptr, Value value) {
	return __atomic_exchange_n(ptr, value, __ATOMIC_SEQ_CST);
}

template <typename Value>
static inline bool atomicCompareSwap(volatile Value *ptr, Value *expected, Value desired) {
	return __atomic_compare_exchange_n(ptr, expected, desired, false, __ATOMIC_SEQ_CST,
			__ATOMIC_SEQ_CST);
}

} // namespace sprt

#endif // RUNTIME_SRC_PRIVATE_SPRTATOMICS_H_
