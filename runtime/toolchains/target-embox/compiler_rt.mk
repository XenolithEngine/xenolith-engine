# Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including the rights
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

# Build libclang_rt.builtins-<arch>.a for the Embox target.
#
# Embox's flat build links no compiler-rt of its own, and clang emits calls to
# the outline-atomics helpers (__aarch64_cas4_acq_rel, __aarch64_ldadd4_*, ...).
# compiler-rt's aarch64 sme-abi.S also leaves __aarch64_sme_accessible undefined
# — SME is irrelevant on Embox/baremetal, so we ship a stub that returns 0.
#
# Baremetal builtins omit emutls.c. Embox/aarch64 has no PT_TLS / tpidr_el0
# save-restore, so the engine compiles with -femulated-tls and this archive
# MUST contain __emutls_get_address (compiled against Embox pthread/malloc).
# Do not drop a /tmp/libemutls.a on the side — it has to live in this archive.
#
# CMAKE_SYSTEM_NAME=Generic is required on Darwin: the compiler-rt Darwin path
# produces empty multi-arch archives. Generic + COMPILER_RT_BAREMETAL_BUILD
# writes lib/baremetal/libclang_rt.builtins-<arch>.a (Linux hosts may still
# land it under lib/linux/ — we find the archive rather than hard-coding).

.DEFAULT_GOAL := all

include ../common/utils/detect-platform.mk
include ../common/utils/init-shell.mk

HOST_CLANG := $(abspath $(TOOLCHAIN_OUTPUT_DIR)/host/bin/clang)
HOST_AR    := $(abspath $(TOOLCHAIN_OUTPUT_DIR)/host/bin/llvm-ar)
LLVM_SRC  ?= $(if $(wildcard $(LIB_SRC_DIR)/llvm-project/compiler-rt),$(LIB_SRC_DIR)/llvm-project,/build/toolchain-src/llvm-project)

TARGET_LIB_DIR := $(TOOLCHAIN_OUTPUT_DIR)/sysroot/usr/lib
EMBOX_INCLUDE := $(TOOLCHAIN_OUTPUT_DIR)/sysroot/usr/include

BUILTINS_CACHE_KEY := $(shell $(HOST_CLANG) --version 2>/dev/null | head -1 | python3 -c 'import hashlib,sys; print(hashlib.md5(sys.stdin.buffer.read()).hexdigest()[:16])')
EMBOX_BUILTINS_CACHE_DIR ?= $(TOOLCHAIN_OUTPUT_DIR)/_compiler-rt-cache-$(SP_ARCH)-$(BUILTINS_CACHE_KEY)
BUILTINS_LIB := $(EMBOX_BUILTINS_CACHE_DIR)/libclang_rt.builtins-$(SP_ARCH).a
SME_STUB_LIB := $(EMBOX_BUILTINS_CACHE_DIR)/libsme_stub.a

EMBOX_COMPILER_RT_MARCH ?= armv8-a

all:
	@mkdir -p $(TARGET_LIB_DIR)
	if [ ! -f "$(BUILTINS_LIB)" ] || [ ! -f "$(SME_STUB_LIB)" ]; then \
		echo "Building compiler-rt builtins for $(SP_ARCH_TARGET_CLANG) (cached at $(EMBOX_BUILTINS_CACHE_DIR))"; \
		[ -d "$(LLVM_SRC)/compiler-rt" ] || { echo "error: LLVM_SRC/compiler-rt not found ($(LLVM_SRC))." >&2; exit 1; }; \
		[ -f "$(EMBOX_INCLUDE)/pthread.h" ] || { echo "error: Embox sysroot pthread.h missing ($(EMBOX_INCLUDE))." >&2; exit 1; }; \
		mkdir -p "$(EMBOX_BUILTINS_CACHE_DIR)/build"; \
		cmake -G "Ninja" -S "$(LLVM_SRC)/compiler-rt" -B "$(EMBOX_BUILTINS_CACHE_DIR)/build" \
			-DCMAKE_SYSTEM_NAME=Generic \
			-DCMAKE_SYSTEM_PROCESSOR=$(SP_ARCH) \
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
			-DCOMPILER_RT_DEFAULT_TARGET_ONLY=ON; \
		ninja -C "$(EMBOX_BUILTINS_CACHE_DIR)/build" builtins; \
		ARCH_A=$$(find "$(EMBOX_BUILTINS_CACHE_DIR)/build" -name 'libclang_rt.builtins*.a' | head -1); \
		[ -n "$$ARCH_A" ] || { echo "error: compiler-rt builtins archive not produced." >&2; exit 1; }; \
		cp "$$ARCH_A" "$(BUILTINS_LIB)"; \
		"$(HOST_CLANG)" --target=$(SP_ARCH_TARGET_CLANG) -march=$(EMBOX_COMPILER_RT_MARCH) \
			-isystem "$(EMBOX_INCLUDE)" -D__EMBOX__ -fPIC -c \
			"$(LLVM_SRC)/compiler-rt/lib/builtins/emutls.c" \
			-o "$(EMBOX_BUILTINS_CACHE_DIR)/emutls.o"; \
		"$(HOST_AR)" rcs "$(BUILTINS_LIB)" "$(EMBOX_BUILTINS_CACHE_DIR)/emutls.o"; \
		"$(HOST_AR)" t "$(BUILTINS_LIB)" | grep -q emutls || { \
			echo "error: emutls.o did not land in $(BUILTINS_LIB)" >&2; exit 1; }; \
		printf '%s\n' \
			'/* SME is not supported on Embox/baremetal. compiler-rt sme-abi.S' \
			'   references this helper; return 0 so the sme-abi paths bail out. */' \
			'int __aarch64_sme_accessible(void) { return 0; }' \
			> "$(EMBOX_BUILTINS_CACHE_DIR)/sme_stub.c"; \
		"$(HOST_CLANG)" --target=$(SP_ARCH_TARGET_CLANG) -march=$(EMBOX_COMPILER_RT_MARCH) \
			-c "$(EMBOX_BUILTINS_CACHE_DIR)/sme_stub.c" \
			-o "$(EMBOX_BUILTINS_CACHE_DIR)/sme_stub.o"; \
		"$(HOST_AR)" rcs "$(SME_STUB_LIB)" "$(EMBOX_BUILTINS_CACHE_DIR)/sme_stub.o"; \
		echo "ok: builtins+emutls cached at $(BUILTINS_LIB)"; \
	fi
	cp -af "$(BUILTINS_LIB)" "$(TARGET_LIB_DIR)/"
	cp -af "$(SME_STUB_LIB)" "$(TARGET_LIB_DIR)/"

.PHONY: all
