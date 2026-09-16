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
# The CLI and the GUI are two front ends of ONE product and share the SDK store, so they must share
# the bundle name and the path scheme: both feed the runtime's App* location categories, and a
# mismatch in either would give the two tools different directories.
APPCONFIG_BUNDLE_NAME := org.stappler.xenolith.installer
# 1 = XDG locations on Linux, the system AppData folder on Windows. Deliberately NOT 3
# (AppContainer): this tool unpacks toolchains and spawns builds outside its own container.
APPCONFIG_APP_PATH_COMMON := 1
# A command line tool is not an application bundle — and two bundles must never claim the same
# CFBundleIdentifier, which is what sharing APPCONFIG_BUNDLE_NAME with the GUI would do.
LOCAL_MACOS_BUNDLE := 0

include $(STAPPLER_BUILD_ROOT)/universal.mk
