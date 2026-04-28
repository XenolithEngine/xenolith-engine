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

#ifndef __SPRT_BUILD
#define __SPRT_BUILD
#endif

#include <sprt/cxx/detail/ctypes.h>
#include <sprt/c/__sprt_string.h>
#include <sprt/wrappers/windows/context_api.h>
#include <sprt/c/__sprt_setjmp.h>

#if __SPRT_ARCH_ID == __SPRT_ARCH_ID_X86_64
typedef struct SPRT_ALIGNAS(16) _jmp_CONTEXT {
	/*DWORD64 P1Home;
	DWORD64 P2Home;
	DWORD64 P3Home;
	DWORD64 P4Home;
	DWORD64 P5Home;
	DWORD64 P6Home;*/

	DWORD ContextFlags; // 4
	DWORD MxCsr; // 8

	WORD SegCs; // 10
	WORD SegDs; // 12
	WORD SegEs; // 14
	WORD SegFs; // 16
	WORD SegGs; // 18
	WORD SegSs; // 20
	DWORD EFlags; // 24

	/*DWORD64 Dr0;
	DWORD64 Dr1;
	DWORD64 Dr2;
	DWORD64 Dr3;
	DWORD64 Dr6;
	DWORD64 Dr7;*/
	DWORD64 Rax;
	DWORD64 Rcx;
	DWORD64 Rdx;
	DWORD64 Rbx;
	DWORD64 Rsp;
	DWORD64 Rbp;
	DWORD64 Rsi;
	DWORD64 Rdi;
	DWORD64 R8;
	DWORD64 R9;
	DWORD64 R10;
	DWORD64 R11;
	DWORD64 R12;
	DWORD64 R13;
	DWORD64 R14;
	DWORD64 R15;
	DWORD64 Rip;
} jmp_CONTEXT;

#elif __SPRT_ARCH_ID == __SPRT_ARCH_ID_AARCH64

typedef struct SPRT_ALIGNAS(8) _jmp_CONTEXT {

	//
	// Control flags.
	//

	DWORD ContextFlags;

	//
	// Integer registers
	//

	DWORD R0;
	DWORD R1;
	DWORD R2;
	DWORD R3;
	DWORD R4;
	DWORD R5;
	DWORD R6;
	DWORD R7;
	DWORD R8;
	DWORD R9;
	DWORD R10;
	DWORD R11;
	DWORD R12;

	//
	// Control Registers
	//

	DWORD Sp;
	DWORD Lr;
	DWORD Pc;
	DWORD Cpsr;

	//
	// Floating Point/NEON Registers
	//

	DWORD Fpscr;
	DWORD Padding;
	union {
		ULONGLONG D[32];
		DWORD S[32];
	};
} jmp_CONTEXT;

#else
#error Not implemented
#endif

struct __jmp_buf {
	jmp_CONTEXT ctx;
	ULONG64 ControlPc;
};

typedef __jmp_buf *jmp_buf;

namespace sprt {

#if __SPRT_ARCH_ID == __SPRT_ARCH_ID_X86_64

static void __writeJmpBuf(jmp_buf env, const CONTEXT *ctx) {
	__sprt_memcpy(&env->ctx.ContextFlags, &ctx->ContextFlags, 24);
	__sprt_memcpy(&env->ctx.Rax, &ctx->Rax,
			((&env->ctx.Rip + 1) - &env->ctx.Rax) * sizeof(DWORD64));
}

static void __writeContext(CONTEXT *ctx, const jmp_buf env) {
	__sprt_memcpy(&ctx->ContextFlags, &env->ctx.ContextFlags, 24);
	__sprt_memcpy(&ctx->Rax, &env->ctx.Rax,
			((&env->ctx.Rip + 1) - &env->ctx.Rax) * sizeof(DWORD64));
}

#elif __SPRT_ARCH_ID == __SPRT_ARCH_ID_AARCH64

static void __writeJmpBuf(jmp_buf env, const CONTEXT *ctx) {
	__sprt_memcpy(&env->ctx, ctx, sizeof(jmp_CONTEXT));
}

static void __writeContext(CONTEXT *ctx, const jmp_buf env) {
	__sprt_memcpy(ctx, &env->ctx, sizeof(jmp_CONTEXT));
}
#else
#error Not implemented
#endif

static_assert(sizeof(__jmp_buf) == sizeof(__sprt_native_jmp_buf));

int setjmp(__sprt_jmp_buf env) {
	env->__result = 0;

	ULONG64 ControlPc = 0;
	ULONG64 EstablisherFrame = 0;
	ULONG64 ImageBase;
	PRUNTIME_FUNCTION FunctionEntry;
	PVOID HandlerData;

	env->__result = 0;

	CONTEXT context;
	RtlCaptureContext(&context);

	if (env->__result == 0) {
		__writeJmpBuf(reinterpret_cast<jmp_buf>(&env->__native), &context);

#if __SPRT_ARCH_ID == __SPRT_ARCH_ID_X86_64
		ControlPc = context.Rip;
#elif __SPRT_ARCH_ID == __SPRT_ARCH_ID_AARCH64
		ControlPc = context.Pc;
#else
#error Not implemented
#endif

		reinterpret_cast<jmp_buf>(&env->__native)->ControlPc = ControlPc;

		FunctionEntry = RtlLookupFunctionEntry(ControlPc, &ImageBase, nullptr);

		if (FunctionEntry != nullptr) {
			RtlVirtualUnwind(UNW_FLAG_NHANDLER, ImageBase, ControlPc, FunctionEntry, &context,
					&HandlerData, &EstablisherFrame, nullptr);
			env->__cfa = EstablisherFrame;
		}
	}

	return env->__result;
}

[[noreturn]]
void longjmp(__sprt_jmp_buf env, int value) {
	env->__result = value;

	CONTEXT context;
	RtlCaptureContext(&context);

	if (env->__cfa && reinterpret_cast<jmp_buf>(&env->__native)->ControlPc) {
		RtlUnwindEx((PVOID)env->__cfa, (PVOID) reinterpret_cast<jmp_buf>(&env->__native)->ControlPc,
				nullptr, (PVOID)intptr_t(value), &context, nullptr);
	}

	__writeContext(&context, reinterpret_cast<jmp_buf>(&env->__native));

	RtlRestoreContext(&context, nullptr);

	// This code is never reached
	__builtin_unreachable();
}

} // namespace sprt
