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
	Set-Content  -Encoding UTF8 -Path "$@" -Value 'set(CMAKE_SYSTEM_NAME Windows)' | Out-Null
	Add-Content  -Encoding UTF8 -Path "$@" -Value 'set(CMAKE_SYSROOT $(realpath $(STAGE0_XWIN_SYSROOT)))' | Out-Null
	Add-Content  -Encoding UTF8 -Path "$@" -Value 'set(CMAKE_C_COMPILER_TARGET $(SP_ARCH_CLANG))' | Out-Null
	Add-Content  -Encoding UTF8 -Path "$@" -Value 'set(CMAKE_CXX_COMPILER_TARGET $(SP_ARCH_CLANG))' | Out-Null
	Add-Content  -Encoding UTF8 -Path "$@" -Value 'set(CMAKE_C_FLAGS_INIT "$(STAGE0_CFLAGS)")' | Out-Null
	Add-Content  -Encoding UTF8 -Path "$@" -Value 'set(CMAKE_CXX_FLAGS_INIT "$(STAGE0_CXXFLAGS)")' | Out-Null
	Add-Content  -Encoding UTF8 -Path "$@" -Value 'set(CMAKE_C_COMPILER "$(STAGE0_CC)")' | Out-Null
	Add-Content  -Encoding UTF8 -Path "$@" -Value 'set(CMAKE_CXX_COMPILER "$(STAGE0_CXX)")' | Out-Null
	Add-Content  -Encoding UTF8 -Path "$@" -Value 'set(CMAKE_EXE_LINKER_FLAGS_INIT "$(STAGE0_EXE_LDFLAGS)")' | Out-Null
	Add-Content  -Encoding UTF8 -Path "$@" -Value 'set(CMAKE_SHARED_LINKER_FLAGS_INIT "$(STAGE0_LIB_LDFLAGS)")' | Out-Null
	Add-Content  -Encoding UTF8 -Path "$@" -Value 'set(CMAKE_VERBOSE_MAKEFILE ON)' | Out-Null
	Add-Content  -Encoding UTF8 -Path "$@" -Value 'set(BUILD_SHARED_LIBS OFF)' | Out-Null
	Add-Content  -Encoding UTF8 -Path "$@" -Value 'set(CMAKE_LINKER_TYPE LLD)' | Out-Null

$(STAGE0_HOSTCXX_TOOLCHAIN_CMAKE): $(lastword $(MAKEFILE_LIST))
	Set-Content  -Encoding UTF8 -Path "$@" -Value 'set(CMAKE_SYSTEM_NAME Windows)' | Out-Null
	Add-Content  -Encoding UTF8 -Path "$@" -Value 'set(CMAKE_SYSROOT $(realpath $(STAGE0_XWIN_SYSROOT)))' | Out-Null
	Add-Content  -Encoding UTF8 -Path "$@" -Value 'set(CMAKE_C_COMPILER_TARGET $(SP_ARCH_CLANG))' | Out-Null
	Add-Content  -Encoding UTF8 -Path "$@" -Value 'set(CMAKE_CXX_COMPILER_TARGET $(SP_ARCH_CLANG))' | Out-Null
	Add-Content  -Encoding UTF8 -Path "$@" -Value 'set(CMAKE_C_FLAGS_INIT "$(STAGE0_LIBC_CFLAGS)")' | Out-Null
	Add-Content  -Encoding UTF8 -Path "$@" -Value 'set(CMAKE_CXX_FLAGS_INIT "$(STAGE0_LIBC_CXXFLAGS)")' | Out-Null
	Add-Content  -Encoding UTF8 -Path "$@" -Value 'set(CMAKE_C_COMPILER "$(abspath $(STAGE0_CC))")' | Out-Null
	Add-Content  -Encoding UTF8 -Path "$@" -Value 'set(CMAKE_CXX_COMPILER "$(abspath $(STAGE0_CXX))")' | Out-Null
	Add-Content  -Encoding UTF8 -Path "$@" -Value 'set(CMAKE_RC_COMPILER "$(abspath $(STAGE0_RC))")' | Out-Null
	Add-Content  -Encoding UTF8 -Path "$@" -Value 'set(CMAKE_EXE_LINKER_FLAGS_INIT "$(STAGE0_LIBCXX_EXE_LDFLAGS)")' | Out-Null
	Add-Content  -Encoding UTF8 -Path "$@" -Value 'set(CMAKE_SHARED_LINKER_FLAGS_INIT "$(STAGE0_LIBCXX_LIB_LDFLAGS)")' | Out-Null
	Add-Content  -Encoding UTF8 -Path "$@" -Value 'set(CMAKE_VERBOSE_MAKEFILE ON)' | Out-Null
	Add-Content  -Encoding UTF8 -Path "$@" -Value 'set(CMAKE_LINKER_TYPE LLD)' | Out-Null

STAGE0_BUILD_LIBCXX := cmake \
	-DCMAKE_TOOLCHAIN_FILE=$(realpath $(STAGE0_HOST_TOOLCHAIN_CMAKE)) \
	-G "Ninja" -S $(LLVM_VER)/runtimes -B build/libcxx_runtime \
	-DCMAKE_BUILD_TYPE=Release \
	-DLLVM_ENABLE_RUNTIMES="libcxx" \
	-DLLVM_TARGETS_TO_BUILD=X86 \
	-DLIBCXX_ENABLE_STATIC=On \
	-DLIBCXX_ENABLE_SHARED=Off \
	-DLIBCXXABI_ENABLE_STATIC=On \
	-DLIBCXXABI_ENABLE_SHARED=Off \
	-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded \
	-DCMAKE_POLICY_DEFAULT_CMP0091=NEW \
	-DCMAKE_INSTALL_PREFIX=$(abspath $(STAGE0_SYSROOT))

$(STAGE0_LIBCXX):
	@echo "Build STAGE0_LIBCXX $(STAGE0_LIBCXX)"
	$(call rule_rm,build/libcxx_runtime)
	$(STAGE0_BUILD_LIBCXX)
	cmake --build build/libcxx_runtime
	cmake --install build/libcxx_runtime

# LLVM root cmake should be patched with this:
# set(CMAKE_ASM_MASM_COMPILER "C:/LLVM/bin/llvm-ml.exe")
# set(CMAKE_ASM_MASM_FLAGS_INIT "--m64")
# set(CMAKE_ASM_MASM_FLAGS "--m64")

STAGE0_BUILD_CC := cmake \
	-DCMAKE_TOOLCHAIN_FILE=$(realpath $(STAGE0_HOSTCXX_TOOLCHAIN_CMAKE)) \
	-G "Ninja" -S $(LLVM_VER)/llvm -B build/llvm_stage0 \
	-DCMAKE_BUILD_TYPE=Release \
	-DLLVM_ENABLE_PROJECTS="clang;lld;lldb" \
	-DLLVM_ENABLE_RUNTIMES="libcxx;compiler-rt" \
	-DLLVM_FORCE_BUILD_RUNTIME=ON \
	-DLLVM_TARGETS_TO_BUILD=X86 \
	-DLLVM_INSTALL_TOOLCHAIN_ONLY=On \
	-DLLDB_INCLUDE_TESTS=Off \
	-DLLVM_ENABLE_SPHINX=Off \
	-DLLVM_TARGET_TRIPLE=$(SP_ARCH_CLANG) \
	-DLIBCXX_ENABLE_STATIC=On \
	-DLIBCXX_ENABLE_SHARED=Off \
	-DLIBCXXABI_ENABLE_STATIC=On \
	-DLIBCXXABI_ENABLE_SHARED=Off \
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
	-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded \
	-DCMAKE_POLICY_DEFAULT_CMP0091=NEW \
	-DCMAKE_INSTALL_PREFIX=$(abspath $(STAGE0_SYSROOT))

$(STAGE0_CLANG_CC): $(STAGE0_LIBCXX)
	@echo "Build STAGE0_CLANG_CC $(STAGE0_CLANG_CC)"
	$(call rule_rm,build/llvm_stage0)
	$(STAGE0_BUILD_CC)
	cmake --build build/llvm_stage0 --parallel
	cmake --install build/llvm_stage0

STAGE0_MAKE_CLANG = $(abspath $(STAGE0_SYSROOT)/bin/clang)
STAGE0_MAKE_CFLAGS = -D_CRT_DECLARE_NONSTDC_NAMES=1 -D__STDC__ -Wno-deprecated-non-prototype -std=gnu11 -I$(abspath include)

$(info $(PATH))

stage0-make:
	$(MAKE) -C build/stage0-make

stage0: $(STAGE0_HOST_TOOLCHAIN_CMAKE) $(STAGE0_HOSTCXX_TOOLCHAIN_CMAKE) \
	$(STAGE0_LIBCXX) $(STAGE0_CLANG_CC)
