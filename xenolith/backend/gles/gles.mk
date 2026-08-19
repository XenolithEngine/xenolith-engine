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

MODULE_XENOLITH_BACKEND_GLES_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_XENOLITH_BACKEND_GLES_PRIVATE_INCLUDE_PCH := XLCommon.h
MODULE_XENOLITH_BACKEND_GLES_PRECOMPILED_HEADERS :=
MODULE_XENOLITH_BACKEND_GLES_SRCS_DIRS :=
MODULE_XENOLITH_BACKEND_GLES_SRCS_OBJS := $(XENOLITH_MODULE_DIR)/backend/gles/XLGles.scu.cpp
MODULE_XENOLITH_BACKEND_GLES_INCLUDES_DIRS :=
MODULE_XENOLITH_BACKEND_GLES_INCLUDES_OBJS := \
	$(XENOLITH_MODULE_DIR)/backend/gles \
	$(XENOLITH_MODULE_DIR)/backend/gles/include
MODULE_XENOLITH_BACKEND_GLES_DEPENDS_ON := xenolith_core

# There is nothing to link: EGL and OpenGL ES are loaded at runtime through sprt::Dso
# (libEGL.so.1 on Linux, the NDK system libraries on Android are resolved the same way), so a
# machine without any GL stack can still carry this module next to the Vulkan one.
#
# The EGL/GLES API headers in include/ are the Khronos registry headers (Apache-2.0), vendored
# the way the WebGPU backend vendors webgpu.h: the build is -nostdinc against the SDK sysroot,
# which carries vulkan/ but no EGL, and the headers are needed by every unit that includes the
# backend (core and application among them).

#spec

MODULE_XENOLITH_BACKEND_GLES_SHARED_SPEC_SUMMARY := Xenolith on OpenGL ES 3.1

define MODULE_XENOLITH_BACKEND_GLES_SHARED_SPEC_DESCRIPTION
Module libxenolith-backend-gles implements graphic engine on OpenGL ES 3.1 over EGL, for
devices whose GPU has no working Vulkan driver (embedded Linux stacks, older Android).
endef

# module name resolution
$(call define_module, xenolith_backend_gles, MODULE_XENOLITH_BACKEND_GLES)
