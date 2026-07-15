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

#include <random>

namespace sprt::test {

void performRandomTest() {
	// --- engines: standard-mandated 10000th value after the default seed ---
	{
		std::mt19937 g;
		g.discard(9'999);
		unsigned long v = g();
		printf("mt19937: 10000th=%lu ok=%d min=%lu max=%lu\n", v, (int)(v == 4'123'659'995UL),
				(unsigned long)std::mt19937::min(), (unsigned long)std::mt19937::max());
	}
	{
		std::mt19937_64 g;
		g.discard(9'999);
		unsigned long long v = g();
		printf("mt19937_64: 10000th=%llu ok=%d\n", v, (int)(v == 9'981'545'732'273'789'042ULL));
	}
	{
		std::minstd_rand g; // 48271 multiplier; standard value is 399268537
		g.discard(9'999);
		unsigned long v = g();
		printf("minstd_rand: 10000th=%lu ok=%d\n", v, (int)(v == 399'268'537UL));
	}
	{
		std::minstd_rand0 g; // 16807 multiplier; standard value is 1043618065
		g.discard(9'999);
		unsigned long v = g();
		printf("minstd_rand0: 10000th=%lu ok=%d\n", v, (int)(v == 1'043'618'065UL));
	}

	// --- seed_seq: generate() output is fully specified ---
	{
		std::seed_seq ss{1, 2, 3, 4};
		unsigned out[6] = {0};
		ss.generate(out, out + 6);
		printf("seed_seq: size=%zu %u %u %u %u %u %u\n", ss.size(), out[0], out[1], out[2], out[3],
				out[4], out[5]);
	}

	// --- distributions: fixed-seed engine, so the draw sequence is reproducible ---
	{
		std::mt19937 g(12'345u);
		std::uniform_int_distribution<int> die(1, 6);
		int inRange = 1;
		int first10[10];
		for (int i = 0; i < 10; ++i) {
			first10[i] = die(g);
			if (first10[i] < 1 || first10[i] > 6) {
				inRange = 0;
			}
		}
		printf("uniform_int[1,6]: inRange=%d a=%d b=%d c=%d min=%d max=%d\n", inRange, first10[0],
				first10[1], first10[2], die.min(), die.max());
	}
	{
		std::mt19937 g(777u);
		std::uniform_real_distribution<double> ur(0.0, 1.0);
		double x = ur(g);
		double y = ur(g);
		printf("uniform_real[0,1): x=%.6f y=%.6f inRange=%d\n", x, y,
				(int)(x >= 0.0 && x < 1.0 && y >= 0.0 && y < 1.0));
	}
	{
		std::mt19937 g(555u);
		std::bernoulli_distribution coin(0.5);
		int heads = 0;
		for (int i = 0; i < 1'000; ++i) {
			if (coin(g)) {
				++heads;
			}
		}
		printf("bernoulli(0.5): heads_of_1000=%d\n", heads);
	}

	// --- random_device: structural only (values are nondeterministic) ---
	{
		std::random_device rd;
		unsigned r = rd();
		printf("random_device: min=%u max=%u inRange=%d\n", (unsigned)std::random_device::min(),
				(unsigned)std::random_device::max(),
				(int)(r >= std::random_device::min() && r <= std::random_device::max()));
	}
}

} // namespace sprt::test
