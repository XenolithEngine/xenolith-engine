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

# Tau kernel side (EL1). Freestanding C++ on the sprt header subset only: no
# libc, no STL, no runtime library. The kernel links nothing but its own objects
# and the compiler-rt builtins; EL0 modules use the TauUser preset instead.

OSTYPE_IS_TAU := 1

OSTYPE_EXEC_SUFFIX := .elf
OSTYPE_DSO_SUFFIX := .so
OSTYPE_LIB_SUFFIX := .a
OSTYPE_LIB_PREFIX := lib

OSTYPE_CONFIG_FLAGS := TAU

# -ffreestanding keeps __SPRT_USE_STL at 0, so sprt headers never reach for an
# STL; the headers are used as <sprt/cxx/...> in namespace sprt.
OSTYPE_TAU_COMMON := -ffreestanding -fno-pic -ffunction-sections -fdata-sections \
	-isystem $(GLOBAL_ROOT)/runtime/include

OSTYPE_GENERAL_CFLAGS := -Wall $(OSTYPE_TAU_COMMON)
OSTYPE_LIB_CFLAGS :=
OSTYPE_EXEC_CFLAGS :=

OSTYPE_GENERAL_CXXFLAGS := -Wall -nostdinc++ -fno-exceptions -fno-rtti \
	-fno-threadsafe-statics $(OSTYPE_TAU_COMMON)
OSTYPE_LIB_CXXFLAGS :=
OSTYPE_EXEC_CXXFLAGS :=

ifeq ($(RELEASE),1)
OSTYPE_GENERAL_CFLAGS += -O2 -DNDEBUG
OSTYPE_GENERAL_CXXFLAGS += -O2 -DNDEBUG
OSTYPE_LDFLAGS :=
else
OSTYPE_GENERAL_CFLAGS += -g -O0
OSTYPE_GENERAL_CXXFLAGS += -g -O0
OSTYPE_LDFLAGS := -g
endif

# The image layout belongs to the project: it passes its own -T script and entry.
OSTYPE_GENERAL_LDFLAGS := $(OSTYPE_LDFLAGS) -fuse-ld=lld
OSTYPE_EXEC_LDFLAGS := -nostdlib -static \
	-Wl,--no-dynamic-linker \
	-Wl,--gc-sections \
	-Wl,-z,max-page-size=4096 \
	$(TARGET_SYSROOT)/lib/clang/lib/tau/libclang_rt.builtins-$(TARGET_ARCH).a
OSTYPE_LIB_LDFLAGS :=

TAU := 1
