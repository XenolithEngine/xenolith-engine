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

#
# Runtime core functions, that can be used to build runtime_libc_impl
#

MODULE_RUNTIME_CORE_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_RUNTIME_CORE_PRIVATE_STANDALONE := 1
MODULE_RUNTIME_CORE_SRCS_DIRS := \
	$(RUNTIME_MODULE_DIR)/core
MODULE_RUNTIME_CORE_PRIVATE_INCLUDES := \
	$(RUNTIME_MODULE_DIR)/include

$(call define_module, runtime_core, MODULE_RUNTIME_CORE)


#
# jemalloc for builtin libc
#

MODULE_RUNTIME_JEMALLOC_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_RUNTIME_JEMALLOC_PRIVATE_STANDALONE := 1
MODULE_RUNTIME_JEMALLOC_SRCS_OBJS := \
	$(RUNTIME_MODULE_DIR)/libc_impl/jemalloc/jemalloc.scu.c
MODULE_RUNTIME_JEMALLOC_PRIVATE_INCLUDES := \
	$(RUNTIME_MODULE_DIR)/include \
	$(RUNTIME_MODULE_DIR)/include_libc \
	$(RUNTIME_MODULE_DIR)/libc_impl/jemalloc/include

MODULE_RUNTIME_JEMALLOC_PRIVATE_COMMON_CFLAGS := \
	-nostdinc \
	-ffreestanding \
	-fbuiltin \
	-fno-unwind-tables \
	-fno-asynchronous-unwind-tables \
	-DJEMALLOC_NO_PRIVATE_NAMESPACE

MODULE_RUNTIME_JEMALLOC_PRIVATE_CFLAGS += $(MODULE_RUNTIME_JEMALLOC_PRIVATE_COMMON_CFLAGS)
MODULE_RUNTIME_JEMALLOC_PRIVATE_CXXFLAGS += $(MODULE_RUNTIME_JEMALLOC_PRIVATE_COMMON_CFLAGS)

$(call define_module, runtime_jemalloc, MODULE_RUNTIME_JEMALLOC)


#
# Adapters for musl libc functions
#

MODULE_RUNTIME_MUSL_LIBC_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_RUNTIME_MUSL_LIBC_PRIVATE_STANDALONE := 1
MODULE_RUNTIME_MUSL_LIBC_DEPENDS_ON := runtime_jemalloc runtime_core
MODULE_RUNTIME_MUSL_LIBC_SRCS_DIRS := \
	$(RUNTIME_MODULE_DIR)/musl-adapters
MODULE_RUNTIME_MUSL_LIBC_PRIVATE_INCLUDES := \
	$(RUNTIME_MODULE_DIR)/musl-adapters/include \
	$(RUNTIME_MODULE_DIR)/musl-libc/arch/$(TARGET_ARCH) \
	$(RUNTIME_MODULE_DIR)/musl-libc/arch/generic \
	$(RUNTIME_MODULE_DIR)/musl-libc/src/internal \
	$(RUNTIME_MODULE_DIR)/musl-libc/src/include \
	$(RUNTIME_MODULE_DIR)/musl-libc/include \
	$(RUNTIME_MODULE_DIR)/include \

MODULE_RUNTIME_MUSL_LIBC_PRIVATE_COMMON_FLAGS := \
	-Wno-pointer-to-int-cast \
	-Werror=implicit-function-declaration \
	-Werror=implicit-int \
	-Werror=pointer-sign \
	-Werror=pointer-arith \
	-Werror=int-conversion \
	-Werror=incompatible-pointer-types \
	-Werror=ignored-qualifiers \
	-Waddress \
	-Warray-bounds \
	-Wchar-subscripts \
	-Wduplicate-decl-specifier \
	-Winit-self \
	-Wreturn-type \
	-Wsequence-point \
	-Wstrict-aliasing \
	-Wunused-function \
	-Wunused-label \
	-Wunused-variable \
	-Wno-bitwise-op-parentheses \
	-Wno-shift-op-parentheses \
	-Wno-unused-but-set-variable

MODULE_RUNTIME_MUSL_LIBC_PRIVATE_COMMON_CFLAGS := \
	$(MODULE_RUNTIME_MUSL_LIBC_PRIVATE_COMMON_FLAGS) \
	-nostdinc \
	-ffreestanding \
	-fexcess-precision=standard \
	-frounding-math \
	-fno-strict-aliasing \
	-fno-align-functions \
	-fomit-frame-pointer \
	-fno-unwind-tables \
	-fno-asynchronous-unwind-tables

MODULE_RUNTIME_MUSL_LIBC_PRIVATE_CFLAGS += $(MODULE_RUNTIME_MUSL_LIBC_PRIVATE_COMMON_CFLAGS) 
	-std=c99 -pipe
MODULE_RUNTIME_MUSL_LIBC_PRIVATE_CXXFLAGS += $(MODULE_RUNTIME_MUSL_LIBC_PRIVATE_COMMON_CFLAGS)

$(call define_module, runtime_musl_libc, MODULE_RUNTIME_MUSL_LIBC)


#
# Standalone libc implementation, based on musl-libc with platform-specific extensions
#

MODULE_RUNTIME_LIBC_IMPL_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_RUNTIME_LIBC_IMPL_PRIVATE_STANDALONE := 1
MODULE_RUNTIME_LIBC_IMPL_DEPENDS_ON := runtime_jemalloc runtime_musl_libc runtime_core
MODULE_RUNTIME_LIBC_IMPL_SRCS_DIRS := \
	$(RUNTIME_MODULE_DIR)/libc_impl/src \
	$(RUNTIME_MODULE_DIR)/libc_impl/asm/$(TARGET_SYSTEM)/$(TARGET_ARCH)
MODULE_RUNTIME_LIBC_IMPL_PRIVATE_INCLUDES := \
	$(RUNTIME_MODULE_DIR)/include \
	$(RUNTIME_MODULE_DIR)/include_libc

MODULE_RUNTIME_LIBC_IMPL_PRIVATE_COMMON_CFLAGS := -nostdinc -ffreestanding -fbuiltin

MODULE_RUNTIME_LIBC_IMPL_PRIVATE_SFLAGS += $(MODULE_RUNTIME_LIBC_IMPL_PRIVATE_COMMON_FLAGS)
MODULE_RUNTIME_LIBC_IMPL_PRIVATE_CFLAGS += $(MODULE_RUNTIME_LIBC_IMPL_PRIVATE_COMMON_CFLAGS) 
	-std=c99 -pipe
MODULE_RUNTIME_LIBC_IMPL_PRIVATE_CXXFLAGS += $(MODULE_RUNTIME_LIBC_IMPL_PRIVATE_COMMON_CFLAGS)

$(call define_module, runtime_libc_impl, MODULE_RUNTIME_LIBC_IMPL)


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

MODULE_RUNTIME_LIBC_WRAPPER_PRIVATE_CXXFLAGS := -nostdinc++

ifdef TARGET_INCLUDE_DIR_LIBC
MODULE_RUNTIME_LIBC_WRAPPER_PRIVATE_CFLAGS += $(addprefix -idirafter ,$(TARGET_INCLUDE_DIR_LIBC))
MODULE_RUNTIME_LIBC_WRAPPER_PRIVATE_CXXFLAGS += $(addprefix -idirafter ,$(TARGET_INCLUDE_DIR_LIBC))
endif

ifeq ($(TARGET_SYSTEM),Darwin)
# Change include ordering by duplicating HOST flags before SDK's flags
MODULE_RUNTIME_LIBC_WRAPPER_PRIVATE_CFLAGS += $(HOST_GENERAL_CFLAGS) \
	-idirafter $(OSTYPE_SDK_PATH)/usr/include -F$(OSTYPE_SDK_PATH)/System/Library/Frameworks
MODULE_RUNTIME_LIBC_WRAPPER_PRIVATE_CXXFLAGS += $(HOST_GENERAL_CFLAGS) \
	-idirafter $(OSTYPE_SDK_PATH)/usr/include -F$(OSTYPE_SDK_PATH)/System/Library/Frameworks
endif

ifeq ($(TARGET_SYSTEM),Windows)
MODULE_RUNTIME_LIBC_WRAPPER_PRIVATE_INCLUDES += $(TARGET_INCLUDE_DIR)
MODULE_RUNTIME_LIBC_WRAPPER_PRIVATE_CFLAGS += -ffreestanding -fbuiltin -funwind-tables -fasynchronous-unwind-tables
MODULE_RUNTIME_LIBC_WRAPPER_PRIVATE_CXXFLAGS += -ffreestanding -fbuiltin -funwind-tables -fasynchronous-unwind-tables
endif

$(call define_module, runtime_libc_wrapper, MODULE_RUNTIME_LIBC_WRAPPER)


MODULE_RUNTIME_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_RUNTIME_PRIVATE_STANDALONE := 1
MODULE_RUNTIME_LIBS :=
MODULE_RUNTIME_FLAGS :=
MODULE_RUNTIME_GENERAL_CFLAGS :=
MODULE_RUNTIME_GENERAL_CXXFLAGS := -nostdinc++
MODULE_RUNTIME_SRCS_DIRS := $(RUNTIME_MODULE_DIR)/src
MODULE_RUNTIME_SRCS_OBJS :=
MODULE_RUNTIME_INCLUDES_DIRS :=
MODULE_RUNTIME_INCLUDES_OBJS := \
	$(RUNTIME_MODULE_DIR)/include \
	$(RUNTIME_MODULE_DIR)/include/sprt/runtime/geom/glsl

MODULE_RUNTIME_SHADERS_INCLUDE := $(RUNTIME_MODULE_DIR)/include/sprt/runtime/geom/glsl

MODULE_RUNTIME_PRIVATE_INCLUDES := \
	$(RUNTIME_MODULE_DIR)/include \
	$(RUNTIME_MODULE_DIR)/include_libc \
	$(RUNTIME_MODULE_DIR)/src

MODULE_RUNTIME_DEPENDS_ON := runtime_libc_wrapper runtime_core
MODULE_RUNTIME_PRIVATE_CXXFLAGS := -nostdinc++


ifeq ($(TARGET_SYSTEM),Linux)
MODULE_RUNTIME_GENERAL_CFLAGS += -idirafter $(RUNTIME_MODULE_DIR)/include_libc
MODULE_RUNTIME_GENERAL_CXXFLAGS += -idirafter $(RUNTIME_MODULE_DIR)/include_libc
MODULE_RUNTIME_LIBS += -l:libbacktrace.a -l:libc++abi.a
endif


ifeq ($(TARGET_SYSTEM),Android)
MODULE_RUNTIME_GENERAL_CFLAGS += -idirafter $(RUNTIME_MODULE_DIR)/include_libc
MODULE_RUNTIME_GENERAL_CXXFLAGS += -idirafter $(RUNTIME_MODULE_DIR)/include_libc
MODULE_RUNTIME_LIBS += -ldl -l:libbacktrace.a -landroid -llog
endif


ifeq ($(TARGET_SYSTEM),Android-NDK)
MODULE_RUNTIME_GENERAL_CFLAGS +=
MODULE_RUNTIME_GENERAL_CXXFLAGS +=
MODULE_RUNTIME_INCLUDES_OBJS += \
	$(RUNTIME_MODULE_DIR)/include_libc \
	$(RUNTIME_MODULE_DIR)/src
MODULE_RUNTIME_LIBS += -ldl -l:libbacktrace.a
endif


ifeq ($(TARGET_SYSTEM),Darwin)
MODULE_RUNTIME_GENERAL_CFLAGS += \
	-idirafter $(RUNTIME_MODULE_DIR)/include_libc \
	-idirafter $(RUNTIME_MODULE_DIR)/include_libc/darwin
MODULE_RUNTIME_GENERAL_CXXFLAGS += \
	-idirafter $(RUNTIME_MODULE_DIR)/include_libc \
	-idirafter $(RUNTIME_MODULE_DIR)/include_libc/darwin
MODULE_RUNTIME_GENERAL_LDFLAGS += -L$(TARGET_LIB_DIR) \
	-F$(OSTYPE_SDK_PATH)/System/Library/Frameworks \
	-framework CoreFoundation \
	-framework Foundation \
	-framework SystemConfiguration \
	-framework Security \
	-framework UniformTypeIdentifiers \
	-framework AppKit \
	-framework Network \
	-framework IOKit \
	-framework QuartzCore \
	-framework Metal \
	-L$(OSTYPE_SDK_PATH)/usr/lib -lSystem -licucore -lobjc -liconv -lc++abi
MODULE_RUNTIME_LIBS += -l:libbacktrace.a
endif


ifeq ($(TARGET_SYSTEM),Windows)
MODULE_RUNTIME_DEPENDS_ON += runtime_libc_impl
MODULE_RUNTIME_PRIVATE_INCLUDES += $(TARGET_INCLUDE_DIR)
MODULE_RUNTIME_INCLUDES_OBJS += $(TARGET_INCLUDE_DIR)

MODULE_RUNTIME_GENERAL_CFLAGS += -I$(RUNTIME_MODULE_DIR)/include_libc
MODULE_RUNTIME_GENERAL_CXXFLAGS += -I$(RUNTIME_MODULE_DIR)/include_libc
endif

#spec

MODULE_RUNTIME_SHARED_SPEC_SUMMARY := Xenolith platform-specific runtime

define MODULE_RUNTIME_SHARED_SPEC_DESCRIPTION
Xenolith platform-specific runtime
endef

# module name resolution
$(call define_module, runtime, MODULE_RUNTIME)
