// AArch64 Windows stack-probe helper, modeled on LLVM compiler-rt
// lib/builtins/aarch64/chkstk.S.
//
// clang emits, for frames larger than a page:
//      mov  x15, #(frame_size / 16)
//      bl   __chkstk
//      sub  sp, sp, x15, lsl #4
// __chkstk touches every guard page in [sp - x15*16, sp); it clobbers x16/x17,
// modifies no memory and does not move sp.
//
// _alloca_probe is the same routine under its older MSVC name, kept as an alias for
// the same reason as on x86_64: object code produced elsewhere still references it by
// plain name on every MSVC target (LLVM's DynamicLibrary explicit_symbols.inc, for one).
.globl __chkstk
.globl _alloca_probe
_alloca_probe:
__chkstk:
	lsl  x16, x15, #4
	mov  x17, sp
1:
	sub  x17, x17, #4096
	subs x16, x16, #4096
	ldr  xzr, [x17]
	b.gt 1b
	ret
