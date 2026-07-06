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

MODULE_XENOLITH_RENDERER_PUG_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_XENOLITH_RENDERER_PUG_PRIVATE_INCLUDE_PCH := XLCommon.h
MODULE_XENOLITH_RENDERER_PUG_LIBS :=
MODULE_XENOLITH_RENDERER_PUG_SRCS_DIRS := $(XENOLITH_MODULE_DIR)/renderer/pug
MODULE_XENOLITH_RENDERER_PUG_SRCS_OBJS :=
MODULE_XENOLITH_RENDERER_PUG_INCLUDES_DIRS := $(XENOLITH_MODULE_DIR)/renderer/pug
MODULE_XENOLITH_RENDERER_PUG_INCLUDES_OBJS :=
MODULE_XENOLITH_RENDERER_PUG_DEPENDS_ON := xenolith_renderer_simpleui stappler_pug

#spec

MODULE_XENOLITH_RENDERER_PUG_SHARED_SPEC_SUMMARY := Xenolith scene-graph builder for PUG templates

define MODULE_XENOLITH_RENDERER_PUG_SHARED_SPEC_DESCRIPTION
Module libxenolith-renderer-pug builds xenolith node trees from PUG templates
(stappler_pug structured mode) using an extensible tag factory registry
endef

# module name resolution
$(call define_module, xenolith_renderer_pug, MODULE_XENOLITH_RENDERER_PUG)
