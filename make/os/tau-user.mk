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

# Tau user side (EL0). A module is a static ELF on the sprt runtime as its libc
# (runtime/libc_impl) with libc++ on top, exactly as EmboxUser is; it reaches the
# kernel only through libtau. Differences from embox-user.mk are the image base
# and the absence of any Embox headers.

OSTYPE_IS_TAU_USER := 1

OSTYPE_EXEC_SUFFIX := .elf
OSTYPE_DSO_SUFFIX := .so
OSTYPE_LIB_SUFFIX := .a
OSTYPE_LIB_PREFIX := lib

OSTYPE_CONFIG_FLAGS := TAU_USER

# Every module has its own address space; the root loader maps the image here
# and keeps the first 4 MiB unmapped as a null guard.
OSTYPE_TAU_USER_IMAGE_BASE := 0x0000000000400000
OSTYPE_TAU_USER_PAGE_SIZE := 4096

# No -ffreestanding: the runtime keys hosted paths off __STDC_HOSTED__ (see the
# note in embox-user.mk); the libc modules add it to their private flags.
OSTYPE_TAU_USER_COMMON := -fvisibility=hidden -ffunction-sections -fdata-sections

OSTYPE_GENERAL_CFLAGS := -Wall $(OSTYPE_TAU_USER_COMMON)
OSTYPE_LIB_CFLAGS :=
OSTYPE_EXEC_CFLAGS :=

OSTYPE_GENERAL_CXXFLAGS := -Wall -Wno-vla-cxx-extension -Wno-overloaded-virtual \
	-frtti -fexceptions -nostdinc++ \
	-fvisibility-inlines-hidden \
	$(OSTYPE_TAU_USER_COMMON)
OSTYPE_LIB_CXXFLAGS :=
OSTYPE_EXEC_CXXFLAGS :=

# Native ELF TLS through TPIDR_EL0; the link below emits PT_TLS. __TAU_USER__ tells the
# EL0 side from the kernel side, which also defines __TAU__.
OSTYPE_GENERAL_CFLAGS += -D__TAU_USER__ -idirafter $(TARGET_SYSROOT)/usr/include
OSTYPE_GENERAL_CXXFLAGS += -D__TAU_USER__ -idirafter $(TARGET_SYSROOT)/usr/include

ifeq ($(RELEASE),1)
OSTYPE_GENERAL_CFLAGS += -O2
OSTYPE_GENERAL_CXXFLAGS += -O2
OSTYPE_LDFLAGS :=
else
OSTYPE_GENERAL_CFLAGS += -g -O0
OSTYPE_GENERAL_CXXFLAGS += -g -O0
OSTYPE_LDFLAGS := -g
endif

# lld's default layout plus share/app-<arch>.lds, which only adds the
# __eh_frame_start/__eh_frame_end symbols the unwinder needs.
OSTYPE_GENERAL_LDFLAGS := $(OSTYPE_LDFLAGS) -fuse-ld=lld -L$(TARGET_SYSROOT)/usr/lib
OSTYPE_EXEC_LDFLAGS := -nostdlib -static \
	-Wl,--no-dynamic-linker \
	-Wl,--eh-frame-hdr \
	-Wl,--image-base=$(OSTYPE_TAU_USER_IMAGE_BASE) \
	-Wl,-z,max-page-size=$(OSTYPE_TAU_USER_PAGE_SIZE) \
	-Wl,--gc-sections \
	-Wl,-e,_start \
	-T $(TARGET_SYSROOT)/share/app-$(TARGET_ARCH).lds
OSTYPE_LIB_LDFLAGS :=

TAU_USER := 1
