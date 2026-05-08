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

#include <sprt/cxx/new>
#include <sprt/cxx/forward_list>
#include <sprt/runtime/stream.h>

namespace sprt {

struct CustomType {
	int x = 0;
	int y = 0;

	constexpr CustomType &operator+=(const CustomType &other) {
		x += other.x;
		y += other.y;
		return *this;
	}
};

consteval int get_value() {
	auto t = sprt::memory::allocate<CustomType>();
	auto v = sprt::memory::allocate<CustomType>();

	using forward_list = sprt::__malloc_forward_list<int>;

	forward_list list{1, 2, 3};

	sprt::construct_at(t, CustomType{1, 2});
	sprt::construct_at(v, CustomType{3, 4});

	*t += *v;

	auto ret = t->x + t->y;

	sprt::memory::deallocate(t);
	sprt::memory::deallocate(v);
	return ret + list.front();
}

consteval int get_max_value() { return sprt::__vmax(1, 3, 5, 7, 2, 4); }

void performConstexprTest() {
	sprt::cout << "Constexpr test:\n calculated: " << get_value() << " " << get_max_value()
			   << "\n\n";
}

} // namespace sprt
