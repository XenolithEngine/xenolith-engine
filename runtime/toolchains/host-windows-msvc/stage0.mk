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

PREBUILTS_PATH := C:/x86_64-pc-windows-msvc/bin
STAGE0_SYSROOT := sysroot-windows
STAGE0_LIBCXX := $(STAGE0_SYSROOT)/bin/c++.dll
STAGE0_ZLIB := $(STAGE0_SYSROOT)/lib/zs.lib
STAGE0_LIBXML2 := $(STAGE0_SYSROOT)/lib/libxml2s.lib
STAGE0_CLANG_CC := $(STAGE0_SYSROOT)/bin/clang.exe
STAGE0_CLANG_CXX := $(STAGE0_SYSROOT)/bin/clang++.exe
STAGE0_MAKE_EXE := $(STAGE0_SYSROOT)/bin/make.exe

WARN_FLAGS :=
OPT_FLAGS :=

STAGE0_HOST_TOOLCHAIN_CMAKE := host.cmake
STAGE0_HOSTCXX_TOOLCHAIN_CMAKE := hostcxx.cmake

# Use precompiled  binaries for bootstrapping
STAGE0_CC := $(PREBUILTS_PATH)/clang-cl.exe
STAGE0_CXX := $(PREBUILTS_PATH)/clang-cl.exe
STAGE0_RC := $(PREBUILTS_PATH)/llvm-rc.exe
STAGE0_LLD := $(PREBUILTS_PATH)/llvm-link.exe
STAGE0_ML := $(PREBUILTS_PATH)/llvm-ml64.exe
PREBUILD_NINJA := $(PREBUILTS_PATH)/ninja.exe

export MSYS_NO_PATHCONV=1
export CC=$(PREBUILTS_PATH)/clang-cl.exe
export CXX=$(PREBUILTS_PATH)/clang-cl.exe
export RC=$(STAGE0_RC)
export ASM_MASM=$(STAGE0_ML)

STAGE0_LIB_PATH :=
STAGE0_INCLUDE_PATH :=

STAGE0_CFLAGS := $(OPT_FLAGS) $(WARN_FLAGS) $(STAGE0_INCLUDE_PATH) -fuse-ld=lld
STAGE0_CXXFLAGS := $(OPT_FLAGS) $(WARN_FLAGS) $(STAGE0_INCLUDE_PATH) -fuse-ld=lld
STAGE0_EXE_LDFLAGS := $(STAGE0_LIB_PATH) -fuse-ld=lld
STAGE0_LIB_LDFLAGS := $(STAGE0_LIB_PATH) -fuse-ld=lld

STAGE0_LIBC_CFLAGS := $(OPT_FLAGS) $(WARN_FLAGS) $(STAGE0_INCLUDE_PATH) -DLIBXML_STATIC
STAGE0_LIBCXX_CFLAGS := $(OPT_FLAGS) $(WARN_FLAGS) $(STAGE0_INCLUDE_PATH) -DLIBXML_STATIC

STAGE0_LIBCXX_EXE_LDFLAGS := \
	-L$(abspath $(STAGE0_SYSROOT)/lib) \
	$(STAGE0_LIB_PATH) bcrypt.lib


STAGE0_LIBCXX_LIB_LDFLAGS := \
	-L$(abspath $(STAGE0_SYSROOT)/lib) \
	$(STAGE0_LIB_PATH) bcrypt.lib

$(STAGE0_HOST_TOOLCHAIN_CMAKE):
	Set-Content  -Encoding UTF8 -Path "$@" -Value 'set(CMAKE_SYSTEM_NAME Windows)' | Out-Null
	Add-Content  -Encoding UTF8 -Path "$@" -Value 'set(CMAKE_SYSROOT $(realpath $(STAGE0_XWIN_SYSROOT)))' | Out-Null
	Add-Content  -Encoding UTF8 -Path "$@" -Value 'set(CMAKE_C_COMPILER_TARGET $(SP_ARCH_CLANG))' | Out-Null
	Add-Content  -Encoding UTF8 -Path "$@" -Value 'set(CMAKE_CXX_COMPILER_TARGET $(SP_ARCH_CLANG))' | Out-Null
	Add-Content  -Encoding UTF8 -Path "$@" -Value 'set(CMAKE_C_FLAGS_INIT "$(STAGE0_CFLAGS)")' | Out-Null
	Add-Content  -Encoding UTF8 -Path "$@" -Value 'set(CMAKE_CXX_FLAGS_INIT "$(STAGE0_CXXFLAGS)")' | Out-Null
	Add-Content  -Encoding UTF8 -Path "$@" -Value 'set(CMAKE_C_COMPILER "$(STAGE0_CC)")' | Out-Null
	Add-Content  -Encoding UTF8 -Path "$@" -Value 'set(CMAKE_RC_COMPILER "$(STAGE0_RC)")' | Out-Null
	Add-Content  -Encoding UTF8 -Path "$@" -Value 'set(CMAKE_CXX_COMPILER "$(STAGE0_CXX)")' | Out-Null
	Add-Content  -Encoding UTF8 -Path "$@" -Value 'set(CMAKE_EXE_LINKER_FLAGS_INIT "$(STAGE0_EXE_LDFLAGS)")' | Out-Null
	Add-Content  -Encoding UTF8 -Path "$@" -Value 'set(CMAKE_SHARED_LINKER_FLAGS_INIT "$(STAGE0_LIB_LDFLAGS)")' | Out-Null
	Add-Content  -Encoding UTF8 -Path "$@" -Value 'set(CMAKE_VERBOSE_MAKEFILE ON)' | Out-Null
	Add-Content  -Encoding UTF8 -Path "$@" -Value 'set(BUILD_SHARED_LIBS OFF)' | Out-Null
	Add-Content  -Encoding UTF8 -Path "$@" -Value 'set(CMAKE_LINKER_TYPE LLD)' | Out-Null
	Add-Content  -Encoding UTF8 -Path "$@" -Value 'set(CMAKE_LINKER "$(STAGE0_LLD)")' | Out-Null

$(STAGE0_HOSTCXX_TOOLCHAIN_CMAKE):
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
	Add-Content  -Encoding UTF8 -Path "$@" -Value 'set(CMAKE_ASM_MASM_COMPILER "$(STAGE0_ML)")' | Out-Null


#
# zlib
#

STAGE0_BUILD_ZLIB := cmake -G "Ninja" \
	-DCMAKE_TOOLCHAIN_FILE=$(abspath $(STAGE0_HOST_TOOLCHAIN_CMAKE)) \
	-G "Ninja" -DCMAKE_MAKE_PROGRAM=$(PREBUILD_NINJA) \
	-S $(ZLIB_DIR) -B build/zlib \
	-DZLIB_BUILD_EXAMPLES=OFF \
	-DZLIB_BUILD_TESTING=Off \
	-DZLIB_BUILD_EXAMPLES=Off \
	-DZLIB_BUILD_SHARED=Off \
	-DZLIB_BUILD_STATIC=On \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL \
	-DINSTALL_BIN_DIR=$(abspath $(STAGE0_SYSROOT))/bin \
	-DINSTALL_MAN_DIR=$(abspath $(STAGE0_SYSROOT))/share/man \
	-DINSTALL_PKGCONFIG_DIR=$(abspath $(STAGE0_SYSROOT))/lib/pkgconfig \
	-DCMAKE_INSTALL_PREFIX=$(abspath $(STAGE0_SYSROOT))

$(STAGE0_ZLIB): $(ZLIB_DIR) $(STAGE1_CLANG_CC) $(STAGE0_HOST_TOOLCHAIN_CMAKE)
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
	-G "Ninja" -DCMAKE_MAKE_PROGRAM=$(PREBUILD_NINJA) \
	-S $(LIBXML2_DIR) -B build/libxml2 \
	-DBUILD_SHARED_LIBS=Off \
	-DLIBXML2_WITH_DEBUG=Off \
	-DLIBXML2_WITH_PROGRAMS=Off \
	-DLIBXML2_WITH_TESTS=Off \
	-DLIBXML2_WITH_TLS=On \
	-DLIBXML2_WITH_ICONV=Off \
	-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=$(abspath $(STAGE0_SYSROOT))

$(STAGE0_LIBXML2): $(ZLIB_DIR)
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
	-G "Ninja" -DCMAKE_MAKE_PROGRAM=$(PREBUILD_NINJA) \
	-S $(LLVM_VER)/runtimes -B build/libcxx_runtime \
	-DCMAKE_BUILD_TYPE=Release \
	-DLLVM_ENABLE_RUNTIMES="libcxx" \
	-DLLVM_TARGETS_TO_BUILD=X86 \
	-DLIBCXX_ENABLE_STATIC=Off \
	-DLIBCXX_ENABLE_SHARED=On \
	-DLIBCXX_INCLUDE_TESTS=Off \
	-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL \
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

STAGE0_BUILD_CC := cmake \
	-DCMAKE_TOOLCHAIN_FILE=$(abspath $(STAGE0_HOSTCXX_TOOLCHAIN_CMAKE)) \
	-G "Ninja" -DCMAKE_MAKE_PROGRAM=$(PREBUILD_NINJA) -S $(LLVM_VER)/llvm -B build/llvm_stage0 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_LINKER_TYPE=LLD \
	-DZLIB_LIBRARY=$(abspath $(STAGE0_ZLIB)) \
	-DLIBXML2_LIBRARY=$(abspath $(STAGE0_LIBXML2)) \
	-DLLVM_BUILD_LLVM_DYLIB_VIS=On \
	-DLLVM_ENABLE_ZLIB=FORCE_ON \
	-DLLVM_ENABLE_LIBXML2=FORCE_ON \
	-DLLVM_ENABLE_PROJECTS="clang;lld;lldb" \
	-DLLVM_ENABLE_RUNTIMES="libcxx;compiler-rt" \
	-DLLVM_FORCE_BUILD_RUNTIME=ON \
	-DLLVM_TARGETS_TO_BUILD=X86 \
	-DLLVM_INSTALL_TOOLCHAIN_ONLY=On \
	-DLLDB_INCLUDE_TESTS=Off \
	-DLLVM_ENABLE_SPHINX=Off \
	-DLLVM_BUILD_LLVM_DYLIB=ON \
	-DLLVM_LINK_LLVM_DYLIB=ON \
	-DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=Off \
	-DLLVM_TARGET_TRIPLE=$(SP_ARCH_CLANG) \
	-DLIBCXX_ENABLE_STATIC=Off \
	-DLIBCXX_ENABLE_SHARED=On \
	-DCLANG_LINK_CLANG_DYLIB=On \
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
	-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL \
	-DLLDB_ENABLE_PYTHON=Off \
	-DCMAKE_POLICY_DEFAULT_CMP0091=NEW \
	-DCMAKE_INSTALL_PREFIX=$(abspath $(STAGE0_SYSROOT))

$(STAGE0_CLANG_CC): $(STAGE0_LIBCXX) $(STAGE0_HOSTCXX_TOOLCHAIN_CMAKE)
	@echo "Build STAGE0_CLANG_CC $(STAGE0_CLANG_CC)"
	$(call rule_rm,build/llvm_stage0)
	$(STAGE0_BUILD_CC)
	cmake --build build/llvm_stage0 --parallel
	cmake --install build/llvm_stage0
	(Get-Item "$(STAGE0_CLANG_CC)").LastWriteTime = $$(Get-Date);

stage0: $(STAGE0_ZLIB) $(STAGE0_LIBXML2) $(STAGE0_LIBCXX) $(STAGE0_CLANG_CC)
