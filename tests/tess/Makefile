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

# force to rebuild if this makefile changed
LOCAL_MAKEFILE := $(lastword $(MAKEFILE_LIST))

STAPPLER_BUILD_ROOT ?= $(dir $(LOCAL_MAKEFILE))../../make

LOCAL_OUTDIR := $(dir $(LOCAL_MAKEFILE))stappler-build
LOCAL_EXECUTABLE := tesstest

LOCAL_PRIVATE_INCLUDE_PCH := SPCommon.h

LOCAL_MODULES_PATHS = \
	stappler/stappler-modules.mk \
	xenolith/xenolith-modules.mk

# The tesselator and the vector layer are what is under test. The icons come from the 2d renderer,
# and they are the reason that module is here - not the renderer itself, which this never starts:
# a Tesselator needs a memory pool and nothing else, so the whole bench runs on a CLI thread with
# no device, no window and no frame.
LOCAL_MODULES ?= \
	runtime \
	stappler_core \
	stappler_filesystem \
	stappler_data \
	stappler_bitmap \
	stappler_document \
	stappler_font \
	stappler_vg \
	xenolith_application \
	xenolith_renderer_basic2d \
	xenolith_backend_soft \
	xenolith_renderer_basic2d_soft

LOCAL_ROOT = $(dir $(LOCAL_MAKEFILE))
LOCAL_SRCS_DIRS := src

include $(STAPPLER_BUILD_ROOT)/universal.mk
