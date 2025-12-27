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

## Shared root builder
#
# Used for classic `make & sudo make install`
#
# Build params:
# - RELEASE=1: build release version of SDK
#
# Install params:
# - prefix (/usr/local): general installation prefix
# - libdir ($prefix/lib): directory to install dynamic librarioes
# - includedir ($prefix/include): directory to install C/C++ headers
# - datadir: ($prefix/share) directory to install buildscripts
# - INSTALL_WITH_RPATH=1: instruct SDK to use rpath for applcations (for non-standart libdir)
#
# Targets:
# - all: build shared root
# - clean: clean build folder
# - install: install shared SDK root into OS
# - uninstall: remove installed SDK root from OS
#

.DEFAULT_GOAL := all
BUILD_HOST := 1
BUILD_SHARED := 1
BUILD_ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

STAPPLER_TARGET := host

LOCAL_LIBRARY ?= libstappler
LOCAL_VERSION ?= 0.0

LOCAL_VERSIONIZED_LIBRARY := $(LOCAL_LIBRARY)-$(LOCAL_VERSION)

include $(BUILD_ROOT)/utils/detect-platform.mk

ifndef MACOS
BUILD_SHARED_PKGCONFIG := 1
endif

include $(BUILD_ROOT)/general/compile.mk

# Assume call from root repo
BUILD_STAPPLER_ROOT := $(abspath $(BUILD_ROOT)/../..)

BUILD_MODULES := $(filter-out $(TOOLKIT_SHARED_CONSUME),$(GLOBAL_MODULES))

BUILD_MAKEFILES := $(subst $(BUILD_ROOT)/,,$(wildcard $(BUILD_ROOT)/*.mk) $(wildcard $(BUILD_ROOT)/*/*.mk))

BUILD_SHARED_LIBS :=
BUILD_STATIC_LIBS :=
BUILD_INCLUDE_FILES :=
BUILD_MODULE_HEADERS :=
BUILD_TARGET_MAKEFILES :=
BUILD_SHARED_CONFIG := $(BUILD_С_OUTDIR)/share/$(LOCAL_VERSIONIZED_LIBRARY)/shared-config.mk

SHARED_PREFIX ?= $$(abspath $$(BUILD_ROOT)/../..)
SHARED_LIBDIR ?= $$(SHARED_PREFIX)/lib
SHARED_INCLUDEDIR ?= $$(SHARED_PREFIX)/include
SHARED_SHAREDIR ?= $$(SHARED_PREFIX)/share
SHARED_RPATH ?= $$(SHARED_LIBDIR)

INSTALL ?= install -p
INSTALL_PREFIX ?= /usr/local

ifdef prefix
INSTALL_PREFIX := $(prefix)
endif


INSTALL_LIBDIR ?= $(INSTALL_PREFIX)/lib

ifdef libdir
INSTALL_LIBDIR := $(libdir)
endif


INSTALL_INCLUDEDIR ?= $(INSTALL_PREFIX)/include

ifdef includedir
INSTALL_INCLUDEDIR := $(includedir)
endif


INSTALL_SHAREDIR ?= $(INSTALL_PREFIX)/share

ifdef datadir
INSTALL_SHAREDIR := $(datadir)
endif

INSTALL_SHARED_CONFIG := $(INSTALL_SHAREDIR)/$(LOCAL_VERSIONIZED_LIBRARY)/shared-config.mk

shared_get_consuming_module = \
	$(if $($(MODULE_$(module))_CONSUMED_BY),$($(MODULE_$(1))_CONSUMED_BY),$(1))

shared_make_shared_library_path = \
	$(foreach module,$(1),\
		$(addsuffix $(OSTYPE_DSO_SUFFIX),$(addprefix $(BUILD_С_OUTDIR)/lib/$(OSTYPE_LIB_PREFIX),\
			$(subst _,-,$(filter-out $(2),$(call shared_get_consuming_module,$(module))))\
	)))

shared_make_static_library_path = \
	$(foreach module,$(1),\
		$(addsuffix $(OSTYPE_LIB_SUFFIX),$(addprefix $(BUILD_С_OUTDIR)/lib/$(OSTYPE_LIB_PREFIX),\
			$(subst _,-,$(filter-out $(2),$(call shared_get_consuming_module,$(module))))\
	)))

shared_consume_modules = \
	$(foreach module,$(1),\
		$(call sp_toolkit_object_list,$(BUILD_С_OUTDIR)/lib_objs,\
			$(call sp_toolkit_source_list,$($(MODULE_$(module))_SRCS_DIRS),$($(MODULE_$(module))_SRCS_OBJS))) \
	)

shared_make_library_flags = \
	$(foreach module,$(1),\
		$(call shared_make_library_flags,$($(MODULE_$(module))_DEPENDS_ON),$(2))\
		$(addprefix -l,$(subst _,-,$(filter-out $(2),$(call shared_get_consuming_module,$(module)))))\
	)

# 1 - recursive dir list
# 2 - single dir list
# 3 - paths to exclude
shared_get_headers_list = \
	$(filter-out $(addsuffix %,$(3)),$(call sp_find_recursive,$(call sp_toolkit_include_list,$(1),$(2)),*.h *.hpp))

shared_consume_includes = \
	$(foreach module,$(1),\
		$(call shared_get_headers_list,\
			$($(MODULE_$(module))_INCLUDES_DIRS),\
			$($(MODULE_$(module))_INCLUDES_OBJS),\
			$($(MODULE_$(module))_INCLUDES_INTERNAL)) \
	)

shared_consume_libs = \
	$(foreach module,$(1),\
		$($(MODULE_$(module))_LIBS_SHARED) \
	)

shared_get_target_location = \
	$(subst $(BUILD_STAPPLER_ROOT),$(BUILD_С_OUTDIR)/include/$(LOCAL_VERSIONIZED_LIBRARY),$(1))


# 1 - target library path
# 2 - object files to build
# 3 - target libraries for dependencies
# 4 - additional lib flags
define make_rule_module_shared_lib =
$(1): $(2) $(3) $(BUILD_COMPILATION_DATABASE)
	@$(GLOBAL_MKDIR) $(dir $(1))
	$(GLOBAL_QUIET_LINK_SHARED) $(GLOBAL_CXX) -shared $(2) $(BUILD_LIB_LDFLAGS) $(4) -o $(1)
endef # make_rule_module_shared_lib


# 1 - target library path
# 2 - object files to build
define make_rule_module_static_lib =
$(1): $(2) $(3) $(BUILD_COMPILATION_DATABASE)
	@$(GLOBAL_MKDIR) $(dir $(1))
	$(GLOBAL_QUIET_LINK_STATIC) $(GLOBAL_AR) rcs $(1) $(2)
endef # make_rule_module_static_lib


# 1 - source file
# 2 - target location
define make_rule_include =

BUILD_INCLUDE_FILES += $(2)
$(2): $(1)
	@$(GLOBAL_MKDIR) $(dir $(2))
	$(GLOBAL_CP) $(1) $(2)
endef # make_rule_include


# 1 - target filename
# 2 - internal module_name
# 3 - include path
# 4 - module name
define make_rule_module_header =

$(call print_verbose,(shared.mk) make_rule_module_header $(1) $(2))

BUILD_MODULE_HEADERS += $(1)
$(1): $(BUILD_TARGET_MAKEFILES)
	@$(GLOBAL_MKDIR) $(dir $(1))
	@echo '# Autogenerated module header' > $(1)
	@echo '$(2)_DEFINED_IN := $(notdir $(1))' >> $(1)
	@echo '$(2)_PRECOMPILED_HEADERS := $(subst $(BUILD_С_OUTDIR)/include/$(LOCAL_VERSIONIZED_LIBRARY),$$$$(SHARED_INCLUDEDIR)/$(LOCAL_VERSIONIZED_LIBRARY),$(call shared_get_target_location,$($(2)_PRECOMPILED_HEADERS)))' >> $(1)
	@echo '$(2)_SRCS_DIRS :=' >> $(1)
	@echo '$(2)_SRCS_OBJS :=' >> $(1)
	@echo '$(2)_INCLUDES_DIRS :=' >> $(1)
	@echo '$(2)_INCLUDES_OBJS := $(sort $(dir $(subst $(BUILD_STAPPLER_ROOT)/,$$$$(SHARED_INCLUDEDIR)/$(LOCAL_VERSIONIZED_LIBRARY)/,$(3))))' >> $(1)
	@echo '$(2)_LIBS := $(addprefix -l, $(subst _,-,$(4)))' >> $(1)
	@echo '$(2)_DEPENDS_ON := $($(2)_DEPENDS_ON) $($(2)_SHARED_DEPENDS_ON)' >> $(1)
	@echo '$(addprefix MODULE_,$(subst .mk,,$(notdir $(1)))) := $(2)' >> $(1)
endef # make_rule_module_header


# 1 - target filename
# 2 - internal module_name
# 3 - consumed by
define make_rule_module_consumed_header =

$(call print_verbose,(shared.mk) make_rule_module_consumed_header $(1) $(2) $(3))

BUILD_MODULE_HEADERS += $(1)
$(1):
	@$(GLOBAL_MKDIR) $(dir $(1))
	@echo '# Autogenerated module header' > $(1)
	@echo '$(2)_DEFINED_IN := $(notdir $(1))' >> $(1)
	@echo '$(2)_PRECOMPILED_HEADERS :=' >> $(1)
	@echo '$(2)_SRCS_DIRS :=' >> $(1)
	@echo '$(2)_SRCS_OBJS :=' >> $(1)
	@echo '$(2)_INCLUDES_DIRS :=' >> $(1)
	@echo '$(2)_INCLUDES_OBJS :=' >> $(1)
	@echo '$(2)_LIBS :=' >> $(1)
	@echo '$(2)_DEPENDS_ON := $(3)' >> $(1)
	@echo '$(addprefix MODULE_,$(subst .mk,,$(notdir $(1)))) := $(2)' >> $(1)
endef # make_rule_module_consumed_header


# 1 - makefile name
define make_rule_makefile =
BUILD_TARGET_MAKEFILES += $(BUILD_С_OUTDIR)/share/$(LOCAL_VERSIONIZED_LIBRARY)/$(1)
$(BUILD_С_OUTDIR)/share/$(LOCAL_VERSIONIZED_LIBRARY)/$(1): $(BUILD_ROOT)/$(1)
	@$(GLOBAL_MKDIR) $(dir $(BUILD_С_OUTDIR)/share/$(LOCAL_VERSIONIZED_LIBRARY)/$(1))
	$(GLOBAL_CP) $(BUILD_ROOT)/$(1) $(BUILD_С_OUTDIR)/share/$(LOCAL_VERSIONIZED_LIBRARY)/$(1)
endef # make_rule_makefile


# 1 - target library path
# 2 - source dirs
# 3 - source flies
# 4 - depends on
# 5 - consumed modules
# 6 - module libflags
# 7 - module name
define compile_shared_module =

$(call print_verbose,(shared.mk) compile_shared_module $(1))

BUILD_SHARED_LIBS += $(1)
$(eval $(call make_rule_module_shared_lib,\
	$(1), \
		$(call sp_toolkit_object_list,$(BUILD_С_OUTDIR)/lib_objs,$(call sp_toolkit_source_list,$(2),$(3))) \
		$(call shared_consume_modules,$(5)), \
	$(call shared_make_shared_library_path,$(4),$(7)), \
	-L $(BUILD_С_OUTDIR)/lib $(call shared_make_library_flags,$(4),$(7)) $(6) \
))
endef # compile_shared_module


# 1 - target library path
# 2 - source dirs
# 3 - source flies
# 4 - depends on
# 5 - consumed modules
define compile_static_module =

$(call print_verbose,(shared.mk) compile_static_module $(1))

BUILD_STATIC_LIBS += $(1)
$(eval $(call make_rule_module_static_lib, \
	$(1), \
		$(call sp_toolkit_object_list,$(BUILD_С_OUTDIR)/lib_objs,$(call sp_toolkit_source_list,$(2),$(3))) \
		$(call shared_consume_modules,$(5)) \
))
endef # compile_static_module



# 1 - include dirs
# 2 - include flies
# 3 - exclude paths
# 4 - consumed modules
define compile_includes =
$(foreach include,\
	$(sort $(call shared_get_headers_list,$(1),$(2),$(3)) $(call shared_consume_includes,$(4))),\
	$(eval $(call make_rule_include,$(include),$(call shared_get_target_location,$(include)))) \
)
endef # compile_includes


# 1 - internal module name
# 2 - module title
define extract_module =

$(call print_verbose,(shared.mk) extract_module $(1) $(2))

$(call print_verbose,(shared.mk) compile_shared_module)

$(eval $(call compile_shared_module,\
	$(call shared_make_shared_library_path,$(2)),\
	$($(1)_SRCS_DIRS),\
	$($(1)_SRCS_OBJS),\
	$($(1)_DEPENDS_ON) $($(1)_SHARED_DEPENDS_ON),\
	$($(1)_SHARED_CONSUME),\
	$($(1)_LIBS_SHARED) $(call shared_consume_libs,$($(1)_SHARED_CONSUME)),\
	$(2)\
))

$(call print_verbose,(shared.mk) compile_static_module)

$(eval $(call compile_static_module,\
	$(call shared_make_static_library_path,$(2)),\
	$($(1)_SRCS_DIRS),\
	$($(1)_SRCS_OBJS),\
	$($(1)_DEPENDS_ON) $($(1)_SHARED_DEPENDS_ON),\
	$($(1)_SHARED_CONSUME)
))

$(call print_verbose,(shared.mk) compile_includes)

$(eval $(call compile_includes,
	$($(1)_INCLUDES_DIRS), $($(1)_INCLUDES_OBJS), $($(1)_INCLUDES_INTERNAL) \
	$($(1)_SHARED_CONSUME) \
))

$(call print_verbose,(shared.mk) make_rule_module_header)

$(eval $(call make_rule_module_header,\
	$(BUILD_С_OUTDIR)/share/$(LOCAL_VERSIONIZED_LIBRARY)/modules/$(2).mk,$(1), \
	$(sort \
		$(call shared_get_headers_list,$($(1)_INCLUDES_DIRS),$($(1)_INCLUDES_OBJS),$($(1)_INCLUDES_INTERNAL)) \
		$(call shared_consume_includes,$($(1)_SHARED_CONSUME))),\
	$(2)\
))

$(call print_verbose,(shared.mk) make_rule_module_consumed_header)

$(foreach module,$($(1)_SHARED_CONSUME),\
	$(eval $(call make_rule_module_consumed_header,\
		$(BUILD_С_OUTDIR)/share/$(LOCAL_VERSIONIZED_LIBRARY)/modules/$(module).mk,$(MODULE_$(module)),$(2)))\
)

endef # extract_module

$(foreach makefile,$(BUILD_MAKEFILES),\
	$(eval $(call make_rule_makefile,$(makefile)))\
)

$(foreach module,$(BUILD_MODULES),$(foreach module_name,$(MODULE_$(module)),\
	$(eval $(call extract_module,$(module_name),$(module)))\
))

$(eval $(call make_rule_include,$(BUILD_LIB_CONFIG),$(BUILD_С_OUTDIR)/include/$(LOCAL_VERSIONIZED_LIBRARY)/stappler-buildconfig.h))

BUILD_INSTALL_SHARED_LIBS := $(subst $(BUILD_С_OUTDIR)/lib/,$(INSTALL_LIBDIR)/,$(filter $(BUILD_С_OUTDIR)/lib/%$(OSTYPE_DSO_SUFFIX),$(BUILD_SHARED_LIBS)))
BUILD_INSTALL_STATIC_LIBS := $(subst $(BUILD_С_OUTDIR)/lib/,$(INSTALL_LIBDIR)/,$(filter $(BUILD_С_OUTDIR)/lib/%$(OSTYPE_LIB_SUFFIX),$(BUILD_STATIC_LIBS)))

BUILD_INSTALL_INCLUDE_FILES := $(subst $(BUILD_С_OUTDIR)/include/,$(INSTALL_INCLUDEDIR)/,$(filter $(BUILD_С_OUTDIR)/include/%,$(BUILD_INCLUDE_FILES)))
BUILD_INSTALL_INCLUDE_DIRS := $(sort $(dir $(BUILD_INSTALL_INCLUDE_FILES)))

BUILD_INSTALL_SHARED_FILES := $(subst $(BUILD_С_OUTDIR)/share/,$(INSTALL_SHAREDIR)/,$(filter $(BUILD_С_OUTDIR)/share/%,\
	$(BUILD_MODULE_HEADERS) $(BUILD_TARGET_MAKEFILES)))
BUILD_INSTALL_SHARED_DIRS := $(sort $(dir $(BUILD_INSTALL_SHARED_FILES)))

$(BUILD_SHARED_CONFIG): $(BUILD_TARGET_MAKEFILES)
	@$(GLOBAL_MKDIR) $(dir $@)
	@echo '# Autogenerated config' > $@
	@echo 'SHARED_PREFIX := $(SHARED_PREFIX)' >> $@
	@echo 'SHARED_LIBDIR := $(SHARED_LIBDIR)' >> $@
	@echo 'SHARED_INCLUDEDIR := $(SHARED_INCLUDEDIR)' >> $@
	@echo 'SHARED_SHAREDIR := $(SHARED_SHAREDIR)' >> $@
	@echo 'SHARED_RPATH := $(SHARED_RPATH)' >> $@
	@echo 'SHARED_MODULE_DIR := $$(SHARED_SHAREDIR)/$(LOCAL_VERSIONIZED_LIBRARY)/modules' >> $@
	@echo 'SHARED_VERSION := $(LOCAL_VERSION)' >> $@
	@echo 'SHARED_MODULES := $(BUILD_MODULES)' >> $@

$(INSTALL_SHARED_CONFIG):
	@$(GLOBAL_MKDIR) $(dir $@)
	@echo '# Autogenerated config' > $@
	@echo 'SHARED_PREFIX := $(INSTALL_PREFIX)' >> $@
	@echo 'SHARED_LIBDIR := $(INSTALL_LIBDIR)' >> $@
	@echo 'SHARED_INCLUDEDIR := $(INSTALL_INCLUDEDIR)' >> $@
	@echo 'SHARED_SHAREDIR := $(INSTALL_SHAREDIR)' >> $@
	@echo 'SHARED_RPATH := $(if $(INSTALL_WITH_RPATH),$(SHARED_RPATH))' >> $@
	@echo 'SHARED_MODULE_DIR := $$(SHARED_SHAREDIR)/$(LOCAL_VERSIONIZED_LIBRARY)/modules' >> $@
	@echo 'SHARED_VERSION := $(LOCAL_VERSION)' >> $@
	@echo 'SHARED_MODULES := $(BUILD_MODULES)' >> $@

$(INSTALL_LIBDIR)/%.so: $(BUILD_С_OUTDIR)/lib/%.so
	@$(GLOBAL_MKDIR) $(dir $@)
	$(INSTALL) -T $< $@

$(INSTALL_INCLUDEDIR)/%.h: $(BUILD_С_OUTDIR)/include/%.h
	@$(GLOBAL_MKDIR) $(dir $@)
	$(INSTALL) -T $< $@

$(INSTALL_INCLUDEDIR)/%.hpp: $(BUILD_С_OUTDIR)/include/%.hpp
	@$(GLOBAL_MKDIR) $(dir $@)
	$(INSTALL) -T $< $@

$(INSTALL_SHAREDIR)/%.mk: $(BUILD_С_OUTDIR)/share/%.mk
	@$(GLOBAL_MKDIR) $(dir $@)
	$(INSTALL) -T $< $@

ifdef DISTRIB
include $(BUILD_ROOT)/distrib/$(DISTRIB).mk
else
ifdef OSTYPE_IS_MACOS
include $(BUILD_ROOT)/distrib/macos.mk
endif
endif

info:
	@echo '// Build config'
	@echo 'SHARED_PREFIX := $(SHARED_PREFIX)'
	@echo 'SHARED_LIBDIR := $(SHARED_LIBDIR)'
	@echo 'SHARED_INCLUDEDIR := $(SHARED_INCLUDEDIR)'
	@echo 'SHARED_SHAREDIR := $(SHARED_SHAREDIR)'
	@echo 'SHARED_RPATH := $(SHARED_RPATH)'
	@echo 'SHARED_MODULE_DIR := $$(SHARED_SHAREDIR)/$(LOCAL_VERSIONIZED_LIBRARY)/modules'
	@echo 'SHARED_VERSION := $(LOCAL_VERSION)'
	@echo 'SHARED_MODULES := $(BUILD_MODULES)'
	@echo '// Install config'
	@echo 'SHARED_PREFIX := $(INSTALL_PREFIX)'
	@echo 'SHARED_LIBDIR := $(INSTALL_LIBDIR)'
	@echo 'SHARED_INCLUDEDIR := $(INSTALL_INCLUDEDIR)'
	@echo 'SHARED_SHAREDIR := $(INSTALL_SHAREDIR)'
	@echo 'SHARED_RPATH := $(if $(INSTALL_WITH_RPATH),$(SHARED_RPATH))'
	@echo 'SHARED_MODULE_DIR := $$(SHARED_SHAREDIR)/$(LOCAL_VERSIONIZED_LIBRARY)/modules'
	@echo 'SHARED_VERSION := $(LOCAL_VERSION)'
	@echo 'SHARED_MODULES := $(BUILD_MODULES)'
	@echo '// Targets:'
	@echo 'Local modules: $(BUILD_MODULES) $(LOCAL_MODULES)'
	@echo 'Modules: $(BUILD_MODULES) $(GLOBAL_MODULES)'
	@echo 'Shared libs: $(BUILD_SHARED_LIBS)'
	@echo 'Static libs: $(BUILD_STATIC_LIBS)'

all: $(BUILD_SHARED_LIBS) $(BUILD_STATIC_LIBS) $(BUILD_INCLUDE_FILES) $(BUILD_MODULE_HEADERS) $(BUILD_TARGET_MAKEFILES) $(BUILD_SHARED_CONFIG)

install: $(BUILD_INSTALL_SHARED_LIBS) $(BUILD_INSTALL_INCLUDE_FILES) $(BUILD_INSTALL_SHARED_FILES) $(INSTALL_SHARED_CONFIG)

uninstall:
	rm -f $(BUILD_INSTALL_SHARED_LIBS) $(BUILD_INSTALL_INCLUDE_FILES)
	rm -rf $(BUILD_INSTALL_INCLUDE_DIRS) $(INSTALL_INCLUDEDIR)/libstappler
	rm -rf $(BUILD_INSTALL_SHARED_DIRS) $(INSTALL_SHAREDIR)/libstappler

clean:
	$(GLOBAL_RM) -r $(BUILD_С_OUTDIR) $(BUILD_SHADERS_OUTDIR) $(BUILD_WIT_OUTDIR) $(BUILD_WASM_OUTDIR)

.PHONY: info all clean install uninstall
