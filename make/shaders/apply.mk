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

TOOLKIT_SHADERS_SRCS_FILES := $(call sp_toolkit_shaders_files,$(TOOLKIT_SHADERS_DIR))
TOOLKIT_SHADERS_EMBEDDED := $(call sp_toolkit_shaders_objs,$(TOOLKIT_SHADERS_SRCS_FILES))

BUILD_SHADERS_SRCS_FILES := $(call sp_toolkit_shaders_files,$(LOCAL_SHADERS_DIR))
BUILD_SHADERS_EMBEDDED := $(call sp_toolkit_shaders_objs,$(BUILD_SHADERS_SRCS_FILES))

BUILD_SHADERS_TARGET_INCLUDE_ALL := $(addprefix -I,$(BUILD_SHADERS_OUTDIR))

BUILD_SHADERS_INCLUDE = $(addprefix -I,$(realpath $(LOCAL_SHADERS_INCLUDE) $(TOOLKIT_SHADERS_INCLUDE)))
BUILD_SHADERS_FLAGS := $(BUILD_SHADERS_INCLUDE) -DSP_GLSL=1

# The project's GPU schemas (LOCAL_SHADERS_SCHEMA, docs/usage/tools/xlkdl-gpu.adoc), generated in
# the build: xlkdl gpu writes their headers and their shaders' preambles before any shader or
# source compiles, and their directory is on both include paths. It does not exist on a clean
# build, so it is named as it is, not through realpath. xlkdl is a tool of the host: XLKDL, or
# the engine's build of it for this host
BUILD_SHADERS_SCHEMA_FILES := $(realpath $(LOCAL_SHADERS_SCHEMA))
BUILD_SHADERS_SCHEMA_DEPS :=
ifneq ($(strip $(LOCAL_SHADERS_SCHEMA)),)
XLKDL ?= $(firstword $(wildcard $(foreach build,release debug,\
	$(GLOBAL_ROOT)/utils/xlkdl/stappler-build/$(STAPPLER_HOST)/$(build)/cc/xlkdl \
	$(GLOBAL_ROOT)/utils/xlkdl/stappler-build/$(STAPPLER_HOST)/$(build)/cc/xlkdl.exe)))
ifeq ($(XLKDL),)
$(error LOCAL_SHADERS_SCHEMA needs xlkdl built for this host - \
	xenolith-cli build $(GLOBAL_ROOT)/utils/xlkdl --engine $(GLOBAL_ROOT) - or XLKDL set to one)
endif
ifneq ($(words $(BUILD_SHADERS_SCHEMA_FILES)),$(words $(LOCAL_SHADERS_SCHEMA)))
$(error LOCAL_SHADERS_SCHEMA names a file that is not there: $(LOCAL_SHADERS_SCHEMA))
endif
BUILD_SHADERS_SCHEMA_DEPS := $(BUILD_SHADERS_SCHEMA_DEPFILE)
BUILD_SHADERS_TARGET_INCLUDE_ALL += -I$(BUILD_SHADERS_SCHEMA_OUTDIR)
BUILD_SHADERS_FLAGS := -I$(BUILD_SHADERS_SCHEMA_OUTDIR) $(BUILD_SHADERS_FLAGS)
# the sources include the headers: they compile after them, as they do after the shaders
BUILD_SHADERS_EMBEDDED += $(BUILD_SHADERS_SCHEMA_DEPFILE)
LOCAL_PRIVATE_DEPS += $(BUILD_SHADERS_SCHEMA_DEPFILE)

define BUILD_SHADERS_schema_rule
$(BUILD_SHADERS_SCHEMA_DEPFILE) : $(BUILD_SHADERS_SCHEMA_FILES) $$(LOCAL_MAKEFILE) $(XLKDL)
	@$$(call rule_mkdir,$(BUILD_SHADERS_SCHEMA_OUTDIR))
	$$(call GLOBAL_QUIET_XLKDL,$(BUILD_SHADERS_SCHEMA_FILES))
	$$(VERBOSE_GUARD) $(XLKDL) gpu --dir $(BUILD_SHADERS_SCHEMA_OUTDIR) \
		--depfile $(BUILD_SHADERS_SCHEMA_DEPFILE) $(BUILD_SHADERS_SCHEMA_FILES)
$(BUILD_SHADERS_SCHEMA_DEPFILE):.TARGET_NAME := [xlkdl] $(notdir $(BUILD_SHADERS_SCHEMA_FILES))
endef

$(eval $(BUILD_SHADERS_schema_rule))

# the schemas each run read, imports too, from the depfile xlkdl wrote
-include $(wildcard $(BUILD_SHADERS_SCHEMA_DEPFILE))
endif

ifdef OSTYPE_IS_MACOS
BUILD_SHADERS_FLAGS += -DSP_MVK=1
endif

define BUILD_SHADERS_compile_single_rule
$(2) : $(1) $$(LOCAL_MAKEFILE) $(3)
	@$$(call rule_mkdir,$$(dir $$@))
	$$(GLOBAL_QUIET_GLSLC)
	$$(call sp_compile_glsl_header,$(2),$(1),$(LOCAL_SHADERS_RULES))
$(2):.TARGET_NAME := [$(notdir $(GLSLC))] $(notdir $(1))
endef

# the project's shaders may include its schemas' preambles: they compile after them
$(foreach FILE,$(BUILD_SHADERS_SRCS_FILES),\
	$(eval $(call BUILD_SHADERS_compile_single_rule,$(FILE),\
		$(addsuffix .h,$(addprefix $(BUILD_SHADERS_OUTDIR)/,$(notdir $(FILE)))),\
		$(BUILD_SHADERS_SCHEMA_DEPS)\
	)))

$(foreach FILE,$(TOOLKIT_SHADERS_SRCS_FILES),\
	$(eval $(call BUILD_SHADERS_compile_single_rule,$(FILE),\
		$(addsuffix .h,$(addprefix $(BUILD_SHADERS_OUTDIR)/,$(notdir $(FILE))))\
	)))

# the headers each shader included, from the depfiles the compiler wrote
-include $(wildcard $(addsuffix .d,$(BUILD_SHADERS_EMBEDDED) $(TOOLKIT_SHADERS_EMBEDDED)))
