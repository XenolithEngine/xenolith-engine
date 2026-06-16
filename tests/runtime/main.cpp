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
#include <inttypes.h>
#include <time.h>
#include <wchar.h>

#include <io.h>

#include <sprt/runtime/log.h>
#include <sprt/runtime/platform.h>

#include <sprt/cxx/new>
#include <sprt/cxx/unordered_map>
#include <sprt/cxx/memory>

#include <sprt/cxx/cmath>

#include "tests.h"

static sprt::__malloc_unordered_map<sprt::StringView, void (*)()> s_testList{
	{"libc_uname", &sprt::performUnameTest},
	{"libc_unistd", &sprt::performUnistdTest},
	{"libc_dir", &sprt::performDirTest},
	{"libc_link", &sprt::performLinkTest},
	{"libc_time", &sprt::performLibcTimeTest},
	{"libc_pthread", &sprt::performPthreadCreateTest},
	{"libc_pthread_mutex", &sprt::performPthreadMutexTest},
	{"libc_pthread_cond", &sprt::performPthreadCondTest},
	{"libc_pthread_rwlock", &sprt::performPthreadRwlockTest},
	{"libc_pthread_barrier", &sprt::performPthreadBarrierTest},
	{"libc_pthread_spinlock", &sprt::performPthreadSpinlockTest},

	{"libcxx_malloc_string", &sprt::performMallocStringTests},
	{"libcxx_malloc_unordered_map", &sprt::performMallocUnorderedMapTests},
	{"libcxx_malloc_unordered_set", &sprt::performMallocUnorderedSetTests},
	{"libcxx_malloc_list", &sprt::performMallocListTests},
	{"libcxx_malloc_forward_list", &sprt::performMallocForwardListTests},
	{"libcxx_thread", &sprt::performThreadTests},
	{"libcxx_variant", &sprt::performVariantTests},
	{"libcxx_optional", &sprt::performOptionalTests},
	{"libcxx_sort", &sprt::performSortTests},
	{"libcxx_constexpr", &sprt::performConstexprTest},
	{"libcxx_shared_mutex", &sprt::performSharedMutexStressTests},
	{"libcxx_bitset", &sprt::performBitsetTests},
	{"libcxx_rtti", &sprt::performRttiTests},

	{"runtime_ref", &sprt::performRefTests},
	{"runtime_dispatch", &sprt::performDispatchTests},
	{"runtime_process", &sprt::performProcessTests},
	{"runtime_unicode", &sprt::performUnicodeTests},
	{"runtime_dtoa", &sprt::performDtoaTests},
};

int main(int argc, const char *argv[]) {
	auto str =
			"Проверяю работоспособность вывода в UTF-8, строка должна быть читаема из терминала\n";

	fwrite(str, ::strlen(str), 1, stdout);

	auto wstr =
			L"Проверяю работоспособность вывода в UTF-8, строка должна быть читаема из терминала\n";

	fputws(wstr, stdout);

	srand(clock_gettime_nsec_np(CLOCK_REALTIME));
	auto v = rand();

	printf("%f %f\n", sin(1.0f / (v % 20)), cos(1.0f / (v % 20)));

	int result = 0;
	sprt::initialize(sprt::AppConfig(), result);
	if (result != 0) {
		return result;
	}

	if (argc == 1) {
		for (auto &it : s_testList) { it.second(); }
		result = 0;
	} else if (argc == 2) {
		auto it = s_testList.find(argv[1]);
		if (it != s_testList.end()) {
			it->second();
			result = 0;
		} else {
			sprt::cerr << "Test not found: " << argv[1] << "\n";
			result = -1;
		}
	}

	sprt::terminate();
	return result;
}
