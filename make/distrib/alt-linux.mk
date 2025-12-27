# Copyright (c) 2024 Stappler LLC <admin@stappler.dev>
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

BUILD_ALTLINUX_GROUP_LIB := System/Libraries
BUILD_ALTLINUX_GROUP_DEVEL := Development/C++
BUILD_ALTLINUX_DEVEL_SUMMARY := Module header and C/C++ headers for%s\n
BUILD_ALTLINUX_SPEC := $(BUILD_С_OUTDIR)/$(LOCAL_LIBRARY).spec

define BUILD_ALTLINUX_SPEC_HEADER
Name:           $(LOCAL_LIBRARY)
Version:        $(LOCAL_VERSION)
Release:        alt0
Summary:        Stappler SDK installation package
Group:          System/Libraries
License:        MIT
URL:            https://stappler.dev/
Source0:        %name-%version.tar

BuildRequires:  make gcc-c++ xxd
BuildRequires:  libgif-devel libjpeg-devel libwebp-devel libpng-devel
BuildRequires:  libbrotli-devel libidn2-devel libssl-devel libcurl-devel
BuildRequires:  libsqlite3-devel libfreetype-devel libzip-devel
BuildRequires:  libxkbcommon-devel libxkbcommon-x11-devel libxcbutil-keysyms-devel
BuildRequires:  vulkan-headers
BuildRequires:  glslang spirv-tools

ExcludeArch:    %ix86

%description
Stappler SDK installation package

%package -n %name-devel
Summary: Build tools for libstappler
Group: Development/Other

%description -n %name-devel
Build tools and header files for libstappler
endef


define BUILD_ALTLINUX_SPEC_BUILD
%prep
%setup -q

%build
%make_build RELEASE=1
%makeinstall RELEASE=1

%files -n %name-devel
endef

BUILD_MODULE_SPEC_COMMON_HEADER := $(BUILD_С_OUTDIR)/spec/header.spec
BUILD_MODULE_SPEC_COMMON_BUILD := $(BUILD_С_OUTDIR)/spec/build.spec
BUILD_MODULE_SPEC_HEADERS :=
BUILD_MODULE_SPEC_FILES :=

convert_list_to_newlines = $(subst $(space),\n,$(foreach value,$(1),$(value)))

shared_make_library_deps = \
	$(foreach module,$(1),\
		$(call shared_make_library_deps,$($(MODULE_$(module))_DEPENDS_ON) $($(MODULE_$(module))_SHARED_DEPENDS_ON),$(2))\
		$(subst _,-,$(filter-out $(2),$(call shared_get_consuming_module,$(module))))\
	)

shared_make_library_deps_line = \
	$(foreach name,$(sort $(call shared_make_library_deps,$(1),$(2))),\nRequires: $(addprefix lib,$(name)) = %%version-%%release)

shared_make_library_deps_line_devel = \
	$(foreach name,$(sort $(call shared_make_library_deps,$(1),$(2))),\nRequires: $(addsuffix -devel,$(addprefix lib,$(name))) = %%version-%%release)

define make_module_spec_header_rule =
BUILD_MODULE_SPEC_HEADERS += $(1)
$(1): $(BUILD_TARGET_MAKEFILES)
	@$(GLOBAL_MKDIR) $(dir $(1))
	@echo '%package -n $(2)' > $(1)
	@echo 'Summary: $(3)' >> $(1)
	@printf 'Group: $(BUILD_ALTLINUX_GROUP_LIB)' >> $(1)
	@printf '$(5)\n' >> $(1)
	@echo '' >> $(1)
	@echo '%description -n $(2)' >> $(1)
	@printf ${4} >> $(1)
	@echo '' >> $(1)
	@echo '' >> $(1)
	@echo '%package -n $(2)-devel' >> $(1)
	@printf 'Summary: $(BUILD_ALTLINUX_DEVEL_SUMMARY)' '$(2)' >> $(1)
	@echo 'Group: $(BUILD_ALTLINUX_GROUP_DEVEL)' >> $(1)
	@echo 'Requires: libstappler-devel = %version-%release' >> $(1)
	@printf 'Requires: $(2) = %%version-%%release' >> $(1)
	@printf '$(6)\n' >> $(1)
	@echo '' >> $(1)
	@echo '%description -n $(2)-devel' >> $(1)
	@printf '$(BUILD_ALTLINUX_DEVEL_SUMMARY)\n' '$(2)' >> $(1)
endef # make_module_spec_header_rule


define make_module_spec_files_rule =
BUILD_MODULE_SPEC_FILES += $(1)
$(1): $(BUILD_TARGET_MAKEFILES)
	@$(GLOBAL_MKDIR) $(dir $(1))
	@echo '%files -n $(2)' > $(1)
	@echo '$(foreach name,$(2),$(addsuffix .so,$(addprefix %_libdir/,$(name))))' >> $(1)
	@echo '' >> $(1)
	@echo '%files -n $(2)-devel' >> $(1)
	@printf '$(call convert_list_to_newlines,$(3))\n' >> $(1)
	@printf '$(call convert_list_to_newlines,$(4))\n' >> $(1)
	@echo '' >> $(1)
endef # make_module_spec_files_rule


# 1 - internal module name
# 2 - module title
define extract_module_altlinux =
$(eval $(call make_module_spec_header_rule,\
	$(addprefix $(BUILD_С_OUTDIR)/spec/,$(addsuffix .header.spec,$(2))),\
	$(addprefix lib,$(subst _,-,$(2))),\
	$($(1)_SHARED_SPEC_SUMMARY),\
	'$(subst $(newline),\n,$($(1)_SHARED_SPEC_DESCRIPTION))',\
	$(call shared_make_library_deps_line,$($(1)_DEPENDS_ON) $($(1)_SHARED_DEPENDS_ON),$(2)),\
	$(call shared_make_library_deps_line_devel,$($(1)_DEPENDS_ON) $($(1)_SHARED_DEPENDS_ON),$(2))\
))
$(eval $(call make_module_spec_files_rule,\
	$(addprefix $(BUILD_С_OUTDIR)/spec/,$(addsuffix .files.spec,$(2))),\
	$(addprefix lib,$(subst _,-,$(2))),\
	$(subst $(BUILD_STAPPLER_ROOT),%%_includedir/$(LOCAL_LIBRARY),\
		$(call shared_get_headers_list,$($(1)_INCLUDES_DIRS),$($(1)_INCLUDES_OBJS))\
		$(call shared_consume_includes,$($(1)_SHARED_CONSUME))),\
	$(addsuffix .mk,$(addprefix %%_datadir/$(LOCAL_LIBRARY)/modules/,$(2) $($(1)_SHARED_CONSUME)))\
))
endef # extract_module_altlinux

$(foreach module,$(BUILD_MODULES),$(foreach module_name,$(MODULE_$(module)),\
	$(eval $(call extract_module_altlinux,$(module_name),$(module)))\
))

$(BUILD_MODULE_SPEC_COMMON_HEADER): $(BUILD_TARGET_MAKEFILES)
	@$(GLOBAL_MKDIR) $(dir $(BUILD_MODULE_SPEC_COMMON_HEADER))
	@printf '$(subst %,%%,$(subst $(newline),\n,$(BUILD_ALTLINUX_SPEC_HEADER)))\n\n' > $(BUILD_MODULE_SPEC_COMMON_HEADER)

$(BUILD_MODULE_SPEC_COMMON_BUILD): $(BUILD_TARGET_MAKEFILES)
	@$(GLOBAL_MKDIR) $(dir $(BUILD_MODULE_SPEC_COMMON_BUILD))
	@printf '$(subst %,%%,$(subst $(newline),\n,$(BUILD_ALTLINUX_SPEC_BUILD)))\n' > $(BUILD_MODULE_SPEC_COMMON_BUILD)
	@printf '$(subst $(space),\n,$(subst $(BUILD_С_OUTDIR)/share,%%_datadir,$(foreach name,$(BUILD_TARGET_MAKEFILES) $(BUILD_SHARED_CONFIG),$(name))))\n\n' >> $(BUILD_MODULE_SPEC_COMMON_BUILD)

$(BUILD_ALTLINUX_SPEC): $(BUILD_MODULE_SPEC_COMMON_HEADER) $(BUILD_MODULE_SPEC_HEADERS) $(BUILD_MODULE_SPEC_COMMON_BUILD) $(BUILD_MODULE_SPEC_FILES)
	@cat $(BUILD_MODULE_SPEC_COMMON_HEADER) $(BUILD_MODULE_SPEC_HEADERS) $(BUILD_MODULE_SPEC_COMMON_BUILD) $(BUILD_MODULE_SPEC_FILES) > $(BUILD_ALTLINUX_SPEC)

all: $(BUILD_ALTLINUX_SPEC) $(BUILD_MODULE_SPEC_HEADERS) $(BUILD_MODULE_SPEC_COMMON_HEADER) $(BUILD_MODULE_SPEC_COMMON_BUILD)
