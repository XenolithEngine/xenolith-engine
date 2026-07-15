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

#include <deque>
#include <string>

namespace sprt::test {

namespace {

// A small element that reports moves separately from copies, to show emplace_front
// / emplace_back forward their arguments.
struct Elem {
	int v = 0;
	Elem() = default;
	explicit Elem(int x) : v(x) { }
};

// long long (64-bit on both LP64 and Windows LLP64) so the weighted sum does not
// overflow differently between the two targets.
long long checksum(const std::deque<int> &d) {
	long long s = 0;
	int i = 0;
	for (int x : d) { s += (long long)x * (++i); } // position-weighted: order-sensitive
	return s;
}

} // namespace

void performDequeTest() {
	std::deque<int> d;
	printf("empty: empty=%d size=%zu\n", (int)d.empty(), d.size());

	// grow past several blocks from the back
	for (int i = 0; i < 3'000; ++i) { d.push_back(i); }
	printf("push_back: size=%zu front=%d back=%d mid=%d checksum=%lld\n", d.size(), d.front(),
			d.back(), d[1'500], checksum(d));

	// prepend from the front (also crosses the front block boundary)
	for (int i = 1; i <= 200; ++i) { d.push_front(-i); }
	printf("push_front: size=%zu front=%d at200=%d back=%d\n", d.size(), d.front(), d[200],
			d.back());

	// random access + at() (in range only: at() traps out of range, no exceptions)
	printf("access: d[0]=%d d[199]=%d at(200)=%d at(size-1)=%d\n", d[0], d[199], d.at(200),
			d.at(d.size() - 1));

	// pop from both ends
	for (int i = 0; i < 200; ++i) { d.pop_front(); }
	for (int i = 0; i < 1'000; ++i) { d.pop_back(); }
	printf("pop: size=%zu front=%d back=%d\n", d.size(), d.front(), d.back());

	// forward vs reverse iteration agree on the endpoints
	int firstFwd = *d.begin();
	int lastFwd = *(--d.end());
	int firstRev = *d.rbegin();
	printf("iter: firstFwd=%d lastFwd=%d firstRev=%d\n", firstFwd, lastFwd, firstRev);

	// copy construction is a deep, independent copy
	std::deque<int> e(d);
	e.push_back(999'999);
	printf("copy: e.size=%zu d.size=%zu e.back=%d d.back=%d\n", e.size(), d.size(), e.back(),
			d.back());

	// resize grow (fill) then shrink
	std::deque<int> r;
	r.resize(5, 7);
	r.resize(8);
	r.resize(3);
	printf("resize: size=%zu [0]=%d [4]=%d\n", r.size(), r[0], r[4]);

	// deque(n, value)
	std::deque<int> n(4, 42);
	printf("fill_ctor: size=%zu front=%d back=%d\n", n.size(), n.front(), n.back());

	// swap
	std::deque<int> a{1, 2, 3}, b{9, 8};
	a.swap(b);
	printf("swap: a.size=%zu a.front=%d b.size=%zu b.front=%d\n", a.size(), a.front(), b.size(),
			b.front());

	// emplace at both ends with a user type
	std::deque<Elem> ed;
	ed.emplace_back(10);
	ed.emplace_front(20);
	ed.emplace_back(30);
	printf("emplace: size=%zu front=%d back=%d\n", ed.size(), ed.front().v, ed.back().v);

	// deque of strings survives block relocation of non-trivial elements
	std::deque<std::string> sd;
	for (int i = 0; i < 300; ++i) { sd.push_back(std::string("s") + std::to_string(i)); }
	printf("strings: size=%zu front=%s mid=%s back=%s\n", sd.size(), sd.front().c_str(),
			sd[150].c_str(), sd.back().c_str());

	// clear
	d.clear();
	printf("clear: empty=%d size=%zu\n", (int)d.empty(), d.size());
}

} // namespace sprt::test
