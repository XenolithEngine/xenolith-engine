LOCAL_MAKEFILE := $(lastword $(MAKEFILE_LIST))
STAPPLER_BUILD_ROOT ?= $(dir $(LOCAL_MAKEFILE))../../make
LOCAL_OUTDIR := $(dir $(LOCAL_MAKEFILE))stappler-build
LOCAL_EXECUTABLE := wgpuengine
LOCAL_MODULES_PATHS = stappler/stappler-modules.mk xenolith/xenolith-modules.mk
LOCAL_MODULES := xenolith_backend_webgpu
LOCAL_ROOT = $(abspath $(dir $(LOCAL_MAKEFILE)))
LOCAL_SRCS_DIRS := src
LOCAL_CXXFLAGS := -ferror-limit=0
LOCAL_MAIN := src/main.cpp
include $(STAPPLER_BUILD_ROOT)/universal.mk
