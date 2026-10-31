# Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
# SPDX-License-Identifier: MIT

# A textured torus over a ground plane, drawn by basic3d under the basic2d UI; mouse drag pans,
# the wheel zooms. tests/window/basic3d-check.py drives it headless.

LOCAL_MAKEFILE := $(lastword $(MAKEFILE_LIST))

STAPPLER_BUILD_ROOT ?= $(dir $(LOCAL_MAKEFILE))../../../make

LOCAL_ROOT   := $(dir $(LOCAL_MAKEFILE))
LOCAL_OUTDIR := $(dir $(LOCAL_MAKEFILE))stappler-build

LOCAL_EXECUTABLE := basic3d

LOCAL_MODULES_PATHS = \
	stappler/stappler-modules.mk \
	xenolith/xenolith-modules.mk
LOCAL_MODULES := \
	xenolith_application_main \
	xenolith_renderer_basic3d

ifeq ($(findstring wasm,$(STAPPLER_TARGET)),wasm)
$(error basic3d has no WebGPU pass yet)
else
LOCAL_MODULES += xenolith_backend_vk
endif

LOCAL_MAIN := main.cpp

APPCONFIG_APP_NAME    := Basic3d Example
APPCONFIG_BUNDLE_NAME := org.stappler.xenolith.examples.basic3d

include $(STAPPLER_BUILD_ROOT)/universal.mk
