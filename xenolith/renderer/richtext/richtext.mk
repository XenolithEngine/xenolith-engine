# Copyright (c) 2024 Stappler LLC <admin@stappler.dev>
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

MODULE_XENOLITH_RENDERER_RICHTEXT_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_XENOLITH_RENDERER_RICHTEXT_PRECOMPILED_HEADERS :=
MODULE_XENOLITH_RENDERER_RICHTEXT_SRCS_DIRS := $(XENOLITH_MODULE_DIR)/renderer/richtext
MODULE_XENOLITH_RENDERER_RICHTEXT_SRCS_OBJS :=
MODULE_XENOLITH_RENDERER_RICHTEXT_INCLUDES_DIRS := $(XENOLITH_MODULE_DIR)/renderer/richtext
MODULE_XENOLITH_RENDERER_RICHTEXT_INCLUDES_OBJS :=
MODULE_XENOLITH_RENDERER_RICHTEXT_DEPENDS_ON := \
	stappler_layout \
	xenolith_renderer_basic2d \
	xenolith_resources_assets \
	xenolith_font

#spec

MODULE_XENOLITH_RENDERER_RICHTEXT_SHARED_SPEC_SUMMARY := libstappler richtext renderer for Xenolith

define MODULE_XENOLITH_RENDERER_RICHTEXT_SHARED_SPEC_DESCRIPTION
Module libstappler-document-richtext implements richtext view for Xenolith engine,
that capable to render structured documents.
endef

# module name resolution
$(call define_module, xenolith_renderer_richtext, MODULE_XENOLITH_RENDERER_RICHTEXT)
