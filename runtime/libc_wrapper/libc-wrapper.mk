# Copyright (c) 2026 Xenolith Team <admin@senolith.studio>
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

#
# Libc umbrella wrapper, that uses libc_impl or platform libc
#

MODULE_RUNTIME_LIBC_WRAPPER_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_RUNTIME_LIBC_WRAPPER_PRIVATE_STANDALONE := 1
MODULE_RUNTIME_LIBC_WRAPPER_LIBS :=
MODULE_RUNTIME_LIBC_WRAPPER_FLAGS :=
MODULE_RUNTIME_LIBC_WRAPPER_GENERAL_CFLAGS :=
MODULE_RUNTIME_LIBC_WRAPPER_GENERAL_CXXFLAGS :=
MODULE_RUNTIME_LIBC_WRAPPER_SRCS_DIRS := $(RUNTIME_MODULE_DIR)/libc_wrapper
MODULE_RUNTIME_LIBC_WRAPPER_SRCS_OBJS :=
MODULE_RUNTIME_LIBC_WRAPPER_INCLUDES_DIRS :=
MODULE_RUNTIME_LIBC_WRAPPER_INCLUDES_OBJS :=
MODULE_RUNTIME_LIBC_WRAPPER_PRIVATE_INCLUDES := \
	$(RUNTIME_MODULE_DIR)/include

MODULE_RUNTIME_LIBC_WRAPPER_PRIVATE_CFLAGS := \
	$(MODULE_RUNTIME_COMMON_CFLAGS)

MODULE_RUNTIME_LIBC_WRAPPER_PRIVATE_CXXFLAGS := \
	$(MODULE_RUNTIME_COMMON_CFLAGS) \
	-nostdinc++ -Wno-unused-command-line-argument


# If target toolchain have include_libc dir, use it
ifdef TARGET_INCLUDE_DIR_LIBC
MODULE_RUNTIME_LIBC_WRAPPER_PRIVATE_CFLAGS += $(addprefix -idirafter ,$(TARGET_INCLUDE_DIR_LIBC))
MODULE_RUNTIME_LIBC_WRAPPER_PRIVATE_CXXFLAGS += $(addprefix -idirafter ,$(TARGET_INCLUDE_DIR_LIBC))
endif # TARGET_INCLUDE_DIR_LIBC


ifneq ($(filter Darwin iOS,$(TARGET_SYSTEM)),)
# Change include ordering by duplicating HOST flags before SDK's flags
MODULE_RUNTIME_LIBC_WRAPPER_PRIVATE_CFLAGS += $(HOST_GENERAL_CFLAGS) \
	-idirafter $(OSTYPE_SDK_PATH)/usr/include -F$(OSTYPE_SDK_PATH)/System/Library/Frameworks
MODULE_RUNTIME_LIBC_WRAPPER_PRIVATE_CXXFLAGS += $(HOST_GENERAL_CFLAGS) \
	-idirafter $(OSTYPE_SDK_PATH)/usr/include -F$(OSTYPE_SDK_PATH)/System/Library/Frameworks
endif # ($(TARGET_SYSTEM),Darwin/iOS)


ifneq ($(filter Android Android-NDK,$(TARGET_SYSTEM)),)
# SPRuntimeCComplex.cpp borrows a few musl complex sources for the <complex.h>
# entries Bionic only ships at API 26 (we target 24). Those sources resolve
# #include "complex_impl.h" against the shim kept here.
MODULE_RUNTIME_LIBC_WRAPPER_PRIVATE_INCLUDES += \
	$(RUNTIME_MODULE_DIR)/libc_wrapper/c/complex
MODULE_RUNTIME_LIBC_WRAPPER_INCLUDES_OBJS += \
	$(RUNTIME_MODULE_DIR)/libc_wrapper/c/complex
endif # ($(TARGET_SYSTEM),Android/Android-NDK)


ifeq ($(TARGET_SYSTEM),NuttX)
# c/SPRuntimeCMathMusl.c borrows the musl math (and the aarch64 fenv it needs)
# for the C99 entries NuttX declares in <math.h> but never implements. Those
# sources reach musl's internal headers ("libm.h", "fp_arch.h", "atomic.h") the
# same way musl's own build does — except through -iquote rather than -I, so
# they apply to `"quoted"` includes only and can never shadow a NuttX or sprt
# <angled> header. C flags only: no C++ unit in this module borrows musl.
MODULE_RUNTIME_LIBC_WRAPPER_PRIVATE_CFLAGS += \
	-iquote $(RUNTIME_MODULE_DIR)/musl-libc/src/internal \
	-iquote $(RUNTIME_MODULE_DIR)/musl-libc/arch/$(TARGET_ARCH) \
	-iquote $(RUNTIME_MODULE_DIR)/musl-libc/arch/generic

# Upstream musl warnings, silenced exactly as musl-adapters/musl_libc.mk does
# for the same sources rather than patched out of the vendored tree.
MODULE_RUNTIME_LIBC_WRAPPER_PRIVATE_CFLAGS += \
	-Wno-shift-op-parentheses \
	-Wno-unused-but-set-variable
endif # ($(TARGET_SYSTEM),NuttX)


ifeq ($(TARGET_SYSTEM),Windows)
MODULE_RUNTIME_LIBC_WRAPPER_PRIVATE_COMMON_FLAGS := \
	-ffreestanding \
	-fbuiltin \
	-funwind-tables \
	-fasynchronous-unwind-tables

MODULE_RUNTIME_LIBC_WRAPPER_PRIVATE_INCLUDES += \
	$(TARGET_INCLUDE_DIR) \
	$(RUNTIME_MODULE_DIR)/include_libc

MODULE_RUNTIME_LIBC_WRAPPER_PRIVATE_CFLAGS += $(MODULE_RUNTIME_LIBC_WRAPPER_PRIVATE_COMMON_FLAGS)
MODULE_RUNTIME_LIBC_WRAPPER_PRIVATE_CXXFLAGS += $(MODULE_RUNTIME_LIBC_WRAPPER_PRIVATE_COMMON_FLAGS)
endif # ($(TARGET_SYSTEM),Windows)


ifeq ($(TARGET_SYSTEM),WASM)
# Freestanding wasm libc, like Windows: -ffreestanding makes __STDC_HOSTED__ == 0
# so the wrapper C units include the runtime's own public libc headers (found in
# include_libc) instead of a host <complex.h>/<stdio.h>/... There is no target
# sysroot to add — the runtime provides the whole libc surface.
MODULE_RUNTIME_LIBC_WRAPPER_PRIVATE_COMMON_FLAGS := \
	-ffreestanding \
	-fbuiltin \
	-funwind-tables \
	-fasynchronous-unwind-tables

MODULE_RUNTIME_LIBC_WRAPPER_PRIVATE_INCLUDES += \
	$(RUNTIME_MODULE_DIR)/include_libc

MODULE_RUNTIME_LIBC_WRAPPER_PRIVATE_CFLAGS += $(MODULE_RUNTIME_LIBC_WRAPPER_PRIVATE_COMMON_FLAGS)
MODULE_RUNTIME_LIBC_WRAPPER_PRIVATE_CXXFLAGS += $(MODULE_RUNTIME_LIBC_WRAPPER_PRIVATE_COMMON_FLAGS)
endif # ($(TARGET_SYSTEM),WASM)

$(call define_module, runtime_libc_wrapper, MODULE_RUNTIME_LIBC_WRAPPER)
