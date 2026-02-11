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

# Функции для вывода правил компиляции

ifeq ($(GLOBAL_COMPILER_IS_CLANG),1)
sp_compile_dep = -MMD -MP -MF $(addsuffix .d,$(1)) $(2)
else
sp_compile_dep = -MMD -MP -MF $(addsuffix .d,$(1)) $(2)
endif

# $(1) - compiler
# $(2) - filetype flags
# $(3) - compile flags
# $(4) - input file
# $(5) - output file

sp_compile_command = $(1) $(2) $(call sp_compile_dep, $(5), $(3)) -c -o $(5) $(4)

sp_compile_gch = $(GLOBAL_QUIET_CPP) $(call rule_mkdir,$(dir $@));\
	$(call sp_compile_command,$(GLOBAL_CXX),$(OSTYPE_GCH_FILE),$(1),$<,$@)

sp_compile_S = $(GLOBAL_QUIET_CC) $(call rule_mkdir,$(dir $@));\
	$(call sp_compile_command,$(GLOBAL_CC),,$(1),$<,$@)

sp_compile_c = $(GLOBAL_QUIET_CC) $(call rule_mkdir,$(dir $@));\
	$(call sp_compile_command,$(GLOBAL_CC),$(OSTYPE_C_FILE),$(1),$<,$@)

sp_compile_cpp = $(GLOBAL_QUIET_CPP) $(call rule_mkdir,$(dir $@));\
	$(call sp_compile_command,$(GLOBAL_CXX),$(OSTYPE_CPP_FILE),$(1),$<,$@)

sp_compile_mm = $(GLOBAL_QUIET_CPP) $(call rule_mkdir,$(dir $@));\
	$(call sp_compile_command,$(GLOBAL_CXX),$(OSTYPE_MM_FILE),$(1) -fobjc-arc,$<,$@)

sp_copy_header = @$(call rule_mkdir,$(dir $@)); $(GLOBAL_CP) $< $@

sp_toolkit_source_list_c = $(call sp_make_general_source_list,$(1),$(2),$(GLOBAL_ROOT),\
	*.cpp *.c *.S $(if $(BUILD_OBJC),*.mm),\
	$(if $(BUILD_OBJC),,%.mm))

sp_toolkit_source_list = $(call sp_toolkit_source_list_c,$(1),$(filter-out %.wit,$(2)))

sp_toolkit_include_list = $(call sp_make_general_include_list,$(1),$(2),$(GLOBAL_ROOT))

sp_toolkit_object_list = \
	$(abspath $(addprefix $(1)/objs/,$(addsuffix .o,$(notdir $(2)))))

sp_toolkit_resolve_prefix_files = \
	$(realpath $(addprefix $(GLOBAL_ROOT)/,$(call sp_list_relpaths,$(1)))) \
	$(realpath $(call sp_list_abspaths,$(1)))

sp_toolkit_prefix_files_list = \
	$(abspath $(addprefix $(1)/include/,$(notdir $(2))))

sp_toolkit_include_flags = \
	$(addprefix -I,$(sort $(dir $(1)))) $(addprefix -I,$(2))

# $(1) - module
# $(2) - filename
# $(3) - build path
# $(4) - default flags
# $(5) - private default flags
sp_toolkit_private_flags = \
	$(if $(filter %.S %.c,$(2)),$($(1)_PRIVATE_CFLAGS)) \
	$(if $(filter %.mm %.cpp,$(2)),\
		$(addprefix -include-pch ,$(addprefix $(strip $(3))/,$(addsuffix $(OSTYPE_GCH_SUFFIX),$($(1)_PRIVATE_INCLUDE_PCH)))) \
		$($(1)_PRIVATE_CXXFLAGS) \
	) \
	$(addprefix -I,$(call sp_toolkit_include_list,,$($(1)_PRIVATE_INCLUDES))) \
	$(if $($(1)_PRIVATE_STANDALONE),\
		$(if $($(1)_PRIVATE_FLAGS_FILTER),$(filter-out $($(1)_PRIVATE_FLAGS_FILTER),$(5)),$(5)),\
		$(if $($(1)_PRIVATE_FLAGS_FILTER),$(filter-out $($(1)_PRIVATE_FLAGS_FILTER),$(4)),$(4))\
	)

# $(1) - filename
# $(2) - build path
# $(3) - default flags
sp_local_private_flags = \
	$(if $(filter %.c,$(1)),$(LOCAL_PRIVATE_CFLAGS)) \
	$(if $(filter %.cpp,$(1)),$(LOCAL_PRIVATE_CXXFLAGS)) \
	$(if $(filter %.mm,$(1)),$(LOCAL_PRIVATE_CXXFLAGS)) \
	$(addprefix -include-pch ,$(addprefix $(strip $(2))/,$(addsuffix $(OSTYPE_GCH_SUFFIX),$(LOCAL_PRIVATE_INCLUDE_PCH)))) \
	$(addprefix -I,$(call sp_toolkit_include_list,,$(LOCAL_PRIVATE_INCLUDES))) \
	$(3)


sp_local_source_list_c = $(call sp_make_general_source_list,$(1),$(2),$(LOCAL_ROOT),\
	*.cpp *.c *.S $(if $(BUILD_OBJC),*.mm),\
	$(if $(BUILD_OBJC),,%.mm))

sp_local_source_list = $(call sp_local_source_list_c,$(1),$(filter-out %.wit,$(2)))

sp_local_include_list = $(call sp_make_general_include_list,$(1),$(2),$(LOCAL_ROOT))

sp_toolkit_transform_lib_ldflag = \
	$(patsubst -l:lib%.a,-l%,$(1))

ifdef OSTYPE_IS_WIN32
sp_toolkit_transform_lib = $(sp_toolkit_transform_lib_ldflag)
else
sp_toolkit_transform_lib = $(1)
endif

ifdef OSTYPE_LIBS_REALPATH
sp_toolkit_resolve_libs = \
	$(if $(BUILD_SHARED_DEPS),$(3),$(subst -l:,$(abspath $(1))/,$(call sp_toolkit_transform_lib,$(2))))
else
sp_toolkit_resolve_libs = \
	$(addprefix -L,$(1)) $(call sp_toolkit_transform_lib,$(if $(BUILD_SHARED_DEPS),$(3),$(2)))
endif

sp_build_target_path = \
	$(abspath $(addprefix $(2)/objs/,$(addsuffix .o,$(notdir $(1)))))


ifeq ($(findstring Windows,$(OS)),Windows)
sp_cdb_convert_cmd = $(1)
sp_cdb_which_cmd = $(1)
else
sp_cdb_convert_cmd = '$(1)'
sp_cdb_which_cmd = `which $(1)`
endif

sp_cdb_process_arg = \
	$(if $(filter -I%,$(1)),-I$(call sp_cdb_convert_cmd,$(patsubst -I%,%,$(1))),\
		$(if $(filter /%,$(1)),$(call sp_cdb_convert_cmd,$(1)),$(1))\
	)

sp_cdb_split_arguments_cmd = \
	"$(call sp_cdb_which_cmd,$(1))"\
	$(foreach arg,$(2),,"$(foreach a,$(call sp_cdb_process_arg,$(arg)),$(a))")


define BUILD_cdb_json_file

endef

# $(1) - source path
# $(2) - target path
define BUILD_include_rule
$(2): $(1) $$(LOCAL_MAKEFILE) $$($TOOLKIT_MODULES)
	$$(call sp_copy_header,$(1),$(2))
endef

# $(1) - source path
# $(2) - compilation flags
# $(3) - extra deps
define BUILD_gch_rule
$(abspath $(1)): $(patsubst %.h$(OSTYPE_GCH_SUFFIX),%.h,$(1)) $$(LOCAL_MAKEFILE) $$($TOOLKIT_MODULES) $(3)
	$$(call sp_compile_gch,$(2))
endef

# $(1) - source path
# $(2) - target path
# $(3) - dependencies & precompiled headers
# $(4) - compilation flags
define BUILD_c_rule
$(2).json: $(1) $$(LOCAL_MAKEFILE) $$(TOOLKIT_MODULES) $$(TOOLKIT_CACHED_FLAGS)
	@$(call rule_mkdir,$$(dir $$@))
	@echo '{"directory":"$$(strip $$(call sp_cdb_convert_cmd,$$(BUILD_WORKDIR)))",\
"file":"$$(strip $$(call sp_cdb_convert_cmd,$(1)))",\
"output":"$$(strip $$(call sp_cdb_convert_cmd,$(2)))",\
"arguments":[$$(call sp_cdb_split_arguments_cmd,$$(GLOBAL_CC),$$(call sp_compile_command,,$$(OSTYPE_C_FILE),$(4),$(1),$(2)))]},' > $$@
	@echo '[Compilation database entry]: $(notdir $(1))'

$(2): \
		$(1) $(3) $$(LOCAL_MAKEFILE) | $(2).json $$(BUILD_COMPILATION_DATABASE)
	$$(call sp_compile_c,$(4))
endef

# $(1) - source path
# $(2) - target path
# $(3) - dependencies & precompiled headers
# $(4) - compilation flags
define BUILD_S_rule
$(2): \
		$(1) $(3)
	$$(call sp_compile_S,$(4))
endef

# $(1) - source path
# $(2) - target path
# $(3) - dependencies & precompiled headers
# $(4) - compilation flags
define BUILD_cpp_rule
$(2).json: $(1) $$(LOCAL_MAKEFILE) $$(TOOLKIT_MODULES) $$(TOOLKIT_CACHED_FLAGS)
	@$(call rule_mkdir,$$(dir $$@))
	@echo '{"directory":"$$(strip $$(call sp_cdb_convert_cmd,$$(BUILD_WORKDIR)))",\
"file":"$$(strip $$(call sp_cdb_convert_cmd,$(1)))",\
"output":"$$(strip $$(call sp_cdb_convert_cmd,$(2)))",\
"arguments":[$$(call sp_cdb_split_arguments_cmd,$$(GLOBAL_CXX),$$(call sp_compile_command,,$$(OSTYPE_CPP_FILE),$(4),$(1),$(2)))]},' > $$@
	@echo '[Compilation database entry]: $(notdir $(1))'

$(2): \
		$(1) $(3) $$(LOCAL_MAKEFILE) | $(2).json $$(BUILD_COMPILATION_DATABASE)
	$$(call sp_compile_cpp,$(4))
endef

# $(1) - source path
# $(2) - target path
# $(3) - dependencies & precompiled headers
# $(4) - compilation flags
define BUILD_mm_rule
$(2).json: $(1) $$(LOCAL_MAKEFILE) $$(TOOLKIT_MODULES) $$(TOOLKIT_CACHED_FLAGS)
	@$(call rule_mkdir,$$(dir $$@))
	@echo '{"directory":"$$(strip $$(call sp_cdb_convert_cmd,$$(BUILD_WORKDIR)))",\
"file":"$$(strip $$(call sp_cdb_convert_cmd,$(1)))",\
"output":"$$(strip $$(call sp_cdb_convert_cmd,$(2)))",\
"arguments":[$$(call sp_cdb_split_arguments_cmd,$$(GLOBAL_CXX),$$(call sp_compile_command,,$$(OSTYPE_MM_FILE),$(4),$(1),$(2)))]},' > $$@
	@echo '[Compilation database entry]: $(notdir $(1))'

$(2): \
		$(1) $(3) $$(LOCAL_MAKEFILE) | $(2).json $$(BUILD_COMPILATION_DATABASE)
	$$(call sp_compile_mm,$(4))
endef

include $(BUILD_ROOT)/c/rules-$(GLOBAL_SHELL).mk
