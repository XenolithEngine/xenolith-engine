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

# Auxiliary-windows scaffold: Root panel that opens Popup/Tooltip windows (native where the
# backend advertises WindowCapabilities::Subwindows, in-scene overlay otherwise).
# Used as the integration harness for the Popup/Tooltip work across macOS → Linux → Windows.

LOCAL_MAKEFILE := $(lastword $(MAKEFILE_LIST))

STAPPLER_BUILD_ROOT ?= $(dir $(LOCAL_MAKEFILE))../../make

LOCAL_ROOT = $(dir $(LOCAL_MAKEFILE))
LOCAL_OUTDIR := $(dir $(LOCAL_MAKEFILE))stappler-build

LOCAL_EXECUTABLE := auxui

LOCAL_MODULES_PATHS = \
	stappler/stappler-modules.mk \
	xenolith/xenolith-modules.mk

LOCAL_MODULES := \
	xenolith_application \
	xenolith_application_main \
	xenolith_renderer_ui \
	xenolith_backend_vk \
	xenolith_resources_assets \
	stappler_makefile

# Metal build variant (macOS/iOS only): `make MTL=1` adds the Metal backend
ifeq ($(MTL),1)
LOCAL_MODULES += \
	xenolith_backend_mtl \
	xenolith_renderer_basic2d_mtl
endif

LOCAL_SRCS_DIRS := src
LOCAL_INCLUDES_OBJS := src

include $(STAPPLER_BUILD_ROOT)/universal.mk

# Bundle app resources into the .app on macOS so the runtime can resolve LocationCategory::Bundled.
ifeq ($(TARGET_SYSTEM),Darwin)
.PHONY: bundle-resources
bundle-resources: $(BUILD_EXECUTABLE)
	@mkdir -p $(abspath $(dir $(BUILD_EXECUTABLE))/..)/Resources

all: bundle-resources
endif
