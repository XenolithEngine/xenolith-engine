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
# Consumer side of the shared runtime (sprt.dll).
#
# This is the counterpart of runtime.mk, not an addition to it. `runtime` compiles the
# runtime sources *into* the consumer, giving every image its own copy of the heap,
# stdio and exception state. `runtime_shared` instead links the import library of a
# runtime that was built once with
#
#     make -C <root>/runtime STAPPLER_TARGET=<triple> SPRT_SHARED=1
#
# so the whole process shares a single runtime instance. A project uses exactly one of
# the two - listing both would defeat the purpose.
#
# Windows only for now: it is the target where the split matters (PE has no symbol
# interposition, so a statically linked runtime really is duplicated per image).
#

RUNTIME_SHARED_MODULE_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

# Where `make -C runtime SPRT_SHARED=1` leaves sprt.dll and its import library. That
# build uses its own output tree so its sprt.lib (an import library) cannot be confused
# with the static archive of the same name. Override to link a runtime staged elsewhere
# (an installed sysroot, say).
RUNTIME_SHARED_BUILD_DIR ?= \
	$(RUNTIME_SHARED_MODULE_DIR)/stappler-build-shared/$(TARGET_NAME)/$(BUILD_TYPE)/cc

MODULE_RUNTIME_SHARED_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_RUNTIME_SHARED_PRIVATE_STANDALONE := 1
MODULE_RUNTIME_SHARED_DEPENDS_ON :=
MODULE_RUNTIME_SHARED_SRCS_DIRS :=

# No sources: the executable-side CRT startup stub - the per-image parts that cannot be
# imported (.CRT sections, TLS directory, /GS cookie, entry point) - is baked into
# sprt.lib as an archive member by the runtime's own Makefile, and the linker pulls it in
# to resolve the entry point.
#
# A consumer that wants its own entry point just defines mainCRTStartup: the one in the
# archive member is a weak external, so the strong definition wins.
MODULE_RUNTIME_SHARED_SRCS_OBJS :=

MODULE_RUNTIME_SHARED_INCLUDES_DIRS :=

# Same header set `runtime` exports to consumers: the runtime is freestanding
# (-nostdinc), so <stdio.h> and the STL have to resolve out of the runtime tree.
MODULE_RUNTIME_SHARED_INCLUDES_OBJS := \
	$(RUNTIME_SHARED_MODULE_DIR)/include \
	$(RUNTIME_SHARED_MODULE_DIR)/include/sprt/wrappers/windows \
	$(RUNTIME_SHARED_MODULE_DIR)/include_libc/cxx \
	$(RUNTIME_SHARED_MODULE_DIR)/libcxx/include \
	$(RUNTIME_SHARED_MODULE_DIR)/include_libc

MODULE_RUNTIME_SHARED_PRIVATE_INCLUDES := \
	$(RUNTIME_SHARED_MODULE_DIR)/include \
	$(RUNTIME_SHARED_MODULE_DIR)/include_libc

# SPRT_SHARED_RUNTIME flips SPRT_API/SPRT_GLOBAL to __declspec(dllimport), so the
# consumer reaches the runtime's data through the import table instead of expecting a
# local definition.
MODULE_RUNTIME_SHARED_GENERAL_CFLAGS := -DSPRT_SHARED_RUNTIME -nostdinc
MODULE_RUNTIME_SHARED_GENERAL_CXXFLAGS := -DSPRT_SHARED_RUNTIME -nostdinc -nostdinc++
MODULE_RUNTIME_SHARED_PRIVATE_CFLAGS := $(MODULE_RUNTIME_SHARED_GENERAL_CFLAGS)
MODULE_RUNTIME_SHARED_PRIVATE_CXXFLAGS := $(MODULE_RUNTIME_SHARED_GENERAL_CXXFLAGS)

ifeq ($(TARGET_SYSTEM),Windows)
MODULE_RUNTIME_SHARED_INCLUDES_OBJS += $(TARGET_INCLUDE_DIR)
MODULE_RUNTIME_SHARED_PRIVATE_INCLUDES += $(TARGET_INCLUDE_DIR)

# sprt.lib here is the *import* library emitted next to sprt.dll, not the static
# runtime archive that a default (SPRT_SHARED=0) build writes to the same name.
MODULE_RUNTIME_SHARED_LIBS := $(RUNTIME_SHARED_BUILD_DIR)/sprt.lib

# The Win32 import library. It also carries the core mem*/str* functions that ntdll
# exports and that the compiler lowers to directly (memset, memcpy, strlen, ...), which
# is why the runtime headers declare them __declspec(dllimport) on Windows.
MODULE_RUNTIME_SHARED_LIBS += -limport

# Out-of-line compiler-rt builtins for code the consumer itself compiles; a static
# archive, so members are pulled only on demand.
MODULE_RUNTIME_SHARED_LIBS += \
	$(TARGET_SYSROOT)/lib/clang/lib/windows/clang_rt.builtins-$(TARGET_ARCH).lib

MODULE_RUNTIME_SHARED_GENERAL_LDFLAGS := -nostdlib
endif

#spec

MODULE_RUNTIME_SHARED_SHARED_SPEC_SUMMARY := Xenolith runtime, linked as a shared library

define MODULE_RUNTIME_SHARED_SHARED_SPEC_DESCRIPTION
Consumer side of the Xenolith runtime built with SPRT_SHARED=1: links sprt.dll's import
library instead of compiling the runtime into the application.
endef

# module name resolution
$(call define_module, runtime_shared, MODULE_RUNTIME_SHARED)
