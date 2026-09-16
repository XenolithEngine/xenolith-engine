/**
 Copyright (c) 2025 Stappler LLC <admin@stappler.dev>
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#ifndef RUNTIME_INCLUDE_SPRT_CXX___ATOMIC_OPS_H_
#define RUNTIME_INCLUDE_SPRT_CXX___ATOMIC_OPS_H_

// sprt::_atomic -- the low-level typed wrappers over the compiler's __atomic_*
// builtins. Split out of <sprt/cxx/atomic> so the synchronization primitives
// (qmutex / rmutex / qonce / ...) can pull just this, without dragging in the
// full C++ <atomic> machinery (and, through it, the concepts/iterator STL). No
// header dependencies: everything here is a compiler builtin.

namespace sprt {
inline namespace __cxx_atomic {

namespace _atomic {

template <typename Value>
static inline Value loadSeq(volatile Value *ptr) {
	return __atomic_load_n(ptr, __ATOMIC_SEQ_CST);
}

template <typename Value>
static inline Value loadRel(volatile Value *ptr) {
	return __atomic_load_n(ptr, __ATOMIC_RELAXED);
}

template <typename Value>
static inline void storeSeq(volatile Value *ptr, Value value) {
	__atomic_store_n(ptr, value, __ATOMIC_SEQ_CST);
}

template <typename Value>
static inline Value fetchOr(volatile Value *ptr, Value value) {
	return __atomic_fetch_or(ptr, value, __ATOMIC_SEQ_CST);
}

template <typename Value>
static inline Value fetchAnd(volatile Value *ptr, Value value) {
	return __atomic_fetch_and(ptr, value, __ATOMIC_SEQ_CST);
}

template <typename Value>
static inline Value fetchAdd(volatile Value *ptr, Value value) {
	return __atomic_fetch_add(ptr, value, __ATOMIC_SEQ_CST);
}

template <typename Value>
static inline Value fetchSub(volatile Value *ptr, Value value) {
	return __atomic_fetch_sub(ptr, value, __ATOMIC_SEQ_CST);
}

template <typename Value>
static inline Value exchange(volatile Value *ptr, Value value) {
	return __atomic_exchange_n(ptr, value, __ATOMIC_SEQ_CST);
}

template <typename Value>
static inline bool compareSwap(volatile Value *ptr, Value *expected, Value desired) {
	return __atomic_compare_exchange_n(ptr, expected, desired, false, __ATOMIC_SEQ_CST,
			__ATOMIC_SEQ_CST);
}

} // namespace _atomic

} // namespace __cxx_atomic
} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_CXX___ATOMIC_OPS_H_
