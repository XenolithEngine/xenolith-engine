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

#include "lfs.h"

#include <dlfcn.h>
#include <string.h>

#if defined(__x86_64__)
#include <cpuid.h>
#elif defined(__aarch64__)
extern "C" unsigned long getauxval(unsigned long);
#endif

// Declares the 13 functions of one implementation under a name prefix.
#define LFS_DECLARE_SET(P) \
	void *P##memcpy(void *, const void *, size_t); \
	void *P##memmove(void *, const void *, size_t); \
	void *P##memset(void *, int, size_t); \
	int P##memcmp(const void *, const void *, size_t); \
	int P##bcmp(const void *, const void *, size_t); \
	void *P##memchr(const void *, int, size_t); \
	void *P##memrchr(const void *, int, size_t); \
	size_t P##strlen(const char *); \
	size_t P##strnlen(const char *, size_t); \
	int P##strcmp(const char *, const char *); \
	int P##strncmp(const char *, const char *, size_t); \
	char *P##strchr(const char *, int); \
	char *P##strrchr(const char *, int);

#define LFS_SET(name, P) \
	Impl{name, P##memcpy, P##memmove, P##memset, P##memcmp, P##bcmp, P##memchr, P##memrchr, \
		P##strlen, P##strnlen, P##strcmp, P##strncmp, P##strchr, P##strrchr}

extern "C" {
LFS_DECLARE_SET(__lfs_c_)
LFS_DECLARE_SET(__lfs_musl_)

// The functions runtime/libc_freestanding has in assembly, at its two levels.
#define LFS_DECLARE_ASM(P) \
	void *P##memcpy(void *, const void *, size_t); \
	void *P##memmove(void *, const void *, size_t); \
	void *P##memset(void *, int, size_t); \
	int P##memcmp(const void *, const void *, size_t); \
	int P##bcmp(const void *, const void *, size_t); \
	void *P##memchr(const void *, int, size_t); \
	size_t P##strlen(const char *); \
	int P##strcmp(const char *, const char *);

#if defined(__ELF__) \
		&& (defined(__x86_64__) || defined(__aarch64__) || defined(__riscv) || defined(__loongarch64))
#define LFS_HAVE_FS_ASM 1
LFS_DECLARE_ASM(__lfs_simd_)
LFS_DECLARE_ASM(__lfs_gpr_)
#if defined(__x86_64__)
LFS_DECLARE_ASM(__lfs_sse2_)
LFS_DECLARE_ASM(__lfs_avx2_)
#elif defined(__aarch64__)
LFS_DECLARE_ASM(__lfs_sve_)
#elif defined(__riscv)
LFS_DECLARE_ASM(__lfs_zbb_)
LFS_DECLARE_ASM(__lfs_rvv_)
#endif
#endif

void *__lfs_musl_asm_memcpy(void *, const void *, size_t);
void *__lfs_musl_asm_memmove(void *, const void *, size_t);
void *__lfs_musl_asm_memset(void *, int, size_t);
}

namespace lfs {

namespace {

// The reference: the plainest loops, kept out of the optimizer's idiom recognition.
#define LFS_REF __attribute__((no_builtin, noinline))

LFS_REF void *refMemcpy(void *d, const void *s, size_t n) {
	auto dp = static_cast<unsigned char *>(d);
	auto sp = static_cast<const unsigned char *>(s);
	for (size_t i = 0; i < n; ++i) { dp[i] = sp[i]; }
	return d;
}

LFS_REF void *refMemmove(void *d, const void *s, size_t n) {
	auto dp = static_cast<unsigned char *>(d);
	auto sp = static_cast<const unsigned char *>(s);
	if (dp < sp) {
		for (size_t i = 0; i < n; ++i) { dp[i] = sp[i]; }
	} else {
		for (size_t i = n; i > 0; --i) { dp[i - 1] = sp[i - 1]; }
	}
	return d;
}

LFS_REF void *refMemset(void *d, int c, size_t n) {
	auto dp = static_cast<unsigned char *>(d);
	for (size_t i = 0; i < n; ++i) { dp[i] = (unsigned char)c; }
	return d;
}

LFS_REF int refMemcmp(const void *l, const void *r, size_t n) {
	auto a = static_cast<const unsigned char *>(l);
	auto b = static_cast<const unsigned char *>(r);
	for (size_t i = 0; i < n; ++i) {
		if (a[i] != b[i]) {
			return a[i] < b[i] ? -1 : 1;
		}
	}
	return 0;
}

LFS_REF void *refMemchr(const void *s, int c, size_t n) {
	auto p = static_cast<const unsigned char *>(s);
	for (size_t i = 0; i < n; ++i) {
		if (p[i] == (unsigned char)c) {
			return const_cast<unsigned char *>(p + i);
		}
	}
	return nullptr;
}

LFS_REF void *refMemrchr(const void *s, int c, size_t n) {
	auto p = static_cast<const unsigned char *>(s);
	for (size_t i = n; i > 0; --i) {
		if (p[i - 1] == (unsigned char)c) {
			return const_cast<unsigned char *>(p + i - 1);
		}
	}
	return nullptr;
}

LFS_REF size_t refStrlen(const char *s) {
	size_t n = 0;
	while (s[n]) { ++n; }
	return n;
}

LFS_REF size_t refStrnlen(const char *s, size_t max) {
	size_t n = 0;
	while (n < max && s[n]) { ++n; }
	return n;
}

LFS_REF int refStrncmp(const char *l, const char *r, size_t n) {
	for (size_t i = 0; i < n; ++i) {
		auto a = (unsigned char)l[i];
		auto b = (unsigned char)r[i];
		if (a != b) {
			return a < b ? -1 : 1;
		}
		if (!a) {
			return 0;
		}
	}
	return 0;
}

LFS_REF int refStrcmp(const char *l, const char *r) { return refStrncmp(l, r, SIZE_MAX); }

LFS_REF char *refStrchr(const char *s, int c) {
	for (size_t i = 0;; ++i) {
		if (s[i] == (char)c) {
			return const_cast<char *>(s + i);
		}
		if (!s[i]) {
			return nullptr;
		}
	}
}

LFS_REF char *refStrrchr(const char *s, int c) {
	const char *found = nullptr;
	for (size_t i = 0;; ++i) {
		if (s[i] == (char)c) {
			found = s + i;
		}
		if (!s[i]) {
			return const_cast<char *>(found);
		}
	}
}

const Impl s_ref{"ref", refMemcpy, refMemmove, refMemset, refMemcmp, refMemcmp, refMemchr,
	refMemrchr, refStrlen, refStrnlen, refStrcmp, refStrncmp, refStrchr, refStrrchr};

#if LFS_HAVE_FS_ASM
#define LFS_ASM_SET(name, P) \
	Impl{name, P##memcpy, P##memmove, P##memset, P##memcmp, P##bcmp, P##memchr, nullptr, \
		P##strlen, nullptr, P##strcmp, nullptr, nullptr, nullptr}
#endif

// The forced variants run only where the CPU has what they force.
bool hasAvx2() {
#if defined(__x86_64__)
	unsigned a, b, c, d;
	if (!__get_cpuid(1, &a, &b, &c, &d) || (c & 0x18000000) != 0x18000000) {
		return false;
	}
	unsigned lo, hi;
	__asm__("xgetbv" : "=a"(lo), "=d"(hi) : "c"(0));
	if ((lo & 6) != 6) {
		return false;
	}
	return __get_cpuid_count(7, 0, &a, &b, &c, &d) && (b & 0x20);
#else
	return false;
#endif
}

bool hasSve() {
#if defined(__aarch64__)
	return getauxval(16 /* AT_HWCAP */) & (1ul << 22) /* HWCAP_SVE */;
#else
	return false;
#endif
}

// riscv_hwprobe(RISCV_HWPROBE_KEY_IMA_EXT_0): bit 2 V, bit 4 Zbb; 0 when the kernel
// (or qemu) does not answer.
unsigned long riscvExtensions() {
#if defined(__riscv)
	struct {
		long key;
		unsigned long value;
	} pair{4, 0};
	register long a0 __asm__("a0") = long(&pair);
	register long a1 __asm__("a1") = 1;
	register long a2 __asm__("a2") = 0;
	register long a3 __asm__("a3") = 0;
	register long a4 __asm__("a4") = 0;
	register long a7 __asm__("a7") = 258;
	__asm__ volatile("ecall"
					 : "+r"(a0)
					 : "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a7)
					 : "memory");
	return a0 == 0 && pair.key == 4 ? pair.value : 0;
#else
	return 0;
#endif
}

// musl ships assembly only for these; the rest of the column stays null.
Impl makeMuslAsm() {
	Impl ret{"musl_asm"};
#if defined(__x86_64__)
	ret.fMemcpy = __lfs_musl_asm_memcpy;
	ret.fMemmove = __lfs_musl_asm_memmove;
	ret.fMemset = __lfs_musl_asm_memset;
#elif defined(__aarch64__)
	ret.fMemcpy = __lfs_musl_asm_memcpy;
	ret.fMemset = __lfs_musl_asm_memset;
#endif
	return ret;
}

// The platform libc, looked up by name so the sprt wrappers are not in the path.
template <typename T>
void hostSym(T &fn, const char *name) {
	fn = reinterpret_cast<T>(dlsym(RTLD_DEFAULT, name));
}

Impl makeHost() {
	Impl ret{"host"};
	hostSym(ret.fMemcpy, "memcpy");
	hostSym(ret.fMemmove, "memmove");
	hostSym(ret.fMemset, "memset");
	hostSym(ret.fMemcmp, "memcmp");
	hostSym(ret.fBcmp, "bcmp");
	hostSym(ret.fMemchr, "memchr");
	hostSym(ret.fMemrchr, "memrchr");
	hostSym(ret.fStrlen, "strlen");
	hostSym(ret.fStrnlen, "strnlen");
	hostSym(ret.fStrcmp, "strcmp");
	hostSym(ret.fStrncmp, "strncmp");
	hostSym(ret.fStrchr, "strchr");
	hostSym(ret.fStrrchr, "strrchr");
	return ret;
}

struct ImplList {
	Impl list[12];
	size_t count = 0;

	ImplList() {
		list[count++] = s_ref;
		list[count++] = LFS_SET("fs_c", __lfs_c_);
#if LFS_HAVE_FS_ASM
		list[count++] = LFS_ASM_SET("fs_gpr", __lfs_gpr_);
#if defined(__x86_64__)
		list[count++] = LFS_ASM_SET("fs_sse2", __lfs_sse2_);
		if (hasAvx2()) {
			list[count++] = LFS_ASM_SET("fs_avx2", __lfs_avx2_);
		}
#endif
#if defined(__riscv)
		if (riscvExtensions() & (1ul << 4)) {
			list[count++] = LFS_ASM_SET("fs_zbb", __lfs_zbb_);
		}
		if (riscvExtensions() & (1ul << 2)) {
			list[count++] = LFS_ASM_SET("fs_rvv", __lfs_rvv_);
		}
#endif
		list[count++] = LFS_ASM_SET("fs_simd", __lfs_simd_);
#if defined(__aarch64__)
		if (hasSve()) {
			list[count++] = LFS_ASM_SET("fs_sve", __lfs_sve_);
		}
#endif
#endif
		list[count++] = LFS_SET("musl_c", __lfs_musl_);
		auto muslAsm = makeMuslAsm();
		if (muslAsm.fMemcpy) {
			list[count++] = muslAsm;
		}
		list[count++] = makeHost();
	}
};

const ImplList &implList() {
	static ImplList s_list;
	return s_list;
}

const char *s_funcNames[] = {"memcpy", "memmove", "memset", "memcmp", "bcmp", "memchr", "memrchr",
	"strlen", "strnlen", "strcmp", "strncmp", "strchr", "strrchr"};

} // namespace

const char *funcName(Func f) { return s_funcNames[uint32_t(f)]; }

bool Impl::has(Func f) const {
	switch (f) {
	case Func::Memcpy: return fMemcpy;
	case Func::Memmove: return fMemmove;
	case Func::Memset: return fMemset;
	case Func::Memcmp: return fMemcmp;
	case Func::Bcmp: return fBcmp;
	case Func::Memchr: return fMemchr;
	case Func::Memrchr: return fMemrchr;
	case Func::Strlen: return fStrlen;
	case Func::Strnlen: return fStrnlen;
	case Func::Strcmp: return fStrcmp;
	case Func::Strncmp: return fStrncmp;
	case Func::Strchr: return fStrchr;
	case Func::Strrchr: return fStrrchr;
	case Func::Count: break;
	}
	return false;
}

const Impl &refImpl() { return s_ref; }

size_t implCount() { return implList().count; }

const Impl &impl(size_t i) { return implList().list[i]; }

const Impl *findImpl(const char *name) {
	for (size_t i = 0; i < implCount(); ++i) {
		if (strcmp(impl(i).name, name) == 0) {
			return &impl(i);
		}
	}
	return nullptr;
}

bool findFunc(const char *name, Func &out) {
	for (uint32_t i = 0; i < uint32_t(Func::Count); ++i) {
		if (strcmp(s_funcNames[i], name) == 0) {
			out = Func(i);
			return true;
		}
	}
	return false;
}

} // namespace lfs
