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

#include <stdio.h>

#include <ratio>

namespace sprt::test {

namespace {

// reduction to lowest terms
static_assert(std::ratio<2, 6>::num == 1 && std::ratio<2, 6>::den == 3);
// sign is normalized onto the numerator
static_assert(std::ratio<3, -4>::num == -3 && std::ratio<3, -4>::den == 4);

// arithmetic, each reduced
static_assert(std::ratio_add<std::ratio<1, 2>, std::ratio<1, 3>>::num == 5
		&& std::ratio_add<std::ratio<1, 2>, std::ratio<1, 3>>::den == 6);
static_assert(std::ratio_subtract<std::ratio<3, 4>, std::ratio<1, 4>>::num == 1
		&& std::ratio_subtract<std::ratio<3, 4>, std::ratio<1, 4>>::den == 2);
static_assert(std::ratio_multiply<std::ratio<2, 3>, std::ratio<3, 4>>::num == 1
		&& std::ratio_multiply<std::ratio<2, 3>, std::ratio<3, 4>>::den == 2);
static_assert(std::ratio_divide<std::ratio<1, 2>, std::ratio<1, 4>>::num == 2
		&& std::ratio_divide<std::ratio<1, 2>, std::ratio<1, 4>>::den == 1);

// comparison traits
static_assert(std::ratio_equal_v<std::ratio<1, 2>, std::ratio<2, 4>>);
static_assert(std::ratio_not_equal_v<std::ratio<1, 2>, std::ratio<1, 3>>);
static_assert(std::ratio_less_v<std::ratio<1, 3>, std::ratio<1, 2>>);
static_assert(std::ratio_less_equal_v<std::ratio<1, 2>, std::ratio<1, 2>>);
static_assert(std::ratio_greater_v<std::ratio<2, 3>, std::ratio<1, 3>>);
static_assert(std::ratio_greater_equal_v<std::ratio<1, 1>, std::ratio<1, 1>>);

// SI-prefix typedefs
static_assert(std::kilo::num == 1'000 && std::kilo::den == 1);
static_assert(std::milli::num == 1 && std::milli::den == 1'000);
static_assert(std::mega::num == 1'000'000 && std::micro::den == 1'000'000);

} // namespace

void performRatioTest() {
	using r56 = std::ratio_add<std::ratio<1, 2>, std::ratio<1, 3>>;
	using rmul = std::ratio_multiply<std::ratio<2, 3>, std::ratio<3, 4>>;
	using rdiv = std::ratio_divide<std::ratio<5, 6>, std::ratio<10, 3>>;
	printf("add 1/2+1/3 = %lld/%lld\n", (long long)r56::num, (long long)r56::den);
	printf("mul 2/3*3/4 = %lld/%lld\n", (long long)rmul::num, (long long)rmul::den);
	printf("div 5/6 / 10/3 = %lld/%lld\n", (long long)rdiv::num, (long long)rdiv::den);
	printf("reduce 2/6 = %lld/%lld\n", (long long)std::ratio<2, 6>::num,
			(long long)std::ratio<2, 6>::den);
	printf("si: kilo=%lld/%lld milli=%lld/%lld\n", (long long)std::kilo::num,
			(long long)std::kilo::den, (long long)std::milli::num, (long long)std::milli::den);
	printf("cmp: 1/3<1/2=%d 1/2==2/4=%d 2/3>1/3=%d\n",
			(int)std::ratio_less_v<std::ratio<1, 3>, std::ratio<1, 2>>,
			(int)std::ratio_equal_v<std::ratio<1, 2>, std::ratio<2, 4>>,
			(int)std::ratio_greater_v<std::ratio<2, 3>, std::ratio<1, 3>>);
}

} // namespace sprt::test
