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

.DEFAULT_GOAL := all

LIBNAME = wasm-micro-runtime

# Patched for winapi should be applied

SP_USER_CFLAGS := -DWASM_API_EXTERN= -D_CRT_SECURE_NO_WARNINGS
SP_USER_CXXFLAGS := -DWASM_API_EXTERN= -D_CRT_SECURE_NO_WARNINGS

include ../common/configure.mk

CONFIGURE_WAMR := \
	-DWAMR_BUILD_INTERP=1 \
	-DWAMR_BUILD_JIT=0 \
	-DWAMR_BUILD_LIBC_BUILTIN=1 \
	-DWAMR_BUILD_LIBC_WASI=1 \
	-DWAMR_BUILD_TAIL_CALL=1 \
	-DWAMR_BUILD_REF_TYPES=1 \
	-DWAMR_BUILD_MODULE_INST_CONTEXT=1 \
	-DWAMR_BUILD_MULTI_MODULE=1 \
	-DWAMR_BUILD_BULK_MEMORY=1 \
	-DWAMR_BUILD_SIMD=1 \
	-DWAMR_BUILD_MEMORY64=1 \
	-DWAMR_BUILD_TARGET=X86_64 \
	-DWAMR_BUILD_AOT=1

ifdef LINUX

CONFIGURE_WAMR += \
	-DWAMR_BUILD_PLATFORM=linux \
	-DWAMR_BUILD_LIB_PTHREAD=1 \
	-DWAMR_BUILD_LIB_PTHREAD_SEMAPHORE=1 \
	-DWAMR_BUILD_LIB_WASI_THREADS=1 \

ifeq ($(ARCH),e2k)
CONFIGURE_WAMR += \
	-DWAMR_BUILD_TARGET=E2K_64 \
	-DWAMR_BUILD_AOT=0
endif

ifeq ($(ARCH),aarch64)
CONFIGURE_WAMR += \
	-DWAMR_BUILD_TARGET=AARCH64 \
	-DWAMR_BUILD_AOT=1
endif

ifeq ($(ARCH),riscv64)
CONFIGURE_WAMR += \
	-DWAMR_BUILD_TARGET=RISCV64 \
	-DWAMR_BUILD_AOT=1
endif

ifeq ($(ARCH),x86_64)
CONFIGURE_WAMR += \
	-DWAMR_BUILD_TARGET=X86_64 \
	-DWAMR_BUILD_AOT=1
endif

endif # LINUX


ifdef WINDOWS

CONFIGURE_WAMR += \
	-DWAMR_BUILD_PLATFORM=windows \
	-DWAMR_BUILD_TARGET=X86_64 \
	-DWAMR_BUILD_LIB_PTHREAD=0 \
	-DWAMR_BUILD_LIB_PTHREAD_SEMAPHORE=0 \
	-DWAMR_BUILD_LIB_WASI_THREADS=0 \

endif # WINDOWS


ifdef ANDROID

CONFIGURE_WAMR += \
	-DWAMR_BUILD_PLATFORM=android \
	-DWAMR_BUILD_LIB_PTHREAD=1 \
	-DWAMR_BUILD_LIB_PTHREAD_SEMAPHORE=1 \
	-DWAMR_BUILD_LIB_WASI_THREADS=1 \

ifeq ($(ARCH),armeabi-v7a)
CONFIGURE_WAMR += \
	-DWAMR_BUILD_MEMORY64=0 \
	-DWAMR_BUILD_TARGET=ARMV7
endif

ifeq ($(ARCH),arm64-v8a)
CONFIGURE_WAMR += \
	-DWAMR_BUILD_TARGET=AARCH64
endif

ifeq ($(ARCH),x86_64)
CONFIGURE_WAMR += \
	-DWAMR_BUILD_TARGET=X86_64
endif

ifeq ($(ARCH),x86)
CONFIGURE_WAMR += \
	-DWAMR_BUILD_MEMORY64=0 \
	-DWAMR_BUILD_TARGET=X86_32
endif

endif # ANDROID



ifdef DARWIN

CONFIGURE_WAMR += \
	-DWAMR_BUILD_PLATFORM=darwin \
	-DWAMR_BUILD_LIB_PTHREAD=1 \
	-DWAMR_BUILD_LIB_PTHREAD_SEMAPHORE=1 \
	-DWAMR_BUILD_LIB_WASI_THREADS=1 \

ifeq ($(SP_ARCH),arm64)
CONFIGURE_WAMR += \
	-DWAMR_BUILD_TARGET=AARCH64 \
	-DWAMR_BUILD_AOT=1
endif

ifeq ($(SP_ARCH),x86_64)
CONFIGURE_WAMR += \
	-DWAMR_BUILD_TARGET=X86_64 \
	-DWAMR_BUILD_AOT=1
endif

endif # LINUX


CONFIGURE := \
	$(CONFIGURE_CMAKE) \
	$(CONFIGURE_WAMR) \
	-DWAMR_BUILD_FAST_INTERP=1

CONFIGURE_DEBUG := \
	$(CONFIGURE_CMAKE) \
	$(CONFIGURE_WAMR) \
	-DWAMR_DISABLE_WRITE_GS_BASE=1 \
	-DWAMR_BUILD_DEBUG_INTERP=1 \
	-DWAMR_BUILD_DEBUG_AOT=1 \
	-DWAMR_BUILD_FAST_INTERP=0

INCLUDES := $(SP_INSTALL_PREFIX)/usr/include/wamr/wasm_export.h $(SP_INSTALL_PREFIX)/usr/include/wamr/lib_export.h

$(SP_INSTALL_PREFIX)/usr/include/wamr/wasm_export.h:
	$(call rule_mkdir,$(SP_INSTALL_PREFIX)/usr/include/wamr)
	$(call rule_cp,$(LIB_SRC_DIR)/$(LIBNAME)/core/iwasm/include/wasm_export.h,$(SP_INSTALL_PREFIX)/usr/include/wamr/wasm_export.h)

$(SP_INSTALL_PREFIX)/usr/include/wamr/lib_export.h:
	$(call rule_mkdir,$(SP_INSTALL_PREFIX)/usr/include/wamr)
	$(call rule_cp,$(LIB_SRC_DIR)/$(LIBNAME)/core/iwasm/include/lib_export.h,$(SP_INSTALL_PREFIX)/usr/include/wamr/lib_export.h)

$(SP_INSTALL_PREFIX)/usr/lib/$(call mklibname,iwasm-release): $(lastword $(MAKEFILE_LIST))
	$(call rule_rm,$(LIBNAME))
	$(call rule_mkdir,$(LIBNAME))
	cd $(LIBNAME); cmake -G "Ninja" $(CONFIGURE) $(LIB_SRC_DIR)/$(LIBNAME)
	cd $(LIBNAME); cmake  --build . --config Release --parallel
	$(call rule_cp,$(LIBNAME)/$(call mklibname,iwasm),$(SP_INSTALL_PREFIX)/usr/lib/$(call mklibname,iwasm-release))
	$(call rule_rm,$(LIBNAME))

$(SP_INSTALL_PREFIX)/usr/lib/$(call mklibname,iwasm-debug): $(lastword $(MAKEFILE_LIST))
	$(call rule_rm,$(LIBNAME))
	$(call rule_mkdir,$(LIBNAME))
	cd $(LIBNAME); cmake -G "Ninja" $(CONFIGURE_DEBUG) $(LIB_SRC_DIR)/$(LIBNAME)
	cd $(LIBNAME); cmake  --build . --config Debug --parallel
	$(call rule_cp,$(LIBNAME)/$(call mklibname,iwasm),$(SP_INSTALL_PREFIX)/usr/lib/$(call mklibname,iwasm-debug))
	$(call rule_rm,$(LIBNAME))

all: $(INCLUDES) \
	$(SP_INSTALL_PREFIX)/usr/lib/$(call mklibname,iwasm-release) \
	$(SP_INSTALL_PREFIX)/usr/lib/$(call mklibname,iwasm-debug)

.PHONY: all
