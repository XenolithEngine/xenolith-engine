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

// <stdatomic.h> is the C atomics facility (the runtime header is a thin wrapper
// over clang's resource <stdatomic.h>), so this part of the test is a C
// translation unit. All operations are single-threaded with deterministic
// results — the read-modify-write builtins return the previous value, and the
// lock-free / memory_order constants are clang builtins that are identical on
// the Linux host and the Windows target. The C++ side only registers it.

#include <stdatomic.h>
#include <stdio.h>

void sprt_libc_stdatomic_test_impl(void) {
	atomic_int a;
	atomic_init(&a, 10);
	printf("init=%d\n", atomic_load(&a));

	atomic_store(&a, 42);
	printf("store/load=%d\n", atomic_load(&a));

	// read-modify-write: each returns the previous value
	int prev = atomic_fetch_add(&a, 5);
	printf("fetch_add(5) prev=%d now=%d\n", prev, atomic_load(&a));
	prev = atomic_fetch_sub(&a, 3);
	printf("fetch_sub(3) prev=%d now=%d\n", prev, atomic_load(&a));
	prev = atomic_fetch_or(&a, 0xF0);
	printf("fetch_or(0xF0) prev=%d now=%d\n", prev, atomic_load(&a));
	prev = atomic_fetch_and(&a, 0x0F);
	printf("fetch_and(0x0F) prev=%d now=%d\n", prev, atomic_load(&a));
	prev = atomic_fetch_xor(&a, 0xFF);
	printf("fetch_xor(0xFF) prev=%d now=%d\n", prev, atomic_load(&a));
	prev = atomic_exchange(&a, 7);
	printf("exchange(7) prev=%d now=%d\n", prev, atomic_load(&a));

	// compare-exchange: success then failure
	int expected = 7;
	int ok = atomic_compare_exchange_strong(&a, &expected, 100) ? 1 : 0;
	printf("cas(7->100) ok=%d now=%d\n", ok, atomic_load(&a));
	expected = 7; // stale: a is 100 now, so this must fail and reload expected
	ok = atomic_compare_exchange_strong(&a, &expected, 200) ? 1 : 0;
	printf("cas_fail ok=%d expected_reloaded=%d a=%d\n", ok, expected, atomic_load(&a));

	// explicit memory order variant
	atomic_store_explicit(&a, 9, memory_order_relaxed);
	printf("load_explicit=%d\n", atomic_load_explicit(&a, memory_order_relaxed));

	// atomic_flag — sequence each test-and-set into its own statement, since two
	// side-effecting calls in one printf would evaluate in ABI-dependent order.
	atomic_flag f = ATOMIC_FLAG_INIT;
	int tas1 = atomic_flag_test_and_set(&f) ? 1 : 0;
	int tas2 = atomic_flag_test_and_set(&f) ? 1 : 0;
	printf("flag tas#1=%d tas#2=%d\n", tas1, tas2);
	atomic_flag_clear(&f);
	printf("flag after clear tas=%d\n", atomic_flag_test_and_set(&f) ? 1 : 0);

	// compile-time properties (clang builtins, identical on both targets)
	printf("lock_free int=%d long=%d ptr=%d\n", ATOMIC_INT_LOCK_FREE, ATOMIC_LONG_LOCK_FREE,
			ATOMIC_POINTER_LOCK_FREE);
	printf("memory_order relaxed=%d acquire=%d release=%d seq_cst=%d\n",
			(int) memory_order_relaxed, (int) memory_order_acquire, (int) memory_order_release,
			(int) memory_order_seq_cst);
}
