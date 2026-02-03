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

STAGE0_SYSROOT := sysroot-windows
STAGE0_LIBCXX := $(STAGE0_SYSROOT)/lib/libc++.lib
STAGE0_CLANG_CC := $(STAGE0_SYSROOT)/bin/clang.exe
STAGE0_CLANG_CXX := $(STAGE0_SYSROOT)/bin/clang++.exe
STAGE0_MAKE_EXE := $(STAGE0_SYSROOT)/bin/make.exe

STAGE0_HOST_TOOLCHAIN := $(realpath ../$(SP_ARCH)-unknown-linux-gnu)
STAGE0_XWIN_ROOT := $(realpath ../src/xwin/splat)

STAGE0_XWIN_SYSROOT := $(STAGE0_XWIN_ROOT)/crt
STAGE0_XWIN_SDK := $(STAGE0_XWIN_ROOT)/sdk

WARN_FLAGS :=
OPT_FLAGS :=

STAGE0_HOST_TOOLCHAIN_CMAKE := host.cmake
STAGE0_HOSTCXX_TOOLCHAIN_CMAKE := hostcxx.cmake

# Use precompiled  binaries for bootstrapping
STAGE0_CC := C:/LLVM/bin/clang-cl.exe
STAGE0_CXX := C:/LLVM/bin/clang-cl.exe
STAGE0_RC := C:/LLVM/bin/llvm-rc.exe

export MSYS_NO_PATHCONV=1
export CC=C:/LLVM/bin/clang-cl.exe
export CXX=C:/LLVM/bin/clang-cl.exe

STAGE0_LIB_PATH :=
STAGE0_INCLUDE_PATH :=

STAGE0_CFLAGS := $(OPT_FLAGS) $(WARN_FLAGS) $(STAGE0_INCLUDE_PATH)
STAGE0_CXXFLAGS := $(OPT_FLAGS) $(WARN_FLAGS) $(STAGE0_INCLUDE_PATH)
STAGE0_EXE_LDFLAGS := $(STAGE0_LIB_PATH)
STAGE0_LIB_LDFLAGS := $(STAGE0_LIB_PATH)

STAGE0_LIBC_CFLAGS := $(OPT_FLAGS) $(WARN_FLAGS) $(STAGE0_INCLUDE_PATH)
STAGE0_LIBCXX_CFLAGS := $(OPT_FLAGS) $(WARN_FLAGS) $(STAGE0_INCLUDE_PATH)

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
	@echo "set(CMAKE_C_FLAGS_INIT \"$(STAGE0_CFLAGS)\")" >> $@
	@echo "set(CMAKE_CXX_FLAGS_INIT \"$(STAGE0_CXXFLAGS)\")" >> $@
	@echo "set(CMAKE_C_COMPILER \"$(STAGE0_CC)\")" >> $@
	@echo "set(CMAKE_CXX_COMPILER \"$(STAGE0_CXX)\")" >> $@
	@echo "set(CMAKE_EXE_LINKER_FLAGS_INIT \"$(STAGE0_EXE_LDFLAGS)\")" >> $@
	@echo "set(CMAKE_SHARED_LINKER_FLAGS_INIT \"$(STAGE0_LIB_LDFLAGS)\")" >> $@
	@echo "set(CMAKE_VERBOSE_MAKEFILE ON)" >> $@
	@echo "set(BUILD_SHARED_LIBS OFF)" >> $@
	@echo "set(CMAKE_LINKER_TYPE LLD)" >> $@

$(STAGE0_HOSTCXX_TOOLCHAIN_CMAKE): $(lastword $(MAKEFILE_LIST))
	@echo "set(CMAKE_SYSTEM_NAME Windows)" > $@
	@echo "set(CMAKE_SYSROOT $(realpath $(STAGE0_XWIN_SYSROOT)))" >> $@
	@echo "set(CMAKE_C_COMPILER_TARGET $(SP_ARCH_CLANG))" >> $@
	@echo "set(CMAKE_CXX_COMPILER_TARGET $(SP_ARCH_CLANG))" >> $@
	@echo "set(CMAKE_C_FLAGS_INIT \"$(STAGE0_LIBC_CFLAGS)\")" >> $@
	@echo "set(CMAKE_CXX_FLAGS_INIT \"$(STAGE0_LIBC_CXXFLAGS)\")" >> $@
	@echo "set(CMAKE_C_COMPILER \"$(abspath $(STAGE0_CC))\")" >> $@
	@echo "set(CMAKE_CXX_COMPILER \"$(abspath $(STAGE0_CXX))\")" >> $@
	@echo "set(CMAKE_RC_COMPILER \"$(abspath $(STAGE0_RC))\")" >> $@
	@echo "set(CMAKE_EXE_LINKER_FLAGS_INIT \"$(STAGE0_LIBCXX_EXE_LDFLAGS)\")" >> $@
	@echo "set(CMAKE_SHARED_LINKER_FLAGS_INIT \"$(STAGE0_LIBCXX_LIB_LDFLAGS)\")" >> $@
	@echo "set(CMAKE_VERBOSE_MAKEFILE ON)" >> $@
	@echo "set(CMAKE_LINKER_TYPE LLD)" >> $@

$(STAGE0_LIBCXX):
	@echo "Build STAGE0_LIBCXX $(STAGE0_LIBCXX)"
	rm -rf build/libcxx_runtime
	cmake \
		-DCMAKE_TOOLCHAIN_FILE=$(realpath $(STAGE0_HOST_TOOLCHAIN_CMAKE)) \
		-G "Ninja" -S $(LLVM_VER)/runtimes -B build/libcxx_runtime \
		-DCMAKE_BUILD_TYPE=Release \
		-DLLVM_ENABLE_RUNTIMES="libcxx" \
		-DLLVM_TARGETS_TO_BUILD=X86 \
		-DCMAKE_INSTALL_PREFIX=$(abspath $(STAGE0_SYSROOT))
	cmake --build build/libcxx_runtime
	cmake --install build/libcxx_runtime

$(STAGE0_CLANG_CC): $(STAGE0_LIBCXX)
	@echo "Build STAGE0_CLANG_CC $(STAGE0_CLANG_CC)"
	rm -rf build/llvm_stage0
	cmake \
		-DCMAKE_TOOLCHAIN_FILE=$(realpath $(STAGE0_HOSTCXX_TOOLCHAIN_CMAKE)) \
		-G "Ninja" -S $(LLVM_VER)/llvm -B build/llvm_stage0 \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_VERBOSE_MAKEFILE:BOOL=ON \
		-DLLVM_ENABLE_PROJECTS="clang;lld" \
		-DLLVM_ENABLE_RUNTIMES="libcxx;compiler-rt" \
 		-DLLVM_FORCE_BUILD_RUNTIME=ON \
		-DLLVM_TARGETS_TO_BUILD=X86 \
		-DLLVM_INSTALL_TOOLCHAIN_ONLY=On \
		-DLLVM_TARGET_TRIPLE=$(SP_ARCH_CLANG) \
		-DCLANG_DEFAULT_CXX_STDLIB=libc++ \
		-DCLANG_DEFAULT_RTLIB=compiler-rt \
		-DCLANG_DEFAULT_LINKER=lld \
		-DCOMPILER_RT_BUILD_BUILTINS=On \
		-DCOMPILER_RT_BUILD_GWP_ASAN=OFF \
		-DCOMPILER_RT_BUILD_SANITIZERS=OFF \
		-DCOMPILER_RT_BUILD_XRAY=OFF \
		-DCOMPILER_RT_BUILD_MEMPROF=OFF \
		-DCOMPILER_RT_BUILD_CTX_PROFILE=OFF \
		-DCOMPILER_RT_BUILD_LIBFUZZER=OFF \
		-DCMAKE_INSTALL_PREFIX=$(abspath $(STAGE0_SYSROOT))
	cmake --build build/llvm_stage0 --parallel
	cmake --install build/llvm_stage0

STAGE0_MAKE_CLANG = $(abspath $(STAGE0_SYSROOT)/bin/clang)
STAGE0_MAKE_CFLAGS = -D_CRT_DECLARE_NONSTDC_NAMES=1 -D__STDC__ -Wno-deprecated-non-prototype -std=gnu11 -I$(abspath include)

$(info $(PATH))

stage0-make:
	$(MAKE) -C build/stage0-make

stage0: $(STAGE0_HOST_TOOLCHAIN_CMAKE) $(STAGE0_HOSTCXX_TOOLCHAIN_CMAKE) \
	$(STAGE0_LIBCXX) $(STAGE0_CLANG_CC)
