# Copyright (c) 2023-2024 Stappler LLC <admin@stappler.dev>
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

# Функция счётчика прогресса
ifeq (4.1,$(firstword $(sort $(MAKE_VERSION) 4.1)))
sp_counter_text = [$(BUILD_TARGET): $$(($(BUILD_CURRENT_COUNTER)*100/$(BUILD_FILES_COUNTER)))% $(BUILD_CURRENT_COUNTER)/$(BUILD_FILES_COUNTER)]
else
sp_counter_text = 
endif

ifdef XLMAKE_VERSION
verbose_log =
target_log =
VERBOSE_GUARD := 
else
ifdef verbose
verbose_log =
target_log = @ $(GLOBAL_ECHO) $1
VERBOSE_GUARD := 
else
verbose_log = @ $(GLOBAL_ECHO) $1
target_log = @ $(GLOBAL_ECHO) $1
VERBOSE_GUARD := @
endif
endif # XLMAKE_VERSION

GLOBAL_QUIET_CC = $(call verbose_log,"$(call sp_counter_text) [$(notdir $(GLOBAL_CC))] $(notdir $@)")
GLOBAL_QUIET_CPP = $(call verbose_log,"$(call sp_counter_text) [$(notdir $(GLOBAL_CXX))] $(notdir $@)")
GLOBAL_QUIET_LINK = $(call verbose_log,"[Link] $@")
GLOBAL_QUIET_LINK_SHARED = $(call verbose_log,"[DSO Link] $$(notdir $$@)")
GLOBAL_QUIET_LINK_STATIC = $(call verbose_log,"[Static Link] $$(notdir $$@)")
GLOBAL_QUIET_GLSLC = $(call verbose_log,"[$(notdir $(GLSLC))] $(notdir $(abspath $(dir $(1))))/$(notdir $(1))")
GLOBAL_QUIET_WIT = $(call verbose_log,"[wit] $(notdir $@)")
GLOBAL_QUIET_WIT_BINDGEN = $(call verbose_log,"[$(notdir $(WIT_BINDGEN))]")

# Progress counter
BUILD_CURRENT_COUNTER ?= 1
BUILD_FILES_COUNTER ?= 1
BUILD_LIB_COUNTER :=
BUILD_EXEC_COUNTER :=
BUILD_TARGET :=

define BUILD_LIB_template =
$(eval BUILD_LIB_COUNTER=$(BUILD_LIB_COUNTER) 1)
$(1):BUILD_CURRENT_COUNTER:=$(words $(BUILD_LIB_COUNTER))
$(1):BUILD_FILES_COUNTER := $(3)
$(1):BUILD_TARGET := $(2)
$(1):.TARGET_NAME := [$(2)] $(notdir $(1))
endef

define BUILD_EXEC_template =
$(eval BUILD_EXEC_COUNTER=$(BUILD_EXEC_COUNTER) 1)
$(1):BUILD_CURRENT_COUNTER:=$(words $(BUILD_EXEC_COUNTER))
$(1):BUILD_FILES_COUNTER := $(3)
$(1):BUILD_TARGET := $(2)
$(1):.TARGET_NAME := [$(2)] $(notdir $(1))
endef
