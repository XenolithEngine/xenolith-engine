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

// Small std-conformance items for the sprt-backed STL vs host libstdc++:
//   - <atomic> std::memory_order enumerator constants
//   - <utility> std::index_sequence_for
//   - <type_traits> builtin bool traits are conforming integral_constants
//     (contextually convertible to bool, not just carrying ::value)
//   - <functional> transparent std::less<>

#include <stdio.h>

#include <atomic>
#include <utility>
#include <type_traits>
#include <functional>

namespace sprt::test {

namespace {

struct Final final {};
struct NotFinal {};

// index_sequence_for size == pack arity
static_assert(std::index_sequence_for<int, char, double, void>().size() == 4);
static_assert(std::is_same_v<std::index_sequence_for<int, char>, std::index_sequence<0, 1>>);

// memory_order enumerator constants alias the scoped enum members
static_assert(std::memory_order_relaxed == std::memory_order::relaxed);
static_assert(std::memory_order_seq_cst == std::memory_order::seq_cst);

// bool traits must be full integral_constants: usable in a bool context and via ::value
static_assert(std::is_final<Final>::value);
static_assert(!std::is_final<NotFinal>::value);
// contextual conversion (was the failure the integral_constant base fixes)
static_assert(std::is_final<Final>{});

template <typename T>
constexpr bool asBool() {
	return std::is_final<T>{} ? true : false; // exercise operator bool()
}

} // namespace

void performStlExtrasTest() {
	// memory_order underlying values (clang and gcc both use __ATOMIC_* = 0..5)
	printf("memory_order: relaxed=%d consume=%d acquire=%d release=%d acq_rel=%d seq_cst=%d\n",
			(int) std::memory_order_relaxed, (int) std::memory_order_consume,
			(int) std::memory_order_acquire, (int) std::memory_order_release,
			(int) std::memory_order_acq_rel, (int) std::memory_order_seq_cst);

	// a std::atomic round-trip using the constants (sequenced so the printed values
	// do not depend on argument-evaluation order)
	std::atomic<int> ai {0};
	ai.store(5, std::memory_order_relaxed);
	int ld = ai.load(std::memory_order_acquire);
	int fa = ai.fetch_add(3, std::memory_order_acq_rel);
	int after = ai.load(std::memory_order_relaxed);
	printf("atomic: load=%d fetch_add=%d after=%d\n", ld, fa, after);

	// index_sequence_for
	printf("index_sequence_for: size=%d\n", (int) std::index_sequence_for<int, char, double>().size());

	// bool traits, contextually converted
	printf("is_final: final=%d notfinal=%d asBoolFinal=%d asBoolNot=%d\n",
			(int) std::is_final<Final>{}, (int) std::is_final<NotFinal>{},
			(int) asBool<Final>(), (int) asBool<NotFinal>());
	printf("bool_traits: ptr=%d const=%d empty=%d v=%d\n", (int) std::is_pointer<int *>{},
			(int) std::is_const<const int>{}, (int) std::is_empty<NotFinal>{},
			(int) std::is_pointer_v<int *>);

	// transparent std::less<>: the observable property is heterogeneous invocation
	// (comparing int vs double without converting), which non-transparent less<int>
	// could not do. (The exact type of ::is_transparent is implementation-defined.)
	std::less<> lt;
	printf("less<>: lt=%d ge=%d hetero=%d\n", (int) lt(1, 2), (int) lt(2, 1),
			(int) lt(1, 2.5));
}

} // namespace sprt::test
