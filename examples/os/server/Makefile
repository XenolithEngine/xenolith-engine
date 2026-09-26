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

# Rebuild everything if this makefile changes
LOCAL_MAKEFILE := $(lastword $(MAKEFILE_LIST))

# Path to make/ (relative to this makefile). examples/os/server -> three levels up.
STAPPLER_BUILD_ROOT ?= $(dir $(LOCAL_MAKEFILE))../../../make

LOCAL_ROOT   := $(dir $(LOCAL_MAKEFILE))
LOCAL_OUTDIR := $(dir $(LOCAL_MAKEFILE))stappler-build

# The window manager server: a host window whose frames are composed from the planes of its
# clients' virtual windows. Both backends are always linked, each with its compositor pass:
# `--gapi soft` selects the CPU one.
LOCAL_EXECUTABLE := wmserver

LOCAL_MODULES_PATHS = \
	stappler/stappler-modules.mk \
	xenolith/xenolith-modules.mk
LOCAL_MODULES := \
	xenolith_application_main \
	xenolith_renderer_ui \
	xenolith_resources_assets \
	xenolith_backend_vk \
	xenolith_backend_soft \
	xenolith_renderer_basic2d_soft \
	xenolith_renderer_compositor \
	xenolith_renderer_compositor_soft \
	xenolith_renderer_compositor_vk

LOCAL_MAIN          := main.cpp
LOCAL_SRCS_DIRS     := src
LOCAL_INCLUDES_OBJS := src

APPCONFIG_APP_NAME    := Xenolith OS
APPCONFIG_BUNDLE_NAME := org.stappler.xenolith.examples.os.server

include $(STAPPLER_BUILD_ROOT)/universal.mk
