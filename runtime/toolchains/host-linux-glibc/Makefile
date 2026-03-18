# Copyright (c) 2026 Xenolith Team <admin@xenloth.studio>
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

SP_NJOBS ?= -j16
SP_ARCH ?= x86_64
SP_ARCH_LD ?= x86-64
SP_ARCH_CLANG ?= $(SP_ARCH)-unknown-linux-gnu
SP_LLVM_VER := 21

include glibc.mk

LINUX_HEADERS_DIR := src/linux-headers-$(SP_ARCH)

$(LINUX_HEADERS_DIR): $(LINUX_KERNEL_DIR)
	mkdir -p $(LINUX_HEADERS_DIR)
	cd $(LINUX_KERNEL_DIR); make headers_install ARCH=$(SP_ARCH) INSTALL_HDR_PATH=../$(notdir $(LINUX_HEADERS_DIR))
	cd $(LINUX_KERNEL_DIR); make clean
	touch -m -r src/$(LINUX_KERNEL_TARBALL) $(LINUX_KERNEL_DIR)

VULKAN_HEADERS_DIR := $(realpath ../src/vulkan-headers)
SPIRV_HEADERS_DIR := $(realpath ../src/spirv-headers)
GLSLANG_DIR := $(realpath ../src/glslang)
SPIRV_TOOLS_DIR := $(realpath ../src/spirv-tools)
LIBXML2_DIR := $(realpath ../src/libxml2)

include stage0.mk
include stage1.mk
include stage2.mk

OUT_SYSROOT ?= ../hosts/$(SP_ARCH_CLANG)

OUT_TOOLS := $(addprefix $(OUT_SYSROOT)/bin/,\
	clang-21 \
	clang-format \
	clang-installapi \
	diagtool \
	glslang \
	lld \
	lldb \
	lldb-argdumper \
	lldb-dap \
	lldb-instr \
	lldb-server \
	llvm-ar \
	llvm-cov \
	llvm-cxxfilt \
	llvm-dwp \
	llvm-ifs \
	llvm-libtool-darwin \
	llvm-link \
	llvm-lipo \
	llvm-ml \
	llvm-mt \
	llvm-nm \
	llvm-objcopy \
	llvm-objdump \
	llvm-pdbutil \
	llvm-rc \
	llvm-readobj \
	llvm-size \
	llvm-strings \
	llvm-symbolizer \
	make \
	spirv-as \
	spirv-cfg \
	spirv-diff \
	spirv-dis \
	spirv-link \
	spirv-lint \
	spirv-objdump \
	spirv-opt \
	spirv-reduce \
	spirv-val)

OUT_LIBS := $(addprefix $(OUT_SYSROOT)/lib/,\
	libc++.so.1.0 \
	libc++abi.so.1.0 \
	libunwind.so.1.0 \
	libLTO.so.21.1 \
	liblldb.so.21.1.8 \
	liblldbIntelFeatures.so.21.1 \
	libLLVM.so.21.1 \
	libRemarks.so.21.1)

OUT_INCLUDES := $(addprefix $(OUT_SYSROOT)/include/,\
	clang \
	clang-c \
	llvm \
	llvm-c\
	lldb)

$(OUT_SYSROOT)/lib/%: $(STAGEOUT_SYSROOT)/lib/% $(STAGE2_CLANG_CC) $(STAGEOUT_LIBCXX) $(STAGEOUT_SYSROOT)/bin/glslang
	@mkdir -p $(OUT_SYSROOT)/lib
	@rm -f $@
	$(STAGEOUT_SYSROOT)/bin/llvm-strip $< -o $@
	chmod +x $@

$(OUT_SYSROOT)/lib/clang: $(STAGEOUT_SYSROOT)/lib/clang
	@mkdir -p $(OUT_SYSROOT)/lib
	@rm -rf $@
	cp -rf $< $@

$(OUT_SYSROOT)/bin/%: $(STAGEOUT_SYSROOT)/bin/% $(STAGE2_CLANG_CC) $(STAGEOUT_LIBCXX) $(STAGEOUT_SYSROOT)/bin/glslang $(STAGEOUT_MAKE)
	@mkdir -p $(OUT_SYSROOT)/bin
	@rm -f $@
	$(STAGEOUT_SYSROOT)/bin/llvm-strip $< -o $@
	chmod +x $@

$(OUT_SYSROOT)/include/%: $(STAGEOUT_SYSROOT)/include/% $(STAGE2_CLANG_CC) $(STAGEOUT_LIBCXX) $(STAGEOUT_SYSROOT)/bin/glslang
	rm -rf $@
	mkdir -p $@
	cp -fr $</* $@

$(OUT_SYSROOT)/share/licenses: licenses
	rm -rf $@
	mkdir -p $@
	cp -fr $</* $@
	touch $@


$(OUT_SYSROOT)/host.mk: $(lastword $(MAKEFILE_LIST))
	@echo 'Build $(OUT_SYSROOT)/host.mk'
	@echo 'HOST_ROOT := $$(patsubst %/,%,$$(dir $$(lastword $$(MAKEFILE_LIST))))' > $@
	@echo 'HOST_BINDIR := $$(HOST_ROOT)/bin' >> $@
	@echo 'HOST_CC := $$(HOST_BINDIR)/cc' >> $@
	@echo 'HOST_CXX := $$(HOST_BINDIR)/c++' >> $@
	@echo 'HOST_AR := $$(HOST_BINDIR)/ar' >> $@
	@echo 'HOST_GLSLANG := $$(HOST_BINDIR)/glslang' >> $@
	@echo 'HOST_SPIRV_LINK := $$(HOST_BINDIR)/spirv-link' >> $@
	@echo 'HOST_GENERAL_CFLAGS := -idirafter $$(HOST_ROOT)/lib/clang/$(SP_LLVM_VER)/include' >> $@
	@echo 'HOST_GENERAL_CXXFLAGS := -idirafter $$(HOST_ROOT)/lib/clang/$(SP_LLVM_VER)/include' >> $@
	@echo 'HOST_GENERAL_LDFLAGS :=' >> $@
	@echo 'HOST_EXEC_CFLAGS :=' >> $@
	@echo 'HOST_EXEC_CXXFLAGS :=' >> $@
	@echo 'HOST_EXEC_LDFLAGS :=' >> $@
	@echo 'HOST_LIB_CFLAGS :=' >> $@
	@echo 'HOST_LIB_CXXFLAGS :=' >> $@
	@echo 'HOST_LIB_LDFLAGS :=' >> $@

$(OUT_SYSROOT)/sysroot: $(OUT_LIBS) $(OUT_TOOLS) $(OUT_SYSROOT)/lib/clang $(OUT_SYSROOT)/share/licenses $(OUT_SYSROOT)/host.mk
	cd $(OUT_SYSROOT)/lib; ln -fs libc++abi.so.1.0 libc++abi.so
	cd $(OUT_SYSROOT)/lib; ln -fs libc++abi.so.1.0 libc++abi.so.1
	cd $(OUT_SYSROOT)/lib; ln -fs libc++.so.1.0 libc++.so
	cd $(OUT_SYSROOT)/lib; ln -fs libc++.so.1.0 libc++.so.1
	cd $(OUT_SYSROOT)/lib; ln -fs libunwind.so.1.0 libunwind.so
	cd $(OUT_SYSROOT)/lib; ln -fs libunwind.so.1.0 libunwind.so.1
	cd $(OUT_SYSROOT)/lib; ln -fs liblldbIntelFeatures.so.21.1 liblldbIntelFeatures.so
	cd $(OUT_SYSROOT)/lib; ln -fs liblldb.so.21.1 liblldb.so
	cd $(OUT_SYSROOT)/lib; ln -fs liblldb.so.21.1.8 liblldb.so.21.1
	cd $(OUT_SYSROOT)/lib; ln -fs libLLVM.so.21.1 libLLVM-21.so
	cd $(OUT_SYSROOT)/lib; ln -fs libLLVM.so.21.1 libLLVM.so
	cd $(OUT_SYSROOT)/lib; ln -fs libLTO.so.21.1 libLTO.so
	cd $(OUT_SYSROOT)/lib; ln -fs libRemarks.so.21.1 libRemarks.so

	cd $(OUT_SYSROOT)/bin; ln -fs clang-21 clang
	cd $(OUT_SYSROOT)/bin; ln -fs clang clang++
	cd $(OUT_SYSROOT)/bin; ln -fs clang clang-cl
	cd $(OUT_SYSROOT)/bin; ln -fs clang clang-cpp
	cd $(OUT_SYSROOT)/bin; ln -fs glslang glslangValidator
	cd $(OUT_SYSROOT)/bin; ln -fs lld ld64.lld
	cd $(OUT_SYSROOT)/bin; ln -fs lld ld.lld
	cd $(OUT_SYSROOT)/bin; ln -fs lld lld-link
	cd $(OUT_SYSROOT)/bin; ln -fs llvm-symbolizer llvm-addr2line
	cd $(OUT_SYSROOT)/bin; ln -fs llvm-objcopy llvm-bitcode-strip
	cd $(OUT_SYSROOT)/bin; ln -fs llvm-ar llvm-dlltool
	cd $(OUT_SYSROOT)/bin; ln -fs llvm-objcopy llvm-install-name-tool
	cd $(OUT_SYSROOT)/bin; ln -fs llvm-ar llvm-lib
	cd $(OUT_SYSROOT)/bin; ln -fs llvm-ml llvm-ml64
	cd $(OUT_SYSROOT)/bin; ln -fs llvm-objdump llvm-otool
	cd $(OUT_SYSROOT)/bin; ln -fs llvm-ar llvm-ranlib
	cd $(OUT_SYSROOT)/bin; ln -fs llvm-readobj llvm-readelf
	cd $(OUT_SYSROOT)/bin; ln -fs llvm-objcopy llvm-strip
	cd $(OUT_SYSROOT)/bin; ln -fs llvm-rc llvm-windres
	cd $(OUT_SYSROOT)/bin; ln -fs lld wasm-ld

	cd $(OUT_SYSROOT)/bin; ln -fs clang-21 cc
	cd $(OUT_SYSROOT)/bin; ln -fs clang-21 c++
	cd $(OUT_SYSROOT)/bin; ln -fs llvm-ar ar

	touch $@

download: $(LINUX_HEADERS_DIR)

out: $(OUT_SYSROOT)/sysroot

all: stage0 stage1 stage2

.PHONY: stage0 stage1 stage2 all download
