# force rebuild if this makefile changed
LOCAL_MAKEFILE := $(lastword $(MAKEFILE_LIST))

STAPPLER_BUILD_ROOT ?= $(dir $(LOCAL_MAKEFILE))../../../make

LOCAL_OUTDIR ?= $(dir $(LOCAL_MAKEFILE))stappler-build

LOCAL_ROOT := $(dir $(LOCAL_MAKEFILE))

LOCAL_EXECUTABLE := xenolith-cli

LOCAL_MODULES_PATHS = \
	stappler/stappler-modules.mk \
	xenolith/xenolith-modules.mk \
	utils/installer/installer-modules.mk

# UI-agnostic core (dirs, triple, manifest, transport, state) shared with the GUI
LOCAL_MODULES := \
	installer_core

LOCAL_MAIN := main.cpp
LOCAL_SRCS_DIRS :=
LOCAL_INCLUDES_OBJS :=

APPCONFIG_APP_NAME := xenolith-cli
APPCONFIG_BUNDLE_NAME := org.stappler.xenolith.cli
APPCONFIG_APP_PATH_COMMON := 0

include $(STAPPLER_BUILD_ROOT)/universal.mk
