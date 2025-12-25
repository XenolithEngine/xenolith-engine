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

define newline


endef

noop=
space = $(noop) $(noop)
tab = $(noop)	$(noop)

GLOBAL_STDXX ?= gnu++2a
GLOBAL_STD ?= gnu17

LOCAL_EXEC_LIVE_RELOAD ?= 0
LOCAL_USE_INTERNAL_TOOLCHAIN ?= 1

LOCAL_ROOT ?= .
LOCAL_OUTDIR ?= stappler-build

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

LOCAL_ANDROID_TARGET ?= application
LOCAL_ANDROID_PLATFORM ?= android-24
LOCAL_OPTIMIZATION ?= -O3

ifdef LOCAL_EXECUTABLE
APPCONFIG_APP_NAME ?= $(LOCAL_EXECUTABLE)
APPCONFIG_BUNDLE_NAME ?= org.stappler.app.$(LOCAL_EXECUTABLE)

ifeq ($(LOCAL_EXEC_LIVE_RELOAD),1)
LOCAL_LIBRARY ?= $(LOCAL_EXECUTABLE)
endif

else
APPCONFIG_APP_NAME ?=
APPCONFIG_BUNDLE_NAME ?=
endif

# Where to search for a bundled files on a platforms without app bundle
APPCONFIG_BUNDLE_PATH ?= $$EXEC_DIR:$$CWD

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

BUILD_WORKDIR = $(patsubst %/,%,$(dir $(realpath $(firstword $(MAKEFILE_LIST)))))

ifdef BUILD_HOST
LOCAL_INSTALL_DIR ?= $(LOCAL_OUTDIR)/host
BUILD_OUTDIR := $(LOCAL_OUTDIR)/host/$(BUILD_TYPE)
endif

ifdef BUILD_ANDROID
LOCAL_INSTALL_DIR ?= $(LOCAL_OUTDIR)/android
BUILD_OUTDIR := $(LOCAL_OUTDIR)/android/$(BUILD_TYPE)
endif

ifdef BUILD_XWIN
LOCAL_INSTALL_DIR ?= $(LOCAL_OUTDIR)/xwin
BUILD_OUTDIR := $(LOCAL_OUTDIR)/xwin/$(BUILD_TYPE)
endif

ifdef SHARED_PREFIX
GLOBAL_ROOT := $(SHARED_PREFIX)
else
ifdef STAPPLER_BUILD_ROOT
GLOBAL_ROOT := $(realpath $(STAPPLER_BUILD_ROOT)/../..)
else
GLOBAL_ROOT := $(realpath $(dir $(lastword $(MAKEFILE_LIST)))/../../..)
endif
endif

GLOBAL_OUTPUT := $(BUILD_OUTDIR)

GLOBAL_RM ?= rm -f
GLOBAL_CP ?= cp -f
GLOBAL_MAKE ?= make
GLOBAL_MKDIR ?= mkdir -p
GLOBAL_AR ?= ar rcs

# Проверяем хостовую систему, у Darwin нет опции -o для uname

ifdef MACOS
# Принудительная сборка MACOS, вероятно, для генерации файлов проекта
UNAME := Darwin
else

UNAME := $(shell uname)

ifneq ($(UNAME),Darwin)
	UNAME := $(shell uname -o)
endif

ifeq ($(findstring MSYS_NT,$(UNAME)),MSYS_NT)
	UNAME := $(shell uname -o)
endif

endif

UNAME_ARCH ?= $(shell uname -m)

#
# WebAssebmly
#

WIT_BINDGEN ?= wit-bindgen

WASI_SDK ?= /opt/wasi-sdk
WASI_SDK_CC ?= $(WASI_SDK)/bin/clang
WASI_SDK_CXX ?= $(WASI_SDK)/bin/clang++
WASI_THREADS ?= 1

GLOBAL_WASM_OPTIMIZATION ?= -Os

# Отладка WebAssembly фактически останавливает выполнение приложения
# в ожидании подключения отладчика LLDB. Потому, это поведение необходимо
# явно включать, оно не включается автоматически в отладочной форме приложения
GLOBAL_WASM_DEBUG ?= 0

ifdef MSYS
sp_os_path = $(shell cygpath -u $(abspath $(1)))
else
sp_os_path = $(1)
endif

ifeq (4.1,$(firstword $(sort $(MAKE_VERSION) 4.1)))
MAKE_4_1 := 1
else
$(info COMPATIBILITY MODE: Some functions may not work. Minimal required make version: 4.1)
endif

# VULKAN_SDK_PREFIX ?= ~/VulkanSDK/<version>/<OS>
# GLSL -> SpirV compiler (default - glslangValidator from https://github.com/KhronosGroup/glslang/releases/tag/master-tot)
ifdef VULKAN_SDK_PREFIX
GLSLC ?= $(call sp_os_path,$(VULKAN_SDK_PREFIX)/bin/glslangValidator)
SPIRV_LINK ?= $(call sp_os_path,$(VULKAN_SDK_PREFIX)/bin/spirv-link)
else
VULKAN_SDK_PREFIX = /usr/local
GLSLC ?= glslangValidator
SPIRV_LINK ?= spirv-link
endif

GLOBAL_GENERAL_CFLAGS :=
GLOBAL_GENERAL_CXXFLAGS :=

ifdef LOCAL_TOOLCHAIN

include $(LOCAL_TOOLCHAIN)/toolchain.mk
include $(BUILD_ROOT)/utils/apply-toolchain.mk

endif # LOCAL_TOOLCHAIN
