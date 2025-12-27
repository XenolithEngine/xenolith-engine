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
ifdef MSYS
# Записываем имя зависимости в формате unix, иначе make не сможет его сопоставить
sp_compile_dep = -MMD -MP -MF $(addsuffix .d,$(1)) $(2) -MT$(shell cygpath -u $(abspath $(1)))
else
sp_compile_dep = -MMD -MP -MF $(addsuffix .d,$(1)) $(2)
endif
else
sp_compile_dep = -MMD -MP -MF $(addsuffix .d,$(1)) $(2)
endif

# $(1) - compiler
# $(2) - filetype flags
# $(3) - compile flags
# $(4) - input file
# $(5) - output file

sp_compile_command = $(1) $(2) $(call sp_compile_dep, $(5), $(3)) -c -o $(5) $(4)

sp_compile_gch = $(GLOBAL_QUIET_CPP) $(GLOBAL_MKDIR) $(dir $@);\
	$(call sp_compile_command,$(GLOBAL_CXX),$(OSTYPE_GCH_FILE),$(1),$<,$@)

sp_compile_S = $(GLOBAL_QUIET_CC) $(GLOBAL_MKDIR) $(dir $@);\
	$(call sp_compile_command,$(GLOBAL_CC),,$(1),$<,$@)

sp_compile_c = $(GLOBAL_QUIET_CC) $(GLOBAL_MKDIR) $(dir $@);\
	$(call sp_compile_command,$(GLOBAL_CC),$(OSTYPE_C_FILE),$(1),$<,$@)

sp_compile_cpp = $(GLOBAL_QUIET_CPP) $(GLOBAL_MKDIR) $(dir $@);\
	$(call sp_compile_command,$(GLOBAL_CXX),$(OSTYPE_CPP_FILE),$(1),$<,$@)

sp_compile_mm = $(GLOBAL_QUIET_CPP) $(GLOBAL_MKDIR) $(dir $@);\
	$(call sp_compile_command,$(GLOBAL_CXX),$(OSTYPE_MM_FILE),$(1) -fobjc-arc,$<,$@)

sp_copy_header = @$(GLOBAL_MKDIR) $(dir $@); cp -f $< $@

sp_toolkit_source_list_c = $(call sp_make_general_source_list,$(1),$(2),$(GLOBAL_ROOT),\
	*.cpp *.c *.S $(if $(BUILD_OBJC),*.mm),\
	$(if $(BUILD_OBJC),,%.mm))

sp_toolkit_source_list = $(call sp_toolkit_source_list_c,$(1),$(filter-out %.wit,$(2)))

sp_toolkit_include_list = $(call sp_make_general_include_list,$(1),$(2),$(GLOBAL_ROOT))

sp_toolkit_object_list = \
	$(abspath $(addprefix $(1)/objs/,$(addsuffix .o,$(notdir $(2)))))

sp_toolkit_resolve_prefix_files = \
	$(realpath $(addprefix $(GLOBAL_ROOT)/,$(filter-out /%,$(1)))) \
	$(realpath $(filter /%,$(1)))

sp_toolkit_prefix_files_list = \
	$(abspath $(addprefix $(1)/include/,$(notdir $(2))))

sp_toolkit_include_flags = \
	$(addprefix -I,$(sort $(dir $(1)))) $(addprefix -I,$(2))

# $(1) - module
# $(2) - filename
sp_toolkit_private_flags = \
	$(if $(filter %.c,$(2)),$($(1)_PRIVATE_CFLAGS)) \
	$(if $(filter %.cpp,$(2)),$($(1)_PRIVATE_CXXFLAGS)) \
	$(if $(filter %.mm,$(2)),$($(1)_PRIVATE_CXXFLAGS)) \
	$(addprefix -I,$(call sp_toolkit_include_list,,$($(1)_PRIVATE_INCLUDES)))

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


ifdef MSYS
sp_cdb_convert_cmd = `cygpath -w $(1) | sed -r 's/\\\\/\\\\\\\\/g'`
sp_cdb_which_cmd = `which $(1) | cygpath -w -f - | sed -r 's/\\\\/\\\\\\\\/g'`
else
sp_cdb_convert_cmd = '$(1)'
sp_cdb_which_cmd = `which $(1)`
endif

sp_cdb_process_arg = \
	$(if $(filter -I%,$(1)),-I'$(call sp_cdb_convert_cmd,$(patsubst -I%,%,$(1)))',\
		$(if $(filter /%,$(1)),'$(call sp_cdb_convert_cmd,$(1))',$(1))\
	)

sp_cdb_split_arguments_cmd = \
	"'$(call sp_cdb_which_cmd,$(1))'"\
	$(foreach arg,$(2),,"$(foreach a,$(call sp_cdb_process_arg,$(arg)),$(a))")

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
$(abspath $(1)): $(patsubst %.h.gch,%.h,$(1)) $$(LOCAL_MAKEFILE) $$($TOOLKIT_MODULES) $(3)
	$$(call sp_compile_gch,$(2))
endef

# $(1) - source path
# $(2) - target path
# $(3) - dependencies & precompiled headers
# $(4) - compilation flags
define BUILD_c_rule
$(2).json: $(1) $$(LOCAL_MAKEFILE) $$(TOOLKIT_MODULES) $$(TOOLKIT_CACHED_FLAGS)
	@$(GLOBAL_MKDIR) $$(dir $$@)
	@echo "{" > $$@
	@echo '"directory":"'$$(call sp_cdb_convert_cmd,$$(BUILD_WORKDIR))'",' >> $$@
	@echo '"file":"'$$(call sp_cdb_convert_cmd,$(1))'",' >> $$@
	@echo '"output":"'$$(call sp_cdb_convert_cmd,$(2))'",' >> $$@
	@echo '"arguments":[$$(call sp_cdb_split_arguments_cmd,$$(GLOBAL_CC),$$(call sp_compile_command,,$$(OSTYPE_C_FILE),$(4),$(1),$(2)))]' >> $$@
	@echo "}," >> $$@
	@echo [Compilation database entry]: $(notdir $(1))

$(2): \
		$(1) $(3) | $(2).json $$(BUILD_COMPILATION_DATABASE)
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
	@$(GLOBAL_MKDIR) $$(dir $$@)
	@echo "{" > $$@
	@echo '"directory":"'$$(call sp_cdb_convert_cmd,$$(BUILD_WORKDIR))'",' >> $$@
	@echo '"file":"'$$(call sp_cdb_convert_cmd,$(1))'",' >> $$@
	@echo '"output":"'$$(call sp_cdb_convert_cmd,$(2))'",' >> $$@
	@echo '"arguments":[$$(call sp_cdb_split_arguments_cmd,$$(GLOBAL_CXX),$$(call sp_compile_command,,$$(OSTYPE_CPP_FILE),$(4),$(1),$(2)))]' >> $$@
	@echo "}," >> $$@
	@echo [Compilation database entry]: $(notdir $(1))

$(2): \
		$(1) $(3) | $(2).json $$(BUILD_COMPILATION_DATABASE)
	$$(call sp_compile_cpp,$(4))
endef

# $(1) - source path
# $(2) - target path
# $(3) - dependencies & precompiled headers
# $(4) - compilation flags
define BUILD_mm_rule
$(2).json: $(1) $$(LOCAL_MAKEFILE) $$(TOOLKIT_MODULES) $$(TOOLKIT_CACHED_FLAGS)
	@$(GLOBAL_MKDIR) $$(dir $$@)
	@echo "{" > $$@
	@echo '"directory":"'$$(call sp_cdb_convert_cmd,$$(BUILD_WORKDIR))'",' >> $$@
	@echo '"file":"'$$(call sp_cdb_convert_cmd,$(1))'",' >> $$@
	@echo '"output":"'$$(call sp_cdb_convert_cmd,$(2))'",' >> $$@
	@echo '"arguments":[$$(call sp_cdb_split_arguments_cmd,$$(GLOBAL_CXX),$$(call sp_compile_command,,$$(OSTYPE_MM_FILE),$(4),$(1),$(2)))]' >> $$@
	@echo "}," >> $$@
	@echo [Compilation database entry]: $(notdir $(1))

$(2): \
		$(1) $(3) | $(2).json $$(BUILD_COMPILATION_DATABASE)
	$$(call sp_compile_mm,$(4))
endef

# $(1) - target path
# $(2) - config flag
define BUILD_write_config_flag
@echo "#define $(2) 1" >> $(1)$(newline)$(tab)
endef

# $(1) - target path
# $(2) - value name
# $(3) - value
define BUILD_write_config_value
@echo "constexpr int $(2) = $(3);" >> $(1)$(newline)$(tab)
endef

# $(1) - target path
# $(2) - string name
# $(3) - string value
define BUILD_write_config_string
@echo 'constexpr auto $(2) = "$(3)";' >> $(1)$(newline)$(tab)
endef

# $(1) - target path
# $(2) - config namespace
# $(3) - config flags
# $(4) - config values
# $(5) - config strings
define BUILD_config_header
$(1): $$(LOCAL_MAKEFILE) $$(TOOLKIT_MODULES) $$(TOOLKIT_CACHED_FLAGS)
	@$(GLOBAL_MKDIR) $(dir $(1))
	@echo "// Autogenerated config" > $(1)
	@echo "" >> $(1)
	@echo "#ifndef STAPPLER_CONFIG_$(2)_H_" >> $(1)
	@echo "#define STAPPLER_CONFIG_$(2)_H_" >> $(1)
	@echo "" >> $(1)
	$(foreach var,$(3),$(call BUILD_write_config_flag,$(1),$(var)))
	@echo "" >> $(1)
	@echo "#ifdef __cplusplus" >> $(1)
	@echo "namespace stappler::$(2) {" >> $(1)
	$(foreach var,$(4),$(call BUILD_write_config_value,$(1),$(firstword $(subst =, ,$(var))),$(lastword $(subst =, ,$(var)))))
	$(foreach var,$(5),$(call BUILD_write_config_string,$(1),$(firstword $(subst =, ,$(var))),$(lastword $(subst =, ,$(var)))))
	@echo "}" >> $(1)
	@echo "" >> $(1)
	@echo "#endif // __cplusplus" >> $(1)
	@echo "#endif // STAPPLER_CONFIG_$(2)_H_" >> $(1)
endef

define BUILD_write_cdb_entry
@cat $(2) >> $(1)$(newline)$(tab)
endef

# $(1) - target path
# $(2) - json files
define BUILD_cdb
$(1): $$(LOCAL_MAKEFILE) $$(TOOLKIT_MODULES) $$(TOOLKIT_CACHED_FLAGS) $(2)
	@echo "[" > $(1)
	$(foreach file,$(2),$(call BUILD_write_cdb_entry,$(1),$(file)))
	@echo "]" >> $(1)
	@echo [Compilation database] $(1)
endef
