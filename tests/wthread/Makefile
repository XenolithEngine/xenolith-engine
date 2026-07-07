LOCAL_MAKEFILE := $(lastword $(MAKEFILE_LIST))
STAPPLER_BUILD_ROOT ?= $(dir $(LOCAL_MAKEFILE))../../make
LOCAL_OUTDIR := $(dir $(LOCAL_MAKEFILE))stappler-build
LOCAL_EXECUTABLE := wthreadtest
LOCAL_MODULES_PATHS =
LOCAL_MODULES ?= \
	runtime_libc_wrapper \
	runtime
LOCAL_ROOT = $(abspath $(dir $(LOCAL_MAKEFILE)))
LOCAL_MAIN := main.cpp
include $(STAPPLER_BUILD_ROOT)/universal.mk
