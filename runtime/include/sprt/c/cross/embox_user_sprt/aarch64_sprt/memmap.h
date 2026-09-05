// The user address-space layout of the Embox EL0 target.
//
// Decision D3 / ABI doc section 2.2. Fixed for every board: the image sits above
// every physical address a supported board has (the 8 GiB Pi 4 reaches
// 0x2_0000_0000, and the kernel identity-maps RAM), so one link runs everywhere.
//
// Not pulled in by any cross/__sprt_*.h dispatcher -- these are not libc values,
// they are the shape of the address space, and only the few places that reason
// about it include this file. The kernel's copy is xenolith-os
// board/embox-qemu/drivers/xlsyscall/xl_abi.h; the two are pinned against each
// other by xenolith-os scripts/check-abi.py, because a disagreement here is not
// a build error -- it is a mapping that lands somewhere else than the other side
// expects.

#ifndef CORE_RUNTIME_INCLUDE_C_CROSS_EMBOX_USER_SPRT_AARCH64_MEMMAP_H_
#define CORE_RUNTIME_INCLUDE_C_CROSS_EMBOX_USER_SPRT_AARCH64_MEMMAP_H_

#include <sprt/c/bits/__sprt_def.h>

#if !SPRT_EMBOX_USER
#error "memmap.h describes the Embox EL0 address space; it means nothing elsewhere"
#endif

// clang-format off

// .text/.rodata/.data/.bss, and the break growing up from just above them.
#define __SPRT_EL0_IMAGE_BASE 0x0000400000000000UL

// The break starts 256 MiB above the image base, which is therefore also the
// size budget for the image itself.
#define __SPRT_EL0_BRK_BASE   0x0000400010000000UL
#define __SPRT_EL0_BRK_MAX    0x0000000010000000UL

// The mmap arena (anonymous memory, and /dev/fb0 once K5 can map a device),
// handed out upward by a bump pointer.
#define __SPRT_EL0_MMAP_BASE  0x0000500000000000UL

// The main thread's stack: 256 MiB of window below the top, growing down.
// Reported by __initNativeHandle so pthread_getattr_np answers truthfully.
#define __SPRT_EL0_STACK_TOP  0x0000600000000000UL
#define __SPRT_EL0_STACK_BASE (__SPRT_EL0_STACK_TOP - 0x0000000010000000UL)

// clang-format on

#endif // CORE_RUNTIME_INCLUDE_C_CROSS_EMBOX_USER_SPRT_AARCH64_MEMMAP_H_
