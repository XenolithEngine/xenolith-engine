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

#ifndef TESTS_LIBC_FREESTANDING_SRC_LFS_H_
#define TESTS_LIBC_FREESTANDING_SRC_LFS_H_

#include <stddef.h>
#include <stdint.h>

namespace lfs {

enum class Func : uint32_t {
	Memcpy,
	Memmove,
	Memset,
	Memcmp,
	Bcmp,
	Memchr,
	Memrchr,
	Strlen,
	Strnlen,
	Strcmp,
	Strncmp,
	Strchr,
	Strrchr,
	Count
};

const char *funcName(Func);

// One implementation of the set; a null entry means it does not provide that function.
struct Impl {
	const char *name;
	void *(*fMemcpy)(void *, const void *, size_t);
	void *(*fMemmove)(void *, const void *, size_t);
	void *(*fMemset)(void *, int, size_t);
	int (*fMemcmp)(const void *, const void *, size_t);
	int (*fBcmp)(const void *, const void *, size_t);
	void *(*fMemchr)(const void *, int, size_t);
	void *(*fMemrchr)(const void *, int, size_t);
	size_t (*fStrlen)(const char *);
	size_t (*fStrnlen)(const char *, size_t);
	int (*fStrcmp)(const char *, const char *);
	int (*fStrncmp)(const char *, const char *, size_t);
	char *(*fStrchr)(const char *, int);
	char *(*fStrrchr)(const char *, int);

	bool has(Func) const;
};

// The byte-loop reference every other implementation is held to.
const Impl &refImpl();

// ref, fs_c, [fs_gpr, fs_sse2, fs_avx2, fs_simd, fs_sve,] musl_c, [musl_asm,] host; the
// forced variants only where the CPU has what they force.
size_t implCount();
const Impl &impl(size_t);
const Impl *findImpl(const char *name);
bool findFunc(const char *name, Func &);

struct Options {
	const char *impl = nullptr; // nullptr: all
	const char *func = nullptr;
	bool full = false;
	unsigned reps = 15;
	unsigned warmup = 3;
	size_t maxSize = 8 << 20;
};

int runConformance(const Options &);

// The <fenv.h> part of the conformance run: --func fenv, or every run without --func.
bool runFenvConformance(const Options &, uint64_t &checks, uint64_t &failures);
int runBench(const Options &);

} // namespace lfs

#endif // TESTS_LIBC_FREESTANDING_SRC_LFS_H_
