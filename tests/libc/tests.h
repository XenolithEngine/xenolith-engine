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

#ifndef TESTS_LIBC_TESTS_H
#define TESTS_LIBC_TESTS_H

namespace sprt::test {

// Each entry is a self-contained deterministic test. Output must not depend on
// addresses, time, locale or uninitialised memory so that the host (Linux libc)
// and x86_64-pc-windows-msvc (freestanding libc_impl) runs can be diffed.
struct LibcTest {
	const char *name;
	void (*fn)();
};

// Returned list is sorted by name; terminated by a {nullptr, nullptr} entry.
const LibcTest *getLibcTests();

// string.h / strings.h
void performStringCompareTest();
void performStringSearchTest();
void performStringCopyTest();
void performStringTokenTest();
void performMemoryTest();

// ctype.h
void performCtypeTest();

// stdlib.h
void performStrtolTest();
void performStrtodTest();
void performAtoiTest();
void performQsortBsearchTest();
void performAbsDivTest();

// stdio.h (formatting)
void performPrintfIntTest();
void performPrintfFloatTest();
void performPrintfStringTest();
void performScanfTest();

// stdio.h (FILE* I/O) / unistd.h (fd I/O) / path forms / dirent.h
void performStdioFileTest();
void performUnistdTest();
void performPathPosixTest();
void performPathWindowsTest();
void performDirentTest();
void performFsExtraTest();
void performFnmatchTest();
void performRegexTest();
void performGlobTest();

// stdlib.h (environment)
void performEnvTest();

// <sys/socket.h>: socket/bind/connect/getsockname/getpeername/send*/recv*/getsockopt/
// setsockopt/shutdown over AF_INET loopback UDP (identical host vs winsock/wine)
void performSocketTest();
// <sys/socket.h>: listen/accept/connect + stream send/recv over loopback TCP
void performSocketStreamTest();
// <poll.h> / <sys/select.h>: poll()/select() readiness on a loopback UDP socket
void performSelectPollTest();

// wchar.h / multibyte / uchar.h
void performWcharStringTest();
void performWcstolTest();
void performMultibyteTest();
void performUcharTest();

// ISO macro/type mappings (signal/fenv/math/wide/stdio/assert)
void performMacrosTest();

// math.h
void performMathTest();

// complex.h
void performComplexTest();

// threads.h (C11)
void performThreadsTest();

// tgmath.h (type-generic math; impl in tgmath_c.c)
void performTgmathTest();

// stdatomic.h (C11 atomics; impl in stdatomic_c.c)
void performStdatomicTest();

// time.h
void performTimeTest();

// inttypes.h
void performInttypesTest();

// <sprt/cxx/optional> (C++ standard conformance)
void performOptionalTest();

// <sprt/cxx/variant> (C++ standard conformance)
void performVariantTest();

// <sprt/cxx/any> (C++ standard conformance)
void performAnyTest();

// C headers + STL counterparts coexistence (string.h/cstring, ...) + unambiguous
// global calls + no infinite recursion
void performCoexistTest();

// <limits> std::numeric_limits (sprt-backed STL header vs system reference)
void performLimitsTest();

// <tuple> std::tuple + tuple protocol for pair/array (sprt-backed STL vs system)
void performTupleTest();

// <chrono> duration / time_point / clocks / <=> (sprt-backed STL vs system)
void performChronoTest();

// <string> std::char_traits (sprt-backed STL vs system)
void performCharTraitsTest();

// <functional> std::hash enabled/disabled split (sprt-backed STL vs system)
void performHashTest();

// <functional> std::function (sprt::__malloc_function) incl. edge cases
void performFunctionTest();

// sprt::call_once / std::call_once (qonce mutable-lambda fix)
void performCallOnceTest();

// <vector> std::vector (sprt::__vector via the full sprt-allocator std::allocator) vs system
void performVectorTest();

// <string> std::string / std::basic_string incl. a custom Traits parameter vs system
void performStringTest();

// <utility> std::pair: default ctor, piecewise_construct, <=>, swap, tuple protocol
void performPairTest();

// <string_view> std::string_view + std::string interop (starts_with/ends_with/contains)
void performStringViewTest();

// <string> std::to_string / to_wstring / stoi-family / operator""s
void performStringConvTest();

// <memory> unique_ptr / shared_ptr / weak_ptr / make_* / uninitialized_* / align
void performStdMemoryTest();

// <numeric> / <span> / <functional> objects+bind / <charconv> / <compare> order CPOs
void performPureLibTest();

// <map> / <set> (rb-tree, CTAD, access_token operator[], erase_if) vs system
void performMapSetTest();

// <list> / <forward_list> (CTAD, list ops, erase/erase_if) vs system
void performListTest();

// <unordered_map> / <unordered_set> (node indirection, reference stability, CTAD, erase_if) vs system
void performUnorderedTest();

// <sstream>/<fstream>/<ostream>/<istream> skeleton: unformatted transfer, str(),
// endl/ends, binary file I/O (sprt-backed STL streams vs system)
void performStreamTest();

// <ostream> formatted numeric insertion + <iomanip> (base/width/fill/precision/
// showbase/showpos/boolalpha) via ostringstream
void performStreamNumericTest();

// <algorithm> / <iterator>: algorithms + iterator adapters (reverse/move/insert,
// rbegin/rend), stable_sort, set ops, copy_n (sprt-backed STL vs system)
void performAlgorithmTest();

// <system_error>: error_code / error_condition / errc / category / system_error
// (portable equivalence + trait tags only; sprt-backed STL vs system)
void performSystemErrorTest();

// small std extras: <atomic> memory_order constants, <utility> index_sequence_for,
// conforming <type_traits> bool traits (contextual bool), transparent std::less<>
void performStlExtrasTest();

// <deque>: block-map container, push/pop/emplace both ends across blocks, random
// access, resize, copy, swap (sprt-backed STL vs itself on the two targets)
void performDequeTest();

// <random>: engines (standard-mandated 10000th values), seed_seq, distributions,
// random_device (structural only)
void performRandomTest();

// <ratio>: compile-time rational arithmetic, comparison traits, SI-prefix typedefs
void performRatioTest();

// <future>: promise/future/shared_future value transfer (happy path under
// -fno-exceptions) + future_errc/future_category strings
void performFutureTest();

// <map> std::multimap: equal-key insertion order, count/equal_range/bounds,
// erase(key) count, initializer_list ctor
void performMultimapTest();

// <functional> classic std::bind with placeholders: selection/reorder, nested bind,
// reference_wrapper, pointer-to-member, is_placeholder/is_bind_expression
void performBindTest();

// incremental STL additions: initializer_list min/max, reverse_copy, bitset
// operator[] proxy, tuple=pair, string=string_view, ATOMIC_FLAG_INIT, cmath integer
// overloads, chrono_literals
void performStlAdditionsTest();

// <algorithm> additions: heap family, merge/inplace_merge, is_permutation,
// search/search_n, nth_element, generate/generate_n, copy_backward, minmax
// (pair/init_list), set_intersection/set_union comparator overloads, equal 4-iter
void performAlgorithmExtTest();

// <stack> / <queue>: std::stack, std::queue, std::priority_queue (custom
// container/comparator, range ctor, CTAD)
void performContainerAdaptorTest();

// <cfenv> / <csignal>: std re-exports (fenv flags + rounding round-trip;
// signal install/raise round-trip)
void performFenvSignalTest();

// container/STL correctness fixes: std::multiset, string operator+ char,
// map::emplace, move-only element support (vector/list/map/set move ctor)
void performStlFixesTest();

} // namespace sprt::test

#endif // TESTS_LIBC_TESTS_H
