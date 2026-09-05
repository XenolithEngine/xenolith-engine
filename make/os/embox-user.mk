# Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

# Embox user mode (EL0). Freestanding: the application is a real process on its
# own libc (runtime/libc_impl), reaching the kernel only through the syscall
# boundary. Nothing of Embox is on the include path.
#
# The contrast with embox.mk is the whole point of the target, so it is worth
# stating line by line:
#
#   embox.mk                          this file
#   ------------------------------    ------------------------------------------
#   hosted on Embox's libc            freestanding on ours (-nostdinc)
#   emits a relocatable (-Wl,-r)      emits a linked static executable
#   Embox owns the entry point        _start is ours (libc_impl embox_user)
#   -femulated-tls (no PT_TLS in      native TLS: the link below emits PT_TLS and
#     Embox's linker scripts)           startup.cc sets TPIDR_EL0 from it
#   image address is the kernel's     fixed base 0x0000_4000_0000_0000 (D3)
#
# See xenolith-os docs/EMBOX-USERSPACE.md, contour B.

OSTYPE_IS_EMBOX_USER := 1

# A real ELF executable, not an object handed to someone else's link. Static
# archives for libraries; there are no shared objects (D5: no ld.so).
OSTYPE_EXEC_SUFFIX := .elf
OSTYPE_DSO_SUFFIX := .so     # unused; kept so callers that ask have an answer
OSTYPE_LIB_SUFFIX := .a
OSTYPE_LIB_PREFIX := lib

OSTYPE_CONFIG_FLAGS := EMBOX_USER

# --- addresses -----------------------------------------------------------
#
# D3 / ABI doc section 2.2. The image sits at a fixed base above every physical
# address the boards have, so the same link works on QEMU and on a Pi 4 with
# 8 GiB. The break follows the image 256 MiB up, which is the size budget for
# text+rodata+data+bss -- the check script asserts the link stayed inside it.
OSTYPE_EMBOX_USER_IMAGE_BASE := 0x0000400000000000
# The kernel maps with 4 KiB pages, so segments have to be 4 KiB aligned to be
# mappable with distinct permissions. lld defaults to 64 KiB for aarch64.
OSTYPE_EMBOX_USER_PAGE_SIZE := 4096

# --- compilation ---------------------------------------------------------
#
# NO -nostdinc and NO -ffreestanding here, and both omissions are deliberate --
# each was tried and each was wrong:
#
#   -nostdinc belongs in target.mk, the way it does for wasm and Windows. It is a
#   property of where this target's headers live, not of the OS preset, and the
#   preset is applied on top of target.mk rather than instead of it.
#
#   -ffreestanding must NOT be applied to the whole build. It sets
#   __STDC_HOSTED__ to 0, and a great deal of the runtime keys off that: with it
#   on, bits/sched_param.h switches struct sched_param to its unprefixed name
#   while pthread_attr.cc still writes the prefixed one, and runtime_core stops
#   compiling. The libc modules that DO want __STDC_HOSTED__ == 0 add the flag to
#   their own private flags (libc_impl/libc.mk, musl_libc.mk,
#   libc-wrapper.mk) -- exactly as they do for wasm, which is why no OS preset in
#   this tree passes it.
OSTYPE_EMBOX_USER_COMMON := -fvisibility=hidden -ffunction-sections -fdata-sections

OSTYPE_GENERAL_CFLAGS := -Wall $(OSTYPE_EMBOX_USER_COMMON)
OSTYPE_LIB_CFLAGS :=
OSTYPE_EXEC_CFLAGS :=

# -frtti/-fexceptions match the hosted Embox target: the engine's C++ is written
# against them, and turning them off for this target alone would change its
# semantics rather than its packaging. What makes throw/catch actually work is
# libunwind finding PT_GNU_EH_FRAME, which the link below emits -- see contour L4.
OSTYPE_GENERAL_CXXFLAGS := -Wall -Wno-vla-cxx-extension -Wno-overloaded-virtual \
	-frtti -fexceptions -nostdinc++ \
	-fvisibility-inlines-hidden \
	$(OSTYPE_EMBOX_USER_COMMON)
OSTYPE_LIB_CXXFLAGS :=
OSTYPE_EXEC_CXXFLAGS :=

# NO -femulated-tls here, deliberately. embox.mk needs it because Embox's flat
# linker scripts carry no .tdata/.tbss and a thread_local would fail the image
# link. This target owns its link and emits PT_TLS, so thread_local is native ELF
# TLS through TPIDR_EL0 -- which is also what lets the runtime drop
# runtime/core/embox/emutls.cc for this target.

# simde (SIMD-everywhere) headers, needed by the geom SIMD headers. -idirafter
# keeps them at the lowest priority so nothing here can shadow the libc/STL
# trees that -nostdinc made authoritative.
OSTYPE_GENERAL_CFLAGS += -idirafter $(TARGET_SYSROOT)/usr/include
OSTYPE_GENERAL_CXXFLAGS += -idirafter $(TARGET_SYSROOT)/usr/include

ifeq ($(RELEASE),1)
OSTYPE_GENERAL_CFLAGS += -O2
OSTYPE_GENERAL_CXXFLAGS += -O2
OSTYPE_LDFLAGS :=
else
OSTYPE_GENERAL_CFLAGS += -g -O0
OSTYPE_GENERAL_CXXFLAGS += -g -O0
OSTYPE_LDFLAGS := -g
endif

# --- linking -------------------------------------------------------------
#
# NO CUSTOM LINKER SCRIPT, and that is a decision rather than an omission.
#
# The plan called for an app-aarch64.lds. It turns out lld's default layout
# already produces everything the ABI asks for once told the base and the page
# size: separate R / RX / RW PT_LOADs, PT_TLS from .tdata/.tbss,
# PT_GNU_EH_FRAME from .eh_frame_hdr, and the __init_array_start/end +
# __preinit_array_start/end brackets that startup.cc walks. A hand-written script
# would have to reproduce all of that -- including RELRO and where .eh_frame_hdr
# goes -- and would then drift from lld's own idea of the layout. Verified
# segment by segment in xenolith-os scripts/check-target-embox-user.sh.
#
# The one thing flags cannot express is a symbol whose value is a section
# address, so share/app-<arch>.lds adds __eh_frame_start/__eh_frame_end and
# nothing else. Having no SECTIONS command, it leaves lld's layout intact --
# see the comment in that file.
#
# -nostdlib: clang's aarch64-none-elf driver would otherwise inject crt0.o (a
# relative name that no -L can find), -lunwind and -lc. Startup is ours.
# --no-dynamic-linker + -static: there is no ld.so and no PT_INTERP (D5).
# --eh-frame-hdr: without it there is no .eh_frame_hdr and hence no
# PT_GNU_EH_FRAME, and the unwinder has nothing to find.
# --gc-sections pairs with -ffunction-sections above; the image budget is 256 MiB
# and the engine plus its dependencies are not small.
OSTYPE_GENERAL_LDFLAGS := $(OSTYPE_LDFLAGS) -fuse-ld=lld -L$(TARGET_SYSROOT)/usr/lib
OSTYPE_EXEC_LDFLAGS := -nostdlib -static \
	-Wl,--no-dynamic-linker \
	-Wl,--eh-frame-hdr \
	-Wl,--image-base=$(OSTYPE_EMBOX_USER_IMAGE_BASE) \
	-Wl,-z,max-page-size=$(OSTYPE_EMBOX_USER_PAGE_SIZE) \
	-Wl,--gc-sections \
	-Wl,-e,_start \
	-T $(TARGET_SYSROOT)/share/app-$(TARGET_ARCH).lds
OSTYPE_LIB_LDFLAGS :=

EMBOX_USER := 1
