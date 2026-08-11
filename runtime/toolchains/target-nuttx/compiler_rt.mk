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
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

# Build libclang_rt.builtins-<arch>.a for the NuttX target.
#
# NuttX's flat build links no compiler-rt of its own, and clang emits calls to
# the outline-atomics helpers (__aarch64_cas4_acq_rel, __aarch64_ldadd4_*, ...),
# the float helpers, and so on. The runtime has to bring them. compiler-rt's
# aarch64 sme-abi.S also leaves __aarch64_sme_accessible undefined — SME is
# irrelevant on NuttX/baremetal, so we ship a stub that returns 0 next to the
# builtins archive.
#
# This is the M2 minimum: compiler-rt builtins + sme_stub. libunwind /
# libc++abi / libc++ come in a later M2 step against the sprt STL (M3), once
# __sprt_def.h has the __NuttX__ branch.
#
# The build is cached per-clang-version under /tmp so a repeated target build
# does not redo it. Override NUTTX_BUILTINS_CACHE_DIR to relocate.

.DEFAULT_GOAL := all

include ../common/utils/detect-platform.mk
include ../common/utils/init-shell.mk

# sysroot host symlink is laid down by init-target.mk: <out>/host -> hosts/$(HOST_ID).
HOST_CLANG := $(abspath $(TOOLCHAIN_OUTPUT_DIR)/host/bin/clang)
HOST_AR    := $(abspath $(TOOLCHAIN_OUTPUT_DIR)/host/bin/llvm-ar)
LLVM_SRC  ?= /build/toolchain-src/llvm-project

# Where the builtins + sme_stub archives end up in the target sysroot.
# install-target.mk copies them via T_INTERMEDIATE/usr/lib/*.a; placing them
# under usr/lib puts them in the link path the app target.mk exposes through
# TARGET_LIB_DIR.
TARGET_LIB_DIR := $(TOOLCHAIN_OUTPUT_DIR)/sysroot/usr/lib

# Cache keyed by the clang version string so an SDK bump rebuilds, but a
# repeat build does not.
BUILTINS_CACHE_KEY := $(shell $(HOST_CLANG) --version 2>/dev/null | head -1 | md5sum 2>/dev/null | cut -d' ' -f1 || echo uncached)
NUTTX_BUILTINS_CACHE_DIR ?= /tmp/nuttx-compiler-rt-builtins-$(SP_ARCH)-$(BUILTINS_CACHE_KEY)
BUILTINS_LIB := $(NUTTX_BUILTINS_CACHE_DIR)/libclang_rt.builtins-$(SP_ARCH).a
SME_STUB_LIB := $(NUTTX_BUILTINS_CACHE_DIR)/libsme_stub.a

# aarch64 builds for armv8-a; riscv64 / arm future profiles override SP_RES_MARCH.
NUTTX_COMPILER_RT_MARCH ?= armv8-a

all:
	@mkdir -p $(TARGET_LIB_DIR)
	@# Build + cache if not already present for this clang version.
	if [ ! -f "$(BUILTINS_LIB)" ] || [ ! -f "$(SME_STUB_LIB)" ]; then \
		echo "Building compiler-rt builtins for $(SP_ARCH_TARGET_CLANG) (cached at $(NUTTX_BUILTINS_CACHE_DIR))"; \
		[ -d "$(LLVM_SRC)/compiler-rt" ] || { echo "error: LLVM_SRC/compiler-rt not found ($(LLVM_SRC))." >&2; exit 1; }; \
		mkdir -p "$(NUTTX_BUILTINS_CACHE_DIR)/build"; \
		cmake -G "Ninja" -S "$(LLVM_SRC)/compiler-rt" -B "$(NUTTX_BUILTINS_CACHE_DIR)/build" \
			-DCMAKE_C_COMPILER="$(HOST_CLANG)" \
			-DCMAKE_CXX_COMPILER="$(patsubst %clang,%clang++,$(HOST_CLANG))" \
			-DCMAKE_AR="$(HOST_AR)" \
			-DCMAKE_RANLIB="$(patsubst %llvm-ar,%llvm-ranlib,$(HOST_AR))" \
			-DCMAKE_ASM_COMPILER="$(HOST_CLANG)" \
			-DCMAKE_C_COMPILER_TARGET=$(SP_ARCH_TARGET_CLANG) \
			-DCMAKE_CXX_COMPILER_TARGET=$(SP_ARCH_TARGET_CLANG) \
			-DCMAKE_ASM_COMPILER_TARGET=$(SP_ARCH_TARGET_CLANG) \
			-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
			-DCOMPILER_RT_BAREMETAL_BUILD=ON \
			-DCOMPILER_RT_BUILD_BUILTINS=ON \
			-DCOMPILER_RT_BUILD_MEMPROF=OFF \
			-DCOMPILER_RT_BUILD_PROFILE=OFF \
			-DCOMPILER_RT_BUILD_SANITIZERS=OFF \
			-DCOMPILER_RT_BUILD_XRAY=OFF \
			-DCOMPILER_RT_BUILD_ORC=OFF \
			-DCOMPILER_RT_BUILD_GWP_ASAN=OFF \
			-DCOMPILER_RT_BUILD_CRT=OFF \
			-DCOMPILER_RT_DEFAULT_TARGET_ONLY=ON \
			>/dev/null; \
		ninja -C "$(NUTTX_BUILTINS_CACHE_DIR)/build" builtins >/dev/null; \
		cp "$(NUTTX_BUILTINS_CACHE_DIR)/build/lib/linux/libclang_rt.builtins-$(SP_ARCH).a" "$(BUILTINS_LIB)"; \
		rm -rf "$(NUTTX_BUILTINS_CACHE_DIR)/build"; \
		\
		printf '%s\n' \
			'/* SME is not supported on NuttX/baremetal. compiler-rt sme-abi.S' \
			'   references this helper; return 0 so the sme-abi paths bail out. */' \
			'int __aarch64_sme_accessible(void) { return 0; }' \
			> "$(NUTTX_BUILTINS_CACHE_DIR)/sme_stub.c"; \
		"$(HOST_CLANG)" --target=$(SP_ARCH_TARGET_CLANG) -march=$(NUTTX_COMPILER_RT_MARCH) \
			-c "$(NUTTX_BUILTINS_CACHE_DIR)/sme_stub.c" \
			-o "$(NUTTX_BUILTINS_CACHE_DIR)/sme_stub.o"; \
		"$(HOST_AR)" rcs "$(SME_STUB_LIB)" "$(NUTTX_BUILTINS_CACHE_DIR)/sme_stub.o"; \
		echo "ok: builtins cached at $(BUILTINS_LIB)"; \
	fi
	@# Drop both archives into the target sysroot so the link picks them up.
	cp -af "$(BUILTINS_LIB)" "$(TARGET_LIB_DIR)/"
	cp -af "$(SME_STUB_LIB)" "$(TARGET_LIB_DIR)/"

.PHONY: all
