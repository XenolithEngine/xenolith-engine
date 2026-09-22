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

# Path to make/ (relative to this makefile).
STAPPLER_BUILD_ROOT ?= $(dir $(LOCAL_MAKEFILE))../../../make

LOCAL_ROOT   := $(dir $(LOCAL_MAKEFILE))
LOCAL_OUTDIR := $(dir $(LOCAL_MAKEFILE))stappler-build

# What to build: one executable demonstrating ui::FilesystemModel behind a two-pane navigator -
# a places tree on the left, an icon grid or a table on the right, with image thumbnails
LOCAL_EXECUTABLE := fileexplorer

# Module catalogs, then the modules to use (deps resolved transitively)
LOCAL_MODULES_PATHS = \
	stappler/stappler-modules.mk \
	xenolith/xenolith-modules.mk
# stappler_bitmap is transitive through xenolith_core, but this example decodes and resamples
# images itself, so it names the dependency it actually uses.
LOCAL_MODULES := \
	xenolith_application_main \
	xenolith_renderer_ui \
	xenolith_resources_assets \
	stappler_bitmap
ifeq ($(findstring wasm,$(STAPPLER_TARGET)),wasm)
LOCAL_MODULES += xenolith_backend_webgpu xenolith_renderer_basic2d_webgpu
else
LOCAL_MODULES += xenolith_backend_vk
endif

# Sources: main.cpp is the TU with the scene factory; src/ holds the panes and the thumbnails.
# The source scan is recursive, so nothing has to be listed by name.
LOCAL_MAIN          := main.cpp
LOCAL_SRCS_DIRS     := src
LOCAL_INCLUDES_OBJS := src

# App identity (baked into the binary / bundle)
APPCONFIG_APP_NAME    := File Explorer Example
APPCONFIG_BUNDLE_NAME := org.stappler.xenolith.examples.fileexplorer

include $(STAPPLER_BUILD_ROOT)/universal.mk
