# Trouble cases from stappler make build system

$(info lastword(MAKEFILE_LIST) $(realpath \
	$(lastword $(MAKEFILE_LIST))))

STAPPLER_MODULE_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

$(info STAPPLER_MODULE_DIR $(STAPPLER_MODULE_DIR))

sp_list_abspaths = $(foreach dir,$(1),$(filter $(abspath $(dir)),$(dir)))
sp_list_relpaths = $(foreach dir,$(1),$(filter-out $(abspath $(dir)),$(dir)))

RELPATH := stappler//core/SPCommon.h
ABSPATH := /home/sbkarr/stappler/xenolith-engine/stappler/core/SPCommon.h

$(info sp_list_abspaths(RELPATH) $(call sp_list_abspaths,$(RELPATH)))
$(info sp_list_relpaths(RELPATH) $(call sp_list_relpaths,$(RELPATH)))

$(info sp_list_abspaths(ABSPATH) $(call sp_list_abspaths,$(ABSPATH)))
$(info sp_list_relpaths(ABSPATH) $(call sp_list_relpaths,$(ABSPATH)))

define newline


endef

noop=
space = $(noop) $(noop)
tab = $(noop)	$(noop)

define BUILD_write_config_flag
@echo "#define $(2) 1" >> $(1)$(newline)$(tab)
endef

define BUILD_config_header
TEST_GOAL:
	$(foreach var,$(2),$(call BUILD_write_config_flag,$(1),$(var)))
endef

$(info BUILD_write_config_flag $(newline)$(call BUILD_write_config_flag,testfile.txt,VARNAME)$(call BUILD_write_config_flag,testfile.txt,VARNAME2)$(call BUILD_write_config_flag,testfile.txt,VARNAME3))

VARS = TEST1 TEST2 TEST3

$(eval $(call BUILD_config_header,testfile.txt,$(VARS)))

ifdef XLMAKE_VERSION
$(info XLMAKE_VERSION $(XLMAKE_VERSION))
else

ifeq (4.1,$(firstword $(sort $(MAKE_VERSION) 4.1)))
$(info MAKE_VERSION $(MAKE_VERSION))
else
$(info no MAKE_VERSION or XLMAKE_VERSION)
endif

endif

VARIABLE := variable

print_verbose_a = $(info (verbose := 1) $(1))
print_verbose_b =

$(call print_verbose_a,(print_verbose_a) test, VARIABLE: $(VARIABLE))
$(call print_verbose_b,(print_verbose_b) test, VARIABLE: $(VARIABLE))
