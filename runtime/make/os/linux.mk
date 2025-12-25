# Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>
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

ifeq ($(LOCAL_USE_INTERNAL_TOOLCHAIN),1)
ifdef STAPPLER_ARCH
STAPPLER_TOOLCHAIN_ARCH := $(STAPPLER_ARCH)
else
STAPPLER_TOOLCHAIN_ARCH ?= $(shell uname -m)
endif

STAPPLER_TOOLCHAIN_VENDOR := unknown
STAPPLER_TOOLCHAIN_SYS := linux
STAPPLER_TOOLCHAIN_ENV := gnu

STAPPLER_TOOLCHAIN_FULL := $(STAPPLER_TOOLCHAIN_ARCH)-$(STAPPLER_TOOLCHAIN_VENDOR)-$(STAPPLER_TOOLCHAIN_SYS)-$(STAPPLER_TOOLCHAIN_ENV)
STAPPLER_TOOLCHAIN_ROOT := $(abspath $(dir $(BUILD_ROOT)))/toolchains/targets
STAPPLER_TOOLCHAIN_RUNTIME_INCLUDE := $(realpath $(dir $(BUILD_ROOT))/toolchains/runtime/include)

-include $(STAPPLER_TOOLCHAIN_ROOT)/$(STAPPLER_TOOLCHAIN_FULL)/toolchain.mk

ifdef TOOLCHAIN_SYSROOT
include $(BUILD_ROOT)/utils/apply-toolchain.mk
endif

endif

OSTYPE_IS_LINUX := 1

OSTYPE_ARCH ?= $(STAPPLER_ARCH)

OSTYPE_DEPS :=
OSTYPE_PREBUILT_PATH :=
OSTYPE_INCLUDE := $(STAPPLER_TOOLCHAIN_RUNTIME_INCLUDE)

ifndef BUILD_SHARED
ifndef TOOLCHAIN_SYSROOT
ifndef LOCAL_SHARED_DEPS
OSTYPE_DEPS += deps/linux/$(OSTYPE_ARCH)
OSTYPE_PREBUILT_PATH += $(OSTYPE_DEPS)/lib
OSTYPE_INCLUDE += $(OSTYPE_DEPS)/include
endif
endif
endif

OSTYPE_EXEC_SUFFIX :=
OSTYPE_DSO_SUFFIX := .so
OSTYPE_LIB_SUFFIX := .a
OSTYPE_LIB_PREFIX := lib

OSTYPE_CONFIG_FLAGS := LINUX

OSTYPE_LIBS_ALLOW_SHARED_DEPS := 1

OSTYPE_GENERAL_CFLAGS := -Wall -fvisibility=hidden
OSTYPE_LIB_CFLAGS := -fPIC -DPIC
OSTYPE_EXEC_CFLAGS :=

OSTYPE_GENERAL_CXXFLAGS := -Wall -Wno-overloaded-virtual -frtti -fvisibility=hidden -fvisibility-inlines-hidden
OSTYPE_LIB_CXXFLAGS := -fPIC -DPIC
OSTYPE_EXEC_CXXFLAGS :=

OSTYPE_GENERAL_LDFLAGS :=
OSTYPE_EXEC_LDFLAGS := 
OSTYPE_LIB_LDFLAGS := -rdynamic -Wl,--exclude-libs,ALL

ifdef TOOLCHAIN_SYSROOT
OSTYPE_GENERAL_CFLAGS += --sysroot $(TOOLCHAIN_SYSROOT)
OSTYPE_GENERAL_CXXFLAGS += --sysroot $(TOOLCHAIN_SYSROOT)
OSTYPE_GENERAL_LDFLAGS += --sysroot $(TOOLCHAIN_SYSROOT)
endif

# ldgold only tested for x86_64 linux
# For others - use default ld
ifndef TOOLCHAIN_TARGET
ifeq ($(OSTYPE_ARCH),x86_64)
	OSTYPE_GENERAL_LDFLAGS += -fuse-ld=gold
endif
endif

ifeq ($(CLANG),1)
	OSTYPE_GENERAL_CXXFLAGS += -Wno-unneeded-internal-declaration -Wno-gnu-string-literal-operator-template \
		-Wno-vla-cxx-extension -Wno-unqualified-std-cast-call
else
	OSTYPE_GENERAL_CXXFLAGS += -Wno-class-memaccess
endif

ifeq ($(ASAN),1)
	OSTYPE_GENERAL_CFLAGS += -fsanitize=address
	OSTYPE_GENERAL_CXXFLAGS += -fsanitize=address
	OSTYPE_EXEC_LDFLAGS += -fsanitize=address -static-libasan
else

endif

ifeq ($(OSTYPE_ARCH),e2k)

OSTYPE_CONFIG_FLAGS += SP_DEDICATED_SIMD

# warning about new/delete pairing for exceptions is wrong, placement new in stappler is noexcept
OSTYPE_GENERAL_CFLAGS += -w830
OSTYPE_GENERAL_CXXFLAGS += -w830

ifneq ($(OSTYPE_ARCH),$(UNAME_ARCH))

LCC_ROOT ?= /opt/mcst/lcc-1.26.20.e2k-v4.5.4
LOCAL_PATH := $(LCC_ROOT)/bin.toolchain:$(PATH)
export PATH = $(LOCAL_PATH)

endif
endif # ($(STAPPLER_ARCH),e2k)

ifdef BUILD_SHARED

OSTYPE_LIB_LDFLAGS += -Wl,-z,defs

endif # BUILD_SHARED
