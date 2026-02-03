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

ifdef verbose
GLOBAL_QUIET_CC =
GLOBAL_QUIET_CPP =
GLOBAL_QUIET_LINK =
GLOBAL_QUIET_LINK_SHARED =
GLOBAL_QUIET_LINK_STATIC =
GLOBAL_QUIET_GLSLC =
GLOBAL_QUIET_SPIRV_LINK =
GLOBAL_QUIET_SPIRV_EMBED =
GLOBAL_QUIET_WIT =
GLOBAL_QUIET_WIT_BINDGEN =
GLOBAL_QUIET_WASM_CC =
GLOBAL_QUIET_WASM_CXX =
GLOBAL_QUIET_WASM_LINK =
else
GLOBAL_QUIET_CC = @ echo "$(call sp_counter_text) [$(notdir $(GLOBAL_CC))] $(notdir $@)" ;
GLOBAL_QUIET_CPP = @ echo "$(call sp_counter_text) [$(notdir $(GLOBAL_CXX))] $(notdir $@)" ;
GLOBAL_QUIET_LINK = @ echo [Link] $@ ;
GLOBAL_QUIET_LINK_SHARED = @ echo [DSO Link] $$(notdir $$@) ;
GLOBAL_QUIET_LINK_STATIC = @ echo [Static Link] $$(notdir $$@) ;
GLOBAL_QUIET_GLSLC = @ echo [$(notdir $(GLSLC))] $(notdir $(abspath $(dir $(1))))/$(notdir $(1)) ;
GLOBAL_QUIET_SPIRV_LINK = @ echo [$(notdir $(SPIRV_LINK))] $(notdir $@) ;
GLOBAL_QUIET_SPIRV_EMBED = @ echo [embed] $(notdir $@) ;
GLOBAL_QUIET_WIT = @ echo [wit] $(notdir $@) ;
GLOBAL_QUIET_WIT_BINDGEN = @ echo [$(notdir $(WIT_BINDGEN))] ;
GLOBAL_QUIET_WASM_CC = @ echo [wasm:$(notdir $(WASI_SDK_CC))] $(notdir $@) ;
GLOBAL_QUIET_WASM_CXX = @ echo [wasm:$(notdir $(WASI_SDK_CXX))] $(notdir $@) ;
GLOBAL_QUIET_WASM_LINK = @ echo [wasm:Link] $@ ;
endif

# Progress counter
BUILD_CURRENT_COUNTER ?= 1
BUILD_FILES_COUNTER ?= 1
BUILD_LIB_COUNTER := 0
BUILD_EXEC_COUNTER := 0
BUILD_TARGET :=

define BUILD_LIB_template =
$(eval BUILD_LIB_COUNTER=$(BUILD_LIB_COUNTER) 1)
$(1):BUILD_CURRENT_COUNTER:=$(BUILD_LIB_COUNTER)
$(1):BUILD_FILES_COUNTER := $(3)
$(1):BUILD_TARGET := $(2)
endef

define BUILD_EXEC_template =
$(eval BUILD_EXEC_COUNTER=$(BUILD_EXEC_COUNTER) 1)
$(1):BUILD_CURRENT_COUNTER:=$(words $(BUILD_EXEC_COUNTER))
$(1):BUILD_FILES_COUNTER := $(3)
$(1):BUILD_TARGET := $(2)
endef
