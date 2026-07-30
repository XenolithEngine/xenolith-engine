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

# Cross-build compiler-rt (builtins, profile, orc) for the Windows target using
# the current host clang. Windows does not need libc++, only compiler-rt, so we
# enable just the compiler-rt runtime. The runtime is below libc, so it is built
# against the real MSVC CRT/SDK (xwin) instead of the sprt target toolchain.

.DEFAULT_GOAL := all

LIBNAME := llvm-project
BUILD_DIR := compiler-rt-build

include ../common/utils/detect-platform.mk
include ../common/utils/init-shell.mk

SP_CLANG_CL := $(dir $(SP_CC))clang-cl

export LIB := $(SP_INSTALL_PREFIX)/lib

CRT_C_INCLUDES := \
	-I$(abspath ../../../runtime/include_libc) \
	-I$(abspath ../../../runtime/include) \
	-I$(abspath ../../../runtime/include/sprt/wrappers/windows)

CRT_CXX_INCLUDES := \
	-I$(abspath ../../../runtime/include_libc/cxx) \
	-I$(abspath ../../../runtime/libcxx/include) \
	$(CRT_C_INCLUDES)

CRT_WARN_FLAGS := \
	-Wno-nonportable-include-path \
	-Wno-ignored-attributes \
	-Wno-ignored-pragma-intrinsic \
	-Wno-pragma-pack \
	-Wno-unknown-pragmas \
	-Wno-microsoft-enum-value

CRT_CONFIGURE := \
	-G "Ninja" \
	-S $(LIB_SRC_DIR)/$(LIBNAME)/runtimes \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_SYSTEM_NAME=Windows \
	-DCMAKE_SYSTEM_PROCESSOR=$(SP_ARCH) \
	-DCMAKE_C_COMPILER="$(SP_CLANG_CL)" \
	-DCMAKE_CXX_COMPILER="$(SP_CLANG_CL)" \
	-DCMAKE_ASM_COMPILER="$(SP_CLANG_CL)" \
	-DCMAKE_RC_COMPILER="$(SP_RC)" \
	-DCMAKE_C_COMPILER_TARGET=$(SP_TARGET) \
	-DCMAKE_CXX_COMPILER_TARGET=$(SP_TARGET) \
	-DCMAKE_ASM_COMPILER_TARGET=$(SP_TARGET) \
	-DCMAKE_C_COMPILER_WORKS=ON \
	-DCMAKE_CXX_COMPILER_WORKS=ON \
	-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
	-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded \
	-DCMAKE_LINKER_TYPE=LLD \
	-DCMAKE_C_FLAGS_INIT="$(CRT_WARN_FLAGS) $(CRT_C_INCLUDES)" \
	-DCMAKE_CXX_FLAGS_INIT="$(CRT_WARN_FLAGS) $(CRT_CXX_INCLUDES)" \
	-DCMAKE_ASM_FLAGS_INIT="$(CRT_C_INCLUDES)" \
	-DCMAKE_INSTALL_PREFIX=$(SP_INSTALL_PREFIX) \
	-DCMAKE_VERBOSE_MAKEFILE=On \
	-DCMAKE_CXX_STANDARD=20 \
	-DLLVM_ENABLE_RUNTIMES=compiler-rt \
	-DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=Off \
	-DLLVM_DEFAULT_TARGET_TRIPLE=$(SP_TARGET) \
	-DLLVM_HOST_TRIPLE=$(SP_TARGET) \
	-DCOMPILER_RT_DEFAULT_TARGET_ONLY=On \
	-DCOMPILER_RT_BUILD_BUILTINS=On \
	-DCOMPILER_RT_BUILD_SANITIZERS=Off \
	-DCOMPILER_RT_BUILD_XRAY=Off \
	-DCOMPILER_RT_BUILD_MEMPROF=Off \
	-DCOMPILER_RT_BUILD_CTX_PROFILE=Off \
	-DCOMPILER_RT_BUILD_LIBFUZZER=Off \
	-DCOMPILER_RT_BUILD_GWP_ASAN=Off \
	-DCOMPILER_RT_TARGET_HAS_FCNTL_LCK=0

all:
	$(call rule_rm,$(BUILD_DIR))
	$(call rule_mkdir,$(BUILD_DIR))
	cd $(BUILD_DIR); cmake $(CRT_CONFIGURE)
	cd $(BUILD_DIR); cmake --build . --config Release --target install
	$(call rule_rm,$(BUILD_DIR))
	$(call rule_mkdir,$(SP_INSTALL_PREFIX)/lib/clang/lib)
	$(call rule_rm,$(SP_INSTALL_PREFIX)/lib/clang/lib/windows)
	$(call rule_mv,$(SP_INSTALL_PREFIX)/lib/windows,$(SP_INSTALL_PREFIX)/lib/clang/lib)

.PHONY: all
