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

# Vulkan for Tau user modules (the target `vulkan` of the Makefile, BLD-A-VulkanLibs-1 of
# xenolith-tau), built into SP_INSTALL_PREFIX: the Vulkan and SPIR-V headers, the loader
# libvulkan.so.1 (vulkan-loader.mk) and lavapipe libvulkan_lvp.so with LLVM inside it
# (llvm.mk, mesa.mk), both shared libraries linked against libsprt.so.

.DEFAULT_GOAL := all
.NOTPARALLEL:

LIB_SRC_DIR ?= $(abspath ../src)

FORWARD_VARS := \
	TAU_USER=1 \
	SP_SYSNAME=TAU_USER \
	SP_ARCH=$(SP_ARCH) \
	SP_ARCH_TARGET_CLANG=$(SP_ARCH_TARGET_CLANG) \
	SP_TAU_ARCH_FLAGS="$(SP_TAU_ARCH_FLAGS)" \
	SP_VK_LINK_FLAGS="$(SP_VK_LINK_FLAGS)" \
	SP_INSTALL_PREFIX=$(SP_INSTALL_PREFIX) \
	SP_TOOLCHAIN_FILE=$(SP_INSTALL_PREFIX)/toolchain-vulkan.cmake \
	SP_TOOLCHAIN_PREFIX=$(SP_INSTALL_PREFIX) \
	SP_TARGET=$(SP_ARCH_TARGET_CLANG) \
	SP_RUNTIME_ROOT=$(SP_RUNTIME_ROOT) \
	SP_CC=$(SP_CC) \
	SP_CXX=$(SP_CXX) \
	SP_AR=$(SP_AR) \
	SP_OPT="-O3" \
	LIB_SRC_DIR=$(LIB_SRC_DIR) \
	MAKE_ROOT=$(MAKE_ROOT)

P := $(SP_INSTALL_PREFIX)/usr
VK_HEADERS := $(P)/include/vulkan/vulkan.h
SPIRV_HEADERS := $(P)/include/spirv/unified1/spirv.h

all: $(VK_HEADERS) $(SPIRV_HEADERS) $(P)/lib/libvulkan.so.1 $(P)/lib/libvulkan_lvp.so

$(VK_HEADERS): ../common/vulkan-headers.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(SPIRV_HEADERS): ../common/spirv-headers.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(P)/lib/libvulkan.so.1: vulkan-loader.mk $(VK_HEADERS)
	$(MAKE) -f $< $(FORWARD_VARS)

$(P)/lib/libLLVMCore.a: llvm.mk lavapipe-options.mk
	$(MAKE) -f $< $(FORWARD_VARS)

$(P)/lib/libvulkan_lvp.so: mesa.mk lavapipe-options.mk $(wildcard $(MAKE_ROOT)../replacements/mesa/26.2.3-tau/*.patch) $(P)/lib/libLLVMCore.a $(VK_HEADERS)
	$(MAKE) -f $< $(FORWARD_VARS)

.PHONY: all
