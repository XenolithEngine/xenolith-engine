# Copyright (c) 2024 Stappler LLC <admin@stappler.dev>
# Copyright (c) 2025 Stappler Team <admin@stappler.org>
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

# key variables:
#	STAPPLER_HOST - current host id (x86_64-unknown-linux-gnu, aarch64-pc-windows-msvc, etc), set by build-system
#
#	STAPPLER_HOST_FILE - file with host's toolchain definition, set by user or defaulted to standert locations:
#		<root>/toolchains/hosts/$(STAPPLER_HOST)/host.mk or <root>/runtime/toolchains/hosts/$(STAPPLER_HOST)/host.mk
#		The host file should define HOST_BINDIR and host-specific tools (HOST_CC, HOST_CXX, HOST_AR).
#		System designed with clang compiler in mind, but you can use GCC cross-compilers or other compilers,
#		that can produce code for the specified target.
#		Host file can provide compiler's host specific flags via HOST_(GENERAL|LIB|EXEC)_(C|CXX|LD)FLAGS
#
#	STAPPLER_TARGET - user-specified target in form <arch>-<vendor>-<os>[-<variant>], or $(STAPPLER_HOST)
#
#	STAPPLER_TARGET_FILE - file with target's toolchain definition, set by user or defaulted to standert locations:
#		<root>/toolchains/targets/$(STAPPLER_TARGET)/target.mk or <root>/runtime/toolchains/targets/$(STAPPLER_TARGET)/host.mk
#		Target file should define:
#			TARGET_SYSROOT - target's sysroot for compiler
#			TARGET_NAME - target's name for compiler's --target argument (can be empty if not supported)
#			TARGET_SYSTEM - target's system name (Linux/Windows/MacOS/Android/iOS, etc)
#		
#		Target file can provide target's specific flags via TARGET_(GENERAL|LIB|EXEC)_(C|CXX|LD)FLAGS
#

ifeq ($(verbose),1)
print_verbose = $(info (verbose := 1) $(1))
else
print_verbose =
endif


ifdef XLMAKE_VERSION
MAKE_4_1 := 1 # xlmake is 4.1-compatible
$(info XLMAKE_VERSION $(XLMAKE_VERSION))
else

ifeq (4.1,$(firstword $(sort $(MAKE_VERSION) 4.1)))
MAKE_4_1 := 1
else
$(info COMPATIBILITY MODE: Some functions may not work. Minimal required make version: 4.1)
endif

endif


TOOLKIT_ALL_MODULES :=

define define_module
$(eval TOOLKIT_ALL_MODULES := $(TOOLKIT_ALL_MODULES) $(1))
$(eval $(addprefix MODULE_,$(1)) := $(2))
$(call print_verbose,(defaults.mk) define_module: $(1) = $(2))
endef


define newline


endef

noop=
space = $(noop) $(noop)
tab = $(noop)	$(noop)


LOCAL_ROOT ?= .
LOCAL_OUTDIR ?= stappler-build
LOCAL_EXEC_LIVE_RELOAD ?= 0

LOCAL_ROOT := $(subst \,/,$(LOCAL_ROOT))
LOCAL_OUTDIR := $(subst \,/,$(LOCAL_OUTDIR))

ifneq ($(filter %/,$(LOCAL_ROOT)),)
LOCAL_ROOT := $(patsubst %/,%,$(LOCAL_ROOT))
endif


$(call print_verbose,(defaults.mk) LOCAL_ROOT: $(LOCAL_ROOT))
$(call print_verbose,(defaults.mk) LOCAL_OUTDIR: $(LOCAL_OUTDIR))
$(call print_verbose,(defaults.mk) LOCAL_EXEC_LIVE_RELOAD: $(LOCAL_EXEC_LIVE_RELOAD))


GLOBAL_STDXX ?= gnu++2a
GLOBAL_STD ?= gnu17

LOCAL_OPTIMIZATION ?= -O3
GLOBAL_OPTIMIZATION := $(LOCAL_OPTIMIZATION)

$(call print_verbose,(defaults.mk) GLOBAL_STDXX: $(GLOBAL_STDXX))
$(call print_verbose,(defaults.mk) GLOBAL_STD: $(GLOBAL_STD))
$(call print_verbose,(defaults.mk) GLOBAL_OPTIMIZATION: $(GLOBAL_OPTIMIZATION))

# Глобальный тип сборки
ifdef RELEASE
	BUILD_TYPE := release
else
	BUILD_TYPE := debug
endif # ifdef RELEASE

ifdef COVERAGE
	BUILD_TYPE := coverage
endif

$(call print_verbose,(defaults.mk) BUILD_TYPE: $(BUILD_TYPE))

# Для live reload собирать библиотеку в режиме отдельного модуля
ifeq ($(LOCAL_EXEC_LIVE_RELOAD),1)
LOCAL_BUILD_SHARED ?= 3
LOCAL_BUILD_STATIC ?= 0
APPCONFIG_EXEC_LIVE_RELOAD = 1
else
LOCAL_BUILD_SHARED ?= 1
LOCAL_BUILD_STATIC ?= 1
APPCONFIG_EXEC_LIVE_RELOAD = 0
endif



ifdef LOCAL_EXECUTABLE
APPCONFIG_APP_NAME ?= $(LOCAL_EXECUTABLE)
APPCONFIG_BUNDLE_NAME ?= org.stappler.app.$(LOCAL_EXECUTABLE)
APPCONFIG_BUNDLE_PATH ?= $$EXEC_DIR:$$CWD

$(call print_verbose,(defaults.mk) LOCAL_EXECUTABLE: $(LOCAL_EXECUTABLE))
$(call print_verbose,(defaults.mk) (Build LOCAL_EXECUTABLE) APPCONFIG_APP_NAME: $(APPCONFIG_APP_NAME))
$(call print_verbose,(defaults.mk) (Build LOCAL_EXECUTABLE) APPCONFIG_BUNDLE_NAME: $(APPCONFIG_BUNDLE_NAME))
$(call print_verbose,(defaults.mk) (Build LOCAL_EXECUTABLE) APPCONFIG_BUNDLE_PATH: $(APPCONFIG_BUNDLE_PATH))

ifeq ($(LOCAL_EXEC_LIVE_RELOAD),1)
LOCAL_LIBRARY ?= $(LOCAL_EXECUTABLE)
$(call print_verbose,(defaults.mk) LOCAL_LIBRARY: $(LOCAL_LIBRARY) (Live reload))
endif

else # LOCAL_EXECUTABLE
APPCONFIG_APP_NAME ?=
APPCONFIG_BUNDLE_NAME ?=
endif # LOCAL_EXECUTABLE


# Where to search for a bundled files on a platforms without app bundle

# Linux: if >0 - use XDG locations for application files
# Windows: if 1 - use System-provided AppData folder
#             2 - use AppContainer paths for application files
#             3 - run application itself in AppContainer
#   (Container name: APPCONFIG_BUNDLE_NAME)
APPCONFIG_APP_PATH_COMMON ?= 0

APPCONFIG_VERSION_VARIANT ?= 0
APPCONFIG_VERSION_API ?= 0
APPCONFIG_VERSION_REV ?= 0
APPCONFIG_VERSION_BUILD ?= 0


ifeq ($(findstring Windows,$(OS)),Windows)
include $(BUILD_ROOT)/utils/init-powershell.mk
else # Windows
include $(BUILD_ROOT)/utils/init-sh.mk
endif # Windows


ifdef SHARED_PREFIX
GLOBAL_ROOT := $(SHARED_PREFIX)
else ifdef STAPPLER_BUILD_ROOT
GLOBAL_ROOT := $(realpath $(STAPPLER_BUILD_ROOT)/..)
else
GLOBAL_ROOT := $(realpath $(dir $(lastword $(MAKEFILE_LIST)))/../..)
endif

# Based on initial makefile location
BUILD_WORKDIR = $(patsubst %/,%,$(dir $(realpath $(firstword $(MAKEFILE_LIST)))))

$(call print_verbose,(defaults.mk) GLOBAL_ROOT: $(GLOBAL_ROOT))
$(call print_verbose,(defaults.mk) BUILD_WORKDIR: $(BUILD_WORKDIR))


#
# Find host toolchain's half
#

ifdef STAPPLER_HOST_FILE

$(call print_verbose,(defaults.mk) Use user-provided host file: $(STAPPLER_HOST_FILE))
include $(STAPPLER_HOST_FILE)

else

$(call print_verbose,(defaults.mk) Try to find host file in GLOBAL_ROOT: $(GLOBAL_ROOT)/toolchains/hosts/$(STAPPLER_HOST)/host.mk)

-include $(GLOBAL_ROOT)/toolchains/hosts/$(STAPPLER_HOST)/host.mk

ifndef HOST_BINDIR
$(call print_verbose,(defaults.mk) Try runtime root: $(GLOBAL_ROOT)/runtime/toolchains/hosts/$(STAPPLER_HOST)/host.mk)
-include $(GLOBAL_ROOT)/runtime/toolchains/hosts/$(STAPPLER_HOST)/host.mk
endif

endif

ifndef HOST_BINDIR
$(error HOST_BINDIR is not defined: no host "$(STAPPLER_HOST)" specification found)
else
$(call print_verbose,(defaults.mk) Build as host: $(STAPPLER_HOST))
endif


#
# Find target toolchain's half
#

STAPPLER_TARGET_DIR :=

ifndef STAPPLER_TARGET
$(call print_verbose,(defaults.mk) No user-provided target, use STAPPLER_HOST: $(STAPPLER_HOST))
STAPPLER_TARGET := $(STAPPLER_HOST)
else
$(call print_verbose,(defaults.mk) User-provided target, $(STAPPLER_TARGET))
endif

ifdef STAPPLER_TARGET_FILE

$(call print_verbose,(defaults.mk) Use user-provided host file: $(STAPPLER_TARGET_FILE))
include $(STAPPLER_TARGET_FILE)
STAPPLER_TARGET_DIR := $(dir $(STAPPLER_TARGET_FILE))
else

$(call print_verbose,(defaults.mk) Try to find target file in GLOBAL_ROOT: $(GLOBAL_ROOT)/toolchains/targets/$(STAPPLER_TARGET)/target.mk)

-include $(GLOBAL_ROOT)/toolchains/targets/$(STAPPLER_TARGET)/target.mk
STAPPLER_TARGET_DIR := $(GLOBAL_ROOT)/toolchains/targets/$(STAPPLER_TARGET)

ifndef TARGET_SYSROOT
$(call print_verbose,(defaults.mk) Failed! Try runtime root: $(GLOBAL_ROOT)/runtime/toolchains/targets/$(STAPPLER_TARGET)/target.mk)
-include $(GLOBAL_ROOT)/runtime/toolchains/targets/$(STAPPLER_TARGET)/target.mk
STAPPLER_TARGET_DIR := $(GLOBAL_ROOT)/runtime/toolchains/targets/$(STAPPLER_TARGET)
endif

endif

ifndef TARGET_SYSROOT
$(error TARGET_SYSROOT is not defined: no target "$(STAPPLER_TARGET)" specification found)
endif


LOCAL_INSTALL_DIR ?= $(LOCAL_OUTDIR)/$(STAPPLER_TARGET)
BUILD_OUTDIR := $(LOCAL_OUTDIR)/$(STAPPLER_TARGET)/$(BUILD_TYPE)

$(call print_verbose,(defaults.mk) STAPPLER_TARGET: $(STAPPLER_TARGET))
$(call print_verbose,(defaults.mk) BUILD_OUTDIR: $(BUILD_OUTDIR))


#
# Android special NDK target: unknown-ndk-linux-android
#

ifeq ($(STAPPLER_TARGET),unknown-ndk-linux-android)

LOCAL_ANDROID_TARGET ?= application
LOCAL_ANDROID_PLATFORM ?= android-24

$(call print_verbose,(defaults.mk) ANDROID_HOST: $(ANDROID_HOST))

$(call print_verbose,(defaults.mk) LOCAL_ANDROID_TARGET: $(LOCAL_ANDROID_TARGET) (android library name))
$(call print_verbose,(defaults.mk) LOCAL_ANDROID_PLATFORM: $(LOCAL_ANDROID_PLATFORM) (minimal target API level))

endif # OS check

include $(BUILD_ROOT)/utils/apply-toolchain.mk

ifndef GLSLC
ifdef VULKAN_SDK_PREFIX
GLSLC ?= $(VULKAN_SDK_PREFIX)/bin/glslangValidator
$(call print_verbose,(defaults.mk) GLSLC: $(GLSLC) (from OS Vulkan SDK) (VULKAN_SDK_PREFIX: $(VULKAN_SDK_PREFIX)))
else # VULKAN_SDK_PREFIX
GLSLC ?= glslangValidator
$(call print_verbose,(defaults.mk) GLSLC: $(GLSLC) (from OS distribution))
endif # VULKAN_SDK_PREFIX
endif

ifndef SPIRV_LINK
ifdef VULKAN_SDK_PREFIX
SPIRV_LINK ?= $(VULKAN_SDK_PREFIX)/bin/spirv-link
$(call print_verbose,(defaults.mk) SPIRV_LINK: $(SPIRV_LINK) (from OS Vulkan SDK) (VULKAN_SDK_PREFIX: $(VULKAN_SDK_PREFIX)))
else # VULKAN_SDK_PREFIX
SPIRV_LINK ?= spirv-link
$(call print_verbose,(defaults.mk) SPIRV_LINK: $(SPIRV_LINK) (from OS distribution))
endif # VULKAN_SDK_PREFIX
endif
