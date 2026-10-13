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
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

# Tau targets: <arch>-tau-none-elf (kernel and bare modules), <arch>-tau-none-elf+user (user
# modules with the runtime linked in) and <arch>-tau-none-elf+user+dll (user modules on the shared
# runtime libsprt.so, RT-A-SharedRuntime-1), for SP_ARCH aarch64 (the default) or x86_64 (modules
# run on a Linux host, the kernel's hosted build). One LLVM runtimes build serves the first two:
# the compiler-rt builtins go to every sysroot; libunwind and libc++abi, built against the sprt
# headers with -D__TAU__, go to the user sysroot. The dll sysroot gets a second build of them
# for libsprt.so: libunwind finds unwind tables through dl_iterate_phdr, and thread-local storage
# is initial-exec, the only model root links. libsprt.so itself is built from the runtime tree
# (make -C runtime SPRT_SHARED=1 STAPPLER_TARGET=<arch>-tau-none-elf+user+dll).

.DEFAULT_GOAL := all
THIS_FILE := $(lastword $(MAKEFILE_LIST))
MAKE_ROOT := $(dir $(realpath $(THIS_FILE)))

include $(MAKE_ROOT)../common/utils/detect-platform.mk
include $(MAKE_ROOT)../common/utils/init-shell.mk
include $(MAKE_ROOT)../common/utils/llvm-version.mk

SP_ARCH ?= aarch64

ifeq ($(SP_ARCH),aarch64)
SP_TARGET_CLANG := aarch64-none-elf
SP_LINK_CLANG := $(SP_TARGET_CLANG)
TAU_ARCH_FLAGS := -march=armv8-a
else ifeq ($(SP_ARCH),x86_64)
SP_TARGET_CLANG := x86_64-unknown-none-elf
# clang hands the link of x86_64-unknown-none-elf to the system gcc; its Linux driver runs
# ld.lld itself and, with -nostdlib -static, adds no libraries or start files.
SP_LINK_CLANG := x86_64-unknown-linux-gnu
# The thread pointer (fs) belongs to the module's TLS block: no canary there.
TAU_ARCH_FLAGS := -march=x86-64 -mstack-protector-guard=global
# The builtins' cpu_model/x86.c needs the compiler's cpuid.h.
TAU_RT_ARCH_INCLUDES = -isystem $(HOST_DIR)/lib/clang/$(SP_LLVM_VER)/include
else
$(error Unknown SP_ARCH: $(SP_ARCH))
endif

LIB_SRC_DIR ?= $(abspath $(MAKE_ROOT)../src)
SP_RUNTIME_ROOT := $(abspath $(MAKE_ROOT)../..)
HOST_DIR := $(abspath $(MAKE_ROOT)../hosts/$(HOST_ID))

OUT ?= $(abspath $(MAKE_ROOT)intermediate)/$(SP_ARCH)-tau
TARGETS ?= $(abspath $(MAKE_ROOT)targets)
T_KERNEL := $(TARGETS)/$(SP_ARCH)-tau-none-elf
T_USER := $(TARGETS)/$(SP_ARCH)-tau-none-elf+user
T_DLL := $(TARGETS)/$(SP_ARCH)-tau-none-elf+user+dll
OUT_DLL := $(OUT)-dll

BUILTINS := lib/clang/lib/tau/libclang_rt.builtins-$(SP_ARCH).a

# Flags of the EL0 runtimes build: the libc is the sprt one, reached through
# -isystem on the runtime include trees. A module exports that libc to the shared
# libraries root links into it (SYS-A-DynLink-1), so the runtimes name it as its
# consumers: SPRT_SHARED_RUNTIME, not hidden references that would keep it from them.
# libc++abi and libunwind are PIC: modules are position-independent (SYS-A-Pie-1), and x86_64
# allows no absolute relocations there.
TAU_C_FLAGS := -nostdinc -ffreestanding $(TAU_ARCH_FLAGS) -DSPRT_SHARED_RUNTIME \
	-isystem $(SP_RUNTIME_ROOT)/include_libc \
	-isystem $(SP_RUNTIME_ROOT)/include \
	$(TAU_RT_ARCH_INCLUDES) \
	-D__TAU__
TAU_CXX_FLAGS := -nostdinc -nostdinc++ $(TAU_ARCH_FLAGS) \
	-isystem $(SP_RUNTIME_ROOT)/libcxx/include \
	-isystem $(SP_RUNTIME_ROOT)/include_libc/cxx \
	-isystem $(SP_RUNTIME_ROOT)/include_libc \
	-isystem $(SP_RUNTIME_ROOT)/include \
	-D__TAU__ -D__SPRT_USE_STL=0 -DSPRT_SHARED_RUNTIME -Wno-deprecated-declarations

define TOOLCHAIN_CMAKE
# Tau EL0 runtimes, freestanding. "Generic" pairs with the *_BAREMETAL options.
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR $(SP_ARCH))
set(CMAKE_SHARED_LIBRARY_SUFFIX ".so")
set(CMAKE_SHARED_LIBRARY_PREFIX "lib")
set(CMAKE_C_COMPILER "$(HOST_DIR)/bin/clang")
set(CMAKE_CXX_COMPILER "$(HOST_DIR)/bin/clang++")
set(CMAKE_ASM_COMPILER "$(HOST_DIR)/bin/clang")
set(CMAKE_AR "$(HOST_DIR)/bin/llvm-ar")
set(CMAKE_RANLIB "$(HOST_DIR)/bin/llvm-ranlib")
set(CMAKE_C_COMPILER_TARGET "$(SP_TARGET_CLANG)")
set(CMAKE_CXX_COMPILER_TARGET "$(SP_TARGET_CLANG)")
set(CMAKE_ASM_COMPILER_TARGET "$(SP_TARGET_CLANG)")
set(CMAKE_C_COMPILER_WORKS ON)
set(CMAKE_CXX_COMPILER_WORKS ON)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_C_FLAGS_INIT "$(TAU_C_FLAGS)")
set(CMAKE_CXX_FLAGS_INIT "$(TAU_CXX_FLAGS)")
set(CMAKE_ASM_FLAGS_INIT "$(TAU_C_FLAGS)")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-nostdlib")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-nostdlib")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
set(CMAKE_POSITION_INDEPENDENT_CODE OFF)
endef

# $(1): TARGET_SYSTEM, $(2): include mode. The kernel keeps the compiler's own
# headers (-nostdlibinc: stdint.h, arm_acle.h); EL0 resolves every header
# through the sprt trees (-nostdinc).
define TARGET_MK
TARGET_SYSROOT := $$(patsubst %/,%,$$(dir $$(lastword $$(MAKEFILE_LIST))))
TARGET_SYSTEM := $(1)
TARGET_ARCH := $(SP_ARCH)
TARGET_NAME := $(SP_TARGET_CLANG)
# -D__TAU__ selects SPRT_TAU in include/sprt/c/bits/__sprt_def.h.
TARGET_GENERAL_CFLAGS := --target=$(SP_TARGET_CLANG) $(2) -resource-dir $$(TARGET_SYSROOT)/lib/clang $(TAU_ARCH_FLAGS) -D__TAU__
TARGET_GENERAL_CXXFLAGS := --target=$(SP_TARGET_CLANG) $(2) -resource-dir $$(TARGET_SYSROOT)/lib/clang $(TAU_ARCH_FLAGS) -D__TAU__
TARGET_GENERAL_SFLAGS := --target=$(SP_TARGET_CLANG) -resource-dir $$(TARGET_SYSROOT)/lib/clang $(TAU_ARCH_FLAGS) -D__TAU__
TARGET_GENERAL_LDFLAGS := --target=$(SP_LINK_CLANG) -resource-dir $$(TARGET_SYSROOT)/lib/clang
TARGET_LIB_DIR := $$(TARGET_SYSROOT)/usr/lib
TARGET_EXEC_CFLAGS :=
TARGET_EXEC_CXXFLAGS :=
TARGET_EXEC_LDFLAGS :=
TARGET_LIB_CFLAGS :=
TARGET_LIB_CXXFLAGS :=
TARGET_LIB_LDFLAGS :=
$(3)
endef

# $(1): the output, $(2): the runtimes, $(3): LIBUNWIND_IS_BAREMETAL, $(4): the compile flags of
# libunwind and libc++abi.
runtimes_configure = \
	-G "Ninja" \
	-S $(LIB_SRC_DIR)/llvm-project/runtimes \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_TOOLCHAIN_FILE=$(OUT)/toolchain.cmake \
	-DCMAKE_PROJECT_INCLUDE=$(MAKE_ROOT)tau-project-include.cmake \
	-DCMAKE_INSTALL_PREFIX=$(1) \
	-DCMAKE_INSTALL_LIBDIR=$(1)/usr/lib \
	-DCMAKE_INSTALL_INCLUDEDIR=$(1)/usr/include \
	-DLLVM_ENABLE_RUNTIMES="$(2)" \
	-DLLVM_INSTALL_TOOLCHAIN_ONLY=On \
	-DLLVM_ENABLE_PIC=Off \
	-DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=Off \
	-DLLVM_DEFAULT_TARGET_TRIPLE=$(SP_TARGET_CLANG) \
	-DLLVM_HOST_TRIPLE=$(SP_TARGET_CLANG) \
	-DCOMPILER_RT_DEFAULT_TARGET_ONLY=On \
	-DCOMPILER_RT_BAREMETAL_BUILD=On \
	-DCOMPILER_RT_BUILD_BUILTINS=On \
	-DCOMPILER_RT_BUILD_SANITIZERS=Off \
	-DCOMPILER_RT_BUILD_XRAY=Off \
	-DCOMPILER_RT_BUILD_MEMPROF=Off \
	-DCOMPILER_RT_BUILD_CTX_PROFILE=Off \
	-DCOMPILER_RT_BUILD_PROFILE=Off \
	-DCOMPILER_RT_BUILD_LIBFUZZER=Off \
	-DCOMPILER_RT_BUILD_ORC=Off \
	-DCOMPILER_RT_BUILD_GWP_ASAN=Off \
	-DCOMPILER_RT_OS_DIR=tau \
	-DCOMPILER_RT_INSTALL_PATH=$(1)/lib/clang \
	-DLIBUNWIND_ENABLE_SHARED=Off \
	-DLIBUNWIND_ADDITIONAL_COMPILE_FLAGS="$(4)" \
	-DLIBUNWIND_ENABLE_THREADS=On \
	-DLIBUNWIND_HAS_PTHREAD_LIB=Off \
	-DLIBUNWIND_HAS_DL_LIB=Off \
	-DLIBUNWIND_IS_BAREMETAL=$(3) \
	-DLIBUNWIND_USE_COMPILER_RT=On \
	-DLIBUNWIND_INSTALL_LIBRARY_DIR=usr/lib \
	-DLIBCXXABI_ENABLE_SHARED=Off \
	-DLIBCXXABI_ADDITIONAL_COMPILE_FLAGS="$(4)" \
	-DLIBCXXABI_ENABLE_THREADS=On \
	-DLIBCXXABI_HAS_PTHREAD_LIB=Off \
	-DLIBCXXABI_ENABLE_EXCEPTIONS=On \
	-DLIBCXXABI_USE_LLVM_UNWINDER=On \
	-DLIBCXXABI_USE_COMPILER_RT=On \
	-DLIBCXXABI_BAREMETAL=On \
	-DLIBCXXABI_ENABLE_STATIC=On \
	-DLIBCXXABI_ENABLE_NEW_DELETE_DEFINITIONS=Off \
	-DLIBCXXABI_INSTALL_LIBRARY_DIR=usr/lib \
	-DLIBCXXABI_LIBCXX_INCLUDES="$(SP_RUNTIME_ROOT)/include_libc/cxx;$(SP_RUNTIME_ROOT)/libcxx/include"

$(OUT) $(OUT_DLL) $(T_KERNEL) $(T_USER) $(T_DLL):
	mkdir -p $@

$(OUT)/toolchain.cmake: $(THIS_FILE) | $(OUT)
	$(file >$@,$(TOOLCHAIN_CMAKE))

$(OUT)/target-kernel.mk: $(THIS_FILE) | $(OUT)
	$(file >$@,$(call TARGET_MK,Tau,-nostdlibinc))

$(OUT)/target-user.mk: $(THIS_FILE) | $(OUT)
	$(file >$@,$(call TARGET_MK,TauUser,-nostdinc))

# TARGET_SPRT_SHARED: user modules link libsprt.so (make/os/tau-user.mk, runtime/runtime-shared.mk).
$(OUT)/target-dll.mk: $(THIS_FILE) | $(OUT)
	$(file >$@,$(call TARGET_MK,TauUser,-nostdinc,TARGET_SPRT_SHARED := 1))

$(OUT)/.runtimes: $(OUT)/toolchain.cmake
	rm -rf $(OUT)/runtimes-build && mkdir -p $(OUT)/runtimes-build
	cd $(OUT)/runtimes-build && cmake $(call runtimes_configure,$(OUT),compiler-rt;libunwind;libcxxabi,On,-fPIC)
	cd $(OUT)/runtimes-build && cmake --build .
	cd $(OUT)/runtimes-build && cmake --install .
	rm -rf $(OUT)/runtimes-build
	touch $@

$(OUT_DLL)/.runtimes: $(OUT)/toolchain.cmake | $(OUT_DLL)
	rm -rf $(OUT_DLL)/runtimes-build && mkdir -p $(OUT_DLL)/runtimes-build
	cd $(OUT_DLL)/runtimes-build && cmake $(call runtimes_configure,$(OUT_DLL),libunwind;libcxxabi,Off,-fPIC;-ftls-model=initial-exec)
	cd $(OUT_DLL)/runtimes-build && cmake --build .
	cd $(OUT_DLL)/runtimes-build && cmake --install .
	rm -rf $(OUT_DLL)/runtimes-build
	touch $@

# $(1): sysroot. The resource dir points at the host clang headers.
define install_common
	mkdir -p $(1)/lib/clang/lib/tau
	cp -f $(OUT)/$(BUILTINS) $(1)/$(BUILTINS)
	ln -sfn $(HOST_DIR)/lib/clang/$(SP_LLVM_VER)/include $(1)/lib/clang/include
endef

kernel: $(OUT)/.runtimes $(OUT)/target-kernel.mk | $(T_KERNEL)
	$(call install_common,$(T_KERNEL))
	cp -f $(OUT)/target-kernel.mk $(T_KERNEL)/target.mk

user: $(OUT)/.runtimes $(OUT)/target-user.mk | $(T_USER)
	$(call install_common,$(T_USER))
	mkdir -p $(T_USER)/usr/lib $(T_USER)/usr/include $(T_USER)/share
	cp -f $(OUT)/usr/lib/libunwind.a $(OUT)/usr/lib/libc++abi.a $(T_USER)/usr/lib/
	cp -rf $(OUT)/usr/include/. $(T_USER)/usr/include/
	if [ -d $(LIB_SRC_DIR)/simde/simde ]; then rm -rf $(T_USER)/usr/include/simde; cp -r $(LIB_SRC_DIR)/simde/simde $(T_USER)/usr/include/simde; fi
	cp -f $(MAKE_ROOT)app.lds $(T_USER)/share/app-$(SP_ARCH).lds
	cp -f $(OUT)/target-user.mk $(T_USER)/target.mk

dll: $(OUT)/.runtimes $(OUT_DLL)/.runtimes $(OUT)/target-dll.mk | $(T_DLL)
	$(call install_common,$(T_DLL))
	mkdir -p $(T_DLL)/usr/lib $(T_DLL)/usr/include $(T_DLL)/share
	cp -f $(OUT_DLL)/usr/lib/libunwind.a $(OUT_DLL)/usr/lib/libc++abi.a $(T_DLL)/usr/lib/
	# libsprt.so takes libc++abi whole; the standard exception classes are libc++'s of the runtime.
	$(HOST_DIR)/bin/llvm-ar d $(T_DLL)/usr/lib/libc++abi.a stdlib_stdexcept.cpp.obj
	cp -rf $(OUT)/usr/include/. $(T_DLL)/usr/include/
	if [ -d $(LIB_SRC_DIR)/simde/simde ]; then rm -rf $(T_DLL)/usr/include/simde; cp -r $(LIB_SRC_DIR)/simde/simde $(T_DLL)/usr/include/simde; fi
	cp -f $(MAKE_ROOT)app.lds $(T_DLL)/share/app-$(SP_ARCH).lds
	cp -f $(OUT)/target-dll.mk $(T_DLL)/target.mk

all: kernel user dll

clean:
	rm -rf $(OUT) $(OUT_DLL) $(T_KERNEL) $(T_USER) $(T_DLL)

.PHONY: all kernel user dll clean
