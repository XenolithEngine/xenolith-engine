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
#include <string.h>
#include <locale.h>

#include "tests.h"

namespace sprt::test {

static const LibcTest s_tests[] = {
	// string.h
	{"string_compare", &performStringCompareTest},
	{"string_search", &performStringSearchTest},
	{"string_copy", &performStringCopyTest},
	{"string_token", &performStringTokenTest},
	{"memory", &performMemoryTest},
	// ctype.h
	{"ctype", &performCtypeTest},
	// stdlib.h
	{"strtol", &performStrtolTest},
	{"strtod", &performStrtodTest},
	{"atoi", &performAtoiTest},
	{"qsort_bsearch", &performQsortBsearchTest},
	{"abs_div", &performAbsDivTest},
	// stdio.h
	{"printf_int", &performPrintfIntTest},
	{"printf_float", &performPrintfFloatTest},
	{"printf_string", &performPrintfStringTest},
	{"scanf", &performScanfTest},
	// stdio.h (FILE* I/O) / unistd.h (fd I/O) / path forms
	{"stdio_file", &performStdioFileTest},
	{"unistd", &performUnistdTest},
	{"path_posix", &performPathPosixTest},
	{"path_windows", &performPathWindowsTest},
	{"dirent", &performDirentTest},
	{"fs_extra", &performFsExtraTest},
	{"env", &performEnvTest},
	// sys/socket.h (loopback UDP + TCP) / poll.h / sys/select.h
	// wasm32 has no BSD sockets (__sprt_socket/bind/poll are unavailable) — socket() returns
	// an invalid descriptor, and select_poll's FD_SET(rcv, ...) with rcv == -1 writes out of
	// the fd_set bounds and traps ("memory access out of bounds"). Skip the whole cluster on
	// wasm; host/Windows still run it (unaffected by this guard).
#if !defined(__wasm__)
	{"socket", &performSocketTest},
	{"socket_stream", &performSocketStreamTest},
	{"select_poll", &performSelectPollTest},
#endif
	// <fnmatch.h> / <regex.h> / <glob.h> forward
	{"fnmatch", &performFnmatchTest},
	{"regex", &performRegexTest},
	{"glob", &performGlobTest},
	// wchar.h / multibyte / uchar.h
	{"wchar_string", &performWcharStringTest},
	{"wcstol", &performWcstolTest},
	{"multibyte", &performMultibyteTest},
	{"uchar", &performUcharTest},
	// ISO macro/type mappings
	{"macros", &performMacrosTest},
	// math.h
	{"math", &performMathTest},
	// complex.h
	{"complex", &performComplexTest},
	// threads.h (C11)
	{"threads", &performThreadsTest},
	// tgmath.h
	{"tgmath", &performTgmathTest},
	// stdatomic.h
	{"stdatomic", &performStdatomicTest},
	// time.h
	{"time", &performTimeTest},
	// inttypes.h
	{"inttypes", &performInttypesTest},
	// <sprt/cxx/optional> (C++ standard conformance)
	{"optional", &performOptionalTest},
	// <sprt/cxx/variant> (C++ standard conformance)
	{"variant", &performVariantTest},
	// <sprt/cxx/any> (C++ standard conformance)
	{"any", &performAnyTest},
	// C <-> STL header coexistence + unambiguous global calls + no recursion
	{"coexist", &performCoexistTest},
	// <limits> std::numeric_limits conformance
	{"numeric_limits", &performLimitsTest},
	// <tuple> std::tuple + structured bindings for tuple/pair/array
	{"tuple", &performTupleTest},

	{"chrono", &performChronoTest},

	{"char_traits", &performCharTraitsTest},
	{"hash", &performHashTest},
	{"function", &performFunctionTest},
	// sprt::call_once invokes the callable exactly once (qonce mutable-lambda fix)
	{"call_once", &performCallOnceTest},

	// std::vector / std::string via the full sprt-allocator std::allocator (+ custom Traits)
	{"std_vector", &performVectorTest},
	{"std_string", &performStringTest},
	// std::pair: default ctor, piecewise_construct, <=>, swap, tuple protocol
	{"std_pair", &performPairTest},
	// std::string_view + std::string interop (starts_with / ends_with)
	{"std_string_view", &performStringViewTest},
	// std::to_string / to_wstring / stoi-family / operator""s
	{"std_string_conv", &performStringConvTest},
	// <memory>: unique_ptr / shared_ptr / weak_ptr / make_* / uninitialized_* / align
	{"std_memory", &performStdMemoryTest},
	// <numeric>/<span>/<functional> objects+bind/<charconv>/<compare> order CPOs
	{"std_purelib", &performPureLibTest},
	// <map>/<set>: rb-tree, CTAD, access_token operator[], erase_if
	{"std_mapset", &performMapSetTest},
	// <list>/<forward_list>: CTAD, list ops, erase/erase_if
	{"std_list", &performListTest},
	// <unordered_map>/<unordered_set>: node indirection (stable refs), CTAD, erase_if
	{"std_unordered", &performUnorderedTest},
	// <sstream>/<fstream>/<ostream>/<istream>: unformatted transfer, str(), endl/ends, file I/O
	{"std_stream", &performStreamTest},
	// <ostream> formatted numeric insertion + <iomanip>
	{"std_stream_numeric", &performStreamNumericTest},
	// <algorithm>/<iterator>: algorithms + iterator adapters, stable_sort, set ops, copy_n
	{"std_algorithm", &performAlgorithmTest},
	// <system_error>: error_code/error_condition/errc/category/system_error (portable facts)
	{"std_system_error", &performSystemErrorTest},
	// std extras: memory_order constants, index_sequence_for, conforming bool traits, less<>
	{"std_stl_extras", &performStlExtrasTest},

	// <deque>: block-map container across block boundaries
	{"std_deque", &performDequeTest},
	// <random>: engines (mandated 10000th values), seed_seq, distributions, random_device
	{"std_random", &performRandomTest},
	// <ratio>: compile-time rational arithmetic + comparison traits + SI typedefs
	{"std_ratio", &performRatioTest},
	// <future>: promise/future/shared_future value transfer + future_errc strings
	{"std_future", &performFutureTest},
	// <map> std::multimap: equal-key ordering, count/equal_range/bounds, erase(key)
	{"std_multimap", &performMultimapTest},
	// <functional> classic std::bind with placeholders / nested / ref / member ptr
	{"std_bind", &performBindTest},
	// incremental STL additions (minmax il, reverse_copy, bitset[], tuple=pair, ...)
	{"std_stl_additions", &performStlAdditionsTest},

	// <algorithm> additions: heap, merge/inplace_merge, is_permutation, search[_n],
	// nth_element, generate[_n], copy_backward, minmax, set-op comparators, equal 4-iter
	{"std_algorithm_ext", &performAlgorithmExtTest},
	// <stack>/<queue>: stack, queue, priority_queue (comparator/container/CTAD)
	{"std_container_adaptor", &performContainerAdaptorTest},
	// <cfenv>/<csignal>: std re-exports (fenv flags/rounding; signal install/raise)
	{"std_cfenv_csignal", &performFenvSignalTest},
	// container/STL fixes: multiset, string+char, map::emplace, move-only elements
	{"std_stl_fixes", &performStlFixesTest},

	{nullptr, nullptr},
};

const LibcTest *getLibcTests() { return s_tests; }

} // namespace sprt::test

using namespace sprt::test;

int main(int argc, const char *argv[]) {
	// Force the neutral "C" locale: glibc may otherwise honour LC_* from the
	// environment (e.g. comma decimal separator) while the freestanding
	// libc_impl always behaves as "C". The compare driver also sets LC_ALL=C.
	setlocale(LC_ALL, "C");

	const LibcTest *tests = getLibcTests();

	// `--list` prints every test name (one per line) so the compare driver can
	// enumerate and run them individually.
	if (argc >= 2 && strcmp(argv[1], "--list") == 0) {
		for (const LibcTest *t = tests; t->name; ++t) { printf("%s\n", t->name); }
		return 0;
	}

	if (argc >= 2) {
		for (const LibcTest *t = tests; t->name; ++t) {
			if (strcmp(t->name, argv[1]) == 0) {
				t->fn();
				return 0;
			}
		}
		fprintf(stderr, "Test not found: %s\n", argv[1]);
		return 1;
	}

	// No argument: run every test, framed by a header line so a full run can
	// also be diffed end to end.
	for (const LibcTest *t = tests; t->name; ++t) {
		printf("==== %s ====\n", t->name);
		t->fn();
	}
	return 0;
}
