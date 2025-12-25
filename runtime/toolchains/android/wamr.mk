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

.DEFAULT_GOAL := all

LIBNAME = wasm-micro-runtime

include ../common/configure.mk

CONFIGURE_ARGS := \
	$(CONFIGURE_CMAKE) \
	-DWAMR_BUILD_PLATFORM=android \
	-DWAMR_BUILD_INTERP=1 \
	-DWAMR_BUILD_AOT=1 \
	-DWAMR_BUILD_JIT=0 \
	-DWAMR_BUILD_LIBC_BUILTIN=1 \
	-DWAMR_BUILD_LIBC_WASI=1 \
	-DWAMR_BUILD_TAIL_CALL=1 \
	-DWAMR_BUILD_REF_TYPES=1 \
	-DWAMR_BUILD_MODULE_INST_CONTEXT=1 \
	-DWAMR_BUILD_MULTI_MODULE=1 \
	-DWAMR_BUILD_BULK_MEMORY=1 \
	-DWAMR_BUILD_SIMD=0 \
	-DWAMR_BUILD_LIB_PTHREAD=1 \
	-DWAMR_BUILD_LIB_PTHREAD_SEMAPHORE=1 \
	-DWAMR_BUILD_LIB_WASI_THREADS=1

ifeq ($(ARCH),armeabi-v7a)
CONFIGURE_ARGS += \
	-DWAMR_BUILD_TARGET=ARMV7
endif

ifeq ($(ARCH),arm64-v8a)
CONFIGURE_ARGS += \
	-DWAMR_BUILD_TARGET=AARCH64
endif

ifeq ($(ARCH),x86_64)
CONFIGURE_ARGS += \
	-DWAMR_BUILD_TARGET=X86_64
endif

ifeq ($(ARCH),x86)
CONFIGURE_ARGS += \
	-DWAMR_BUILD_TARGET=X86_32
endif

CONFIGURE := \
	$(CONFIGURE_ARGS) \
	-DWAMR_BUILD_FAST_INTERP=1

CONFIGURE_DEBUG := \
	$(CONFIGURE_ARGS) \
	-DWAMR_DISABLE_WRITE_GS_BASE=1 \
	-DWAMR_BUILD_DEBUG_INTERP=1 \
	-DWAMR_BUILD_DEBUG_AOT=1 \
	-DWAMR_BUILD_FAST_INTERP=0

INCLUDES := $(SP_INSTALL_PREFIX)/include/wamr/wasm_export.h $(SP_INSTALL_PREFIX)/include/wamr/lib_export.h

$(SP_INSTALL_PREFIX)/include/wamr/wasm_export.h:
	mkdir -p $(SP_INSTALL_PREFIX)/include/wamr
	cp -f $(LIB_SRC_DIR)/$(LIBNAME)/core/iwasm/include/wasm_export.h $(SP_INSTALL_PREFIX)/include/wamr/wasm_export.h

$(SP_INSTALL_PREFIX)/include/wamr/lib_export.h:
	mkdir -p $(SP_INSTALL_PREFIX)/include/wamr
	cp -f $(LIB_SRC_DIR)/$(LIBNAME)/core/iwasm/include/lib_export.h $(SP_INSTALL_PREFIX)/include/wamr/lib_export.h

$(SP_INSTALL_PREFIX)/lib/libiwasm-release.a:
	@mkdir -p $(LIBNAME)
	cd $(LIBNAME); cmake $(CONFIGURE) $(LIB_SRC_DIR)/$(LIBNAME)
	cd $(LIBNAME); cmake  --build . --config Release --parallel
	cp -f $(LIBNAME)/libiwasm.a $(SP_INSTALL_PREFIX)/lib/libiwasm-release.a
	rm -rf $(LIBNAME)

$(SP_INSTALL_PREFIX)/lib/libiwasm-debug.a:
	@mkdir -p $(LIBNAME)
	cd $(LIBNAME); cmake $(CONFIGURE_DEBUG) $(LIB_SRC_DIR)/$(LIBNAME)
	cd $(LIBNAME); cmake  --build . --config Debug --parallel
	cp -f $(LIBNAME)/libiwasm.a $(SP_INSTALL_PREFIX)/lib/libiwasm-debug.a
	rm -rf $(LIBNAME)

all: $(INCLUDES) $(SP_INSTALL_PREFIX)/lib/libiwasm-release.a $(SP_INSTALL_PREFIX)/lib/libiwasm-debug.a

.PHONY: all
