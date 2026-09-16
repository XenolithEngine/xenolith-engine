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

#ifndef RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_SHUFFLE_H_
#define RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_SHUFFLE_H_

#include <sprt/cxx/iterator> // iter_swap

namespace sprt {
inline namespace __cxx_algorithm {

// [alg.random.shuffle] Fisher-Yates shuffle driven by a uniform random bit generator.
// The index in [0, i] is drawn by reducing g()'s output modulo (i+1). This has the usual
// modulo bias for ranges that do not divide the generator's period; that is acceptable
// for the freestanding runtime's callers (deterministic test/reduction shuffles), which
// do not depend on perfect uniformity.
template <typename _RandomIt, typename _URBG>
void shuffle(_RandomIt __first, _RandomIt __last, _URBG &&__g) {
	auto __n = __last - __first;
	if (__n <= 1) {
		return;
	}
	using __gen_t = decltype(__g());
	for (decltype(__n) __i = __n - 1; __i > 0; --__i) {
		auto __j = static_cast<decltype(__n)>(__g() % static_cast<__gen_t>(__i + 1));
		sprt::iter_swap(__first + __i, __first + __j);
	}
}

} // namespace __cxx_algorithm
} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_SHUFFLE_H_
