# Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including limitation the rights
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

# NuttX is hosted-POSIX-on-its-own-libc (the kernel exposes services through its
# libc/syscall stubs; the runtime does NOT build a second libc). The final image
# is flat: one statically linked ELF that includes the kernel, no shared objects.
# So the OS preset is a hosted shape (real libc, pthread) but without any of the
# shared-library / dynamic-symbol / ASAN machinery of linux.mk.

OSTYPE_IS_NUTTX := 1

# Flat build: everything is static. The final executable is produced by the
# NuttX image link (it owns the linker script and startup objects), so the
# Xenolith build only ever emits object files and static archives.
OSTYPE_EXEC_SUFFIX :=
OSTYPE_DSO_SUFFIX := .so     # unused (no shared libs in flat build); kept to satisfy callers
OSTYPE_LIB_SUFFIX := .a
OSTYPE_LIB_PREFIX := lib

OSTYPE_CONFIG_FLAGS := NUTTX

OSTYPE_GENERAL_CFLAGS := -Wall -fvisibility=hidden -ffunction-sections -fdata-sections
OSTYPE_LIB_CFLAGS :=
OSTYPE_EXEC_CFLAGS :=

# NuttX libc provides pthread / errno / stdio; exceptions are enabled (libc++abi
# is built with EH on). -frtti keeps dynamic_cast working.
OSTYPE_GENERAL_CXXFLAGS := -Wall -Wno-vla-cxx-extension -Wno-overloaded-virtual \
	-frtti -fexceptions -fvisibility=hidden -fvisibility-inlines-hidden \
	-ffunction-sections -fdata-sections

# NuttX flat-build arm64 linker scripts have no .tbss/.tdata sections, so the
# link fails with "STT_TLS symbol but no PT_TLS segment" when libsprt.a uses
# `thread_local`. -femulated-tls routes thread_local through __emutls_* calls
# (which live in the regular .bss/.data) instead of ELF TLS sections, sidestepping
# the missing PT_TLS without changes to the NuttX linker scripts.
OSTYPE_GENERAL_CFLAGS += -femulated-tls
OSTYPE_GENERAL_CXXFLAGS += -femulated-tls
OSTYPE_LIB_CXXFLAGS :=
OSTYPE_EXEC_CXXFLAGS :=

# simde (SIMD-everywhere) headers live in the sysroot usr/include; the geom SIMD
# headers pull <simde/x86/*.h>. -idirafter puts the NuttX libc at the lowest
# search priority so the sprt libc++ wrappers in include_libc/cxx/ (which
# intercept <stddef.h>, <ctype.h>, ... and re-export the libc symbols under
# std::) resolve first. -isystem here breaks libc++ by putting the raw libc
# ahead of the wrappers.
OSTYPE_GENERAL_CFLAGS += -idirafter $(TARGET_INCLUDE_DIR_LIBC)
OSTYPE_GENERAL_CXXFLAGS += -idirafter $(TARGET_INCLUDE_DIR_LIBC)

ifeq ($(RELEASE),1)
OSTYPE_GENERAL_CFLAGS += -O2
OSTYPE_GENERAL_CXXFLAGS += -O2
OSTYPE_LDFLAGS :=
else
OSTYPE_GENERAL_CFLAGS += -g -O0
OSTYPE_GENERAL_CXXFLAGS += -g -O0
OSTYPE_LDFLAGS := -g
endif

# Static archives feed the NuttX image link. lld is the host's linker.
# Clang's aarch64-none-elf driver injects crt0.o (a relative name, not found
# via -L), -lunwind, and compiler-rt from -resource-dir. NuttX owns startup
# and the kernel ELF (two-stage: this build emits objects; NuttX make links
# the image), so drop those defaults. -Wl,-r makes a relocatable object so
# kernel symbols can stay undefined. --gc-sections is incompatible with -r.
# --no-dependent-libraries drops the .deplibs (pthread) note that lld would
# otherwise try to resolve as -lpthread on the NuttX image link.
OSTYPE_GENERAL_LDFLAGS := $(OSTYPE_LDFLAGS) -fuse-ld=lld -L$(TARGET_LIB_DIR) -L$(TARGET_LIB_DIR_LIBC)
OSTYPE_EXEC_LDFLAGS := -nostdlib -Wl,-r -Wl,--no-dependent-libraries
OSTYPE_LIB_LDFLAGS :=

NUTTX := 1
