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

# NuttX target. Hosted POSIX on NuttX's own libc — sysroot is IMPORTED from a
# `make export` package produced by a NuttX source tree (or built on the spot
# from NUTTX_DIR+NUTTX_CONFIG), then augmented with our LLVM runtimes built
# against that libc. No libc_impl, no musl: libc/pthread/mm/... come from NuttX.
#
# Triple layout (see docs/platforms/nuttx.adoc §Triple naming):
#   <arch>-nuttx-none-elf      bare-metal ARMv8/RISC-V boards (RPi4, VisionFive 2, ...)
#   arm-nuttx-none-eabi        Cortex-M / -R / 32-bit -A boards
#   x86_64-nuttx-sim-linux-gnu the NuttX sim target (a Linux process)
# LLVM does not know an "OSType nuttx", so the OS field stays "none"/"linux" and
# NuttX rides in the vendor field; the clang --target uses the baremetal triple.
#
# Per-board profiles arrive as a +<board> suffix (e.g. aarch64-nuttx-none-elf+rpi4)
# mirroring the +dll/+sprt suffix scheme; the suffix selects a specific export
# package via NUTTX_BOARD / NUTTX_CONFIG.

.DEFAULT_GOAL := all
THIS_FILE := $(lastword $(MAKEFILE_LIST))
MAKE_FILE := $(realpath $(lastword $(MAKEFILE_LIST)))
MAKE_ROOT := $(dir $(MAKE_FILE))
include $(dir $(THIS_FILE))../common/utils/detect-platform.mk
include $(dir $(THIS_FILE))../common/utils/init-shell.mk

IMPORT_FILE := $(MAKE_ROOT)import-export.mk
TARGET_FILE := $(MAKE_ROOT)init-target.mk
COMPILER_RT_FILE := $(MAKE_ROOT)compiler_rt.mk
LIBCXX_FILE := $(MAKE_ROOT)libc++.mk
INSTALL_TARGET_FILE := $(MAKE_ROOT)install-target.mk
TOOLCHAIN_OUTPUT_DIR ?= $(abspath $(MAKE_ROOT)intermediate)
TOOLCHAIN_TARGETS ?= $(abspath $(MAKE_ROOT)targets)

SP_OPT := -O2

# A NuttX export package is REQUIRED input. We accept either a prebuilt tarball
# (preferred — reproducible, what `make export` in a NuttX tree produces) or a
# NuttX source tree + board:config that we build the export from on the spot.
ifndef SP_ARCH
# --- outer pass: validate inputs, describe targets, drive sub-passes ---

NUTTX_EXPORT ?=
NUTTX_DIR ?=
NUTTX_CONFIG ?=
NUTTX_BOARD ?=

ifeq ($(NUTTX_EXPORT),)
ifeq ($(NUTTX_DIR),)
$(error NuttX target requires NUTTX_EXPORT=<pkg.tar.gz> or NUTTX_DIR=<nuttx-tree> [NUTTX_CONFIG=<board>:<config>])
endif
ifeq ($(NUTTX_CONFIG),)
$(error NuttX target with NUTTX_DIR also requires NUTTX_CONFIG=<board>:<config> (e.g. raspberrypi-4b:fb))
endif
endif

# Per-arch parameters. The STAPPLER_TARGET name carries the nuttx vendor; the
# clang --target strips it (LLVM has no "nuttx" OSLiteral). SP_ARCH_TARGET_CLANG
# is what we pass to --target=; SP_TARGET is the on-disk directory name.
SP_ARCH_aarch64_INFO_BASE := \
	SP_RUNTIME_ROOT=$(realpath $(MAKE_ROOT)/../..) \
	SP_ARCH=aarch64 \
	SP_RES_ARCH=aarch64 \
	SP_ARCH_TARGET_CLANG=aarch64-none-elf

SP_ARCH_aarch64_INFO := \
	$(SP_ARCH_aarch64_INFO_BASE) \
	SP_TARGET=aarch64-nuttx-none-elf \
	TOOLCHAIN_OUTPUT_DIR=$(TOOLCHAIN_OUTPUT_DIR)/aarch64-nuttx-none-elf

SP_ARCH_aarch64_INSTALL := \
	$(SP_ARCH_aarch64_INFO_BASE) \
	T_INTERMEDIATE=$(TOOLCHAIN_OUTPUT_DIR)/aarch64-nuttx-none-elf \
	T_TARGET=$(TOOLCHAIN_TARGETS)/aarch64-nuttx-none-elf

# Future arches (M1 ships aarch64 only; the rest are added by copying this block
# and swapping the clang triple / STAPPLER_TARGET name):
#   riscv64-nuttx-none-elf   -> SP_ARCH_TARGET_CLANG=riscv64-none-elf
#   arm-nuttx-none-eabi      -> SP_ARCH_TARGET_CLANG=arm-none-eabi
#   x86_64-nuttx-sim-linux-gnu -> SP_ARCH_TARGET_CLANG=x86_64-unknown-linux-gnu

ARCHES := aarch64

all: $(addprefix target-,$(ARCHES))

clean:
	$(call rule_rm,$(TOOLCHAIN_OUTPUT_DIR))

# Build one NuttX target:
#   1. import-export.mk  — unpack / build the nuttx-export package, lay out headers
#                           and libraries into the sysroot, extract arch flags
#   2. init-target.mk    — generate toolchain.cmake + the app-facing target.mk,
#                           link the shared host toolchain into this sysroot
#   3. (M2) inner pass   — build compiler-rt / libunwind / libc++abi / libc++
#                           against the imported NuttX libc
#   4. install-target.mk — copy intermediate/ into targets/<triple>/
target-aarch64:
	@mkdir -p $(TOOLCHAIN_OUTPUT_DIR)/aarch64-nuttx-none-elf
	$(MAKE) -f $(IMPORT_FILE) \
		$(SP_ARCH_aarch64_INFO) \
		NUTTX_EXPORT="$(NUTTX_EXPORT)" NUTTX_DIR="$(NUTTX_DIR)" \
		NUTTX_CONFIG="$(NUTTX_CONFIG)" NUTTX_BOARD="$(NUTTX_BOARD)"
	$(MAKE) -f $(TARGET_FILE) $(SP_ARCH_aarch64_INFO)
	# M2 will insert the LLVM runtimes inner pass here.
	$(MAKE) -f $(INSTALL_TARGET_FILE) $(SP_ARCH_aarch64_INSTALL)

.PHONY: all clean target-aarch64

endif # outer pass
