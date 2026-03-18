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
LOCAL_ROOT := $(dir $(THIS_FILE))

include $(LOCAL_ROOT)../../common/utils/detect-platform.mk

SP_NJOBS := -j16
SP_ARCH := aarch64
SP_ARCH_LD := aarch64
SP_ARCH_APPLE := arm64
SP_ARCH_CLANG ?= $(SP_ARCH)-apple-macosx

SP_LLVM_VER := 21

MAKE_SRC_VER := 4.4.1
MAKE_SRC_TARBALL := src/make-$(MAKE_SRC_VER).tar.lz
MAKE_SRC_DIR := src/make-$(MAKE_SRC_VER)
MAKE_SRC_URL := https://ftp.gnu.org/gnu/make/make-$(MAKE_SRC_VER).tar.lz

$(MAKE_SRC_TARBALL):
	mkdir -p src
	wget -O $(MAKE_SRC_TARBALL) $(MAKE_SRC_URL)

$(MAKE_SRC_DIR): $(MAKE_SRC_TARBALL)
	mkdir -p src
	cd src; tar -xf $(notdir $(MAKE_SRC_TARBALL))
	touch $(MAKE_SRC_DIR)

LLVM_DIR := $(realpath $(LOCAL_ROOT)../../src/llvm-project)
SDK_DIR ?= $(realpath $(LOCAL_ROOT)../../src/MacOSX.sdk)

VULKAN_HEADERS_DIR := $(realpath $(LOCAL_ROOT)../../src/vulkan-headers)
SPIRV_HEADERS_DIR := $(realpath $(LOCAL_ROOT)../../src/spirv-headers)
GLSLANG_DIR := $(realpath $(LOCAL_ROOT)../../src/glslang)
SPIRV_TOOLS_DIR := $(realpath $(LOCAL_ROOT)../../src/spirv-tools)

HOST_ROOT := $(realpath $(LOCAL_ROOT)../../hosts/$(HOST_ID))

$(info HOST_ROOT $(HOST_ROOT))

HOST_CC := $(HOST_ROOT)/bin/clang
HOST_CXX := $(HOST_ROOT)/bin/clang++
HOST_STRIP := $(HOST_ROOT)/bin/llvm-strip
HOST_NAMETOOL := $(HOST_ROOT)/bin/llvm-install-name-tool
HOST_LLVM_LIBTOOL := $(HOST_ROOT)/bin/llvm-libtool-darwin
HOST_LLVM_LIPO := $(HOST_ROOT)/bin/llvm-lipo

CODESIGN ?= $(realpath ~/.cargo/bin/rcodesign)

export LIBTOOL=$(abspath $(HOST_LLVM_LIBTOOL))
export LIPO=$(abspath $(HOST_LLVM_LIPO))

TARGET_SYSROOT := sysroot-target
TARGET_TOOLCHAIN_FILE := sysroot-target/toolchain.cmake

TARGET_CLANG_CC := $(TARGET_SYSROOT)/bin/clang
TARGET_CLANG_CXX := $(TARGET_SYSROOT)/bin/clang++

$(TARGET_SYSROOT)/sysroot:
	mkdir -p $(dir $@)
	cp -rf $(SDK_DIR)/System $(dir $@)
	cp -rf $(SDK_DIR)/usr $(dir $@)
	touch $@

$(TARGET_TOOLCHAIN_FILE): $(TARGET_SYSROOT)/sysroot # $(LOCAL_MAKEFILE)
	@echo "set(CMAKE_SYSTEM_NAME Darwin)" > $@
	@echo "set(CMAKE_OSX_DEPLOYMENT_TARGET \"14.4\")" >> $@
	@echo "set(CMAKE_OSX_SYSROOT \"$(abspath $(TARGET_SYSROOT))\")" >> $@
	@echo "set(CMAKE_SYSROOT \"$(abspath $(TARGET_SYSROOT))\")" >> $@
	@echo "set(CMAKE_C_COMPILER_TARGET \"$(SP_ARCH_CLANG)\")" >> $@
	@echo "set(CMAKE_CXX_COMPILER_TARGET \"$(SP_ARCH_CLANG)\")" >> $@
	@echo "set(CMAKE_C_FLAGS_INIT \"\")" >> $@
	@echo "set(CMAKE_CXX_FLAGS_INIT \"\")" >> $@
	@echo "set(CMAKE_LINKER_TYPE LLD)" >> $@
	@echo "set(CMAKE_C_COMPILER \"$(abspath $(HOST_CC))\")" >> $@
	@echo "set(CMAKE_CXX_COMPILER \"$(abspath $(HOST_CXX))\")" >> $@
	@echo "set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)" >> $@
	@echo "set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)" >> $@
	@echo "set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)" >> $@
	@echo "set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)" >> $@
	@echo "set(CMAKE_LIPO \"$(abspath $(HOST_LLVM_LIPO))\")" >> $@
	@echo "set(CMAKE_LIBTOOL \"$(abspath $(HOST_LLVM_LIBTOOL))\")" >> $@
	@echo "set(CMAKE_SHARED_LIBRARY_RUNTIME_C_FLAG \"-Wl,-rpath,\")" >> $@
	@echo "set(CMAKE_SHARED_LIBRARY_RUNTIME_C_FLAG_SEP \":\")" >> $@
	@echo "set(CMAKE_MACOSX_RPATH TRUE)" >> $@

$(TARGET_CLANG_CC): $(TARGET_TOOLCHAIN_FILE)
	@echo "Build TARGET_CLANG_CC: $(TARGET_CLANG_CC)"
	rm -rf build/target-llvm
	cmake \
		-DCMAKE_TOOLCHAIN_FILE=$(realpath $(TARGET_TOOLCHAIN_FILE)) \
		-G Ninja -S $(LLVM_DIR)/llvm -B build/target-llvm \
		-DCMAKE_C_COMPILER="$(abspath $(HOST_CC))" \
		-DCMAKE_CXX_COMPILER="$(abspath $(HOST_CXX))" \
		-DLLVM_ENABLE_PROJECTS="lld;clang;lldb" \
		-DLLVM_TARGETS_TO_BUILD="X86;ARM;AArch64;RISCV;WebAssembly" \
		-DLLVM_ENABLE_PIC=On \
		-DLLVM_ENABLE_LIBXML2=FORCE_ON \
		-DLLVM_BUILD_LLVM_DYLIB=ON \
		-DLLVM_LINK_LLVM_DYLIB=ON \
		-DLLVM_ENABLE_LTO=Full \
		-DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=Off \
		-DCLANG_DEFAULT_CXX_STDLIB=libc++ \
		-DCLANG_DEFAULT_RTLIB=compiler-rt \
		-DCLANG_DEFAULT_LINKER=lld \
		-DCLANG_DEFAULT_UNWINDLIB=libunwind \
		-DCLANG_INCLUDE_TESTS=Off \
		-DCLANG_LINK_CLANG_DYLIB=Off \
		-DCLANG_ENABLE_ARCMT=Off \
		-DCLANG_ENABLE_STATIC_ANALYZER=Off \
		-DLIBCLANG_BUILD_STATIC=On \
		-DLLDB_NO_INSTALL_DEFAULT_RPATH=On \
		-DLLDB_ENABLE_PYTHON=Off \
		-DLLDB_INCLUDE_TESTS=OFF \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_INSTALL_PREFIX=$(abspath $(TARGET_SYSROOT)) \
		-DCMAKE_MACOSX_RPATH=On \
		-DCMAKE_VERBOSE_MAKEFILE:BOOL=TRUE \
		-DDEFAULT_SYSROOT=.. \
		-DMIG_ARCHS=$(SP_ARCH_APPLE)
	cmake --build build/target-llvm
	cmake --install build/target-llvm

$(TARGET_SYSROOT)/include/vulkan/vulkan.h: $(TARGET_TOOLCHAIN_FILE)
	rm -rf build/target-vulkan-headers
	@echo Build Vulkan Headers $@
	cmake \
		-DCMAKE_TOOLCHAIN_FILE=$(realpath $(TARGET_TOOLCHAIN_FILE)) \
		-G Ninja -S $(VULKAN_HEADERS_DIR) -B build/target-vulkan-headers \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_INSTALL_PREFIX=$(abspath $(TARGET_SYSROOT))
	cmake --build build/target-vulkan-headers
	cmake --install build/target-vulkan-headers
	touch $@

$(TARGET_SYSROOT)/include/spirv/unified1/spirv.h: $(TARGET_TOOLCHAIN_FILE)
	rm -rf build/target-spirv-headers
	@echo Build SPIR-V Headers $@
	cmake \
		-DCMAKE_TOOLCHAIN_FILE=$(realpath $(TARGET_TOOLCHAIN_FILE)) \
		-G Ninja -S $(SPIRV_HEADERS_DIR) -B build/target-spirv-headers \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_INSTALL_PREFIX=$(abspath $(TARGET_SYSROOT))
	cmake --build build/target-spirv-headers
	cmake --install build/target-spirv-headers
	touch $@

$(TARGET_SYSROOT)/bin/spirv-opt: $(TARGET_SYSROOT)/include/spirv/unified1/spirv.h
	rm -rf build/target-spirv-tools
	@echo Build SPIR-V tools $@
	cmake \
		-DCMAKE_TOOLCHAIN_FILE=$(realpath $(TARGET_TOOLCHAIN_FILE)) \
		-G Ninja -S $(SPIRV_TOOLS_DIR) -B build/target-spirv-tools \
		-DSPIRV-Headers_SOURCE_DIR=$(abspath $(TARGET_SYSROOT)) \
		-DSPIRV_TOOLS_BUILD_STATIC=Off \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_INSTALL_PREFIX=$(abspath $(TARGET_SYSROOT)) \
		-DCMAKE_C_FLAGS_INIT="-flto" \
		-DCMAKE_CXX_FLAGS_INIT="-flto" \
		-DCMAKE_C_FLAGS="-flto" \
		-DCMAKE_CXX_FLAGS="-flto" \
		-DCMAKE_EXE_LINKER_FLAGS="-flto" \
		-DCMAKE_SHARED_LINKER_FLAGS="-flto" \
		-DCMAKE_MACOSX_RPATH=On \
		-DCMAKE_VERBOSE_MAKEFILE:BOOL=TRUE \
		-DBUILD_SHARED_LIBS=On
	cmake --build build/target-spirv-tools
	cmake --install build/target-spirv-tools
	touch $@

$(TARGET_SYSROOT)/bin/glslang: $(TARGET_SYSROOT)/bin/spirv-opt $(TARGET_SYSROOT)/include/vulkan/vulkan.h
	rm -rf build/target-glslang
	@echo Build glslang compiler $@
	cmake \
		-DCMAKE_TOOLCHAIN_FILE=$(realpath $(TARGET_TOOLCHAIN_FILE)) \
		-G Ninja -S $(GLSLANG_DIR) -B build/target-glslang \
		-DGLSLANG_TESTS=Off \
		-DGLSLANG_ENABLE_INSTALL=Off \
		-DENABLE_HLSL=Off \
		-DENABLE_OPT=On \
		-DALLOW_EXTERNAL_SPIRV_TOOLS=On \
		-DGLSLANG_ENABLE_INSTALL=On \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_INSTALL_PREFIX=$(abspath $(TARGET_SYSROOT)) \
		-DCMAKE_C_FLAGS_INIT="-flto" \
		-DCMAKE_CXX_FLAGS_INIT="-flto" \
		-DCMAKE_C_FLAGS="-flto" \
		-DCMAKE_CXX_FLAGS="-flto" \
		-DCMAKE_EXE_LINKER_FLAGS="-flto" \
		-DCMAKE_SHARED_LINKER_FLAGS="-flto" \
		-DCMAKE_MACOSX_RPATH=On \
		-DCMAKE_VERBOSE_MAKEFILE:BOOL=TRUE
	cmake --build build/target-glslang
	cmake --install build/target-glslang
	touch $@

$(TARGET_SYSROOT)/bin/make: $(TARGET_TOOLCHAIN_FILE) $(MAKE_SRC_DIR)
	rm -rf build/target-make
	mkdir -p build/target-make
	cd build/target-make; $(abspath $(MAKE_SRC_DIR))/configure --prefix $(abspath $(TARGET_SYSROOT)) \
		--host=$(SP_ARCH_CLANG) \
		CC=$(abspath $(HOST_CC)) \
		CXX=$(abspath $(HOST_CXX)) \
		CPPFLAGS="-std=gnu11 -arch $(SP_ARCH_APPLE) --target=$(SP_ARCH_CLANG) --sysroot $(realpath $(TARGET_SYSROOT))" \
		CFLAGS=" -flto" \
		LDFLAGS=" -arch $(SP_ARCH_APPLE) --target=$(SP_ARCH_CLANG) --sysroot $(realpath $(TARGET_SYSROOT)) -flto"
	make -C build/target-make
	make -C build/target-make install

download: $(MAKE_SRC_DIR)

target: $(TARGET_CLANG_CC) \
	$(TARGET_SYSROOT)/bin/make \
	$(TARGET_SYSROOT)/include/vulkan/vulkan.h $(TARGET_SYSROOT)/include/spirv/unified1/spirv.h \
	$(TARGET_SYSROOT)/bin/spirv-opt $(TARGET_SYSROOT)/bin/glslang

OUT_SYSROOT ?= ../../hosts/$(SP_ARCH_CLANG)

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
	liblldb.21.1.8.dylib \
	libLLVM.dylib \
	libLTO.dylib \
	libRemarks.dylib \
	libSPIRV.16.0.0.dylib \
	libSPIRV-Tools-diff.dylib \
	libSPIRV-Tools.dylib \
	libSPIRV-Tools-link.dylib \
	libSPIRV-Tools-lint.dylib \
	libSPIRV-Tools-opt.dylib \
	libSPIRV-Tools-reduce.dylib \
	libSPIRV-Tools-shared.dylib \
)

OUT_INCLUDES := $(addprefix $(OUT_SYSROOT)/include/,\
	clang \
	clang-c \
	llvm \
	llvm-c\
	lldb)

$(OUT_SYSROOT)/lib/%: $(TARGET_SYSROOT)/lib/% $(TARGET_SYSROOT)/bin/glslang $(TARGET_SYSROOT)/bin/clang
	@mkdir -p $(OUT_SYSROOT)/lib
	@rm -f $@
	$(HOST_STRIP) $< -o $@
	$(if $(CODESIGN),$(CODESIGN) sign $@)
	chmod +x $@

$(OUT_SYSROOT)/lib/clang: $(TARGET_SYSROOT)/lib/clang $(TARGET_SYSROOT)/bin/clang
	@mkdir -p $(OUT_SYSROOT)/lib
	@rm -rf $@
	cp -rf $< $@

$(OUT_SYSROOT)/bin/%: $(TARGET_SYSROOT)/bin/% $(TARGET_SYSROOT)/bin/glslang $(TARGET_SYSROOT)/bin/make $(TARGET_SYSROOT)/bin/clang
	@mkdir -p $(OUT_SYSROOT)/bin
	@rm -f $@
	$(HOST_STRIP) $< -o $@
	$(HOST_NAMETOOL) -delete_all_rpaths $@
	$(HOST_NAMETOOL) -prepend_rpath @loader_path/../lib $@
	$(if $(CODESIGN),$(CODESIGN) sign $@)
	chmod +x $@

$(OUT_SYSROOT)/include/%: $(TARGET_SYSROOT)/include/% $(TARGET_SYSROOT)/bin/clang
	rm -rf $@
	mkdir -p $@
	cp -fr $</* $@

$(OUT_SYSROOT)/share/licenses: ../licenses
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
	cd $(OUT_SYSROOT)/lib; ln -fs liblldb.21.1.8.dylib liblldb.dylib
	cd $(OUT_SYSROOT)/lib; ln -fs libLLVM.dylib libLLVM-21.dylib
	cd $(OUT_SYSROOT)/lib; ln -fs libSPIRV.16.0.0.dylib libSPIRV.16.dylib
	cd $(OUT_SYSROOT)/lib; ln -fs libSPIRV.16.dylib libSPIRV.dylib

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

out: $(OUT_SYSROOT)/sysroot

all: host target

.PHONY: host target all 
