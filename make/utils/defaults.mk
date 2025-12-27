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

ifeq ($(verbose),1)
print_verbose = $(info (verbose := 1) $(1))
else
print_verbose =
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

BUILD_WORKDIR = $(patsubst %/,%,$(dir $(realpath $(firstword $(MAKEFILE_LIST)))))

LOCAL_INSTALL_DIR ?= $(LOCAL_OUTDIR)/host
BUILD_OUTDIR := $(LOCAL_OUTDIR)/host/$(BUILD_TYPE)

ifdef BUILD_ANDROID
LOCAL_INSTALL_DIR ?= $(LOCAL_OUTDIR)/android
BUILD_OUTDIR := $(LOCAL_OUTDIR)/android/$(BUILD_TYPE)
endif

ifdef BUILD_XWIN
LOCAL_INSTALL_DIR ?= $(LOCAL_OUTDIR)/xwin
BUILD_OUTDIR := $(LOCAL_OUTDIR)/xwin/$(BUILD_TYPE)
endif

$(call print_verbose,(defaults.mk) BUILD_OUTDIR: $(BUILD_OUTDIR))


ifdef SHARED_PREFIX
GLOBAL_ROOT := $(SHARED_PREFIX)
else # SHARED_PREFIX
ifdef STAPPLER_BUILD_ROOT
GLOBAL_ROOT := $(realpath $(STAPPLER_BUILD_ROOT)/..)
else
GLOBAL_ROOT := $(realpath $(dir $(lastword $(MAKEFILE_LIST)))/../..)
endif
endif # SHARED_PREFIX

$(call print_verbose,(defaults.mk) GLOBAL_ROOT: $(GLOBAL_ROOT))

GLOBAL_RM ?= rm -f
GLOBAL_CP ?= cp -f
GLOBAL_MAKE ?= make
GLOBAL_MKDIR ?= mkdir -p
GLOBAL_AR ?= ar rcs

# Проверяем хостовую систему, у Darwin нет опции -o для uname

UNAME := $(shell uname)

ifneq ($(UNAME),Darwin)
	UNAME := $(shell uname -o)
endif

ifeq ($(findstring MSYS_NT,$(UNAME)),MSYS_NT)
	UNAME := $(shell uname -o)
endif

$(call print_verbose,(defaults.mk) UNAME: $(UNAME))


#
# WebAssebmly
#

ifdef LOCAL_WASM_MODULE

$(call print_verbose,(defaults.mk) LOCAL_WASM_MODULE: $(LOCAL_WASM_MODULE))

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

$(call print_verbose,(defaults.mk) WIT_BINDGEN: $(WIT_BINDGEN))
$(call print_verbose,(defaults.mk) WASI_SDK_CC: $(WASI_SDK_CC))
$(call print_verbose,(defaults.mk) WASI_SDK_CXX: $(WASI_SDK_CXX))
$(call print_verbose,(defaults.mk) WASI_THREADS: $(WASI_THREADS))

endif # LOCAL_WASM_MODULE

ifeq (4.1,$(firstword $(sort $(MAKE_VERSION) 4.1)))
MAKE_4_1 := 1
else
$(info COMPATIBILITY MODE: Some functions may not work. Minimal required make version: 4.1)
endif

GLOBAL_GENERAL_CFLAGS :=
GLOBAL_GENERAL_CXXFLAGS :=

ifdef STAPPLER_ARCH
STAPPLER_TARGET_ARCH := $(STAPPLER_ARCH)
else # STAPPLER_ARCH
STAPPLER_TARGET_ARCH ?= $(shell uname -m)
endif # STAPPLER_ARCH

ifeq ($(STAPPLER_TARGET),android)

LOCAL_ANDROID_TARGET ?= application
LOCAL_ANDROID_PLATFORM ?= android-24

$(call print_verbose,(defaults.mk) LOCAL_ANDROID_TARGET: $(LOCAL_ANDROID_TARGET) (android library name))
$(call print_verbose,(defaults.mk) LOCAL_ANDROID_PLATFORM: $(LOCAL_ANDROID_PLATFORM) (minimal target API level))

STAPPLER_TARGET_VENDOR :=
STAPPLER_TARGET_SYS := linux
STAPPLER_TARGET_ENV := android

# Unified sysroot
STAPPLER_TARGET_FULL := android

else ifneq ($(or $(filter Darwin,$(UNAME)),$(filter macos,$(STAPPLER_TARGET))),)

STAPPLER_TARGET_VENDOR := apple
STAPPLER_TARGET_SYS := darwin

STAPPLER_TARGET_FULL := $(STAPPLER_TARGET_ARCH)-$(STAPPLER_TARGET_VENDOR)-$(STAPPLER_TARGET_SYS)

else ifneq ($(or $(filter Msys,$(UNAME)),$(filter xwin,$(STAPPLER_TARGET)),$(filter windows,$(STAPPLER_TARGET))),)

STAPPLER_TARGET_VENDOR := pc
STAPPLER_TARGET_SYS := windows
STAPPLER_TARGET_ENV := msvc

STAPPLER_TARGET_FULL := $(STAPPLER_TARGET_ARCH)-$(STAPPLER_TARGET_VENDOR)-$(STAPPLER_TARGET_SYS)-$(STAPPLER_TARGET_ENV)

else ifneq ($(or $(filter %Linux,$(UNAME)),$(filter linux,$(STAPPLER_TARGET))),)

STAPPLER_TARGET_VENDOR := unknown
STAPPLER_TARGET_SYS := linux
STAPPLER_TARGET_ENV := gnu

STAPPLER_TARGET_FULL := $(STAPPLER_TARGET_ARCH)-$(STAPPLER_TARGET_VENDOR)-$(STAPPLER_TARGET_SYS)-$(STAPPLER_TARGET_ENV)

else # OS check

$(error Unknown OS)

endif # OS check

$(call print_verbose,(defaults.mk) STAPPLER_TARGET_ARCH: $(STAPPLER_TARGET_ARCH))
$(call print_verbose,(defaults.mk) STAPPLER_TARGET_VENDOR: $(STAPPLER_TARGET_VENDOR))
$(call print_verbose,(defaults.mk) STAPPLER_TARGET_SYS: $(STAPPLER_TARGET_SYS))
$(call print_verbose,(defaults.mk) STAPPLER_TARGET_ENV: $(STAPPLER_TARGET_ENV))
$(call print_verbose,(defaults.mk) STAPPLER_TARGET_FULL: $(STAPPLER_TARGET_FULL))

$(info Build for $(STAPPLER_TARGET_FULL))

# Check for in-tree runtime

GLOBAL_HAS_RUNTIME := 0
GLOBAL_RUNTIME_PATH :=

LOCAL_USE_RUNTIME_FROM_TOOLCHAIN ?= 0
$(call print_verbose,(defaults.mk) LOCAL_USE_RUNTIME_FROM_TOOLCHAIN: $(LOCAL_USE_RUNTIME_FROM_TOOLCHAIN))

ifneq ($(LOCAL_USE_RUNTIME_FROM_TOOLCHAIN),1)
LOCAL_RUNTIME_PATH ?= $(abspath $(GLOBAL_ROOT))/runtime

$(call print_verbose,(defaults.mk) Try to use in-tree runtime (LOCAL_RUNTIME_PATH): $(LOCAL_RUNTIME_PATH)/runtime.mk)

-include $(LOCAL_RUNTIME_PATH)/runtime.mk

ifeq ($(filter runtime,$(TOOLKIT_ALL_MODULES)),)
$(call print_verbose,(defaults.mk) No runtime found in path: $(LOCAL_RUNTIME_PATH)/runtime.mk, try to use runtime from toolchain)
else
GLOBAL_RUNTIME_PATH := $(LOCAL_RUNTIME_PATH)
GLOBAL_HAS_RUNTIME := 1
$(call print_verbose,(defaults.mk) GLOBAL_RUNTIME_PATH: $(GLOBAL_RUNTIME_PATH))
endif
endif


ifdef LOCAL_TOOLCHAIN

$(call print_verbose,(defaults.mk) LOCAL_TOOLCHAIN: $(LOCAL_TOOLCHAIN))

include $(LOCAL_TOOLCHAIN)/toolchain.mk

else # LOCAL_TOOLCHAIN

LOCAL_USE_INTERNAL_TOOLCHAIN ?= optional

$(call print_verbose,(defaults.mk) LOCAL_USE_INTERNAL_TOOLCHAIN: $(LOCAL_USE_INTERNAL_TOOLCHAIN) (variants: 0 | optional | required))

ifneq ($(LOCAL_USE_INTERNAL_TOOLCHAIN),0)

STAPPLER_TARGET_ROOT := $(abspath $(GLOBAL_ROOT))/toolchains/targets

$(call print_verbose,(defaults.mk) Try to load internal toolchain: $(STAPPLER_TARGET_ROOT)/$(STAPPLER_TARGET_FULL)/toolchain.mk)

-include $(STAPPLER_TARGET_ROOT)/$(STAPPLER_TARGET_FULL)/toolchain.mk

ifndef TOOLCHAIN_SYSROOT
ifeq ($(GLOBAL_HAS_RUNTIME),1)
$(call print_verbose,(defaults.mk) Fail to load internal toolchain from main source tree; try runtime source tree)

STAPPLER_TARGET_ROOT := $(abspath $(GLOBAL_RUNTIME_PATH))/toolchains/targets

$(call print_verbose,(defaults.mk) Try to load internal toolchain: $(STAPPLER_TARGET_ROOT)/$(STAPPLER_TARGET_FULL)/toolchain.mk)

-include $(STAPPLER_TARGET_ROOT)/$(STAPPLER_TARGET_FULL)/toolchain.mk

endif # ($(GLOBAL_HAS_RUNTIME),1)
endif # TOOLCHAIN_SYSROOT




ifeq ($(LOCAL_USE_INTERNAL_TOOLCHAIN),required)
ifndef TOOLCHAIN_SYSROOT
$(error Fail to load internal toolchain (LOCAL_USE_INTERNAL_TOOLCHAIN: $(LOCAL_USE_INTERNAL_TOOLCHAIN)))
endif
else
ifndef TOOLCHAIN_SYSROOT
$(info Fail to load internal toolchain (LOCAL_USE_INTERNAL_TOOLCHAIN: $(LOCAL_USE_INTERNAL_TOOLCHAIN)))
endif
endif # ($(LOCAL_USE_INTERNAL_TOOLCHAIN),required)

endif # ($(LOCAL_USE_INTERNAL_TOOLCHAIN),0)
endif # LOCAL_TOOLCHAIN


ifdef TOOLCHAIN_SYSROOT
$(call print_verbose,(defaults.mk) TOOLCHAIN_SYSROOT: $(TOOLCHAIN_SYSROOT))
include $(BUILD_ROOT)/utils/apply-toolchain.mk

ifneq ($(GLOBAL_HAS_RUNTIME),1)

$(call print_verbose,(defaults.mk) Try to use runtime from toolchain: $(TOOLCHAIN_SYSROOT)/share/stappler/runtime.mk)

-include $(TOOLCHAIN_SYSROOT)/share/stappler/runtime.mk

ifneq ($(filter runtime,$(TOOLKIT_ALL_MODULES)),)
GLOBAL_RUNTIME_PATH := $(TOOLCHAIN_SYSROOT)/share/stappler
GLOBAL_HAS_RUNTIME := 1
$(call print_verbose,(defaults.mk) GLOBAL_RUNTIME_PATH: $(GLOBAL_RUNTIME_PATH) (from toolchain source tree))
else
$(call print_verbose,(defaults.mk) Fail to load runtime from toolchain)
endif

endif # ifneq ($(GLOBAL_HAS_RUNTIME),1)

endif # TOOLCHAIN_SYSROOT


ifneq ($(GLOBAL_HAS_RUNTIME),1)
$(error Fail to find runtime for comppilation)
endif

ifndef GLSLC
ifdef VULKAN_SDK_PREFIX
GLSLC ?= $(VULKAN_SDK_PREFIX)/bin/glslangValidator
$(call print_verbose,(defaults.mk) GLSLC: $(GLSLC) (from OS Vulkan SDK) (VULKAN_SDK_PREFIX: $(VULKAN_SDK_PREFIX)))
else # VULKAN_SDK_PREFIX
#VULKAN_SDK_PREFIX = /usr/local
GLSLC ?= glslangValidator
$(call print_verbose,(defaults.mk) GLSLC: $(GLSLC) (from OS distribution))
endif # VULKAN_SDK_PREFIX
endif

ifndef SPIRV_LINK
ifdef VULKAN_SDK_PREFIX
SPIRV_LINK ?= $(VULKAN_SDK_PREFIX)/bin/spirv-link
$(call print_verbose,(defaults.mk) SPIRV_LINK: $(SPIRV_LINK) (from OS Vulkan SDK) (VULKAN_SDK_PREFIX: $(VULKAN_SDK_PREFIX)))
else # VULKAN_SDK_PREFIX
#VULKAN_SDK_PREFIX = /usr/local
SPIRV_LINK ?= spirv-link
$(call print_verbose,(defaults.mk) SPIRV_LINK: $(SPIRV_LINK) (from OS distribution))
endif # VULKAN_SDK_PREFIX
endif
