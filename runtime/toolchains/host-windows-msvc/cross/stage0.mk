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

STAGE0_SYSROOT := sysroot-windows/$(SP_ARCH_CLANG)
STAGE0_LIBCXX := $(STAGE0_SYSROOT)/bin/c++.dll
STAGE0_ZLIB := $(STAGE0_SYSROOT)/lib/zs.lib
STAGE0_LIBXML2 := $(STAGE0_SYSROOT)/lib/libxml2s.lib
STAGE0_CLANG_CC := $(STAGE0_SYSROOT)/bin/clang.exe
STAGE0_CLANG_CXX := $(STAGE0_SYSROOT)/bin/clang++.exe
STAGE0_MAKE_EXE := $(STAGE0_SYSROOT)/bin/make.exe
STAGE0_GLSLANG := $(STAGE0_SYSROOT)/bin/glslang.exe

PREBUILTS_PATH := $(HOST_ROOT)/bin

STAGE0_HOST_TOOLCHAIN_CMAKE := $(STAGE0_SYSROOT)/host.cmake
STAGE0_HOSTCXX_TOOLCHAIN_CMAKE := $(STAGE0_SYSROOT)/hostcxx.cmake

STAGE0_CC := $(PREBUILTS_PATH)/clang-cl
STAGE0_CXX := $(PREBUILTS_PATH)/clang-cl
STAGE0_RC := $(PREBUILTS_PATH)/llvm-rc
STAGE0_LLD := $(PREBUILTS_PATH)/lld-link
STAGE0_ML := $(PREBUILTS_PATH)/llvm-ml64

MSCV_RUNTIME := MultiThreaded

STAGE0_LIB_PATH :=

STAGE0_INCLUDE_PATH :=

STAGE0_WARN_FLAGS := \
	-Wno-nonportable-include-path \
	-Wno-language-extension-token \
	-Wno-ignored-attributes \
	-Wno-dollar-in-identifier-extension \
	-Wno-c23-extensions \
	-Wno-ignored-pragma-intrinsic \
	-Wno-pragma-pack \
	-Wno-deprecated-declarations \
	-Wno-microsoft-enum-value \
	-Wno-microsoft-anon-tag \
	-Wno-extra-semi \
	-Wno-unknown-pragmas \
	-Wno-strict-prototypes \
	-Wno-unused-local-typedef

STAGE0_CXX_WARN_FLAGS := \
	-Wno-cast-qual \
	-Wno-non-virtual-dtor

STAGE0_CFLAGS := $(OPT_FLAGS) \
	$(STAGE0_WARN_FLAGS) $(STAGE0_INCLUDE_PATH) -fuse-ld=lld
STAGE0_CXXFLAGS := $(OPT_FLAGS) \
	$(STAGE0_WARN_FLAGS) $(STAGE0_CXX_WARN_FLAGS) $(STAGE0_INCLUDE_PATH) -fuse-ld=lld
STAGE0_RCFLAGS := $(OPT_FLAGS) \
	$(STAGE0_WARN_FLAGS) $(STAGE0_INCLUDE_PATH) -fuse-ld=lld
STAGE0_EXE_LDFLAGS := $(STAGE0_LIB_PATH) -fuse-ld=lld
STAGE0_LIB_LDFLAGS := $(STAGE0_LIB_PATH) -fuse-ld=lld

STAGE0_LIBC_CFLAGS := $(OPT_FLAGS) \
	$(STAGE0_WARN_FLAGS) $(STAGE0_INCLUDE_PATH) -DLIBXML_STATIC \
	$(STAGE0_LIB_PATH)
STAGE0_LIBC_CXXFLAGS := $(OPT_FLAGS) \
	$(STAGE0_WARN_FLAGS) $(STAGE0_CXX_WARN_FLAGS) $(STAGE0_INCLUDE_PATH) -DLIBXML_STATIC \
	$(STAGE0_LIB_PATH)
STAGE0_LIBC_RCFLAGS := $(OPT_FLAGS) \
	$(STAGE0_WARN_FLAGS) $(STAGE0_INCLUDE_PATH) -DLIBXML_STATIC

STAGE0_LIBCXX_EXE_LDFLAGS := \
	bcrypt.lib

STAGE0_LIBCXX_LIB_LDFLAGS := \
	bcrypt.lib

export CC=$(PREBUILTS_PATH)/clang
export CXX=$(PREBUILTS_PATH)/clang++
export RC=$(STAGE0_RC)
export ASM_MASM=$(STAGE0_ML)

export INCLUDE := $(CRT_DIR)/include;$(SDK_DIR)/include/cppwinrt;$(SDK_DIR)/include/shared;$(SDK_DIR)/include/ucrt;$(SDK_DIR)/include/um;$(SDK_DIR)/include/winrt
export LIB := $(abspath $(STAGE0_SYSROOT))/lib;$(CRT_DIR)/lib/$(SP_ARCH_WINDOWS);$(SDK_DIR)/lib/ucrt/$(SP_ARCH_WINDOWS);$(SDK_DIR)/lib/um/$(SP_ARCH_WINDOWS)

$(info INCLUDE: $(INCLUDE))
$(info LIB: $(LIB))

$(STAGE0_HOST_TOOLCHAIN_CMAKE):
	@mkdir -p $(dir $@)
	@echo 'set(CMAKE_SYSTEM_NAME Windows)' > $@
	@echo 'set(CMAKE_SYSROOT $(realpath $(STAGE0_SYSROOT)))'>> $@
	@echo 'set(CMAKE_C_COMPILER_TARGET $(SP_ARCH_CLANG))'>> $@
	@echo 'set(CMAKE_CXX_COMPILER_TARGET $(SP_ARCH_CLANG))'>> $@
	@echo 'set(CMAKE_C_FLAGS_INIT "$(STAGE0_CFLAGS)")'>> $@
	@echo 'set(CMAKE_CXX_FLAGS_INIT "$(STAGE0_CXXFLAGS)")'>> $@
	@echo 'set(CMAKE_RC_FLAGS_INIT "$(STAGE0_RCFLAGS)")'>> $@
	@echo 'set(CMAKE_C_COMPILER "$(STAGE0_CC)")'>> $@
	@echo 'set(CMAKE_RC_COMPILER "$(STAGE0_RC)")'>> $@
	@echo 'set(CMAKE_CXX_COMPILER "$(STAGE0_CXX)")'>> $@
	@echo 'set(CMAKE_EXE_LINKER_FLAGS_INIT "$(STAGE0_EXE_LDFLAGS)")'>> $@
	@echo 'set(CMAKE_SHARED_LINKER_FLAGS_INIT "$(STAGE0_LIB_LDFLAGS)")'>> $@
	@echo 'set(CMAKE_VERBOSE_MAKEFILE ON)'>> $@
	@echo 'set(BUILD_SHARED_LIBS OFF)'>> $@
	@echo 'set(CMAKE_LINKER_TYPE LLD)'>> $@
	@echo 'set(CMAKE_LINKER "$(STAGE0_LLD)")'>> $@
	@echo 'set(CMAKE_ASM_MASM_COMPILER "$(STAGE0_ML)")'>> $@

$(STAGE0_HOSTCXX_TOOLCHAIN_CMAKE):
	@mkdir -p $(dir $@)
	@echo 'set(CMAKE_SYSTEM_NAME Windows)' > $@
	@echo 'set(CMAKE_SYSROOT $(realpath $(STAGE0_SYSROOT)))'>> $@
	@echo 'set(CMAKE_C_COMPILER_TARGET $(SP_ARCH_CLANG))'>> $@
	@echo 'set(CMAKE_CXX_COMPILER_TARGET $(SP_ARCH_CLANG))'>> $@
	@echo 'set(CMAKE_ASM_COMPILER_TARGET $(SP_ARCH_CLANG))'>> $@
	@echo 'set(CMAKE_C_FLAGS_INIT "$(STAGE0_LIBC_CFLAGS)")'>> $@
	@echo 'set(CMAKE_CXX_FLAGS_INIT "$(STAGE0_LIBC_CXXFLAGS)")'>> $@
	@echo 'set(CMAKE_RC_FLAGS_INIT "$(STAGE0_LIBC_RCFLAGS)")'>> $@
	@echo 'set(CMAKE_ASM_FLAGS_INIT "$(STAGE0_LIBC_CFLAGS)")' >> $@
	@echo 'set(CMAKE_C_COMPILER "$(abspath $(STAGE0_CC))")'>> $@
	@echo 'set(CMAKE_CXX_COMPILER "$(abspath $(STAGE0_CXX))")'>> $@
	@echo 'set(CMAKE_RC_COMPILER "$(abspath $(STAGE0_RC))")'>> $@
	@echo 'set(CMAKE_ASM_COMPILER "$(abspath $(STAGE0_CC))")' >> $@
	@echo 'set(CMAKE_EXE_LINKER_FLAGS_INIT "$(STAGE0_LIBCXX_EXE_LDFLAGS)")'>> $@
	@echo 'set(CMAKE_SHARED_LINKER_FLAGS_INIT "$(STAGE0_LIBCXX_LIB_LDFLAGS)")'>> $@
	@echo 'set(CMAKE_EXE_LINKER_FLAGS "$(STAGE0_LIBCXX_EXE_LDFLAGS)")'>> $@
	@echo 'set(CMAKE_SHARED_LINKER_FLAGS "$(STAGE0_LIBCXX_LIB_LDFLAGS)")'>> $@
	@echo 'set(CMAKE_VERBOSE_MAKEFILE ON)'>> $@
	@echo 'set(CMAKE_LINKER_TYPE LLD)'>> $@
	@echo 'set(CMAKE_LINKER "$(STAGE0_LLD)")'>> $@
	@echo 'set(CMAKE_ASM_MASM_COMPILER "$(STAGE0_ML)")'>> $@


STAGE0_BUILD_ZLIB := cmake -G "Ninja" \
	-DCMAKE_TOOLCHAIN_FILE=$(abspath $(STAGE0_HOST_TOOLCHAIN_CMAKE)) \
	-G "Ninja" \
	-S $(ZLIB_DIR) -B build/zlib \
	-DZLIB_BUILD_EXAMPLES=OFF \
	-DZLIB_BUILD_TESTING=Off \
	-DZLIB_BUILD_EXAMPLES=Off \
	-DZLIB_BUILD_SHARED=Off \
	-DZLIB_BUILD_STATIC=On \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_MSVC_RUNTIME_LIBRARY=$(MSCV_RUNTIME) \
	-DINSTALL_BIN_DIR=$(abspath $(STAGE0_SYSROOT))/bin \
	-DINSTALL_MAN_DIR=$(abspath $(STAGE0_SYSROOT))/share/man \
	-DINSTALL_PKGCONFIG_DIR=$(abspath $(STAGE0_SYSROOT))/lib/pkgconfig \
	-DCMAKE_INSTALL_PREFIX=$(abspath $(STAGE0_SYSROOT))

$(STAGE0_ZLIB): $(ZLIB_DIR) $(STAGE0_HOST_TOOLCHAIN_CMAKE)
	@echo "Build STAGE0_ZLIB $(STAGE0_ZLIB)"
	$(call rule_rm,build/zlib)
	$(STAGE0_BUILD_ZLIB)
	cmake --build build/zlib
	cmake --install build/zlib


#
# libxml
#

STAGE0_BUILD_LIBXML2 := cmake -G "Ninja" \
	-DCMAKE_TOOLCHAIN_FILE=$(abspath $(STAGE0_HOST_TOOLCHAIN_CMAKE)) \
	-G "Ninja" \
	-S $(LIBXML2_DIR) -B build/libxml2 \
	-DBUILD_SHARED_LIBS=Off \
	-DLIBXML2_WITH_DEBUG=Off \
	-DLIBXML2_WITH_PROGRAMS=Off \
	-DLIBXML2_WITH_TESTS=Off \
	-DLIBXML2_WITH_TLS=On \
	-DLIBXML2_WITH_ICONV=Off \
	-DCMAKE_MSVC_RUNTIME_LIBRARY=$(MSCV_RUNTIME) \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=$(abspath $(STAGE0_SYSROOT))

$(STAGE0_LIBXML2): $(ZLIB_DIR) $(STAGE0_HOST_TOOLCHAIN_CMAKE)
	@echo "Build STAGE0_LIBXML2 $(STAGE0_LIBXML2)"
	$(call rule_rm,build/libxml2)
	$(STAGE0_BUILD_LIBXML2)
	cmake --build build/libxml2
	cmake --install build/libxml2

#
# libcxx
#

STAGE0_BUILD_LIBCXX := cmake \
	-DCMAKE_TOOLCHAIN_FILE=$(abspath $(STAGE0_HOST_TOOLCHAIN_CMAKE)) \
	-G "Ninja" \
	-S $(LLVM_DIR)/runtimes -B build/libcxx_runtime \
	-DCMAKE_BUILD_TYPE=Release \
	-DLLVM_ENABLE_RUNTIMES="libcxx" \
	-DLLVM_TARGETS_TO_BUILD=X86 \
	-DLIBCXX_ENABLE_STATIC=Off \
	-DLIBCXX_ENABLE_SHARED=On \
	-DLIBCXX_INCLUDE_TESTS=Off \
	-DCMAKE_MSVC_RUNTIME_LIBRARY=$(MSCV_RUNTIME) \
	-DLIBXML2_LIBRARY=$(abspath $(STAGE0_LIBXML2)) \
	-DCMAKE_INSTALL_PREFIX=$(abspath $(STAGE0_SYSROOT))

$(STAGE0_LIBCXX): $(STAGE0_HOST_TOOLCHAIN_CMAKE) $(STAGE0_ZLIB) $(STAGE0_LIBXML2)
	@echo "Build STAGE0_LIBCXX $(STAGE0_LIBCXX)"
	$(call rule_rm,build/libcxx_runtime)
	$(STAGE0_BUILD_LIBCXX)
	cmake --build build/libcxx_runtime
	cmake --install build/libcxx_runtime


#
# clang,lldb,lld
#

ifneq ($(SP_ARCH_HOST),aarch64)
STAGE0_BUILD_CC_LTO := -DLLVM_ENABLE_LTO=Full
endif

STAGE0_BUILD_CC := cmake \
	-DCMAKE_TOOLCHAIN_FILE=$(abspath $(STAGE0_HOSTCXX_TOOLCHAIN_CMAKE)) \
	-G "Ninja" -S $(LLVM_DIR)/llvm -B build/llvm_stage0 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_LINKER_TYPE=LLD \
	-DZLIB_LIBRARY=$(abspath $(STAGE0_ZLIB)) \
	-DLIBXML2_LIBRARY=$(abspath $(STAGE0_LIBXML2)) \
	-DLLVM_ENABLE_ZLIB=FORCE_ON \
	-DLLVM_ENABLE_LIBXML2=FORCE_ON \
	-DLLVM_ENABLE_PROJECTS="clang;lld;lldb" \
	-DLLVM_ENABLE_RUNTIMES="compiler-rt" \
	-DLLVM_FORCE_BUILD_RUNTIME=ON \
	-DLLVM_TARGETS_TO_BUILD="X86;ARM;AArch64;RISCV;WebAssembly" \
	-DLLVM_INSTALL_TOOLCHAIN_ONLY=On \
	-DLLDB_INCLUDE_TESTS=Off \
	-DLLVM_ENABLE_SPHINX=Off \
	-DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=Off \
	-DLLVM_TARGET_TRIPLE=$(SP_ARCH_CLANG) \
	-DLLVM_ENABLE_EH=On \
	-DLLVM_ENABLE_RTTI=On \
	$(STAGE0_BUILD_CC_LTO) \
	-DLLVM_BUILD_BENCHMARKS=Off \
	-DLLVM_INCLUDE_BENCHMARKS=Off \
	-DCROSS_TOOLCHAIN_FLAGS_NATIVE="-DCMAKE_C_COMPILER=/usr/bin/clang;-DCMAKE_CXX_COMPILER=/usr/bin/clang++;-DCMAKE_CXX_STANDARD=20" \
	-DLIBCXX_ENABLE_STATIC=Off \
	-DLIBCXX_ENABLE_SHARED=On \
	-DCLANG_DEFAULT_CXX_STDLIB=libc++ \
	-DCLANG_DEFAULT_RTLIB=compiler-rt \
	-DCLANG_DEFAULT_LINKER=lld \
	-DCOMPILER_RT_BUILD_BUILTINS=On \
	-DCOMPILER_RT_DEFAULT_TARGET_ONLY=ON \
	-DCOMPILER_RT_BUILD_GWP_ASAN=OFF \
	-DCOMPILER_RT_BUILD_SANITIZERS=OFF \
	-DCOMPILER_RT_BUILD_XRAY=OFF \
	-DCOMPILER_RT_BUILD_MEMPROF=OFF \
	-DCOMPILER_RT_BUILD_CTX_PROFILE=OFF \
	-DCOMPILER_RT_BUILD_LIBFUZZER=OFF \
	-DBUILTINS_CMAKE_TOOLCHAIN_FILE=$(abspath $(STAGE0_HOSTCXX_TOOLCHAIN_CMAKE)) \
	-DRUNTIMES_CMAKE_TOOLCHAIN_FILE=$(abspath $(STAGE0_HOSTCXX_TOOLCHAIN_CMAKE)) \
	-DBUILTINS_$(SP_ARCH_CLANG)_CMAKE_TOOLCHAIN_FILE=$(abspath $(STAGE0_HOSTCXX_TOOLCHAIN_CMAKE)) \
	-DRUNTIMES_$(SP_ARCH_CLANG)_CMAKE_TOOLCHAIN_FILE=$(abspath $(STAGE0_HOSTCXX_TOOLCHAIN_CMAKE)) \
	-DBUILTINS_CMAKE_ARGS="-DCMAKE_TOOLCHAIN_FILE=$(abspath $(STAGE0_HOSTCXX_TOOLCHAIN_CMAKE))" \
	-DRUNTIMES_CMAKE_ARGS="-DCMAKE_TOOLCHAIN_FILE=$(abspath $(STAGE0_HOSTCXX_TOOLCHAIN_CMAKE))" \
	-DCMAKE_MSVC_RUNTIME_LIBRARY=$(MSCV_RUNTIME) \
	-DLLDB_ENABLE_PYTHON=Off \
	-DCMAKE_POLICY_DEFAULT_CMP0091=NEW \
	-DCMAKE_INSTALL_PREFIX=$(abspath $(STAGE0_SYSROOT))

$(STAGE0_CLANG_CC): $(STAGE0_HOSTCXX_TOOLCHAIN_CMAKE)
	@echo "Build STAGE0_CLANG_CC $(STAGE0_CLANG_CC)"
	$(call rule_rm,build/llvm_stage0)
	$(STAGE0_BUILD_CC)
	cmake --build build/llvm_stage0 --parallel
	cmake --install build/llvm_stage0
	touch $(STAGE0_CLANG_CC)

#
# Vulkan/SPIR-V
#

STAGE0_VULKAN_HEADERS_CONF := cmake \
	-DCMAKE_TOOLCHAIN_FILE=$(abspath $(STAGE0_HOSTCXX_TOOLCHAIN_CMAKE)) \
	-G "Ninja" \
	-S $(VULKAN_HEADERS_DIR) -B build/stage0-vulkan-headers \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_MSVC_RUNTIME_LIBRARY=$(MSCV_RUNTIME) \
	-DCMAKE_INSTALL_PREFIX=$(abspath $(STAGE0_SYSROOT))

$(STAGE0_SYSROOT)/include/vulkan/vulkan.h: $(STAGE0_CLANG_CC)
	@echo "Build Vulkan Headers $@"
	$(call rule_rm,build/stage0-vulkan-headers)
	$(STAGE0_VULKAN_HEADERS_CONF)
	cmake --build build/stage0-vulkan-headers
	cmake --install build/stage0-vulkan-headers
	$(call rule_touch,$@)

STAGE0_SPIRV_HEADERS_CONF := cmake \
	-DCMAKE_TOOLCHAIN_FILE=$(abspath $(STAGE0_HOSTCXX_TOOLCHAIN_CMAKE)) \
	-G "Ninja" \
	-S $(SPIRV_HEADERS_DIR) -B build/stage0-spirv-headers \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_C_COMPILER_WORKS=1 \
	-DCMAKE_CXX_COMPILER_WORKS=1 \
	-DCMAKE_MSVC_RUNTIME_LIBRARY=$(MSCV_RUNTIME) \
	-DCMAKE_INSTALL_PREFIX=$(abspath $(STAGE0_SYSROOT))

$(STAGE0_SYSROOT)/include/spirv/unified1/spirv.h: $(STAGE0_CLANG_CC)
	@echo "Build SPIR_V Headers $@"
	$(call rule_rm,build/stage0-spirv-headers)
	$(STAGE0_SPIRV_HEADERS_CONF)
	cmake --build build/stage0-spirv-headers
	cmake --install build/stage0-spirv-headers
	$(call rule_touch,$@)

STAGE0_SPIRV_CONF := cmake \
	-DCMAKE_TOOLCHAIN_FILE=$(abspath $(STAGE0_HOSTCXX_TOOLCHAIN_CMAKE)) \
	-G "Ninja" \
	-S $(SPIRV_TOOLS_DIR) -B build/stage0-spirv-tools \
	-DCMAKE_MSVC_RUNTIME_LIBRARY=$(MSCV_RUNTIME) \
	-DSPIRV-Headers_SOURCE_DIR=$(abspath $(STAGE0_SYSROOT)) \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=$(abspath $(STAGE0_SYSROOT)) \
	-DSPIRV_TOOLS_BUILD_STATIC=On \
	-DSPIRV_TOOLS_LIBRARY_TYPE=STATIC \
	-DCMAKE_C_FLAGS_INIT="-flto" \
	-DCMAKE_CXX_FLAGS_INIT="-flto" \
	-DCMAKE_C_FLAGS="-flto" \
	-DCMAKE_CXX_FLAGS="-flto" \
	-DCMAKE_EXE_LINKER_FLAGS="-flto" \
	-DCMAKE_SHARED_LINKER_FLAGS="-flto"

$(STAGE0_SYSROOT)/bin/spirv-opt.exe: $(STAGE0_SYSROOT)/include/spirv/unified1/spirv.h
	@echo "Build SPIR-V tools $@"
	$(call rule_rm,build/stage0-spirv-tools)
	$(STAGE0_SPIRV_CONF)
	cmake --build build/stage0-spirv-tools
	cmake --install build/stage0-spirv-tools
	$(call rule_touch,$@)

STAGE0_GLSLANG_CONF := cmake \
	-DCMAKE_TOOLCHAIN_FILE=$(abspath $(STAGE0_HOSTCXX_TOOLCHAIN_CMAKE)) \
	-G "Ninja" \
	-S $(GLSLANG_DIR) -B build/stage0-glslang \
	-DCMAKE_MSVC_RUNTIME_LIBRARY=$(MSCV_RUNTIME) \
	-DSPIRV-Headers_SOURCE_DIR=$(abspath $(STAGE0_SYSROOT)) \
	-DSPIRV-SPIRV-Tools-opt_ROOT=$(abspath $(STAGE0_SYSROOT)) \
	-DSPIRV-SPIRV-Tools-opt_DIR=$(abspath $(STAGE0_SYSROOT)) \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=$(abspath $(STAGE0_SYSROOT)) \
	-DGLSLANG_TESTS=Off \
	-DENABLE_HLSL=Off \
	-DENABLE_OPT=On \
	-DALLOW_EXTERNAL_SPIRV_TOOLS=On \
	-DGLSLANG_ENABLE_INSTALL=On \
	-DCMAKE_C_FLAGS_INIT="-flto" \
	-DCMAKE_CXX_FLAGS_INIT="-flto" \
	-DCMAKE_C_FLAGS="-flto" \
	-DCMAKE_CXX_FLAGS="-flto" \
	-DCMAKE_EXE_LINKER_FLAGS="-flto" \
	-DCMAKE_SHARED_LINKER_FLAGS="-flto"

$(STAGE0_GLSLANG): $(STAGE0_SYSROOT)/bin/spirv-opt.exe $(STAGE0_SYSROOT)/include/vulkan/vulkan.h
	@echo "Build glslang compiler $@"
	$(call rule_rm,build/stage0-glslang)
	$(STAGE0_GLSLANG_CONF)
	cmake --build build/stage0-glslang
	cmake --install build/stage0-glslang
	$(call rule_touch,$@)

stage0: $(STAGE0_ZLIB) $(STAGE0_LIBXML2) $(STAGE0_CLANG_CC) \
	$(STAGE0_SYSROOT)/include/vulkan/vulkan.h \
	$(STAGE0_SYSROOT)/include/spirv/unified1/spirv.h \
	$(STAGE0_SYSROOT)/bin/spirv-opt.exe \
	$(STAGE0_GLSLANG)
