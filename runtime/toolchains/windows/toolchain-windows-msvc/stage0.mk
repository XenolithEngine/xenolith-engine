# Copyright (c) 2025 Stappler Team <admin@stappler.org>
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

STAGE0_SYSROOT := sysroot-windows
STAGE0_LIBCXX := $(STAGE0_SYSROOT)/lib/libc++.lib
STAGE0_LIBC := $(STAGE0_SYSROOT)/lib/libc.lib
STAGE0_LIBCRT := $(STAGE0_SYSROOT)/include/orc/c_api.h
STAGE0_CLANG_CC := $(STAGE0_SYSROOT)/bin/clang
STAGE0_CLANG_CXX := $(STAGE0_SYSROOT)/bin/clang++

STAGE0_HOST_TOOLCHAIN := $(realpath ../$(SP_ARCH)-unknown-linux-gnu)
STAGE0_XWIN_ROOT := $(realpath ../src/xwin/splat)

STAGE0_XWIN_SYSROOT := $(STAGE0_XWIN_ROOT)/crt
STAGE0_XWIN_SDK := $(STAGE0_XWIN_ROOT)/sdk

WARN_FLAGS := -Wno-ignored-attributes -Wno-deprecated-declarations -Wno-nonportable-include-path -Wno-pragma-pack \
	-Wno-ignored-pragma-intrinsic -Wno-microsoft-anon-tag -Wno-unknown-pragmas -Wno-extra-semi-stmt \
	-Wno-unused-parameter -Wno-strict-prototypes -Wno-cast-qual -Wno-unused-local-typedef \
	-Wno-language-extension-token -Wno-non-virtual-dtor -Wno-cast-qual \
	-Wno-deprecated-dynamic-exception-spec

OPT_FLAGS := -O3

STAGE0_HOST_TOOLCHAIN_CMAKE := host.cmake
STAGE0_HOSTCXX_TOOLCHAIN_CMAKE := hostcxx.cmake

STAGE0_CC := $(STAGE0_HOST_TOOLCHAIN)/bin/clang
STAGE0_CXX := $(STAGE0_HOST_TOOLCHAIN)/bin/clang++
STAGE0_RC := $(STAGE0_HOST_TOOLCHAIN)/bin/llvm-rc

STAGE0_LIB_PATH := \
	-L$(STAGE0_XWIN_SYSROOT)/lib/$(SP_ARCH) \
	-L$(STAGE0_XWIN_SDK)/lib/ucrt/$(SP_ARCH) \
	-L$(STAGE0_XWIN_SDK)/lib/um/$(SP_ARCH)

STAGE0_INCLUDE_PATH := \
	-I$(abspath replacements/include) \
	-I$(STAGE0_XWIN_SYSROOT)/include \
	-I$(STAGE0_XWIN_SDK)/include/ucrt \
	-I$(STAGE0_XWIN_SDK)/include/shared \
	-I$(STAGE0_XWIN_SDK)/include/um

STAGE0_CFLAGS := $(OPT_FLAGS) $(WARN_FLAGS) $(STAGE0_INCLUDE_PATH) -D__STDC__=0 -D_LIBCPP_NO_VCRUNTIME=1
STAGE0_CXXFLAGS := $(OPT_FLAGS) $(WARN_FLAGS) $(STAGE0_INCLUDE_PATH) -D__STDC__=0 -D_LIBCPP_NO_VCRUNTIME=1
STAGE0_EXE_LDFLAGS := $(STAGE0_LIB_PATH)
STAGE0_LIB_LDFLAGS := $(STAGE0_LIB_PATH)

STAGE0_LIBC_CFLAGS := --sysroot=$(abspath $(STAGE0_SYSROOT)) \
	$(OPT_FLAGS) $(WARN_FLAGS) \
	-I$(abspath $(STAGE0_SYSROOT)/include/c++/v1) $(STAGE0_INCLUDE_PATH) \
	-D__STDC__=0 -D_LIBCPP_NO_VCRUNTIME=1

STAGE0_LIBC_CXXFLAGS := --sysroot=$(abspath $(STAGE0_SYSROOT)) \
	$(OPT_FLAGS) $(WARN_FLAGS) \
	-I$(abspath $(STAGE0_SYSROOT)/include/c++/v1) $(STAGE0_INCLUDE_PATH) \
	-D__STDC__=0 -D_LIBCPP_NO_VCRUNTIME=1


STAGE0_LIBCXX_EXE_LDFLAGS := \
	-L$(abspath $(STAGE0_SYSROOT)/lib) \
	$(STAGE0_LIB_PATH)
STAGE0_LIBCXX_LIB_LDFLAGS := \
	-L$(abspath $(STAGE0_SYSROOT)/lib) \
	$(STAGE0_LIB_PATH)

$(STAGE0_HOST_TOOLCHAIN_CMAKE): $(lastword $(MAKEFILE_LIST))
	@echo "set(CMAKE_SYSTEM_NAME Windows)" > $@
	@echo "set(CMAKE_SYSROOT $(realpath $(STAGE0_XWIN_SYSROOT)))" >> $@
	@echo "set(CMAKE_C_COMPILER_TARGET $(SP_ARCH_CLANG))" >> $@
	@echo "set(CMAKE_CXX_COMPILER_TARGET $(SP_ARCH_CLANG))" >> $@
	@echo "set(CMAKE_C_FLAGS_INIT \"$(STAGE0_CFLAGS) -include malloc.h\")" >> $@
	@echo "set(CMAKE_CXX_FLAGS_INIT \"$(STAGE0_CXXFLAGS) -include malloc.h\")" >> $@
	@echo "set(CMAKE_C_COMPILER $(abspath $(STAGE0_CC)))" >> $@
	@echo "set(CMAKE_CXX_COMPILER $(abspath $(STAGE0_CXX)))" >> $@
	@echo "set(CMAKE_RC_COMPILER $(abspath $(STAGE0_RC)))" >> $@
	@echo "set(CMAKE_EXE_LINKER_FLAGS_INIT \"$(STAGE0_EXE_LDFLAGS)\")" >> $@
	@echo "set(CMAKE_SHARED_LINKER_FLAGS_INIT \"$(STAGE0_LIB_LDFLAGS)\")" >> $@
	@echo "set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)" >> $@
	@echo "set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)" >> $@
	@echo "set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)" >> $@
	@echo "set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)" >> $@
	@echo "set(CMAKE_VERBOSE_MAKEFILE ON)" >> $@
	@echo "set(BUILD_SHARED_LIBS OFF)" >> $@
	@echo "set(CMAKE_LINKER_TYPE LLD)" >> $@


$(STAGE0_HOSTCXX_TOOLCHAIN_CMAKE): $(lastword $(MAKEFILE_LIST))
	@echo "set(CMAKE_SYSTEM_NAME Windows)" > $@
	@echo "set(CMAKE_SYSROOT $(realpath $(STAGE0_XWIN_SYSROOT)))" >> $@
	@echo "set(CMAKE_C_COMPILER_TARGET $(SP_ARCH_CLANG))" >> $@
	@echo "set(CMAKE_CXX_COMPILER_TARGET $(SP_ARCH_CLANG))" >> $@
	@echo "set(CMAKE_C_FLAGS_INIT \"$(STAGE0_LIBC_CFLAGS)\")" >> $@
	@echo "set(CMAKE_CXX_FLAGS_INIT \"$(STAGE0_LIBC_CXXFLAGS) -stdlib=libc++\")" >> $@
	@echo "set(CMAKE_C_COMPILER $(abspath $(STAGE0_CC)))" >> $@
	@echo "set(CMAKE_CXX_COMPILER $(abspath $(STAGE0_CXX)))" >> $@
	@echo "set(CMAKE_RC_COMPILER $(abspath $(STAGE0_RC)))" >> $@
	@echo "set(CMAKE_EXE_LINKER_FLAGS_INIT \"$(STAGE0_LIBCXX_EXE_LDFLAGS)\")" >> $@
	@echo "set(CMAKE_SHARED_LINKER_FLAGS_INIT \"$(STAGE0_LIBCXX_LIB_LDFLAGS)\")" >> $@
	@echo "set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)" >> $@
	@echo "set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)" >> $@
	@echo "set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)" >> $@
	@echo "set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)" >> $@
	@echo "set(CMAKE_VERBOSE_MAKEFILE ON)" >> $@
	@echo "set(BUILD_SHARED_LIBS OFF)" >> $@
	@echo "set(CMAKE_LINKER_TYPE LLD)" >> $@


$(STAGE0_LIBCXX):
	@echo "Build STAGE0_LIBCXX $(STAGE0_LIBCXX)"
	rm -rf build/libcxx_runtime
	cmake \
		-DCMAKE_TOOLCHAIN_FILE=$(realpath $(STAGE0_HOST_TOOLCHAIN_CMAKE)) \
		-S $(LLVM_VER)/runtimes -B build/libcxx_runtime \
		-DCMAKE_BUILD_TYPE=Release \
		-DBUILD_SHARED_LIBS:BOOL=OFF \
		-DCMAKE_VERBOSE_MAKEFILE:BOOL=ON \
		-DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi;libunwind;" \
		-DLLVM_TARGETS_TO_BUILD=X86 \
		-DLIBCXXABI_USE_LLVM_UNWINDER=On \
		-DLIBCXX_ENABLE_SHARED=OFF \
		-DLIBCXXABI_ENABLE_SHARED=OFF \
		-DLIBUNWIND_ENABLE_SHARED=OFF \
		-DCMAKE_INSTALL_PREFIX=$(abspath $(STAGE0_SYSROOT))
	cmake --build build/libcxx_runtime
	cmake --install build/libcxx_runtime

$(STAGE0_LIBC): $(STAGE0_LIBCXX)
	@echo "Build STAGE0_LIBC $(STAGE0_LIBC)"
	rm -rf build/libc_runtime
	cmake \
		-DCMAKE_TOOLCHAIN_FILE=$(realpath $(STAGE0_HOSTCXX_TOOLCHAIN_CMAKE)) \
		-S $(LLVM_VER)/runtimes -B build/libc_runtime \
		-DCMAKE_BUILD_TYPE=Release \
		-DBUILD_SHARED_LIBS:BOOL=OFF \
		-DCMAKE_VERBOSE_MAKEFILE:BOOL=ON \
		-DLLVM_ENABLE_RUNTIMES="libc" \
		-DLLVM_TARGETS_TO_BUILD=X86 \
		-DLIBC_TARGET_TRIPLE=$(SP_ARCH_CLANG) \
		-DLIBC_ENABLE_USE_BY_CLANG=On \
		-DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=Off \
		-DLIBC_ENABLE_UNITTESTS=Off \
		-DLIBC_WNO_ERROR=ON \
		-DLLVM_LIBC_FULL_BUILD=On \
		-DCMAKE_INSTALL_PREFIX=$(abspath $(STAGE0_SYSROOT))
	cmake --build build/libc_runtime
	cmake --install build/libc_runtime

$(STAGE0_LIBCRT): $(LLVM_VER) $(STAGE0_LIBCXX)
	@echo "Build STAGE0_LIBCRT $(STAGE0_LIBCRT)"
	rm -rf build/libcxx_crt
	cmake \
		-DCMAKE_TOOLCHAIN_FILE=$(realpath $(STAGE0_HOSTCXX_TOOLCHAIN_CMAKE)) \
		-S $(LLVM_VER)/runtimes -B build/libcxx_crt \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_VERBOSE_MAKEFILE:BOOL=ON \
		-DBUILD_SHARED_LIBS:BOOL=OFF \
		-DLLVM_TARGETS_TO_BUILD=X86 \
		-DLLVM_ENABLE_RUNTIMES="compiler-rt;" \
		-DCOMPILER_RT_BUILD_BUILTINS=On \
		-DCOMPILER_RT_BUILD_GWP_ASAN=OFF \
		-DCOMPILER_RT_BUILD_SANITIZERS=OFF \
		-DCOMPILER_RT_BUILD_XRAY=OFF \
		-DCOMPILER_RT_BUILD_MEMPROF=OFF \
		-DCOMPILER_RT_BUILD_CTX_PROFILE=OFF \
		-DCOMPILER_RT_BUILD_LIBFUZZER=OFF \
		-DCOMPILER_RT_SCUDO_STANDALONE_SYSROOT_PATH=$(realpath $(STAGE0_SYSROOT)) \
		-DCMAKE_INSTALL_PREFIX=$(realpath $(STAGE0_SYSROOT))
	cmake --build build/libcxx_crt
	cmake --install build/libcxx_crt
	touch $(STAGE0_SYSROOT)/include/orc/c_api.h

$(STAGE0_CLANG_CC): $(LLVM_VER) $(STAGE0_LIBCRT)
	@echo "Build STAGE0_CLANG_CC $(STAGE0_CLANG_CC)"
	rm -rf build/llvm_stage0
	cmake \
		-DCMAKE_TOOLCHAIN_FILE=$(realpath $(STAGE0_HOSTCXX_TOOLCHAIN_CMAKE)) \
		-S $(LLVM_VER)/llvm -B build/llvm_stage0 \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_VERBOSE_MAKEFILE:BOOL=ON \
		-DDEFAULT_SYSROOT=$(realpath $(STAGE0_SYSROOT)) \
		-DLLVM_ENABLE_PROJECTS="clang;compiler-rt;lld" \
		-DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi;libunwind" \
 		-DLLVM_FORCE_BUILD_RUNTIME=ON \
		-DLLVM_TARGETS_TO_BUILD=X86 \
		-DLLVM_INSTALL_TOOLCHAIN_ONLY=On \
		-DLLVM_TARGET_TRIPLE=$(SP_ARCH_CLANG) \
		-DCLANG_DEFAULT_CXX_STDLIB=libc++ \
		-DCLANG_DEFAULT_RTLIB=compiler-rt \
		-DCLANG_DEFAULT_LINKER=lld \
		-DCLANG_DEFAULT_UNWINDLIB=libunwind \
		-DLIBCXXABI_USE_LLVM_UNWINDER=On \
		-DCMAKE_INSTALL_PREFIX=$(STAGE0_SYSROOT) \
		-DCMAKE_POLICY_DEFAULT_CMP0091=NEW
	cmake --build build/llvm_stage0 --parallel
	cmake --install build/llvm_stage0

stage0: $(STAGE0_HOST_TOOLCHAIN_CMAKE) $(STAGE0_HOSTCXX_TOOLCHAIN_CMAKE) $(STAGE0_LIBCXX) $(STAGE0_LIBCRT) $(STAGE0_CLANG_CC)