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

MODULE_XENOLITH_RENDERER_BASIC2D_GLES_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_XENOLITH_RENDERER_BASIC2D_GLES_PRIVATE_INCLUDE_PCH := XLCommon.h
MODULE_XENOLITH_RENDERER_BASIC2D_GLES_PRECOMPILED_HEADERS :=
MODULE_XENOLITH_RENDERER_BASIC2D_GLES_SRCS_DIRS := $(XENOLITH_MODULE_DIR)/renderer/basic2d/backend/gles
MODULE_XENOLITH_RENDERER_BASIC2D_GLES_SRCS_OBJS :=
MODULE_XENOLITH_RENDERER_BASIC2D_GLES_INCLUDES_DIRS :=
MODULE_XENOLITH_RENDERER_BASIC2D_GLES_INCLUDES_OBJS := $(XENOLITH_MODULE_DIR)/renderer/basic2d/backend/gles
MODULE_XENOLITH_RENDERER_BASIC2D_GLES_DEPENDS_ON := xenolith_backend_gles xenolith_renderer_basic2d

#spec

MODULE_XENOLITH_RENDERER_BASIC2D_GLES_SHARED_SPEC_SUMMARY := Xenolith 2d renderer OpenGL ES backend

define MODULE_XENOLITH_RENDERER_BASIC2D_GLES_SHARED_SPEC_DESCRIPTION
Module libxenolith-renderer-basic2d-gles implements the 2d render queue for the
OpenGL ES backend: the flat draw path, plus a clear-only pass for bring-up.
endef

# module name resolution
$(call define_module, xenolith_renderer_basic2d_gles, MODULE_XENOLITH_RENDERER_BASIC2D_GLES)
