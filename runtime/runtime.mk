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

#
# Runtime module definitions
#
# Note that it is not used when you build application using toolchain
# with integrated runtime (+sprt targets)
#

RUNTIME_MODULE_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

# Communicate with <root>/runtime/Makefile
ifdef SPRT_BUILD_RUNTIME
MODULE_RUNTIME_COMMON_CFLAGS := -DSPRT_BUILD_RUNTIME
else
MODULE_RUNTIME_COMMON_CFLAGS :=
endif

include $(RUNTIME_MODULE_DIR)/core/core.mk
include $(RUNTIME_MODULE_DIR)/musl-adapters/musl_libc.mk
include $(RUNTIME_MODULE_DIR)/libc_impl/malloc.mk
include $(RUNTIME_MODULE_DIR)/libc_impl/libc.mk
include $(RUNTIME_MODULE_DIR)/libc_wrapper/libc-wrapper.mk
include $(RUNTIME_MODULE_DIR)/window/window.mk


MODULE_RUNTIME_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_RUNTIME_PRIVATE_STANDALONE := 1
MODULE_RUNTIME_LIBS :=
MODULE_RUNTIME_FLAGS :=
MODULE_RUNTIME_GENERAL_CFLAGS :=
MODULE_RUNTIME_GENERAL_CXXFLAGS :=
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

MODULE_RUNTIME_PRIVATE_CFLAGS := $(MODULE_RUNTIME_COMMON_CFLAGS) -nostdinc++ -Wno-unused-command-line-argument
MODULE_RUNTIME_PRIVATE_CXXFLAGS := $(MODULE_RUNTIME_COMMON_CFLAGS) -nostdinc++ -Wno-unused-command-line-argument
MODULE_RUNTIME_GENERAL_LDFLAGS :=

ifeq ($(TARGET_SYSTEM),Linux)
MODULE_RUNTIME_GENERAL_CFLAGS += -idirafter $(RUNTIME_MODULE_DIR)/include_libc
MODULE_RUNTIME_GENERAL_CXXFLAGS += -idirafter $(RUNTIME_MODULE_DIR)/include_libc/stl
MODULE_RUNTIME_GENERAL_CXXFLAGS += -idirafter $(RUNTIME_MODULE_DIR)/include_libc
MODULE_RUNTIME_LIBS += -l:libbacktrace.a -l:libc++abi.a -lm
endif


ifeq ($(TARGET_SYSTEM),Android)
MODULE_RUNTIME_GENERAL_CFLAGS += -idirafter $(RUNTIME_MODULE_DIR)/include_libc
MODULE_RUNTIME_GENERAL_CXXFLAGS += -idirafter $(RUNTIME_MODULE_DIR)/include_libc/stl
MODULE_RUNTIME_GENERAL_CXXFLAGS += -idirafter $(RUNTIME_MODULE_DIR)/include_libc
MODULE_RUNTIME_LIBS += -ldl -l:libbacktrace.a -landroid -llog
endif


ifeq ($(TARGET_SYSTEM),Android-NDK)
MODULE_RUNTIME_GENERAL_CFLAGS += -nostdinc++
MODULE_RUNTIME_GENERAL_CXXFLAGS += -nostdinc++
MODULE_RUNTIME_INCLUDES_OBJS += \
	$(RUNTIME_MODULE_DIR)/include_libc/stl \
	$(RUNTIME_MODULE_DIR)/include_libc \
	$(RUNTIME_MODULE_DIR)/src
MODULE_RUNTIME_LIBS += -ldl -l:libbacktrace.a
endif


# Shared Darwin family (macOS + iOS): same libSystem/Foundation/Metal stack.
# Differs only in the UI framework (AppKit on macOS, UIKit on iOS), handled below.
ifneq ($(filter Darwin iOS,$(TARGET_SYSTEM)),)
ifeq ($(findstring +open,$(TARGET_SYSROOT)),)
# Stock Xcode-SDK build: the SDK's usr/include is authoritative; the runtime libc
# wrappers only supplement it — kept AFTER the SDK (-idirafter).
MODULE_RUNTIME_GENERAL_CFLAGS += \
	-idirafter $(RUNTIME_MODULE_DIR)/include_libc \
	-idirafter $(RUNTIME_MODULE_DIR)/include_libc/darwin
MODULE_RUNTIME_GENERAL_CXXFLAGS += \
	-idirafter $(RUNTIME_MODULE_DIR)/include_libc/stl \
	-idirafter $(RUNTIME_MODULE_DIR)/include_libc \
	-idirafter $(RUNTIME_MODULE_DIR)/include_libc/darwin
else
# +open (Xcode-SDK-free): the sysroot usr/include carries only the apple-oss BASE
# libc, so the runtime's own libc wrappers (include_libc) must be found BEFORE it:
#   - the runtime's __SPRT_BUILD sources then #include_next through to the base;
#   - consumers (tests/apps) get the full SPRT-extended libc API (strverscmp, the
#     *_l locale variants, ...) routed to <sprt/wrappers/libc/*> instead of the
#     bare apple-oss headers.
# -isystem places them ahead of the --sysroot search; GENERAL so consumers inherit.
MODULE_RUNTIME_GENERAL_CFLAGS += \
	-isystem $(RUNTIME_MODULE_DIR)/include_libc \
	-isystem $(RUNTIME_MODULE_DIR)/include_libc/darwin
MODULE_RUNTIME_GENERAL_CXXFLAGS += \
	-isystem $(RUNTIME_MODULE_DIR)/include_libc \
	-isystem $(RUNTIME_MODULE_DIR)/include_libc/darwin
endif
MODULE_RUNTIME_GENERAL_LDFLAGS += -L$(TARGET_LIB_DIR) \
	-F$(OSTYPE_SDK_PATH)/System/Library/Frameworks \
	-framework CoreFoundation \
	-framework Foundation \
	-framework SystemConfiguration \
	-framework Security \
	-framework UniformTypeIdentifiers \
	-framework Network \
	-framework IOKit \
	-framework QuartzCore \
	-framework CoreGraphics \
	-L$(OSTYPE_SDK_PATH)/usr/lib -lSystem -lobjc -lc++abi
MODULE_RUNTIME_LIBS += -l:libbacktrace.a

# On a "+open" (Xcode-SDK-free) target, OSTYPE_SDK_PATH points at the target
# sysroot itself, so the -F/-L above resolve against the generated .tbd link
# stubs (target-apple/open-sysroot.mk + gen-oss-stubs.sh). The stubs place every
# symbol in the SAME library as the real SDK, so the link line matches the stock
# one: libicucore (uidna_*) and libiconv resolve via their own stubs, CoreText is
# added (font deps pull it in via functions_<arch>.txt); only Metal (provided
# through MoltenVK) is dropped. The baked stubs are complete, so the link uses
# the default two-level namespace with NO -undefined dynamic_lookup escape hatch
# — any symbol not carried by a stub is a hard link error (re-bake to add it).
ifeq ($(findstring +open,$(TARGET_SYSROOT)),)
MODULE_RUNTIME_GENERAL_LDFLAGS += -framework Metal -licucore -liconv
else
MODULE_RUNTIME_GENERAL_LDFLAGS += -framework CoreText -licucore -liconv
endif
endif

ifeq ($(TARGET_SYSTEM),Darwin)
MODULE_RUNTIME_GENERAL_LDFLAGS += -framework AppKit
endif

ifeq ($(TARGET_SYSTEM),iOS)
MODULE_RUNTIME_GENERAL_LDFLAGS += -framework UIKit
endif


ifeq ($(TARGET_SYSTEM),Windows)
MODULE_RUNTIME_DEPENDS_ON += runtime_libc_impl
MODULE_RUNTIME_PRIVATE_INCLUDES += $(TARGET_INCLUDE_DIR)
MODULE_RUNTIME_INCLUDES_OBJS += $(TARGET_INCLUDE_DIR) \
	$(RUNTIME_MODULE_DIR)/include/sprt/wrappers/windows \
	$(RUNTIME_MODULE_DIR)/include_libc \
	$(RUNTIME_MODULE_DIR)/include_libc/stl

MODULE_RUNTIME_LIBS += -limport
# The runtime links freestanding (-nostdlib), so clang does not auto-link the
# compiler-rt builtins. Link it explicitly for the out-of-line builtins the
# runtime does not provide itself — notably the C99 _Complex __mulXc3/__divXc3
# helpers (clang-cl makes CMake drop them from the archive; compiler_rt.mk
# rebuilds them). It is a static archive, so members are pulled only on demand.
MODULE_RUNTIME_LIBS += $(TARGET_SYSROOT)/lib/clang/lib/windows/clang_rt.builtins-$(TARGET_ARCH).lib
MODULE_RUNTIME_GENERAL_CFLAGS +=
MODULE_RUNTIME_GENERAL_CXXFLAGS +=
MODULE_RUNTIME_GENERAL_LDFLAGS += -nostdlib
endif

#spec

MODULE_RUNTIME_SHARED_SPEC_SUMMARY := Xenolith platform-specific runtime

define MODULE_RUNTIME_SHARED_SPEC_DESCRIPTION
Xenolith platform-specific runtime
endef

# module name resolution
$(call define_module, runtime, MODULE_RUNTIME)
