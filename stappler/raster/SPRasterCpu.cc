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

#include "SPRasterKernel.h"

// What this CPU can execute.
//
// Only x86 asks the question at runtime, and that is not a shortcut - it is the shape of the
// problem. On AArch64 the NEON base is architectural, so a probe could only ever answer "yes";
// beyond it (dotprod, i8mm, SVE) the answer lives in the ELF auxiliary vector, and sprt exposes no
// getauxval at all. On riscv, loongarch and wasm there is likewise nothing here to ask with, so
// those targets take the SWAR path, which needs no permission from anyone.

#if SP_RASTER_X86
#include <cpuid.h>
#endif

namespace STAPPLER_VERSIONIZED stappler::raster {

#if SP_RASTER_X86

// XCR0, which says whether the OS actually saves the register state an instruction set needs.
// Skipping this is the classic way to earn a SIGILL on a kernel or a sandbox that has AVX
// disabled: CPUID says the silicon has YMM, and only XCR0 says anyone is preserving it.
static inline uint64_t Cpu_xgetbv0() {
	uint32_t lo = 0, hi = 0;
	asm volatile("xgetbv" : "=a"(lo), "=d"(hi) : "c"(0));
	return (uint64_t(hi) << 32) | lo;
}

// __get_cpuid rather than __builtin_cpu_supports: the builtin pulls __cpu_model and
// __cpu_indicator_init out of compiler-rt, which is present for the linux targets in this tree but
// was not found for x86_64-pc-windows-msvc. <cpuid.h> is inline asm and depends on nothing.
struct X86Features {
	bool sse2 = false;
	bool sse41 = false;
	bool avx2 = false;
};

static X86Features Cpu_detectX86() {
	X86Features out;

	uint32_t eax = 0, ebx = 0, ecx = 0, edx = 0;
	if (!__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
		return out;
	}

	out.sse2 = (edx & (1u << 26)) != 0;
	out.sse41 = (ecx & (1u << 19)) != 0;

	const bool osxsave = (ecx & (1u << 27)) != 0;
	const bool avx = (ecx & (1u << 28)) != 0;

	if (!osxsave || !avx) {
		return out;
	}

	// bit 1 = XMM state, bit 2 = YMM state; AVX2 needs both saved across a context switch.
	if ((Cpu_xgetbv0() & 0x6) != 0x6) {
		return out;
	}

	uint32_t maxLeaf = __get_cpuid_max(0, nullptr);
	if (maxLeaf < 7) {
		return out;
	}

	if (__get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx)) {
		out.avx2 = (ebx & (1u << 5)) != 0;
	}

	return out;
}

static const X86Features &Cpu_x86() {
	static const X86Features s_features = Cpu_detectX86();
	return s_features;
}

bool cpuHasSse2() { return Cpu_x86().sse2; }
bool cpuHasSse41() { return Cpu_x86().sse41; }
bool cpuHasAvx2() { return Cpu_x86().avx2; }

#else

bool cpuHasSse2() { return false; }
bool cpuHasSse41() { return false; }
bool cpuHasAvx2() { return false; }

#endif

} // namespace stappler::raster
