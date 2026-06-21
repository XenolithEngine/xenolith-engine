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

GIT_TAG ?= $(shell git describe --tags --abbrev=0)

SP_USE_STAPPLER_DISTRIB ?= 1

LOCAL_MAKEFILE := $(lastword $(MAKEFILE_LIST))
LOCAL_ROOT := $(dir $(THIS_FILE))

include $(LOCAL_ROOT)../../common/utils/detect-platform.mk
include $(LOCAL_ROOT)../../common/utils/init-shell.mk

SP_ARCH := x86_64
SP_ARCH_LD := x86_64
SP_ARCH_WINDOWS := x86_64


SP_NJOBS := -j16
SP_ARCH_CLANG ?= $(SP_ARCH)-pc-windows-msvc

SP_LLVM_VER := 21

LLVM_DIR := $(realpath $(LOCAL_ROOT)../../src/llvm-project)
SDK_DIR ?= $(realpath $(LOCAL_ROOT)../../src/xwin/splat/sdk)
CRT_DIR ?= $(realpath $(LOCAL_ROOT)../../src/xwin/splat/crt)

VULKAN_HEADERS_DIR := $(realpath ../../src/vulkan-headers)
SPIRV_HEADERS_DIR := $(realpath ../../src/spirv-headers)
GLSLANG_DIR := $(realpath ../../src/glslang)
SPIRV_TOOLS_DIR := $(realpath ../../src/spirv-tools)
LIBXML2_DIR := $(realpath ../../src/libxml2)
ZLIB_DIR := $(realpath ../../src/zlib)

HOST_ROOT := $(realpath $(LOCAL_ROOT)../../hosts/$(HOST_ID))

$(info HOST_ROOT $(HOST_ROOT))

OUT_SYSROOT := ../../hosts/$(SP_ARCH_CLANG)

include stage0.mk

$(OUT_SYSROOT)/release: $(MAKE_SRC_EXE) $(STAGE0_CLANG_CC) $(STAGE0_GLSLANG)
	$(call rule_rm,$(OUT_SYSROOT))
	$(call rule_mkdir,$(OUT_SYSROOT))
	$(call rule_mkdir,$(OUT_SYSROOT)/lib)
	$(call rule_cpr,$(STAGE0_SYSROOT)/lib/clang,$(OUT_SYSROOT)/lib)
	$(call rule_mkdir,$(OUT_SYSROOT)/bin)
	$(call rule_cp,$(STAGE0_SYSROOT)/bin/glslang.exe,$(OUT_SYSROOT)/bin)
	$(call rule_cp,$(STAGE0_SYSROOT)/bin/spirv-link.exe,$(OUT_SYSROOT)/bin)
	$(call rule_cp,$(STAGE0_SYSROOT)/bin/clang.exe,$(OUT_SYSROOT)/bin)
	$(call rule_hardlink,$(OUT_SYSROOT)/bin/clang.exe,$(OUT_SYSROOT)/bin/clang++.exe)
	$(call rule_hardlink,$(OUT_SYSROOT)/bin/clang.exe,$(OUT_SYSROOT)/bin/clang-cl.exe)
	$(call rule_hardlink,$(OUT_SYSROOT)/bin/clang.exe,$(OUT_SYSROOT)/bin/clang-cpp.exe)
	$(call rule_cp,$(STAGE0_SYSROOT)/bin/lld.exe,$(OUT_SYSROOT)/bin)
	$(call rule_hardlink,$(OUT_SYSROOT)/bin/lld.exe,$(OUT_SYSROOT)/bin/lld-link.exe)
	$(call rule_hardlink,$(OUT_SYSROOT)/bin/lld.exe,$(OUT_SYSROOT)/bin/ld.lld.exe)
	$(call rule_hardlink,$(OUT_SYSROOT)/bin/lld.exe,$(OUT_SYSROOT)/bin/ld64.lld.exe)
	$(call rule_hardlink,$(OUT_SYSROOT)/bin/lld.exe,$(OUT_SYSROOT)/bin/wasm-ld.exe)
	$(call rule_cp,$(STAGE0_SYSROOT)/bin/llvm-ar.exe,$(OUT_SYSROOT)/bin)
	$(call rule_hardlink,$(OUT_SYSROOT)/bin/llvm-ar.exe,$(OUT_SYSROOT)/bin/llvm-lib.exe)
	$(call rule_hardlink,$(OUT_SYSROOT)/bin/llvm-ar.exe,$(OUT_SYSROOT)/bin/llvm-ranlib.exe)
	$(call rule_hardlink,$(OUT_SYSROOT)/bin/llvm-ar.exe,$(OUT_SYSROOT)/bin/llvm-dlltool.exe)
	$(call rule_cp,$(STAGE0_SYSROOT)/bin/llvm-objcopy.exe,$(OUT_SYSROOT)/bin)
	$(call rule_hardlink,$(OUT_SYSROOT)/bin/llvm-objcopy.exe,$(OUT_SYSROOT)/bin/llvm-strip.exe)
	$(call rule_cp,$(STAGE0_SYSROOT)/bin/llvm-rc.exe,$(OUT_SYSROOT)/bin)
	$(call rule_cp,$(STAGE0_SYSROOT)/bin/lldb.exe,$(OUT_SYSROOT)/bin)
	$(call rule_cp,$(STAGE0_SYSROOT)/bin/lldb-server.exe,$(OUT_SYSROOT)/bin)
	$(call rule_cp,$(STAGE0_SYSROOT)/bin/lldb-dap.exe,$(OUT_SYSROOT)/bin)
	$(call rule_cp,$(STAGE0_SYSROOT)/bin/LTO.dll,$(OUT_SYSROOT)/bin)
	$(call rule_cp,$(STAGE0_SYSROOT)/bin/liblldb.dll,$(OUT_SYSROOT)/bin)
	@echo '$(GIT_TAG)' > $@

$(OUT_SYSROOT)/host.mk: $(lastword $(MAKEFILE_LIST))
	@echo 'HOST_ROOT := $$(patsubst %/,%,$$(dir $$(lastword $$(MAKEFILE_LIST))))' > $@
	@echo 'HOST_BINDIR := $$(HOST_ROOT)/bin' >> $@
	@echo 'HOST_CC := $$(HOST_BINDIR)/clang.exe' >> $@
	@echo 'HOST_CXX := $$(HOST_BINDIR)/clang.exe' >> $@
	@echo 'HOST_AR := $$(HOST_BINDIR)/llvm-ar.exe' >> $@
	@echo 'HOST_GLSLANG := $$(HOST_BINDIR)/glslang.exe' >> $@
	@echo 'HOST_SPIRV_LINK := $$(HOST_BINDIR)/spirv-link.exe' >> $@
	@echo 'HOST_GENERAL_CFLAGS := -idirafter $$(HOST_ROOT)/lib/clang/$(SP_LLVM_VER)/include' >> $@
	@echo 'HOST_GENERAL_CXXFLAGS := -idirafter $$(HOST_ROOT)/lib/clang/$(SP_LLVM_VER)/include' >> $@
	@echo 'HOST_GENERAL_LDFLAGS :=' >> $@
	@echo 'HOST_EXEC_CFLAGS :=' >> $@
	@echo 'HOST_EXEC_CXXFLAGS :=' >> $@
	@echo 'HOST_EXEC_LDFLAGS :=' >> $@
	@echo 'HOST_LIB_CFLAGS :=' >> $@
	@echo 'HOST_LIB_CXXFLAGS :=' >> $@
	@echo 'HOST_LIB_LDFLAGS :=' >> $@

download: $(MAKE_SRC_DIR) $(LLVM_DIR)

make: $(MAKE_SRC_EXE)

out: stage0 $(OUT_SYSROOT)/release $(OUT_SYSROOT)/host.mk

all: stage0

.PHONY: stage0 all out make
