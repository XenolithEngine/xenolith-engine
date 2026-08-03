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

LOCAL_OUTDIR ?= $(dir $(LOCAL_MAKEFILE))stappler-build

LOCAL_EXECUTABLE := xenolith-installer

LOCAL_PRIVATE_INCLUDE_PCH := XLCommon.h

LOCAL_MODULES_PATHS = \
	stappler/stappler-modules.mk \
	xenolith/xenolith-modules.mk \
	utils/installer/installer-modules.mk

# xenolith_application_main provides application main()
# xenolith_backend_vk - use Vulkan backend
# installer_core - UI-agnostic core (dirs/catalogue/install/state/engine) shared with the CLI
LOCAL_MODULES ?= \
	xenolith_application_main \
	xenolith_backend_vk \
	xenolith_renderer_ui \
	installer_core

LOCAL_ROOT = $(dir $(LOCAL_MAKEFILE))

LOCAL_SRCS_DIRS := src
LOCAL_SRCS_OBJS :=

LOCAL_INCLUDES_DIRS := src
LOCAL_INCLUDES_OBJS :=

APPCONFIG_APP_NAME := xenolith-installer
# Shared with utils/installer/cli — see the note there: the bundle name and the path scheme are what
# place the App* location categories, and both tools must land on the same SDK store.
APPCONFIG_BUNDLE_NAME := org.stappler.xenolith.installer
# 1 = XDG locations on Linux, the system AppData folder on Windows. Was 3 (run inside an
# AppContainer), which both sandboxed the installer away from the toolchains it installs and put its
# App* paths somewhere the CLI could not reach.
APPCONFIG_APP_PATH_COMMON := 1

include $(STAPPLER_BUILD_ROOT)/universal.mk

# Bundle app resources into the .app on macOS: the runtime resolves
# LocationCategory::Bundled to Contents/Resources when running as a bundle
# (see SPRuntimeFilesystem-macos.mm). Mirrors tests/window; without this the
# StyleSystem can't load resources/style.css and no CSS layout is applied.
ifeq ($(TARGET_SYSTEM),Darwin)
.PHONY: bundle-resources
bundle-resources: $(BUILD_EXECUTABLE)
	@mkdir -p $(abspath $(dir $(BUILD_EXECUTABLE))/..)/Resources
	rsync -ar --delete $(LOCAL_ROOT)/resources $(abspath $(dir $(BUILD_EXECUTABLE))/..)/Resources/

all: bundle-resources
endif
