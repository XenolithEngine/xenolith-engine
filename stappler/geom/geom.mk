# Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>
# Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
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

MODULE_STAPPLER_GEOM_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_STAPPLER_GEOM_PRIVATE_INCLUDE_PCH := SPCommon.h
MODULE_STAPPLER_GEOM_SRCS_DIRS := $(STAPPLER_MODULE_DIR)/geom
MODULE_STAPPLER_GEOM_SRCS_OBJS := 
MODULE_STAPPLER_GEOM_INCLUDES_DIRS :=
MODULE_STAPPLER_GEOM_INCLUDES_OBJS := $(STAPPLER_MODULE_DIR)/geom $(STAPPLER_MODULE_DIR)/geom/glsl
MODULE_STAPPLER_GEOM_DEPENDS_ON := stappler_core
MODULE_STAPPLER_GEOM_SHADERS_INCLUDE := $(STAPPLER_MODULE_DIR)/geom/glsl

# spec

MODULE_STAPPLER_GEOM_SHARED_SPEC_SUMMARY := libstappler 2D/3D geometry types

define MODULE_STAPPLER_GEOM_SHARED_SPEC_DESCRIPTION
Module libstappler-geom provides types for 2D and 2D rendering, like
- Points (Vec2, Vec3, Vec4)
- Matrices (Mat4)
- Quaterinons
- Float and integral sizes
- Float and integral colors
- HCT color engine
- GLSL-compatible interface
endef

# module name resolution
$(call define_module, stappler_geom, MODULE_STAPPLER_GEOM)
