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

#include "src/lfs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// lfstest: acceptance tests of runtime/libc_freestanding.
//
//   lfstest [conformance] [--full] [--impl NAME] [--func NAME]
//   lfstest bench [--impl NAME] [--func NAME] [--reps N] [--warmup N] [--max-size BYTES]
//   lfstest --list

static void printUsage() {
	printf("usage: lfstest [conformance|bench|--list] [--full] [--impl NAME] [--func NAME]\n"
		   "               [--reps N] [--warmup N] [--max-size BYTES]\n");
}

int main(int argc, const char *argv[]) {
	lfs::Options opts;
	const char *mode = "conformance";
	for (int i = 1; i < argc; ++i) {
		auto arg = argv[i];
		auto value = [&]() -> const char * { return i + 1 < argc ? argv[++i] : nullptr; };
		if (strcmp(arg, "conformance") == 0 || strcmp(arg, "bench") == 0) {
			mode = arg;
		} else if (strcmp(arg, "--list") == 0) {
			for (size_t n = 0; n < lfs::implCount(); ++n) {
				auto &im = lfs::impl(n);
				printf("%s:", im.name);
				for (uint32_t f = 0; f < uint32_t(lfs::Func::Count); ++f) {
					if (im.has(lfs::Func(f))) {
						printf(" %s", lfs::funcName(lfs::Func(f)));
					}
				}
				printf("\n");
			}
			return 0;
		} else if (strcmp(arg, "--full") == 0) {
			opts.full = true;
		} else if (strcmp(arg, "--impl") == 0) {
			opts.impl = value();
		} else if (strcmp(arg, "--func") == 0) {
			opts.func = value();
		} else if (strcmp(arg, "--reps") == 0) {
			auto v = value();
			opts.reps = v ? unsigned(atoi(v)) : opts.reps;
		} else if (strcmp(arg, "--warmup") == 0) {
			auto v = value();
			opts.warmup = v ? unsigned(atoi(v)) : opts.warmup;
		} else if (strcmp(arg, "--max-size") == 0) {
			auto v = value();
			opts.maxSize = v ? size_t(atoll(v)) : opts.maxSize;
		} else {
			printUsage();
			return 1;
		}
	}
	if (opts.reps == 0) {
		opts.reps = 1;
	}
	if (strcmp(mode, "bench") == 0) {
		return lfs::runBench(opts);
	}
	return lfs::runConformance(opts);
}
