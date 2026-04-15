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

#include <sprt/cxx/forward_list>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sprt/runtime/log.h>
#include <sprt/runtime/platform.h>

#include <sprt/cxx/new>
#include <sprt/cxx/memory>

namespace sprt {

void performUnameTest();
void performUnistdTest();
void performDirTest();
void performLinkTest();
void performListTests();

void performPthreadCreateTest();
void performPthreadMutexTest();
void performPthreadCondTest();
void performPthreadRwlockTest();
void performPthreadBarrierTest();
void performPthreadSpinlockTest();

void performMallocStringTests();

void performMallocUnorderedMapTests();
void performMallocUnorderedSetTests();
void performMallocListTests();
void performThreadTests();
void performMallocForwardListTests();
void performVariantTests();
void performOptionalTests();
void performSortTests();

void performHashTests();

} // namespace sprt

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

int main(int argc, const char *argv[]) {
	auto str =
			"Проверяю работоспособность вывода в UTF-8, строка должна быть читаема из терминала\n";

	fwrite(str, strlen(str), 1, stdout);

	sprt::cout << "\nTest constevals: " << get_value() << "\n";

	sprt::cout << &fwrite << " " << (void *)str << " " << 12'345 << " " << sprt::io_hex(12'345)
			   << " " << sprt::io_hex(-12'345) << "\n";

	sprt::cout << get_max_value() << "\n";
	/*sprt::performMallocUnorderedMapTests();
	sprt::performMallocUnorderedSetTests();
	sprt::performMallocForwardListTests();
	sprt::performMallocListTests();*/

	sprt::performSortTests();
	sprt::performOptionalTests();
	sprt::performVariantTests();
	sprt::performThreadTests();

	sprt::performPthreadCreateTest();
	sprt::performPthreadMutexTest();
	sprt::performPthreadCondTest();
	sprt::performPthreadRwlockTest();
	sprt::performPthreadBarrierTest();
	sprt::performPthreadSpinlockTest();
	/**/

	/*sprt::performHashTests();
	sprt::performMallocStringTests();

	sprt::oslog::vpinfo(__SPRT_LOCATION, "main", "Exec path: ", sprt::platform::getExecPath());
	sprt::oslog::vpinfo(__SPRT_LOCATION, "main", "Home path: ", sprt::platform::getHomePath());
	sprt::oslog::vpinfo(__SPRT_LOCATION, "main",
			"Unique Device Id: ", sprt::platform::getUniqueDeviceId());

	sprt::performUnameTest();
	sprt::performUnistdTest();
	sprt::performDirTest();
	sprt::performLinkTest();*/
	return 0;
}
