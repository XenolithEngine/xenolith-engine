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

# Metal is available only on the Darwin family; the module is not defined for
# other targets (its sources are ObjC++ and need the Apple SDK)
ifneq ($(filter Darwin iOS,$(TARGET_SYSTEM)),)

MODULE_XENOLITH_BACKEND_MTL_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_XENOLITH_BACKEND_MTL_PRIVATE_INCLUDE_PCH := XLCommon.h
MODULE_XENOLITH_BACKEND_MTL_PRECOMPILED_HEADERS :=
MODULE_XENOLITH_BACKEND_MTL_SRCS_DIRS := $(XENOLITH_MODULE_DIR)/backend/mtl
MODULE_XENOLITH_BACKEND_MTL_SRCS_OBJS :=
MODULE_XENOLITH_BACKEND_MTL_INCLUDES_DIRS :=
MODULE_XENOLITH_BACKEND_MTL_INCLUDES_OBJS := $(XENOLITH_MODULE_DIR)/backend/mtl
MODULE_XENOLITH_BACKEND_MTL_DEPENDS_ON := xenolith_core

# The default compile flags carry no Apple SDK search paths (only the runtime
# modules see them); Metal/QuartzCore headers live in the SDK frameworks, so
# the SDK is wired in via OSTYPE_SDK_PATH as PRIVATE flags of this module.
# usr/include goes in as -isystem so the REAL SDK libc and AvailabilityMacros
# win over the include_libc wrapper stubs (-idirafter); the SCU additionally
# defines __SPRT_BUILD so the sprt type shims take their namespaced form and
# do not collide with the SDK declarations - the same TU-level contract the
# runtime's own .mm sources follow.
# The clang builtin headers (HOST_GENERAL_CFLAGS carries them as -idirafter)
# must be lifted BEFORE the SDK: SDK headers dispatch through clang's
# <stddef.h> (__need_ptrdiff_t and friends - in C++20 __has_feature(modules)
# is true upstream), and the SDK's own stddef.h must not shadow it
MODULE_XENOLITH_BACKEND_MTL_PRIVATE_CFLAGS := \
	$(patsubst -idirafter,-isystem,$(HOST_GENERAL_CFLAGS)) \
	-isystem $(OSTYPE_SDK_PATH)/usr/include \
	-F$(OSTYPE_SDK_PATH)/System/Library/Frameworks
MODULE_XENOLITH_BACKEND_MTL_PRIVATE_CXXFLAGS := \
	$(patsubst -idirafter,-isystem,$(HOST_GENERAL_CFLAGS)) \
	-isystem $(OSTYPE_SDK_PATH)/usr/include \
	-F$(OSTYPE_SDK_PATH)/System/Library/Frameworks

# Metal and QuartzCore are linked by the runtime on stock-SDK builds; listed
# here explicitly since this module is what actually requires them. On a
# "+open" (Xcode-SDK-free) sysroot there is no Metal stub - the module is not
# usable there
MODULE_XENOLITH_BACKEND_MTL_GENERAL_LDFLAGS := \
	-F$(OSTYPE_SDK_PATH)/System/Library/Frameworks \
	-framework Metal \
	-framework QuartzCore

#spec

MODULE_XENOLITH_BACKEND_MTL_SHARED_SPEC_SUMMARY := Xenolith on Metal API

define MODULE_XENOLITH_BACKEND_MTL_SHARED_SPEC_DESCRIPTION
Module libxenolith-backend-mtl implements graphic engine with backend on Apple
Metal API. Shaders are native MSL sources (no SPIR-V translation).
endef

# module name resolution
$(call define_module, xenolith_backend_mtl, MODULE_XENOLITH_BACKEND_MTL)

endif # ($(TARGET_SYSTEM),Darwin/iOS)
