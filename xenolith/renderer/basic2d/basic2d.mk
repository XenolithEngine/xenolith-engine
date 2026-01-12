# Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
# Copyright (c) 2025 Stappler Team <admin@stappler.org>
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

MODULE_XENOLITH_RENDERER_BASIC2D_SHADERS_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_XENOLITH_RENDERER_BASIC2D_SHADERS_PRECOMPILED_HEADERS :=
MODULE_XENOLITH_RENDERER_BASIC2D_SHADERS_SRCS_DIRS :=
MODULE_XENOLITH_RENDERER_BASIC2D_SHADERS_SRCS_OBJS := $(XENOLITH_MODULE_DIR)/renderer/basic2d/glsl/XL2dShaders.cpp
MODULE_XENOLITH_RENDERER_BASIC2D_SHADERS_INCLUDES_DIRS :=
MODULE_XENOLITH_RENDERER_BASIC2D_SHADERS_INCLUDES_OBJS :=
MODULE_XENOLITH_RENDERER_BASIC2D_SHADERS_SHADERS_DIR := $(XENOLITH_MODULE_DIR)/renderer/basic2d/glsl/shaders
MODULE_XENOLITH_RENDERER_BASIC2D_SHADERS_SHADERS_INCLUDE := $(XENOLITH_MODULE_DIR)/renderer/basic2d/glsl/include
MODULE_XENOLITH_RENDERER_BASIC2D_SHADERS_DEPENDS_ON := xenolith_application xenolith_font stappler_tess stappler_vg
MODULE_XENOLITH_RENDERER_BASIC2D_SHADERS_PRIVATE_DEPS := $(call sp_toolkit_shaders_objs,\
	$(call sp_toolkit_shaders_files,$(MODULE_XENOLITH_RENDERER_BASIC2D_SHADERS_SHADERS_DIR)))

# module name resolution
$(call define_module, xenolith_renderer_basic2d_shaders, MODULE_XENOLITH_RENDERER_BASIC2D_SHADERS)

MODULE_XENOLITH_RENDERER_BASIC2D_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_XENOLITH_RENDERER_BASIC2D_PRIVATE_INCLUDE_PCH := XLCommon.h
MODULE_XENOLITH_RENDERER_BASIC2D_PRECOMPILED_HEADERS :=
MODULE_XENOLITH_RENDERER_BASIC2D_SRCS_DIRS :=
MODULE_XENOLITH_RENDERER_BASIC2D_SRCS_OBJS := \
	$(XENOLITH_MODULE_DIR)/renderer/basic2d/XL2d.cpp \
	$(XENOLITH_MODULE_DIR)/renderer/basic2d/scroll/XL2dScroll.scu.cpp \
	$(XENOLITH_MODULE_DIR)/renderer/basic2d/icons/XL2dIcons.cpp
MODULE_XENOLITH_RENDERER_BASIC2D_INCLUDES_DIRS := 
MODULE_XENOLITH_RENDERER_BASIC2D_INCLUDES_OBJS := \
	$(XENOLITH_MODULE_DIR)/renderer/basic2d \
	$(XENOLITH_MODULE_DIR)/renderer/basic2d/scroll \
	$(XENOLITH_MODULE_DIR)/renderer/basic2d/particle \
	$(XENOLITH_MODULE_DIR)/renderer/basic2d/icons
MODULE_XENOLITH_RENDERER_BASIC2D_DEPENDS_ON := xenolith_renderer_basic2d_shaders

MODULE_XENOLITH_RENDERER_BASIC2D_SHARED_DEPENDS_ON := xenolith_backend_vk xenolith_application xenolith_font stappler_tess stappler_vg
MODULE_XENOLITH_RENDERER_BASIC2D_SHARED_CONSUME := \
	xenolith_renderer_basic2d_shaders

#spec

MODULE_XENOLITH_RENDERER_BASIC2D_SHARED_SPEC_SUMMARY := Xenolith basic GUI primitives

define MODULE_XENOLITH_RENDERER_BASIC2D_SHARED_SPEC_DESCRIPTION
Module libxenolith-renderer-basic2d implements basic 2d GUI primitives:
- Text labels
- Sprites (with VG support)
- ScrollView
endef

# module name resolution
$(call define_module, xenolith_renderer_basic2d, MODULE_XENOLITH_RENDERER_BASIC2D)
