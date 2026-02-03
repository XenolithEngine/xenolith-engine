# Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>
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

# to use without Stappler SDK, just watch for root module list
LOCAL_MODULES_PATHS ?= $(GLOBAL_ROOT)/modules.mk

define include_module_source =
TOOLKIT_MODULE_PATH := $(1)
include $(1)
endef

define include_module_optional =
$(info Optional module "$(1)": $(if $(MODULE_$(1)),<found>,<not found>))
LOCAL_MODULES += $(if $(MODULE_$(1)),$(1))
endef

# inject debug module
ifndef SHARED_PREFIX
LOCAL_MODULES_PATHS += $(realpath $(BUILD_ROOT)/../module/module.mk)
endif # SHARED_PREFIX


ifeq ($(TOOLCHAIN_REQUIRES_STAPPLER_ABI),1)
LOCAL_MODULES += stappler_abi
endif


TOOLKIT_MODULE_LIST :=

ifdef SHARED_PREFIX
TOOLKIT_MODULE_LIST += $(wildcard $(BUILD_ROOT)/modules/*.mk)
$(foreach include,$(filter /%,$(LOCAL_MODULES_PATHS)),$(eval $(call include_module_path,$(include))))
else
include $(filter /%,$(LOCAL_MODULES_PATHS)) $(addprefix $(GLOBAL_ROOT)/,$(filter-out /%,$(LOCAL_MODULES_PATHS)))
endif

ifdef MAKE_4_1
$(foreach include,$(TOOLKIT_MODULE_LIST),$(eval $(call include_module_source,$(include))))
else
include $(TOOLKIT_MODULE_LIST)
endif # MAKE_4_1

$(foreach module,$(LOCAL_MODULES_OPTIONAL),$(eval $(call include_module_optional,$(module))))

define emplace_module =
$(eval LOCAL_MODULES = $(LOCAL_MODULES) $(1))
$(eval $(call follow_deps_module,$(MODULE_$(1))))
endef

define follow_deps_module =
$(foreach dep,$(filter-out $(LOCAL_MODULES),$($(1)_DEPENDS_ON)),\
	$(eval $(call emplace_module,$(dep))) \
)
endef # follow_deps_module

define follow_consume_module =
$(foreach dep,$(filter-out $(LOCAL_MODULES),$($(1)_SHARED_CONSUME)),\
	$(eval $(call emplace_module,$(dep))) \
)
endef # follow_consume_module

define define_consumed =
$(2)_CONSUMED_BY := $(1)
endef # define_consumed

define merge_module =
TOOLKIT_MODULE_VARS += $(1)
$(foreach module,$($(1)_SHARED_CONSUME),$(eval $(call define_consumed,$(2),$(MODULE_$(module)))))
endef # merge_module

reverse_modules = $(if $(wordlist 2,2,$(1)),$(call reverse_modules,$(wordlist 2,$(words $(1)),$(1))) $(firstword $(1)),$(1))
unique_modules = $(if $1,$(firstword $1) $(call unique_modules,$(filter-out $(firstword $1),$1)))


ifdef MAKE_4_1
$(foreach module,$(LOCAL_MODULES),$(foreach MOD,$(MODULE_$(module)),\
	$(eval $(call follow_deps_module,$(MOD)))))

ifdef BUILD_SHARED
$(foreach module,$(LOCAL_MODULES),$(foreach MOD,$(MODULE_$(module)),\
	$(eval $(call follow_consume_module,$(MOD)))))
endif

else
# WARNING: no infinite recursion control!
read_dependencies = $(filter-out $(LOCAL_MODULES),\
	$(foreach module,$(1),$(foreach MOD,$(MODULE_$(module)),$($(MOD)_DEPENDS_ON))) \
)

read_dependencies_recursive = $(call read_dependencies,$(1)) \
	$(if $(call read_dependencies,$(1)),$(call read_dependencies_recursive,$(call read_dependencies,$(1))))

LOCAL_MODULES := $(LOCAL_MODULES) $(call read_dependencies_recursive,$(LOCAL_MODULES))
endif # MAKE_4_1


GLOBAL_MODULES := $(call reverse_modules,$(call unique_modules,$(call reverse_modules,$(LOCAL_MODULES))))

$(foreach module,$(GLOBAL_MODULES),$(if $(MODULE_$(module)),,$(error Module not found: $(module))))

$(info Enabled modules: $(GLOBAL_MODULES))

ifdef MAKE_4_1
$(foreach module,$(GLOBAL_MODULES),$(foreach MOD,$(MODULE_$(module)),\
	$(eval $(call merge_module,$(MOD),$(module)))))
else
# Fallback for macOS
TOOLKIT_MODULE_VARS := $(foreach module,$(GLOBAL_MODULES),$(foreach MOD,$(MODULE_$(module)),\
	$(MOD)))
endif # MAKE_4_1

TOOLKIT_CONFIG_STRINGS := $(foreach module,$(GLOBAL_MODULES),$(foreach MOD,$(MODULE_$(module)),\
	$($(MOD)_CONFIG_STRINGS) $(MOD)_NAME=$(module)))

TOOLKIT_CONFIG_FLAGS := \
	$(foreach MOD,$(TOOLKIT_MODULE_VARS), $($(MOD)_CONFIG_FLAGS) $(MOD))

TOOLKIT_CONFIG_VALUES := \
	$(foreach MOD,$(TOOLKIT_MODULE_VARS), $($(MOD)_CONFIG_VALUES))

TOOLKIT_PRECOMPILED_HEADERS := \
	$(foreach MOD,$(TOOLKIT_MODULE_VARS), $($(MOD)_PRECOMPILED_HEADERS))

TOOLKIT_GENERAL_CFLAGS := \
	$(foreach MOD,$(TOOLKIT_MODULE_VARS), $($(MOD)_FLAGS) $($(MOD)_GENERAL_CFLAGS))

TOOLKIT_GENERAL_CXXFLAGS :=  \
	$(foreach MOD,$(TOOLKIT_MODULE_VARS), $($(MOD)_FLAGS) $($(MOD)_GENERAL_CXXFLAGS))

TOOLKIT_GENERAL_LDFLAGS :=  \
	$(foreach MOD,$(TOOLKIT_MODULE_VARS), $($(MOD)_GENERAL_LDFLAGS))

TOOLKIT_LIB_CFLAGS :=  \
	$(foreach MOD,$(TOOLKIT_MODULE_VARS), $($(MOD)_LIB_CFLAGS))

TOOLKIT_LIB_CXXFLAGS :=  \
	$(foreach MOD,$(TOOLKIT_MODULE_VARS), $($(MOD)_LIB_CXXFLAGS))

TOOLKIT_LIB_LDFLAGS :=  \
	$(foreach MOD,$(TOOLKIT_MODULE_VARS), $($(MOD)_LIB_LDFLAGS))

TOOLKIT_EXEC_CFLAGS :=  \
	$(foreach MOD,$(TOOLKIT_MODULE_VARS), $($(MOD)_EXEC_CFLAGS))

TOOLKIT_EXEC_CXXFLAGS :=  \
	$(foreach MOD,$(TOOLKIT_MODULE_VARS), $($(MOD)_EXEC_CXXFLAGS))

TOOLKIT_EXEC_LDFLAGS :=  \
	$(foreach MOD,$(TOOLKIT_MODULE_VARS), $($(MOD)_EXEC_LDFLAGS))

TOOLKIT_SRCS_DIRS :=  \
	$(foreach MOD,$(TOOLKIT_MODULE_VARS), $($(MOD)_SRCS_DIRS))

TOOLKIT_SRCS_OBJS :=  \
	$(foreach MOD,$(TOOLKIT_MODULE_VARS), $($(MOD)_SRCS_OBJS))

TOOLKIT_INCLUDES_DIRS :=  \
	$(foreach MOD,$(TOOLKIT_MODULE_VARS), $($(MOD)_INCLUDES_DIRS))

TOOLKIT_INCLUDES_OBJS :=  \
	$(foreach MOD,$(TOOLKIT_MODULE_VARS), $($(MOD)_INCLUDES_OBJS))

TOOLKIT_SHADERS_DIR :=  \
	$(foreach MOD,$(TOOLKIT_MODULE_VARS), $($(MOD)_SHADERS_DIR))

TOOLKIT_SHADERS_INCLUDE :=  \
	$(foreach MOD,$(TOOLKIT_MODULE_VARS), $($(MOD)_SHADERS_INCLUDE))

TOOLKIT_LIBS :=  \
	$(foreach MOD,$(TOOLKIT_MODULE_VARS), $($(MOD)_LIBS))

TOOLKIT_LIBS_SHARED :=  \
	$(foreach MOD,$(TOOLKIT_MODULE_VARS), $($(MOD)_LIBS_SHARED))

TOOLKIT_MODULES := $(BUILD_С_OUTDIR)/modules.info
TOOLKIT_CACHED_FLAGS := $(BUILD_С_OUTDIR)/cached_flags.mk

TOOLKIT_CACHED_MODULES := $(call shell_cat,$(TOOLKIT_MODULES))

ifdef MAKE_4_1
ifneq ($(LOCAL_MODULES),$(TOOLKIT_CACHED_MODULES))
$(info Modules was updated)

$(call shell_mkdir,$(BUILD_С_OUTDIR))
$(call shell_override_file,$(TOOLKIT_MODULES),$(LOCAL_MODULES))

endif
endif # MAKE_4_1
	