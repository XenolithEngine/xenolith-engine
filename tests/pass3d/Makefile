# Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
# SPDX-License-Identifier: MIT

# A depth-tested 3d pass under the basic2d UI. The pass is added to the stock basic2d queue through
# the Scene2d::init builder callback and draws into the same Output before the 2d pass composites
# over it. tests/window/pass3d-check.py drives it headless and checks the picture.

LOCAL_MAKEFILE := $(lastword $(MAKEFILE_LIST))

STAPPLER_BUILD_ROOT ?= $(dir $(LOCAL_MAKEFILE))../../make

LOCAL_ROOT   := $(dir $(LOCAL_MAKEFILE))
LOCAL_OUTDIR := $(dir $(LOCAL_MAKEFILE))stappler-build

LOCAL_EXECUTABLE := pass3d

LOCAL_MODULES_PATHS = \
	stappler/stappler-modules.mk \
	xenolith/xenolith-modules.mk
LOCAL_MODULES := \
	xenolith_application_main \
	xenolith_renderer_basic2d \
	xenolith_backend_vk

LOCAL_MAIN := main.cpp

LOCAL_SHADERS_DIR := $(LOCAL_ROOT)shaders
LOCAL_PRIVATE_DEPS = $(BUILD_SHADERS_EMBEDDED)

APPCONFIG_APP_NAME    := Pass3d
APPCONFIG_BUNDLE_NAME := org.stappler.xenolith.tests.pass3d

include $(STAPPLER_BUILD_ROOT)/universal.mk
