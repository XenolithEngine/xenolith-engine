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

// The runtime's allocator under threads. Written for Embox EL1, where the
// runtime's malloc became mimalloc on a per-thread block behind TPIDR_EL0 and
// the kernel keeps its own malloc under the public names -- but nothing here
// is specific to it, and it runs everywhere:
//
//   threads     four threads allocate, fill, check and free, and hand a share
//               of their blocks to the next thread to free (cross-thread free)
//   aligned     posix_memalign / aligned_alloc up to 64 KiB
//   realloc     grow and shrink keep the contents
//   foreign     strdup's result goes back through free(); on Embox EL1 that is
//               the kernel's allocation, freed through the runtime's entry
//   churn       more short threads than the per-thread block pool holds (256
//               on Embox EL1), each allocating: a block that does not come back
//               at thread exit aborts this

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../tests.h"

namespace sprt {

namespace {

constexpr int Threads = 4;
constexpr int Rounds = 20'000;
constexpr int Live = 64;
constexpr int Handoff = 256;
constexpr int ChurnThreads = 300;

struct Block {
	unsigned char *ptr = nullptr;
	size_t size = 0;
	unsigned char mark = 0;
};

// Blocks one thread hands to the next to free.
struct Mailbox {
	pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	Block blocks[Handoff];
	int count = 0;
};

Mailbox s_mailbox[Threads];
int s_corrupt = 0;
int s_crossFreed = 0;
pthread_mutex_t s_statsMutex = PTHREAD_MUTEX_INITIALIZER;

bool blockCheck(const Block &b) {
	for (size_t i = 0; i < b.size; i += 61) {
		if (b.ptr[i] != b.mark) {
			return false;
		}
	}
	return b.size == 0 || b.ptr[b.size - 1] == b.mark;
}

void *threadWorker(void *arg) {
	const int id = int(reinterpret_cast<intptr_t>(arg));
	Block live[Live];
	uint64_t x = 0x9e3779b97f4a7c15ull * uint64_t(id + 1);
	int corrupt = 0, crossFreed = 0;

	for (int round = 0; round < Rounds; ++round) {
		x = x * 6'364'136'223'846'793'005ull + 1'442'695'040'888'963'407ull;
		auto &b = live[(x >> 33) % Live];

		if (b.ptr) {
			if (!blockCheck(b)) {
				++corrupt;
			}
			if ((x >> 20) % 8 == 0) {
				// Hand it to the next thread instead of freeing it here.
				auto &box = s_mailbox[(id + 1) % Threads];
				pthread_mutex_lock(&box.mutex);
				if (box.count < Handoff) {
					box.blocks[box.count++] = b;
					b.ptr = nullptr;
				}
				pthread_mutex_unlock(&box.mutex);
			}
			if (b.ptr) {
				free(b.ptr);
				b.ptr = nullptr;
			}
			continue;
		}

		// Mostly small, sometimes large: the shapes an engine frame makes.
		b.size = ((x >> 12) % 16 == 0) ? 4'096 + (x >> 40) % 65'536 : 1 + (x >> 40) % 512;
		b.ptr = static_cast<unsigned char *>(malloc(b.size));
		if (!b.ptr) {
			++corrupt;
			continue;
		}
		b.mark = static_cast<unsigned char>(id * 16 + round % 16 + 1);
		memset(b.ptr, b.mark, b.size);

		// Free what the previous thread handed over.
		auto &mine = s_mailbox[id];
		pthread_mutex_lock(&mine.mutex);
		while (mine.count > 0) {
			auto &h = mine.blocks[--mine.count];
			if (!blockCheck(h)) {
				++corrupt;
			}
			free(h.ptr);
			++crossFreed;
		}
		pthread_mutex_unlock(&mine.mutex);
	}

	for (auto &b : live) {
		if (b.ptr) {
			if (!blockCheck(b)) {
				++corrupt;
			}
			free(b.ptr);
		}
	}

	pthread_mutex_lock(&s_statsMutex);
	s_corrupt += corrupt;
	s_crossFreed += crossFreed;
	pthread_mutex_unlock(&s_statsMutex);
	return nullptr;
}

#if defined(__EMBOX__) && !defined(__EMBOX_USER__)
uintptr_t readTpidr() {
	uintptr_t p;
	__asm__ volatile("mrs %0, tpidr_el0" : "=r"(p));
	return p;
}
#endif

void *churnWorker(void *arg) {
	auto ok = static_cast<int *>(arg);
	void *p = malloc(128);
	if (p) {
		memset(p, 0x5a, 128);
		free(p);
	}
#if defined(__EMBOX__) && !defined(__EMBOX_USER__)
	// The thread's block is behind the register once it has allocated.
	*ok = (p != nullptr && readTpidr() != 0) ? 1 : 0;
#else
	*ok = (p != nullptr) ? 1 : 0;
#endif
	return nullptr;
}

} // namespace

void performMallocThreadsTest() {
	int failures = 0;
	auto check = [&](bool cond, const char *msg) {
		printf("  %s: %s\n", cond ? "PASS" : sprt::test::failed("FAIL"), msg);
		if (!cond) {
			++failures;
		}
	};

	// threads
	pthread_t t[Threads];
	bool created = true;
	for (int i = 0; i < Threads; ++i) {
		if (pthread_create(&t[i], nullptr, threadWorker, reinterpret_cast<void *>(intptr_t(i)))
				!= 0) {
			created = false;
		}
	}
	for (int i = 0; i < Threads; ++i) {
		pthread_join(t[i], nullptr);
	}
	for (auto &box : s_mailbox) {
		while (box.count > 0) {
			free(box.blocks[--box.count].ptr);
		}
	}
	printf("malloc_threads: %d thread(s) x %d rounds, %d freed by another thread, %d corrupt\n",
			Threads, Rounds, s_crossFreed, s_corrupt);
	check(created, "four threads started");
	check(s_corrupt == 0, "no block was corrupted or refused");
	check(s_crossFreed > 0, "blocks were freed by a thread other than their allocator");

	// aligned
	bool alignedOk = true;
	for (size_t align = 16; align <= 65'536; align *= 2) {
		void *p = nullptr;
		if (posix_memalign(&p, align, align * 3 + 5) != 0 || !p
				|| (reinterpret_cast<uintptr_t>(p) % align) != 0) {
			alignedOk = false;
		}
		free(p);
		void *q = aligned_alloc(align, align * 2);
		if (!q || (reinterpret_cast<uintptr_t>(q) % align) != 0) {
			alignedOk = false;
		}
		free(q);
	}
	check(alignedOk, "posix_memalign and aligned_alloc honour 16 B .. 64 KiB");

	// realloc
	auto r = static_cast<unsigned char *>(malloc(100));
	bool reallocOk = r != nullptr;
	if (r) {
		for (int i = 0; i < 100; ++i) {
			r[i] = static_cast<unsigned char>(i);
		}
		r = static_cast<unsigned char *>(realloc(r, 1'000'000));
		for (int i = 0; r && i < 100; ++i) {
			reallocOk = reallocOk && r[i] == static_cast<unsigned char>(i);
		}
		r = r ? static_cast<unsigned char *>(realloc(r, 10)) : nullptr;
		for (int i = 0; r && i < 10; ++i) {
			reallocOk = reallocOk && r[i] == static_cast<unsigned char>(i);
		}
		reallocOk = reallocOk && r != nullptr;
		free(r);
	}
	check(reallocOk, "realloc grows and shrinks and keeps the contents");

	// foreign
	char *d = strdup("allocated by whoever strdup belongs to");
	check(d && strcmp(d, "allocated by whoever strdup belongs to") == 0, "strdup copies");
	d = static_cast<char *>(realloc(d, 4'096));
	check(d && strncmp(d, "allocated by", 12) == 0, "... realloc of its result keeps it");
	free(d);
	check(true, "... and free takes it back");

	// churn
	int churnOk = 0, churnStarted = 0;
	for (int i = 0; i < ChurnThreads; ++i) {
		pthread_t c;
		int ok = 0;
		if (pthread_create(&c, nullptr, churnWorker, &ok) == 0) {
			pthread_join(c, nullptr);
			++churnStarted;
			churnOk += ok;
		}
	}
	printf("malloc_threads: %d short thread(s), %d allocated as expected\n", churnStarted,
			churnOk);
	check(churnStarted == ChurnThreads && churnOk == ChurnThreads,
			"more short threads than there are per-thread blocks, one after another");

	printf("malloc_threads: %s (%d failure(s))\n", failures ? sprt::test::failed("FAIL") : "PASS",
			failures);
}

} // namespace sprt
