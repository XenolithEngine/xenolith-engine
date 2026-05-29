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

LOCAL_MAKEFILE := $(lastword $(MAKEFILE_LIST))

SHELL = powershell.exe

rule_mkdir = powershell New-Item -ItemType Directory -Force -Path "$(1)" | Out-Null
rule_rm = powershell 'if (Test-Path "$(1)") { Remove-Item -Recurse -Force -ErrorAction Ignore -Path "$(1)" }'
rule_cp = powershell Copy-Item -Path "$(1)" -Destination "$(2)" -Force
rule_cpr = powershell Copy-Item -Path "$(1)" -Destination "$(2)" -Force -Recurse
rule_echo = Add-Content  -Encoding UTF8 -Path "$(1)" -Value 

SP_NJOBS := -j16
SP_ARCH := x86_64
SP_ARCH_LD := x86-64
SP_ARCH_CLANG ?= $(SP_ARCH)-pc-windows-msvc
SP_ARCH_LINUX := x86

# https://ftp.gnu.org/gnu/make/make-4.4.1.tar.lz
MAKE_SRC_VER := 4.4.1
MAKE_SRC_TARBALL := make-$(MAKE_SRC_VER).tar.lz
MAKE_SRC_DIR := make-$(MAKE_SRC_VER)
MAKE_SRC_URL := https://ftp.gnu.org/gnu/make/make-$(MAKE_SRC_VER).tar.lz
MAKE_SRC_EXE := $(MAKE_SRC_DIR)/WinRel\gnumake.exe

$(MAKE_SRC_TARBALL):
	Invoke-WebRequest -Uri "$(MAKE_SRC_URL)" -OutFile "$(MAKE_SRC_TARBALL)"

$(MAKE_SRC_DIR): $(MAKE_SRC_TARBALL)
	tar -xf $(MAKE_SRC_TARBALL)
	(Get-Item "$(MAKE_SRC_DIR)").LastWriteTime = $$(Get-Date);
	cd $(MAKE_SRC_DIR); git apply ../make-patch/make-4.4.1-sub_proc.patch
	cd $(MAKE_SRC_DIR); git apply ../make-patch/make-4.4.1-win32-ctrl-c.patch
	cd $(MAKE_SRC_DIR); git apply ../make-patch/make-4.4.1-SV49841.patch
	cd $(MAKE_SRC_DIR); git apply ../make-patch/make-4.4.1-warn-noargs.patch
	cd $(MAKE_SRC_DIR); git apply ../make-patch/make-4.4.1-expand.patch
	cd $(MAKE_SRC_DIR); git apply ../make-patch/make-4.4.1-sort.patch
	cd $(MAKE_SRC_DIR); git apply ../make-patch/make-4.4.1-filter.patch
	cd $(MAKE_SRC_DIR); git apply ../make-patch/make-4.4.1-win32-colors.patch
	(Get-Content -Path "$(MAKE_SRC_DIR)/src/config.h.W32").Replace('/* #undef HAVE_CASE_INSENSITIVE_FS */', '#define HAVE_CASE_INSENSITIVE_FS 1') | Set-Content -Encoding UTF8 -Path "$(MAKE_SRC_DIR)/src/config.h.W32"

$(MAKE_SRC_EXE): $(MAKE_SRC_DIR)
	cd $(MAKE_SRC_DIR); cmd.exe /c 'start cmd.exe /c "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x86_amd64 && build_w32.bat --verbose --without-guile'

LLVM_MAJOR := 21
LLVM_V := $(LLVM_MAJOR).1.8
LLVM_VER := llvm-project-$(LLVM_V).src
LLVM_DIR := $(LLVM_VER)
LLVM_TARBALL := $(addsuffix .tar.xz,$(LLVM_VER))
LLVM_URL := https://github.com/llvm/llvm-project/releases/download/llvmorg-$(LLVM_V)/llvm-project-$(LLVM_V).src.tar.xz

$(LLVM_TARBALL):
	powershell Invoke-WebRequest -Uri "$(LLVM_URL)" -OutFile "$(LLVM_TARBALL)"

$(LLVM_DIR): $(LLVM_TARBALL)
	powershell tar -xf $(LLVM_TARBALL)
	powershell touch $(LLVM_DIR)

OUT_SYSROOT := ../$(SP_ARCH_CLANG)

include stage0.mk

$(OUT_SYSROOT): $(STAGE0_SYSROOT) $(MAKE_SRC_EXE)
	$(call rule_rm,$(OUT_SYSROOT))
	$(call rule_mkdir,$(OUT_SYSROOT))
	$(call rule_cpr,$(STAGE0_SYSROOT)/lib,$(OUT_SYSROOT))
	$(call rule_cpr,$(STAGE0_SYSROOT)/include,$(OUT_SYSROOT))
	$(call rule_mkdir,$(OUT_SYSROOT)/bin)
	$(call rule_cp,$(STAGE0_SYSROOT)/bin/clang.exe,$(OUT_SYSROOT)/bin)
	$(call rule_cp,$(STAGE0_SYSROOT)/bin/lld.exe,$(OUT_SYSROOT)/bin)
	$(call rule_cp,$(STAGE0_SYSROOT)/bin/llvm-ar.exe,$(OUT_SYSROOT)/bin)
	$(call rule_cp,$(STAGE0_SYSROOT)/bin/llvm-lib.exe,$(OUT_SYSROOT)/bin)
	$(call rule_cp,$(STAGE0_SYSROOT)/bin/llvm-rc.exe,$(OUT_SYSROOT)/bin)
	$(call rule_cp,$(STAGE0_SYSROOT)/bin/lldb.exe,$(OUT_SYSROOT)/bin)
	$(call rule_cp,$(STAGE0_SYSROOT)/bin/lldb-dap.exe,$(OUT_SYSROOT)/bin)
	$(call rule_cp,$(STAGE0_SYSROOT)/bin/LLVM-C.dll,$(OUT_SYSROOT)/bin)
	$(call rule_cp,$(STAGE0_SYSROOT)/bin/LTO.dll,$(OUT_SYSROOT)/bin)
	$(call rule_cp,$(STAGE0_SYSROOT)/bin/libclang.dll,$(OUT_SYSROOT)/bin)
	$(call rule_cp,$(STAGE0_SYSROOT)/bin/liblldb.dll,$(OUT_SYSROOT)/bin)
	$(call rule_cp,$(MAKE_SRC_EXE),$(OUT_SYSROOT)/bin/make.exe)

	$(call rule_rm,$(OUT_SYSROOT)/toolchain.mk)
	$(call rule_echo,$(OUT_SYSROOT)/toolchain.mk) 'TOOLCHAIN_SYSROOT := $$(abspath $$(dir $$(lastword $$(MAKEFILE_LIST))))'
	$(call rule_echo,$(OUT_SYSROOT)/toolchain.mk) 'TOOLCHAIN_TARGET := $(SP_ARCH_CLANG)'
	$(call rule_echo,$(OUT_SYSROOT)/toolchain.mk) 'TOOLCHAIN_LIBDIR := $$(TOOLCHAIN_SYSROOT)/lib'
	$(call rule_echo,$(OUT_SYSROOT)/toolchain.mk) 'TOOLCHAIN_INCLUDEDIR := $$(TOOLCHAIN_SYSROOT)/include'
	$(call rule_echo,$(OUT_SYSROOT)/toolchain.mk) 'TOOLCHAIN_INCLUDEDIR_LIBCXX := $$(TOOLCHAIN_INCLUDEDIR)/c++/v1'
	$(call rule_echo,$(OUT_SYSROOT)/toolchain.mk) 'TOOLCHAIN_CC := $$(TOOLCHAIN_SYSROOT)/bin/clang.exe'
	$(call rule_echo,$(OUT_SYSROOT)/toolchain.mk) 'TOOLCHAIN_CXX := $$(TOOLCHAIN_SYSROOT)/bin/clang.exe'
	$(call rule_echo,$(OUT_SYSROOT)/toolchain.mk) 'TOOLCHAIN_AR := $$(TOOLCHAIN_SYSROOT)/bin/llvm-ar.exe'
	$(call rule_echo,$(OUT_SYSROOT)/toolchain.mk) 'TOOLCHAIN_GLSLANG := $$(TOOLCHAIN_SYSROOT)/bin/glslang.exe'
	$(call rule_echo,$(OUT_SYSROOT)/toolchain.mk) 'TOOLCHAIN_SPIRV_LINK := $$(TOOLCHAIN_SYSROOT)/bin/spirv-link.exe'
	$(call rule_echo,$(OUT_SYSROOT)/toolchain.mk) 'TOOLCHAIN_GENERAL_CFLAGS := -nostdinc -isystem $$(TOOLCHAIN_SYSROOT)/lib/clang/21/include'
	$(call rule_echo,$(OUT_SYSROOT)/toolchain.mk) 'TOOLCHAIN_GENERAL_CXXFLAGS := -nostdinc -isystem $$(TOOLCHAIN_SYSROOT)/lib/clang/21/include'
	$(call rule_echo,$(OUT_SYSROOT)/toolchain.mk) 'TOOLCHAIN_LIB_LDFLAGS := -L$$(TOOLCHAIN_LIBDIR) -llibc++'
	$(call rule_echo,$(OUT_SYSROOT)/toolchain.mk) 'TOOLCHAIN_EXEC_LDFLAGS := -L$$(TOOLCHAIN_LIBDIR) -llibc++'
	$(call rule_echo,$(OUT_SYSROOT)/toolchain.mk) '$$(info Available toolchain: $$(TOOLCHAIN_TARGET): $$(TOOLCHAIN_SYSROOT))'

download: $(MAKE_SRC_DIR) $(LLVM_DIR)

make: $(MAKE_SRC_EXE)

out: $(OUT_SYSROOT)

all: stage0

.PHONY: stage0 all out make
