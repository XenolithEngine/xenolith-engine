# Copyright (c) 2025 Stappler Team <admin@stappler.org>
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

RUNTIME_MODULE_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

# Isolate the umbrella libc so that other modules do not see the platform libc headers
MODULE_RUNTIME_LIBC_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_RUNTIME_LIBC_LIBS :=
MODULE_RUNTIME_LIBC_FLAGS :=
MODULE_RUNTIME_LIBC_GENERAL_CFLAGS :=
MODULE_RUNTIME_LIBC_GENERAL_CXXFLAGS :=
MODULE_RUNTIME_LIBC_SRCS_DIRS := $(RUNTIME_MODULE_DIR)/src_libc
MODULE_RUNTIME_LIBC_SRCS_OBJS :=
MODULE_RUNTIME_LIBC_INCLUDES_DIRS :=
MODULE_RUNTIME_LIBC_INCLUDES_OBJS := \
	$(RUNTIME_MODULE_DIR)/include \
	$(RUNTIME_MODULE_DIR)/src

ifdef TOOLCHAIN_INCLUDEDIR_LIBC
MODULE_RUNTIME_LIBC_PRIVATE_CXXFLAGS := $(addprefix -idirafter ,$(TOOLCHAIN_INCLUDEDIR_LIBC))
endif

$(call define_module, runtime_libc, MODULE_RUNTIME_LIBC)

ifdef WIN32
MODULE_RUNTIME_LIBC_PRIVATE_FLAGS_FILTER := -nostdinc
MODULE_RUNTIME_LIBC_PRIVATE_CFLAGS :=
MODULE_RUNTIME_LIBC_PRIVATE_CXXFLAGS :=
endif


MODULE_RUNTIME_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_RUNTIME_LIBS :=
MODULE_RUNTIME_FLAGS :=
MODULE_RUNTIME_GENERAL_CFLAGS :=
MODULE_RUNTIME_GENERAL_CXXFLAGS :=
MODULE_RUNTIME_SRCS_DIRS := $(RUNTIME_MODULE_DIR)/src
MODULE_RUNTIME_SRCS_OBJS :=
MODULE_RUNTIME_INCLUDES_DIRS :=
MODULE_RUNTIME_INCLUDES_OBJS :=

MODULE_RUNTIME_INCLUDES_INTERNAL := \
	$(RUNTIME_MODULE_DIR)/src

MODULE_RUNTIME_DEPENDS_ON := runtime_libc

ifdef LINUX
MODULE_RUNTIME_GENERAL_CFLAGS += -idirafter $(RUNTIME_MODULE_DIR)/include_libc
MODULE_RUNTIME_GENERAL_CXXFLAGS += -idirafter $(RUNTIME_MODULE_DIR)/include_libc
MODULE_RUNTIME_LIBS += -ldl -l:libbacktrace.a -l:libicuuc.a -l:libicudata.a
endif

ifdef ANDROID
MODULE_RUNTIME_GENERAL_CFLAGS += -nostdinc++
MODULE_RUNTIME_GENERAL_CXXFLAGS += -nostdinc++
MODULE_RUNTIME_INCLUDES_OBJS += \
	$(ANDROID_SYSROOT_INCLUDE_CXX) \
	$(RUNTIME_MODULE_DIR)/include_libc
MODULE_RUNTIME_LIBS += -ldl -l:libbacktrace.a
endif

ifdef MACOS
MODULE_RUNTIME_GENERAL_LDFLAGS += -framework CoreFoundation  -framework Foundation
endif

ifdef WIN32
MODULE_RUNTIME_GENERAL_CFLAGS += -idirafter $(RUNTIME_MODULE_DIR)/include_libc
MODULE_RUNTIME_GENERAL_CXXFLAGS += -idirafter $(RUNTIME_MODULE_DIR)/include_libc

MODULE_RUNTIME_LIBS_COMMON := -lSynchronization -lShlwapi -lPathcch -lOle32 -lNormaliz -lAdvapi32 -lBcrypt \
	-lUserenv -lOneCore -lUser32 -lShell32 -lDxva2 -lUxTheme -lDwmapi

ifeq ($(BUILD_TYPE),debug)
MODULE_RUNTIME_LIBS += -llibucrtd -llibvcruntimed -llibcmtd -llibcpmtd $(MODULE_RUNTIME_LIBS_COMMON)
else
MODULE_RUNTIME_LIBS += -llibucrt -llibvcruntime -llibcmt -llibcpmt $(MODULE_RUNTIME_LIBS_COMMON)
endif

endif

#spec

MODULE_RUNTIME_SHARED_SPEC_SUMMARY := Xenolith platform-specific runtime

define MODULE_RUNTIME_SHARED_SPEC_DESCRIPTION
Xenolith platform-specific runtime
endef

# module name resolution
$(call define_module, runtime, MODULE_RUNTIME)
