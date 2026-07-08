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

// <memory> conformance: unique_ptr / shared_ptr / weak_ptr / enable_shared_from_this /
// make_unique / make_shared, the uninitialized_* algorithms and std::align. On the
// freestanding x86_64-pc-windows-msvc build these are the sprt implementations; on the
// Linux host they are libstdc++. compare.sh diffs the two. Output is deterministic: only
// values, booleans, use_counts and destructor counts are printed (never an address), and
// std::align is anchored to an alignas() buffer so the byte-skip is fixed.

#include <stdio.h>

#include <memory>
#include <functional>
#include <utility>

namespace sprt::test {

namespace {

int g_deleter_calls = 0;
int g_track_dtors = 0;

struct Track {
	~Track() { ++g_track_dtors; }
};

struct IntDeleter {
	void operator()(int *__p) const noexcept {
		++g_deleter_calls;
		__SPRT_PUSH_ALLOW_CXXABI_ALLOC
		delete __p;
		__SPRT_POP_ALLOW_CXXABI_ALLOC
	}
};

__SPRT_PUSH_ALLOW_CXXABI_ALLOC
struct Base {
	virtual ~Base() { }

	virtual int id() const { return 1; }
};

struct Derived : Base {
	int id() const override { return 2; }
};
__SPRT_POP_ALLOW_CXXABI_ALLOC

struct Widget : std::enable_shared_from_this<Widget> {
	int v = 42;
	std::shared_ptr<Widget> self() { return shared_from_this(); }
};

} // namespace

void performStdMemoryTest() {
	// ---- unique_ptr ----
	auto u = std::make_unique<int>(5);
	*u += 2;
	printf("uptr: val=%d bool=%d\n", *u, (int)(bool)u);
	auto u2 = std::move(u);
	printf("uptr move: src_empty=%d dst=%d\n", (int)!u, *u2);

	__SPRT_PUSH_ALLOW_CXXABI_ALLOC
	std::unique_ptr<int, IntDeleter> ud(new int(9), IntDeleter{});
	__SPRT_POP_ALLOW_CXXABI_ALLOC

	ud.reset();
	printf("uptr custom_deleter: calls=%d empty=%d\n", g_deleter_calls, (int)!ud);

	auto ua = std::make_unique<int[]>(4);
	for (int i = 0; i < 4; ++i) { ua[i] = i * i; }
	printf("uptr array: [0]=%d [3]=%d\n", ua[0], ua[3]);

	// ---- shared_ptr use_count / weak_ptr ----
	auto s = std::make_shared<int>(10);
	printf("sptr: val=%d uc=%ld\n", *s, s.use_count());
	{
		auto s2 = s;
		printf("sptr copy: uc=%ld\n", s.use_count());
		std::weak_ptr<int> w = s;
		// Sequence lock() before reading use_count(): lock()'s temporary shared_ptr lives to
		// end-of-statement, and printf argument evaluation order is unspecified (differs
		// between the Itanium and MSVC ABIs), which would otherwise make use_count() race it.
		bool locked_ok = static_cast<bool>(w.lock());
		printf("weak: uc=%ld expired=%d locked_ok=%d\n", w.use_count(), (int)w.expired(),
				(int)locked_ok);
	}
	printf("sptr after scope: uc=%ld\n", s.use_count());

	// ---- destructor timing ----
	{
		auto t = std::make_shared<Track>();
		auto t2 = t;
		printf("track: uc=%ld dtors=%d\n", t.use_count(), g_track_dtors);
	}
	printf("track after scope: dtors=%d\n", g_track_dtors);

	// weak_ptr outliving the owner
	std::weak_ptr<Track> wt;
	{
		auto sp = std::make_shared<Track>();
		wt = sp;
	}
	printf("weak expired after owner gone: expired=%d lock_null=%d dtors=%d\n", (int)wt.expired(),
			(int)!wt.lock(), g_track_dtors);

	// ---- polymorphism + casts ----
	std::shared_ptr<Base> b = std::make_shared<Derived>();
	auto d = std::dynamic_pointer_cast<Derived>(b);
	printf("cast: id=%d dyn_ok=%d\n", b->id(), (int)(bool)d);
	std::shared_ptr<Base> bb = std::make_shared<Base>();
	auto dfail = std::dynamic_pointer_cast<Derived>(bb);
	printf("dyn_fail_null=%d\n", (int)!dfail);
	std::shared_ptr<const Base> cb = b;
	auto ncb = std::const_pointer_cast<Base>(cb);
	printf("const_cast: same=%d\n", (int)(b.get() == ncb.get()));

	// ---- get_deleter (named deleter matches, other type does not) ----

	__SPRT_PUSH_ALLOW_CXXABI_ALLOC
	std::shared_ptr<int> pd(new int(3), IntDeleter{});
	__SPRT_POP_ALLOW_CXXABI_ALLOC

	IntDeleter *gd = std::get_deleter<IntDeleter>(pd);
	int *gd2 = std::get_deleter<int>(pd);
	printf("get_deleter: match=%d mismatch_null=%d\n", (int)(gd != nullptr), (int)(gd2 == nullptr));

	// ---- enable_shared_from_this ----
	auto wgt = std::make_shared<Widget>();
	auto again = wgt->self();
	printf("esft: same=%d uc=%ld v=%d\n", (int)(wgt.get() == again.get()), wgt.use_count(), wgt->v);

	// ---- hash usable (value is address-dependent, so only check it is callable) ----
	size_t h1 = std::hash<std::shared_ptr<int>>{}(s);
	size_t h2 = std::hash<std::unique_ptr<int>>{}(u2);
	printf("hash callable: %d\n", (int)((h1 | h2) != 0 || (h1 == 0 && h2 == 0)));

	// ---- uninitialized_* algorithms ----
	alignas(int) unsigned char raw[sizeof(int) * 5];
	int *ip = reinterpret_cast<int *>(raw);
	std::uninitialized_value_construct_n(ip, 3);
	int src[2] = {11, 22};
	std::uninitialized_copy(src, src + 2, ip + 3);
	int sum = 0;
	for (int i = 0; i < 5; ++i) { sum += ip[i]; }
	printf("uninit: sum=%d (0+0+0+11+22)\n", sum);
	std::destroy_n(ip, 5);

	std::uninitialized_fill_n(ip, 4, 7);
	printf("uninit_fill: [0]=%d [3]=%d\n", ip[0], ip[3]);
	std::destroy_n(ip, 4);

	// ---- std::align (anchored to alignas so the skip is deterministic) ----
	alignas(64) unsigned char abuf[64];
	void *aptr = abuf + 3; // 3 bytes past a 64-aligned base
	size_t aspace = 61;
	void *aligned = std::align(8, 16, aptr, aspace);
	printf("align: nonnull=%d offset=%d space=%d aligned=%d\n", (int)(aligned != nullptr),
			(int)(reinterpret_cast<unsigned char *>(aligned) - abuf), (int)aspace,
			(int)(reinterpret_cast<__UINTPTR_TYPE__>(aligned) % 8 == 0));
}

} // namespace sprt::test
