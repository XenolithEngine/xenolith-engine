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

// musl arch atomic primitives for wasm32 (adapter-provided — wasm is not a musl
// arch). musl's src/internal/atomic.h includes this and synthesises everything
// else from a_cas. Every primitive is expressed with the compiler's __atomic
// builtins, which lower to the wasm threads atomic.rmw/atomic.cmpxchg
// instructions under -matomics (and to plain memory ops in a single-threaded,
// non-shared-memory build). No inline asm — wasm has none.

#ifndef RUNTIME_MUSL_ADAPTERS_ARCH_WASM32_ATOMIC_ARCH_H_
#define RUNTIME_MUSL_ADAPTERS_ARCH_WASM32_ATOMIC_ARCH_H_

#define a_barrier a_barrier
static inline void a_barrier(void) { __atomic_thread_fence(__ATOMIC_SEQ_CST); }

#define a_cas a_cas
static inline int a_cas(volatile int *p, int t, int s) {
	// __atomic_compare_exchange_n leaves `t` holding the observed value (the old
	// *p) on failure and the expected value (== old *p) on success, so returning
	// `t` yields musl's "old value" contract in both cases.
	__atomic_compare_exchange_n(p, &t, s, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
	return t;
}

#define a_cas_p a_cas_p
static inline void *a_cas_p(volatile void *p, void *t, void *s) {
	__atomic_compare_exchange_n((void *volatile *)p, &t, s, 0, __ATOMIC_SEQ_CST,
			__ATOMIC_SEQ_CST);
	return t;
}

#define a_swap a_swap
static inline int a_swap(volatile int *p, int v) {
	return __atomic_exchange_n(p, v, __ATOMIC_SEQ_CST);
}

#define a_fetch_add a_fetch_add
static inline int a_fetch_add(volatile int *p, int v) {
	return __atomic_fetch_add(p, v, __ATOMIC_SEQ_CST);
}

#define a_and a_and
static inline void a_and(volatile int *p, int v) { __atomic_and_fetch(p, v, __ATOMIC_SEQ_CST); }

#define a_or a_or
static inline void a_or(volatile int *p, int v) { __atomic_or_fetch(p, v, __ATOMIC_SEQ_CST); }

#define a_and_64 a_and_64
static inline void a_and_64(volatile uint64_t *p, uint64_t v) {
	__atomic_and_fetch(p, v, __ATOMIC_SEQ_CST);
}

#define a_or_64 a_or_64
static inline void a_or_64(volatile uint64_t *p, uint64_t v) {
	__atomic_or_fetch(p, v, __ATOMIC_SEQ_CST);
}

#define a_store a_store
static inline void a_store(volatile int *p, int v) {
	__atomic_store_n(p, v, __ATOMIC_SEQ_CST);
}

#define a_spin a_spin
static inline void a_spin(void) {}

#define a_crash a_crash
static inline void a_crash(void) { __builtin_trap(); }

#endif // RUNTIME_MUSL_ADAPTERS_ARCH_WASM32_ATOMIC_ARCH_H_
