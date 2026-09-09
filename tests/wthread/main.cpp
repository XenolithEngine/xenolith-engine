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

// Thread + allocator stress for the wasm target.
//
// Two things are under test, both about the single linear-memory growth lock:
//
//  1. malloc/free fan-out. mimalloc grows the heap through sbrk ->
//     memory.grow; concurrent growers used to corrupt page metadata
//     (mi_page_fresh_alloc, block_size == 0). Workers churn allocations of
//     every size class with fill/verify patterns while the heap grows well
//     past its initial size.
//  2. Raw sbrk from several threads at once: every handed-out range must be
//     above the heap base and pairwise non-overlapping. Only positive deltas -
//     handing memory back under a live malloc heap would corrupt it.
//
// Output is deterministic: results are aggregated on the main thread.

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

namespace {

constexpr int kWorkers = 8;
constexpr int kRounds = 24;
constexpr int kSlots = 16;

// Sizes cycle through mimalloc's small size classes and the page/multi-page
// ranges, so both the thread-free queues and the arena path grow.
constexpr size_t kSizes[] = {
	16, 64, 256, 1024, 4096, 16384, 65536, 262144,
};

struct WorkerResult {
	long fillErrors = 0;
	long allocFails = 0;
};

unsigned char patternByte(int worker, int round, int slot, size_t i) {
	return static_cast<unsigned char>((worker * 131 + round * 37 + slot * 17 + i) & 0xff);
}

void *stressWorker(void *arg) {
	const int worker = static_cast<int>(reinterpret_cast<intptr_t>(arg));
	WorkerResult result;
	unsigned char *slots[kSlots] = {};
	size_t slotSize[kSlots] = {};

	for (int round = 0; round < kRounds; ++round) {
		for (int slot = 0; slot < kSlots; ++slot) {
			const size_t size = kSizes[(worker + round + slot) % (sizeof(kSizes) / sizeof(kSizes[0]))];
			unsigned char *p = static_cast<unsigned char *>(malloc(size));
			if (!p) {
				++result.allocFails;
				continue;
			}
			for (size_t i = 0; i < size; ++i) {
				p[i] = patternByte(worker, round, slot, i);
			}
			slots[slot] = p; // leaks the previous slot value on purpose: churn
			slotSize[slot] = size;
		}
		// Verify and free every other slot. The fill pass above overwrites the
		// old pointer (that leak is the churn that keeps the heap growing), so
		// every slot in the array was written with THIS round's pattern.
		for (int slot = round % 2; slot < kSlots; slot += 2) {
			unsigned char *p = slots[slot];
			if (!p) {
				continue;
			}
			const size_t size = slotSize[slot];
			for (size_t i = 0; i < size; ++i) {
				if (p[i] != patternByte(worker, round, slot, i)) {
					++result.fillErrors;
					break;
				}
			}
			free(p);
			slots[slot] = nullptr;
		}
	}

	// Drop whatever is still live; verify it first (last fill was round kRounds-1).
	for (int slot = 0; slot < kSlots; ++slot) {
		unsigned char *p = slots[slot];
		if (!p) {
			continue;
		}
		const size_t size = slotSize[slot];
		for (size_t i = 0; i < size; ++i) {
			if (p[i] != patternByte(worker, kRounds - 1, slot, i)) {
				++result.fillErrors;
				break;
			}
		}
		free(p);
	}

	return new WorkerResult(result);
}

constexpr int kSbrkWorkers = 8;
constexpr int kSbrkRounds = 8;
constexpr size_t kSbrkChunk = 65536;

struct SbrkRange {
	uintptr_t start;
	uintptr_t end;
};

struct SbrkResult {
	int count = 0;
	int fails = 0;
	SbrkRange ranges[kSbrkRounds];
};

void *sbrkWorker(void *arg) {
	SbrkResult *result = static_cast<SbrkResult *>(arg);
	for (int i = 0; i < kSbrkRounds; ++i) {
		void *p = sbrk(static_cast<intptr_t>(kSbrkChunk));
		if (p == reinterpret_cast<void *>(static_cast<intptr_t>(-1))) {
			++result->fails;
			continue;
		}
		result->ranges[result->count].start = reinterpret_cast<uintptr_t>(p);
		result->ranges[result->count].end = reinterpret_cast<uintptr_t>(p) + kSbrkChunk;
		++result->count;
	}
	return nullptr;
}

} // namespace

int main(int, char **) {
	pthread_t threads[kWorkers];

	printf("wthread: spawning %d malloc workers x %d rounds\n", kWorkers, kRounds);
	for (int i = 0; i < kWorkers; ++i) {
		const int rc = pthread_create(&threads[i], nullptr, stressWorker,
				reinterpret_cast<void *>(static_cast<intptr_t>(i)));
		if (rc != 0) {
			printf("wthread: pthread_create failed rc=%d\n", rc);
			return 1;
		}
	}
	long fillErrors = 0, allocFails = 0;
	for (int i = 0; i < kWorkers; ++i) {
		void *ret = nullptr;
		pthread_join(threads[i], &ret);
		const WorkerResult *r = static_cast<const WorkerResult *>(ret);
		if (!r) {
			printf("wthread: worker %d returned no result\n", i);
			return 1;
		}
		fillErrors += r->fillErrors;
		allocFails += r->allocFails;
		delete r;
	}
	printf("wthread: malloc-stress fillErrors=%ld allocFails=%ld -> %s\n",
			fillErrors, allocFails, (fillErrors == 0 && allocFails == 0) ? "PASS" : "FAIL");

	printf("wthread: spawning %d sbrk workers x %d rounds\n", kSbrkWorkers, kSbrkRounds);
	pthread_t sbrkThreads[kSbrkWorkers];
	SbrkResult sbrkResults[kSbrkWorkers];
	memset(sbrkResults, 0, sizeof(sbrkResults));
	for (int i = 0; i < kSbrkWorkers; ++i) {
		const int rc = pthread_create(&sbrkThreads[i], nullptr, sbrkWorker, &sbrkResults[i]);
		if (rc != 0) {
			printf("wthread: pthread_create failed rc=%d\n", rc);
			return 1;
		}
	}
	for (int i = 0; i < kSbrkWorkers; ++i) {
		void *ret = nullptr;
		pthread_join(sbrkThreads[i], &ret);
	}

	long overlaps = 0, sbrkFails = 0, total = 0;
	for (int i = 0; i < kSbrkWorkers; ++i) {
		sbrkFails += sbrkResults[i].fails;
		total += sbrkResults[i].count;
		for (int j = 0; j < sbrkResults[i].count; ++j) {
			for (int k = i + 1; k < kSbrkWorkers; ++k) {
				for (int l = 0; l < sbrkResults[k].count; ++l) {
					const SbrkRange &a = sbrkResults[i].ranges[j];
					const SbrkRange &b = sbrkResults[k].ranges[l];
					if (a.start < b.end && b.start < a.end) {
						++overlaps;
					}
				}
			}
		}
	}
	printf("wthread: sbrk-race ranges=%ld fails=%ld overlaps=%ld -> %s\n",
			total, sbrkFails, overlaps,
			(overlaps == 0 && sbrkFails == 0) ? "PASS" : "FAIL");

	const bool ok = (fillErrors == 0 && allocFails == 0 && overlaps == 0 && sbrkFails == 0);
	printf("wthread: %s\n", ok ? "ALL PASS" : "FAILED");
	return ok ? 0 : 1;
}
