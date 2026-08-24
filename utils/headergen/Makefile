# Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>
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

# Ported from the old tree (libstappler-root/xenolith/utils/headergen). Only the paths moved: the
# build root and the module list live in different places in this layout, and the module set is
# spelled `stappler/...` rather than `core/...`. The generator itself is unchanged.
LOCAL_MAKEFILE := $(lastword $(MAKEFILE_LIST))

STAPPLER_BUILD_ROOT ?= $(dir $(LOCAL_MAKEFILE))../../make

LOCAL_OUTDIR ?= $(dir $(LOCAL_MAKEFILE))stappler-build
LOCAL_EXECUTABLE := headergen

LOCAL_ROOT = $(dir $(LOCAL_MAKEFILE))

# The Vulkan REGISTRY generator (src/RegistryData.*) is not built here.
#
# It is a separate errand from the icons: it writes through std::ofstream and reads its own
# stringstreams by data()/size(), neither of which exists in this tree, and porting it means
# rewriting its output layer rather than adjusting paths. What was needed - `headergen material`,
# which regenerates XL2dIcons from material-design-icons - does not touch any of it.
LOCAL_SRCS_DIRS :=
LOCAL_SRCS_OBJS :=

LOCAL_INCLUDES_DIRS := src
LOCAL_INCLUDES_OBJS :=

LOCAL_MAIN := main.cpp

LOCAL_MODULES_PATHS = \
	stappler/stappler-modules.mk \
	xenolith/xenolith-modules.mk

LOCAL_MODULES ?= \
	stappler_filesystem \
	stappler_network \
	stappler_bitmap \
	stappler_vg

include $(STAPPLER_BUILD_ROOT)/universal.mk
