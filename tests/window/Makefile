# Copyright (c) 2024-2025 Stappler LLC <admin@stappler.dev>
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

# Рабочая директория
LOCAL_ROOT = $(dir $(LOCAL_MAKEFILE))

# Директория для результатов сборки
LOCAL_OUTDIR := $(dir $(LOCAL_MAKEFILE))stappler-build

# Название исполняемого файла
LOCAL_EXECUTABLE := testapp

# Пути к наборам модулей
LOCAL_MODULES_PATHS = \
	stappler/stappler-modules.mk \
	xenolith/xenolith-modules.mk

# Используемые модули
LOCAL_MODULES := \
	xenolith_application \
	xenolith_application_main \
	xenolith_renderer_simpleui \
	xenolith_renderer_ui \
	xenolith_renderer_pug \
	xenolith_backend_vk \
	xenolith_resources_assets \
	stappler_makefile

# WebGPU build variant: `make WGPU=1` adds the WebGPU backend and the
# 2d renderer WebGPU passes; select at runtime with `testapp --gapi webgpu`
ifeq ($(WGPU),1)
LOCAL_MODULES += \
	xenolith_backend_webgpu \
	xenolith_renderer_basic2d_webgpu
endif

# Metal build variant (macOS/iOS only): `make MTL=1` adds the Metal backend
# and the 2d renderer Metal passes; select at runtime with `testapp --gapi metal`
ifeq ($(MTL),1)
LOCAL_MODULES += \
	xenolith_backend_mtl \
	xenolith_renderer_basic2d_mtl
endif

# Software rasterizer build variant: `make SOFT=1` adds the CPU backend and the
# 2d renderer flat pass for it; select at runtime with `testapp --headless --gapi soft`
ifeq ($(SOFT),1)
LOCAL_MODULES += \
	xenolith_backend_soft \
	xenolith_renderer_basic2d_soft
endif

LOCAL_SRCS_DIRS :=  src
LOCAL_SRCS_OBJS :=

LOCAL_INCLUDES_DIRS :=
LOCAL_INCLUDES_OBJS := src

LOCAL_SHADERS_DIR := shaders

# Android config
LOCAL_ANDROID_MK := $(LOCAL_ROOT)/proj.android/app/Android.mk # path to root Android.mk
LOCAL_APPLICATION_MK := $(LOCAL_ROOT)/proj.android/app/Application.mk # path to Application.mk
LOCAL_ANDROID_TARGET := application # target to build
LOCAL_ANDROID_PLATFORM := android-24 # android platform version

# Подключения системы сборки
include $(STAPPLER_BUILD_ROOT)/universal.mk

ifneq ($(TARGET_SYSTEM),Android-NDK)
all:
	$(MAKE) -C $(LOCAL_ROOT)/client STAPPLER_TARGET=$(STAPPLER_TARGET) all
endif

# Bundle app resources into the .app: the runtime resolves
# LocationCategory::Bundled to Contents/Resources when running as a bundle
# (see SPRuntimeFilesystem-macos.mm), mirroring the Android assets rsync below
ifeq ($(TARGET_SYSTEM),Darwin)
.PHONY: bundle-resources
bundle-resources: $(BUILD_EXECUTABLE)
	@mkdir -p $(abspath $(dir $(BUILD_EXECUTABLE))/..)/Resources
	rsync -ar --delete $(LOCAL_ROOT)/resources $(abspath $(dir $(BUILD_EXECUTABLE))/..)/Resources/

all: bundle-resources
endif

prepare-android:
	@mkdir -p $(LOCAL_ROOT)/proj.android/app/assets
	rsync -ar $(LOCAL_ROOT)/resources $(LOCAL_ROOT)/proj.android/app/assets

ifdef STAPPLER_TARGET

android-export: prepare-android

endif

.PHONY: prepare-android

#
# Cross-target build helpers
#
# "Available" targets are the ones for which a toolchain description is actually
# installed in this checkout, i.e. every <root>/[runtime/]toolchains/targets/*/target.mk.
# The list is therefore discovered, not hardcoded: a target appears here as soon as
# its toolchain is built/unpacked and disappears when it is not available.
#
#   make list-targets                          # print the discovered target list
#   make all-targets                           # build the app for every target
#   make -k all-targets                        # ... without stopping on first failure
#   make all-targets -j8                       # targets build in parallel
#   make all-targets RELEASE=1                 # command-line vars are forwarded
#   make all-targets WGPU=1                    # ... including the backend variants
#   make all-targets-clean                     # clean every target
#   make target-x86_64-pc-windows-msvc         # build a single target
#   make all-targets TARGETS="a b"             # restrict the list explicitly
#   make all-targets TARGETS_EXCLUDE="aarch64-apple-macosx"
#
# Cross-targets are only compiled here, not run.

# Only meaningful in the dispatch pass (no STAPPLER_TARGET/STAPPLER_BUILD yet);
# inside a per-target sub-make universal.mk has already switched to host.mk.
ifeq ($(or $(STAPPLER_TARGET),$(STAPPLER_BUILD)),)

LOCAL_STAPPLER_ROOT := $(abspath $(STAPPLER_BUILD_ROOT)/..)

LOCAL_TARGET_FILES := \
	$(wildcard $(LOCAL_STAPPLER_ROOT)/toolchains/targets/*/target.mk) \
	$(wildcard $(LOCAL_STAPPLER_ROOT)/runtime/toolchains/targets/*/target.mk)

# %-unknown-linux-android[eabi] are the sprt Android *library* targets: on ANDROID the entry
# point is JNI_OnLoad/ANativeActivity_onCreate, not main (see xenolith/application/XLMain.cpp),
# so an app cannot be linked as an executable there. The Android app is built through
# unknown-ndk-linux-android instead (ndk-build -> libapplication.so for every ABI), and that
# one is deliberately NOT matched by the patterns below.
TARGETS_EXCLUDE ?= %+dll %+sprt %-ios %-ios-simulator \
	%-unknown-linux-android %-unknown-linux-androideabi

TARGETS ?= $(sort $(foreach f,$(LOCAL_TARGET_FILES),$(notdir $(patsubst %/,%,$(dir $(f))))))
TARGETS := $(filter-out $(TARGETS_EXCLUDE),$(TARGETS))

LOCAL_TARGET_GOALS := $(addprefix target-,$(TARGETS))
LOCAL_TARGET_CLEAN_GOALS := $(addsuffix -clean,$(LOCAL_TARGET_GOALS))

$(LOCAL_TARGET_GOALS): target-%:
	@$(MAKE) -f $(LOCAL_MAKEFILE) $(FORWARD_ARGS) STAPPLER_TARGET=$* all

$(LOCAL_TARGET_CLEAN_GOALS): target-%-clean:
	@$(MAKE) -f $(LOCAL_MAKEFILE) $(FORWARD_ARGS) STAPPLER_TARGET=$* clean

all-targets: $(LOCAL_TARGET_GOALS)

all-targets-clean: $(LOCAL_TARGET_CLEAN_GOALS)

list-targets:
	$(info Available targets ($(words $(TARGETS))):)$(foreach t,$(TARGETS),$(info - $(t)))

.PHONY: all-targets all-targets-clean list-targets
.PHONY: $(LOCAL_TARGET_GOALS) $(LOCAL_TARGET_CLEAN_GOALS)

endif
