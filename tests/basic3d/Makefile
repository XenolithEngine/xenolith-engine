# Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
# SPDX-License-Identifier: MIT

# basic3dtest: the device-free part of basic3d - the camera's math and the mesh builders. It links
# the module without a backend, so the Vulkan pass is not compiled in.

LOCAL_MAKEFILE := $(lastword $(MAKEFILE_LIST))

STAPPLER_BUILD_ROOT ?= $(dir $(LOCAL_MAKEFILE))../../make

LOCAL_OUTDIR := $(dir $(LOCAL_MAKEFILE))stappler-build
LOCAL_EXECUTABLE := basic3dtest

LOCAL_MODULES_PATHS = stappler/stappler-modules.mk xenolith/xenolith-modules.mk
LOCAL_MODULES := xenolith_renderer_basic3d

LOCAL_ROOT = $(dir $(LOCAL_MAKEFILE))
LOCAL_SRCS_DIRS := checks
LOCAL_INCLUDES_OBJS := .
LOCAL_MAIN := main.cpp

APPCONFIG_APP_NAME := Basic3dTest
APPCONFIG_BUNDLE_NAME := org.stappler.Basic3dTest

include $(STAPPLER_BUILD_ROOT)/universal.mk
