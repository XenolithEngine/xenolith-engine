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

// New container adaptors: <stack>, <queue> (std::queue + std::priority_queue).
// Exercises push/pop/emplace, top/front/back, size/empty, comparison operators,
// custom underlying containers, custom comparators and CTAD. Deterministic output
// diffs identically across the sprt targets; Makefile.system gives the libstdc++
// reference.

#include <stdio.h>

#include <functional>
#include <queue>
#include <stack>
#include <vector>

namespace sprt::test {

void performContainerAdaptorTest() {
	// ---- std::stack (LIFO over the default deque) --------------------------
	std::stack<int> st;
	for (int i = 1; i <= 5; ++i) { st.push(i); }
	printf("stack: size=%d top=%d\n", (int) st.size(), st.top());
	st.emplace(99);
	printf("stack: after_emplace top=%d\n", st.top());
	printf("stack_pop=");
	while (!st.empty()) {
		printf("%d", st.top());
		st.pop();
	}
	printf("\n");

	// stack equality operator and empty flag
	std::stack<int> s1;
	std::stack<int> s2;
	s1.push(1);
	s1.push(2);
	s2.push(1);
	s2.push(2);
	s2.push(3);
	printf("stack_cmp: eq=%d neq=%d empty=%d\n", (int) (s1 == s2), (int) (s1 != s2),
			(int) std::stack<int>().empty());

	// ---- std::queue (FIFO over the default deque) --------------------------
	std::queue<int> q;
	for (int i = 1; i <= 4; ++i) { q.push(i * 10); }
	printf("queue: size=%d front=%d back=%d\n", (int) q.size(), q.front(), q.back());
	q.emplace(50);
	printf("queue_pop=");
	while (!q.empty()) {
		printf("%d,", q.front());
		q.pop();
	}
	printf("\n");

	// ---- std::priority_queue (max-heap over vector, default less) ----------
	std::priority_queue<int> pq;
	int feed[6] = {3, 1, 4, 1, 5, 9};
	for (int v : feed) { pq.push(v); }
	printf("pqueue_max=");
	while (!pq.empty()) {
		printf("%d,", pq.top());
		pq.pop();
	}
	printf("\n");

	// priority_queue with greater<> yields a min-heap
	std::priority_queue<int, std::vector<int>, std::greater<int>> minpq;
	for (int v : feed) { minpq.push(v); }
	printf("pqueue_min=");
	while (!minpq.empty()) {
		printf("%d,", minpq.top());
		minpq.pop();
	}
	printf("\n");

	// priority_queue built from a range (heapifies on construction)
	int rng[5] = {7, 2, 8, 4, 1};
	std::priority_queue<int> rpq(rng, rng + 5);
	printf("pqueue_range: size=%d top=%d\n", (int) rpq.size(), rpq.top());
	rpq.emplace(10);
	printf("pqueue_range: after_emplace top=%d\n", rpq.top());

	// ---- CTAD: stack/queue deduced from an underlying container ------------
	std::vector<int> under = {5, 6, 7};
	std::stack ded(under);
	printf("ctad_stack: size=%d top=%d\n", (int) ded.size(), ded.top());
}

} // namespace sprt::test
