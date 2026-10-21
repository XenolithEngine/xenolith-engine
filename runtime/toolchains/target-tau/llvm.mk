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

# LLVM for Tau: the static, position-independent libraries lavapipe JITs with, linked into
# libvulkan_lvp.so (mesa.mk). Two stages, as for Embox (target-embox/llvm.mk):
#
#   native   llvm-tblgen, llvm-min-tblgen and llvm-config for the build host, with the system
#            compiler; llvm-config is copied into usr/bin, and Mesa's meson asks it for the
#            libraries, so it is configured with the target's triple: its `native` is the target
#   cross    the LLVM libraries for the target against sprt (toolchain-vulkan.cmake), with the
#            native tablegen
#
# The options are lavapipe-options.mk's, shared with the host's reference build. Probes link
# (toolchain-vulkan.cmake), so a function libsprt.so does not have is detected absent.
# The two build trees stay (llvm-native, llvm-build).

.DEFAULT_GOAL := all

LIBNAME = llvm

include ../common/configure.mk
include lavapipe-options.mk

LLVM_SRC ?= $(LIB_SRC_DIR)/llvm-project
LLVM_NATIVE := $(SP_INSTALL_PREFIX)/llvm-native
LLVM_BUILD := $(SP_INSTALL_PREFIX)/llvm-build

ifeq ($(SP_ARCH),aarch64)
LLVM_TRIPLE := aarch64-unknown-none-elf
LLVM_TARGET := AArch64
else
LLVM_TRIPLE := x86_64-unknown-none-elf
LLVM_TARGET := X86
endif

LLVM_TARGET_OPTIONS := \
	-DLLVM_TARGETS_TO_BUILD=$(LLVM_TARGET) \
	-DLLVM_HOST_TRIPLE=$(LLVM_TRIPLE) \
	-DLLVM_DEFAULT_TARGET_TRIPLE=$(LLVM_TRIPLE)

$(LLVM_NATIVE)/bin/llvm-config: lavapipe-options.mk
	cmake -G Ninja -S $(LLVM_SRC)/llvm -B $(LLVM_NATIVE) \
		-DCMAKE_C_COMPILER=/usr/bin/cc -DCMAKE_CXX_COMPILER=/usr/bin/c++ \
		$(LLVM_COMMON) $(LLVM_TARGET_OPTIONS)
	cmake --build $(LLVM_NATIVE) --target llvm-min-tblgen llvm-tblgen llvm-config

all: $(LLVM_NATIVE)/bin/llvm-config
	cmake -G Ninja -S $(LLVM_SRC)/llvm -B $(LLVM_BUILD) \
		$(CONFIGURE_CMAKE) $(LLVM_COMMON) $(LLVM_LIBRARIES) $(LLVM_TARGET_OPTIONS) \
		-DCMAKE_INSTALL_PREFIX=$(SP_INSTALL_PREFIX)/usr \
		-DCMAKE_PROJECT_LLVM_INCLUDE=$(MAKE_ROOT)tau-llvm-project-include.cmake \
		-DLLVM_TARGET_ARCH=$(LLVM_TARGET) \
		-DLLVM_NATIVE_TOOL_DIR=$(LLVM_NATIVE)/bin \
		-DLLVM_TABLEGEN=$(LLVM_NATIVE)/bin/llvm-tblgen \
		-DHAVE_CXX_ATOMICS_WITHOUT_LIB=ON \
		-DHAVE_CXX_ATOMICS64_WITHOUT_LIB=ON
	cmake --build $(LLVM_BUILD) --parallel --target llvm-libraries
	cmake --build $(LLVM_BUILD) --target $(LLVM_INSTALL_TARGETS)
	mkdir -p $(SP_INSTALL_PREFIX)/usr/bin
	cp -f $(LLVM_NATIVE)/bin/llvm-config $(SP_INSTALL_PREFIX)/usr/bin/llvm-config
	@libdir=$$($(SP_INSTALL_PREFIX)/usr/bin/llvm-config --libdir); \
	test "$$(realpath $$libdir)" = "$$(realpath $(SP_INSTALL_PREFIX)/usr/lib)" \
		|| { echo "llvm-config --libdir is $$libdir, not $(SP_INSTALL_PREFIX)/usr/lib" >&2; exit 1; }; \
	test -f $$libdir/libLLVMCore.a || { echo "no libLLVMCore.a in $$libdir" >&2; exit 1; }

.PHONY: all
