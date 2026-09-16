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

#include <sprt/runtime/stream.h>
#include <sprt/cxx/int_set>
#include <sprt/cxx/unordered_set>

namespace sprt {

template <typename T>
static void runIntSetTests(const char *label) {
	using int_set = __malloc_int_set<T>;

	sprt::cout << "\n== int_set<" << label << "> tests ==\n";

	// Test 1: Default constructor and empty check
	{
		int_set s;
		sprt::cout << "Test 1 - Default constructor: ";
		if (s.empty() && s.size() == 0 && s.max_size() > 0) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 2: Insert and duplicate insert (no growth on repeated insert of existing key)
	{
		int_set s;
		auto r1 = s.insert(T(42));
		auto r2 = s.insert(T(42));
		bool ok = r1.second && !r2.second && s.size() == 1 && *r2.first == T(42);
		for (int i = 0; i < 100; ++i) { s.insert(T(42)); }
		ok = ok && s.size() == 1;
		sprt::cout << "Test 2 - Insert and duplicate insert: " << (ok ? "PASS" : "FAIL") << "\n";
	}

	// Test 3: Zero key
	{
		int_set s;
		auto r = s.insert(T(0));
		bool ok = r.second && s.contains(T(0)) && s.find(T(0)) != s.end() && s.count(T(0)) == 1;
		ok = ok && s.erase(T(0)) == 1 && !s.contains(T(0)) && s.empty();
		sprt::cout << "Test 3 - Zero key: " << (ok ? "PASS" : "FAIL") << "\n";
	}

	// Test 4: Find / contains / count / erase by key
	{
		int_set s;
		s.insert(T(1));
		s.insert(T(2));
		s.insert(T(3));
		bool ok = s.find(T(2)) != s.end() && s.contains(T(3)) && s.count(T(1)) == 1
				&& s.count(T(4)) == 0 && !s.contains(T(4)) && s.find(T(4)) == s.end();
		ok = ok && s.erase(T(2)) == 1 && s.erase(T(2)) == 0 && s.size() == 2 && !s.contains(T(2));
		sprt::cout << "Test 4 - Find/contains/count/erase: " << (ok ? "PASS" : "FAIL") << "\n";
	}

	// Test 5: initializer_list, emplace, iteration completeness
	{
		int_set s{T(1), T(2), T(3), T(4), T(5)};
		s.emplace(T(6));
		s.insert({T(7), T(8)});
		T arr[] = {T(9), T(10)};
		s.insert(arr, arr + 2);

		size_t visited = 0;
		T sum = 0;
		for (auto &it : s) {
			++visited;
			sum += it;
		}
		bool ok = s.size() == 10 && visited == 10 && sum == T(55);
		sprt::cout << "Test 5 - initializer_list/emplace/iteration: " << (ok ? "PASS" : "FAIL")
				   << "\n";
	}

	// Test 6: Copy/move constructors and assignment, operator==
	{
		int_set s1{T(10), T(20), T(30)};

		int_set s2(s1);
		bool ok = s2.size() == 3 && s2.contains(T(20)) && s1 == s2;

		int_set s3(sprt::move(s1));
		ok = ok && s3.size() == 3 && s1.empty() && s3 == s2;

		int_set s4;
		s4 = s3;
		ok = ok && s4.size() == 3 && s4.contains(T(30));

		int_set s5;
		s5 = sprt::move(s4);
		ok = ok && s5.size() == 3 && s4.empty() && s5 == s2;

		sprt::cout << "Test 6 - Copy/move/assignment: " << (ok ? "PASS" : "FAIL") << "\n";
	}

	// Test 7: Swap and clear
	{
		int_set s1{T(1), T(2)};
		int_set s2{T(3)};
		s1.swap(s2);
		bool ok = s1.size() == 1 && s1.contains(T(3)) && s2.size() == 2 && s2.contains(T(1));
		s2.clear();
		ok = ok && s2.empty() && !s2.contains(T(1));
		sprt::cout << "Test 7 - Swap and clear: " << (ok ? "PASS" : "FAIL") << "\n";
	}

	// Test 8: Erase by iterator (returned iterator drains the whole set)
	{
		int_set s;
		for (T i = 0; i < T(64); ++i) { s.insert(i); }
		auto it = s.begin();
		size_t erased = 0;
		while (it != s.end()) {
			it = s.erase(it);
			++erased;
		}
		bool ok = erased == 64 && s.empty();
		sprt::cout << "Test 8 - Erase by iterator: " << (ok ? "PASS" : "FAIL") << "\n";
	}

	// Test 9: Dense sequential keys (identity-hash clusters), erase from chain middles
	{
		int_set s;
		s.max_load_factor(2.0f);
		const T base = T(0x12345600);
		const T n = T(300);
		for (T i = 0; i < n; ++i) { s.insert(base + i); }
		bool ok = s.size() == 300;
		for (T i = 0; i < n; i += 3) { ok = ok && s.erase(base + i) == 1; }
		for (T i = 0; i < n; ++i) {
			if (i % 3 == 0) {
				ok = ok && !s.contains(base + i);
			} else {
				ok = ok && s.contains(base + i);
			}
		}
		size_t visited = 0;
		for (auto &it : s) {
			(void)it;
			++visited;
		}
		ok = ok && visited == s.size();
		sprt::cout << "Test 9 - Dense sequential clusters: " << (ok ? "PASS" : "FAIL") << "\n";
	}

	// Test 10: Same-residue keys (multiples of a power of two collide at every table size)
	{
		int_set s;
		s.max_load_factor(4.0f);
		const T step = T(4096);
		for (T i = 1; i <= T(128); ++i) { s.insert(i * step); }
		bool ok = s.size() == 128;
		for (T i = 2; i <= T(128); i += 2) { ok = ok && s.erase(i * step) == 1; }
		for (T i = 1; i <= T(128); ++i) {
			ok = ok && (s.contains(i * step) == (i % 2 == 1));
		}
		sprt::cout << "Test 10 - Same-residue chains: " << (ok ? "PASS" : "FAIL") << "\n";
	}

	// Test 11: Randomized stress against reference unordered_set (deterministic LCG)
	{
		int_set s;
		__malloc_unordered_set<T> model;

		// high-bit base exercises full 64-bit values for uint64_t
		const T base = T(0x9ABCDEF012345678ull);

		uint64_t state = 0x853c49e6748fea9bull;
		auto next = [&state] {
			state = state * 6364136223846793005ull + 1442695040888963407ull;
			return uint64_t(state >> 16);
		};

		bool ok = true;
		for (int i = 0; i < 20'000 && ok; ++i) {
			auto r = next();
			T value = base + T(next() % 4'096);
			auto op = r % 100;
			if (op < 55) {
				auto ri = s.insert(value);
				auto rm = model.insert(value);
				ok = ri.second == rm.second && *ri.first == value;
			} else if (op < 85) {
				ok = s.erase(value) == model.erase(value);
			} else {
				ok = s.contains(value) == (model.find(value) != model.end())
						&& s.count(value) == model.count(value);
			}
		}

		ok = ok && s.size() == model.size();
		for (auto &it : model) { ok = ok && s.contains(it); }
		size_t visited = 0;
		for (auto &it : s) {
			ok = ok && model.find(it) != model.end();
			++visited;
		}
		ok = ok && visited == model.size();
		sprt::cout << "Test 11 - Randomized stress vs unordered_set: " << (ok ? "PASS" : "FAIL")
				   << "\n";
	}

	// Test 12: Growth from small capacity with high load factor
	{
		int_set s(4);
		s.max_load_factor(2.0f);
		for (T i = 0; i < T(2'048); ++i) { s.insert(i * T(7)); }
		bool ok = s.size() == 2'048 && s.load_factor() > 0.0f;
		for (T i = 0; i < T(2'048); ++i) { ok = ok && s.contains(i * T(7)); }
		sprt::cout << "Test 12 - Growth under load: " << (ok ? "PASS" : "FAIL") << "\n";
	}
}

void performIntSetTests() {
	runIntSetTests<uint32_t>("uint32_t");
	runIntSetTests<uint64_t>("uint64_t");

	sprt::cout << "\nint_set tests completed.\n";
}

} // namespace sprt
