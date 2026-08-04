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

MODULE_STAPPLER_RASTER_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_STAPPLER_RASTER_PRECOMPILED_HEADERS :=
MODULE_STAPPLER_RASTER_SRCS_DIRS := $(STAPPLER_MODULE_DIR)/raster
MODULE_STAPPLER_RASTER_SRCS_OBJS :=
MODULE_STAPPLER_RASTER_INCLUDES_DIRS :=
MODULE_STAPPLER_RASTER_INCLUDES_OBJS := $(STAPPLER_MODULE_DIR)/raster
MODULE_STAPPLER_RASTER_DEPENDS_ON := stappler_core

# The whole point of a separate module: the pixel loops are compiled optimized even in a debug
# build, so a profile can be taken without rebuilding the project in release. clang ignores
# __attribute__((optimize)) and a debug build passes no -O at all, so a per-module compiler flag is
# the only mechanism that works; attributes still carry ISA selection and inlining.
#
# -ffp-contract=off is not defensive: -O0 does not contract into FMA and -O2 does, and the parity
# gates compare frames byte for byte.
MODULE_STAPPLER_RASTER_PRIVATE_CXXFLAGS := -O2 -ffp-contract=off

#spec

MODULE_STAPPLER_RASTER_SHARED_SPEC_SUMMARY := Stappler CPU rasterizer

define MODULE_STAPPLER_RASTER_SHARED_SPEC_DESCRIPTION
Module libstappler-raster rasterizes triangles and glyph coverage into a bitmap on the CPU, with
no graphics driver involved. It takes plain data in and writes pixels out, and knows nothing about
a graphics API, a window system or a scene graph.
endef

# module name resolution
$(call define_module, stappler_raster, MODULE_STAPPLER_RASTER)
