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

# Cross-build the LLVM runtimes (compiler-rt builtins, libunwind, libc++abi,
# libc++) for the freestanding wasm32 target using the host clang. Mirrors the
# two-phase target-windows/target-linux flow: the outer pass (no SP_ARCH) sets up
# the per-arch variables and drives init-target -> build -> install; the inner
# pass (SP_ARCH set) builds against the generated wasm toolchain.cmake.

.DEFAULT_GOAL := all
THIS_FILE := $(lastword $(MAKEFILE_LIST))
MAKE_FILE := $(realpath $(lastword $(MAKEFILE_LIST)))
MAKE_ROOT := $(dir $(MAKE_FILE))
include $(dir $(THIS_FILE))../common/utils/detect-platform.mk
include $(dir $(THIS_FILE))../common/utils/init-shell.mk
TARGET_FILE := $(MAKE_ROOT)init-target.mk
COMPILER_RT_FILE := $(MAKE_ROOT)compiler_rt.mk
INSTALL_TARGET_FILE := $(MAKE_ROOT)install-target.mk
TOOLCHAIN_OUTPUT_DIR ?= $(abspath $(MAKE_ROOT)intermediate)
TOOLCHAIN_TARGETS ?= $(abspath $(MAKE_ROOT)targets)

SP_OPT := -O3

ifndef SP_ARCH
# --- outer pass: describe the wasm32 target and drive the sub-passes ---
ARCHS := wasm32
LIB_SRC_DIR := $(abspath $(MAKE_ROOT)../src)

SP_ARCH_wasm32_INFO_BASE := \
	LIB_SRC_DIR=$(LIB_SRC_DIR) \
	SP_RUNTIME_ROOT=$(realpath $(MAKE_ROOT)/../..) \
	SP_ARCH=wasm32 \
	SP_RES_ARCH=wasm32 \
	SP_ARCH_TARGET_CLANG=wasm32-unknown-unknown

SP_ARCH_wasm32_INFO := \
	$(SP_ARCH_wasm32_INFO_BASE) \
	SP_TARGET=wasm32-unknown-unknown \
	TOOLCHAIN_OUTPUT_DIR=$(TOOLCHAIN_OUTPUT_DIR)/wasm32-unknown-unknown

SP_ARCH_wasm32_INSTALL := \
	$(SP_ARCH_wasm32_INFO_BASE) \
	T_INTERMEDIATE=$(TOOLCHAIN_OUTPUT_DIR)/wasm32-unknown-unknown \
	T_TARGET=$(TOOLCHAIN_TARGETS)/wasm32-unknown-unknown

all: $(ARCHS)

clean:
	$(call rule_rm,$(TOOLCHAIN_OUTPUT_DIR))

wasm32:
	@mkdir -p $(TOOLCHAIN_OUTPUT_DIR)/wasm32-unknown-unknown/usr/include $(TOOLCHAIN_OUTPUT_DIR)/wasm32-unknown-unknown/usr/lib
	$(MAKE) -f $(TARGET_FILE) $(SP_ARCH_wasm32_INFO)
	$(MAKE) -f $(THIS_FILE) $(SP_ARCH_wasm32_INFO)
	$(MAKE) -f $(INSTALL_TARGET_FILE) $(SP_ARCH_wasm32_INSTALL)

.PHONY: all clean wasm32

else # SP_ARCH
# --- inner pass: build the runtimes against the generated toolchain.cmake ---
SP_INSTALL_PREFIX ?= $(TOOLCHAIN_OUTPUT_DIR)
SP_CC := $(abspath $(TOOLCHAIN_OUTPUT_DIR)/host/bin/cc)
SP_CXX := $(abspath $(TOOLCHAIN_OUTPUT_DIR)/host/bin/c++)
SP_AR := $(abspath $(TOOLCHAIN_OUTPUT_DIR)/host/bin/ar)
LIB_SRC_DIR := $(abspath $(MAKE_ROOT)../src)

FORWARD_VARS := \
	WASM=1 \
	SP_SYSNAME=WASM \
	SP_ARCH=$(SP_ARCH) \
	SP_RES_ARCH=$(SP_RES_ARCH) \
	SP_INSTALL_PREFIX=$(SP_INSTALL_PREFIX) \
	SP_TOOLCHAIN_FILE=$(TOOLCHAIN_OUTPUT_DIR)/toolchain.cmake \
	SP_TOOLCHAIN_PREFIX=$(TOOLCHAIN_OUTPUT_DIR) \
	SP_TARGET=$(SP_TARGET) \
	SP_RUNTIME_ROOT=$(SP_RUNTIME_ROOT) \
	SP_CC=$(SP_CC) \
	SP_CXX=$(SP_CXX) \
	SP_AR=$(SP_AR) \
	SP_OPT="$(SP_OPT)" \
	LIB_SRC_DIR=$(LIB_SRC_DIR) \
	MAKE_ROOT=$(MAKE_ROOT)

all:
	$(MAKE) -C $(MAKE_ROOT) -f $(COMPILER_RT_FILE) $(FORWARD_VARS)

.PHONY: all

endif # SP_ARCH
