# Copyright (c) 2026 Stappler LLC <admin@stappler.dev>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons whom the Software is
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

# Path to make/ (relative to this makefile). examples/window/particles -> four levels up.
STAPPLER_BUILD_ROOT ?= $(dir $(LOCAL_MAKEFILE))../../../make

LOCAL_ROOT   := $(dir $(LOCAL_MAKEFILE))
LOCAL_OUTDIR := $(dir $(LOCAL_MAKEFILE))stappler-build

# What to build: one executable driving the GPU particle system of basic2d
LOCAL_EXECUTABLE := particles

# Module catalogs, then the modules to use (deps resolved transitively)
LOCAL_MODULES_PATHS = \
	stappler/stappler-modules.mk \
	xenolith/xenolith-modules.mk
LOCAL_MODULES := \
	xenolith_application_main \
	xenolith_renderer_ui \
	xenolith_resources_assets
# Particles are simulated by a Vulkan compute pass of the Default queue; WebGPU has no such pass
ifeq ($(findstring wasm,$(STAPPLER_TARGET)),wasm)
$(error GPU particles need the Vulkan Default queue: WebGPU has no particle pass)
endif
LOCAL_MODULES += xenolith_backend_vk

# Sources: main.cpp is the TU with the scene factory; src/ holds the layout and its textures.
# The source scan is recursive, so nothing has to be listed by name.
LOCAL_MAIN          := main.cpp
LOCAL_SRCS_DIRS     := src
LOCAL_INCLUDES_OBJS := src

# App identity (baked into the binary / bundle)
APPCONFIG_APP_NAME    := Particles Example
APPCONFIG_BUNDLE_NAME := org.stappler.xenolith.examples.particles

include $(STAPPLER_BUILD_ROOT)/universal.mk
