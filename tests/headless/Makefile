LOCAL_MAKEFILE := $(lastword $(MAKEFILE_LIST))
STAPPLER_BUILD_ROOT ?= $(dir $(LOCAL_MAKEFILE))../../make
LOCAL_OUTDIR := $(dir $(LOCAL_MAKEFILE))stappler-build
LOCAL_EXECUTABLE := headlesstest
LOCAL_MODULES_PATHS = stappler/stappler-modules.mk xenolith/xenolith-modules.mk
LOCAL_MODULES := xenolith_application xenolith_renderer_basic2d xenolith_backend_vk
LOCAL_ROOT = $(abspath $(dir $(LOCAL_MAKEFILE)))
LOCAL_MAIN := src/main.cpp
include $(STAPPLER_BUILD_ROOT)/universal.mk
