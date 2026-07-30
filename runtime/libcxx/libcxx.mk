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

#
# Vendored libc++ port module (see libcxx/README.adoc). It compiles the out-of-line
# translation units of the ported subsystems (regex, pmr) into the runtime.
#
# Unlike the freestanding runtime modules, these units are hosted-style STL consumers:
# the vendored libc++ code pulls the FULL sprt STL layer (<string>, <locale>,
# <system_error>, ...). They must therefore be built WITHOUT __SPRT_BUILD and WITHOUT
# -nostdinc++ — i.e. exactly like an application that consumes the runtime's std:: —
# and NOT with the freestanding libc mode the other runtime C++ units use. The wrapper
# TUs in src/ #include the verbatim vendored sources under src/libcxx/, so only the
# wrappers are listed (SRCS_OBJS); the vendored sources are never scanned standalone.

MODULE_RUNTIME_LIBCXX_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_RUNTIME_LIBCXX_PRIVATE_STANDALONE := 1
MODULE_RUNTIME_LIBCXX_LIBS :=
MODULE_RUNTIME_LIBCXX_FLAGS :=
MODULE_RUNTIME_LIBCXX_GENERAL_CFLAGS :=
MODULE_RUNTIME_LIBCXX_GENERAL_CXXFLAGS :=
MODULE_RUNTIME_LIBCXX_SRCS_DIRS :=
# Only the wrappers compile; each #includes the verbatim vendored TU under src/libcxx/.
MODULE_RUNTIME_LIBCXX_SRCS_OBJS := \
	$(RUNTIME_MODULE_DIR)/libcxx/src/SPRTCxxRegex.cpp \
	$(RUNTIME_MODULE_DIR)/libcxx/src/SPRTCxxMemoryResource.cpp \
	$(RUNTIME_MODULE_DIR)/libcxx/src/SPRTCxxValarray.cpp \
	$(RUNTIME_MODULE_DIR)/libcxx/src/SPRTCxxRandom.cpp \
	$(RUNTIME_MODULE_DIR)/libcxx/src/SPRTCxxChrono.cpp \
	$(RUNTIME_MODULE_DIR)/libcxx/src/SPRTCxxSystemError.cpp \
	$(RUNTIME_MODULE_DIR)/libcxx/src/SPRTCxxErrorCategory.cpp \
	$(RUNTIME_MODULE_DIR)/libcxx/src/SPRTCxxString.cpp \
	$(RUNTIME_MODULE_DIR)/libcxx/src/SPRTCxxHash.cpp \
	$(RUNTIME_MODULE_DIR)/libcxx/src/SPRTCxxMemory.cpp \
	$(RUNTIME_MODULE_DIR)/libcxx/src/SPRTCxxLocale.cpp \
	$(RUNTIME_MODULE_DIR)/libcxx/src/SPRTCxxIos.cpp \
	$(RUNTIME_MODULE_DIR)/libcxx/src/SPRTCxxOstream.cpp \
	$(RUNTIME_MODULE_DIR)/libcxx/src/SPRTCxxFstream.cpp \
	$(RUNTIME_MODULE_DIR)/libcxx/src/SPRTCxxStrstream.cpp \
	$(RUNTIME_MODULE_DIR)/libcxx/src/SPRTCxxFilesystem.cpp \
	$(RUNTIME_MODULE_DIR)/libcxx/src/SPRTCxxCharconv.cpp \
	$(RUNTIME_MODULE_DIR)/libcxx/src/SPRTCxxStdexcept.cpp \
	$(RUNTIME_MODULE_DIR)/libcxx/src/SPRTCxxThreading.cpp \
	$(RUNTIME_MODULE_DIR)/libcxx/src/SPRTCxxThreadDtors.cpp \
	$(RUNTIME_MODULE_DIR)/libcxx/src/SPRTCxxFuture.cpp \
	$(RUNTIME_MODULE_DIR)/libcxx/src/SPRTCxxBind.cpp \
	$(RUNTIME_MODULE_DIR)/libcxx/src/SPRTCxxNewHelpers.cpp \
	$(RUNTIME_MODULE_DIR)/libcxx/src/SPRTCxxVerboseAbort.cpp \
	$(RUNTIME_MODULE_DIR)/libcxx/src/SPRTCxxIostream.cpp \
	$(RUNTIME_MODULE_DIR)/libcxx/src/SPRTCxxAlgorithm.cpp
MODULE_RUNTIME_LIBCXX_INCLUDES_DIRS :=
MODULE_RUNTIME_LIBCXX_INCLUDES_OBJS :=

# The port entry shims resolve here: libcxx/include (shim layer + vendored headers),
# then the sprt STL layer, then the sprt libc. Matches the include order the rest of
# the STL uses (see runtime.mk); include is the sprt public root.
MODULE_RUNTIME_LIBCXX_PRIVATE_INCLUDES := \
	$(RUNTIME_MODULE_DIR)/include

MODULE_RUNTIME_LIBCXX_PRIVATE_CFLAGS := \
	$(MODULE_RUNTIME_COMMON_CFLAGS)

MODULE_RUNTIME_LIBCXX_PRIVATE_CXXFLAGS := \
	$(MODULE_RUNTIME_COMMON_CFLAGS) \
	-frtti -funwind-tables -Wno-unused-command-line-argument -nostdinc++\
	-isystem $(RUNTIME_MODULE_DIR)/include_libc/cxx \
	-isystem $(RUNTIME_MODULE_DIR)/libcxx/include \
	-isystem $(RUNTIME_MODULE_DIR)/include_libc \
	-idirafter $(RUNTIME_MODULE_DIR)/libcxx/src/libcxx \
	-idirafter $(RUNTIME_MODULE_DIR)/libcxx/src/libcxx/libc-shared


ifeq ($(TARGET_SYSTEM),Android-NDK)
MODULE_RUNTIME_LIBCXX_GENERAL_CXXFLAGS := \
	-idirafter $(RUNTIME_MODULE_DIR)/libcxx/src/libcxx \
	-idirafter $(RUNTIME_MODULE_DIR)/libcxx/src/libcxx/libc-shared
endif

ifeq ($(TARGET_SYSTEM),Windows)
# sprt/wrappers/windows is the sprt <windows.h> (+ casemap) surface: several vendored
# libc++ TUs (system_error.cpp, chrono.cpp, fstream.cpp, filesystem/*) include <windows.h>
# directly for the Win32 API. It must be on the path here exactly as the runtime module
# adds it (runtime.mk) and the rest of the toolchain does (toolchains/common/configure.mk).
MODULE_RUNTIME_LIBCXX_PRIVATE_INCLUDES += \
	$(TARGET_INCLUDE_DIR) \
	$(RUNTIME_MODULE_DIR)/include/sprt/wrappers/windows \
	$(RUNTIME_MODULE_DIR)/include/sprt/wrappers/windows/casemap \
	$(RUNTIME_MODULE_DIR)/include_libc
MODULE_RUNTIME_LIBCXX_PRIVATE_CXXFLAGS += -fbuiltin -fasynchronous-unwind-tables
endif # Windows

ifeq ($(TARGET_SYSTEM),WASM)
MODULE_RUNTIME_LIBCXX_PRIVATE_INCLUDES += \
	$(RUNTIME_MODULE_DIR)/include_libc
MODULE_RUNTIME_LIBCXX_PRIVATE_CXXFLAGS += -fbuiltin -fasynchronous-unwind-tables
endif # WASM

$(call define_module, runtime_libcxx, MODULE_RUNTIME_LIBCXX)
