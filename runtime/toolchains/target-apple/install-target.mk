# Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
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

GIT_TAG ?= $(shell git describe --tags --abbrev=0)

T_INTERMEDIATE ?= $(abspath $(LIBS_MAKE_ROOT))/intermediate/x86_64-unknown-linux-gnu
T_TARGET ?= $(abspath $(LIBS_MAKE_ROOT))/targets/x86_64-unknown-linux-gnu

# The +sprt variant ships a prebuilt libsprt (dylib + tbd + runtime.mk). The +open
# export deliberately does NOT: it is a pure SDK-free sysroot — the engine builds
# the runtime from source against it, exactly like the intermediate flow.
ifeq ($(filter %+sprt, $(T_TARGET)),)
else
T_SPRT = 1
endif

# +open instead exports the SDK-free sysroot pieces: include_libc (the SDK-like
# headers), the baked .tbd link stubs in usr/lib, and the framework bundles
# (Headers + tbds).
T_OPEN := $(filter %+open,$(T_TARGET))

ALL_STATIC_LIBS := \
	$(filter-out \
			%/libSPIRV-Tools.a \
			%/libSPIRV-Tools-diff.a \
			%/libSPIRV-Tools-link.a \
			%/libSPIRV-Tools-lint.a \
			%/libSPIRV-Tools-opt.a \
			%/libSPIRV-Tools-reduce.a \
		,$(wildcard $(T_INTERMEDIATE)/usr/lib/*.a)) \
	$(wildcard $(T_INTERMEDIATE)/usr/lib/*.o)
ALL_INSTALL_STATIC_LIBS := $(patsubst $(T_INTERMEDIATE)/%,$(T_TARGET)/%,$(ALL_STATIC_LIBS))

# The Vulkan validation layer is a flat .dylib on macOS but a .framework bundle
# on iOS. install-target.mk is platform-agnostic (no SP_SYSNAME), so detect
# whichever artifact the target build actually produced.
VVL_DYLIB := $(wildcard $(T_INTERMEDIATE)/usr/lib/libVkLayer_khronos_validation.dylib)
VVL_FRAMEWORK := $(wildcard $(T_INTERMEDIATE)/usr/lib/VkLayer_khronos_validation.framework)

ALL_SHARED_LIBS := $(addprefix $(T_INTERMEDIATE)/usr/lib/,\
	libvulkan.dylib \
	libMoltenVK.dylib \
) $(VVL_DYLIB)

ALL_INSTALL_SHARED_LIBS := $(patsubst $(T_INTERMEDIATE)/%,$(T_TARGET)/%,$(ALL_SHARED_LIBS))
ALL_INSTALL_FRAMEWORKS := $(patsubst $(T_INTERMEDIATE)/%,$(T_TARGET)/%,$(VVL_FRAMEWORK))

$(T_TARGET):
	mkdir -p $(T_TARGET)/share $(T_TARGET)/usr/lib

$(T_TARGET)/usr/include: $(T_INTERMEDIATE)/usr/include | $(T_TARGET)
	@mkdir -p $(dir $@)
	rm -rf $@
	cp -rf $< $@

$(T_TARGET)/lib: $(T_INTERMEDIATE)/lib | $(T_TARGET)
	@mkdir -p $(dir $@)
	rm -rf $@
	cp -rf $< $@
	rm -rf $@/clang/include
	touch $@

# Framework bundles are directories — copy them recursively. This more specific
# pattern (shorter stem) wins over the generic file-copy rule below.
$(T_TARGET)/usr/lib/%.framework: $(T_INTERMEDIATE)/usr/lib/%.framework | $(T_TARGET)
	@mkdir -p $(dir $@)
	rm -rf $@
	cp -rf $< $@

$(T_TARGET)/%: $(T_INTERMEDIATE)/% | $(T_TARGET)
	@mkdir -p $(dir $@)
	cp -f $< $@

$(T_TARGET)/share/licenses: | $(T_TARGET)
	@mkdir -p $(dir $@)
	rm -rf $@
	cp -rf ../licenses $(T_TARGET)/share

$(T_TARGET)/share/vulkan: | $(T_TARGET)
	@mkdir -p $(dir $@)
	rm -rf $@
	cp -rf $(T_INTERMEDIATE)/usr/share/vulkan $(T_TARGET)/share
	rm -rf $@/registry

ALL_TARGETS := $(ALL_INSTALL_STATIC_LIBS) $(ALL_INSTALL_SHARED_LIBS) $(ALL_INSTALL_FRAMEWORKS) \
	$(T_TARGET)/lib $(T_TARGET)/usr/include $(T_TARGET)/share/licenses $(T_TARGET)/share/vulkan \
	$(T_TARGET)/target.mk

ifdef T_OPEN
# The +open usr/lib is copied VERBATIM (cp -a), then trimmed: it carries the
# product dylibs (vulkan/MoltenVK/VVL + their symlink chains), the baked .tbd
# link stubs for the system libraries (libSystem/CoreFoundation/libobjc/libc++/
# ...), and crt1.o. Trimmed from the export:
#   - the SYSTEM dylibs our libcxx.mk builds (libc++/libc++abi/libunwind chains):
#     consumers link the tbds; at run time macOS provides the real /usr/lib ones;
#   - stale static LLVM runtimes (no longer built; belt-and-braces);
#   - the SPIRV-Tools statics the stock flow also drops;
#   - every service SUBDIRECTORY (pkgconfig/cmake/engines-3/ossl-modules/...).
# The +open copy rules are FORCE'd: their sources are directory trees whose files
# the stamps overwrite IN PLACE (cp -f/-Rf), so neither the directory mtime nor
# any single file reliably signals staleness. An install run is explicit and the
# copies are cheap relative to the build, so just always refresh them.
FORCE:
.PHONY: FORCE

$(T_TARGET)/usr/lib/libSystem.tbd: FORCE | $(T_TARGET)
	rm -rf $(T_TARGET)/usr/lib
	cp -a $(T_INTERMEDIATE)/usr/lib $(T_TARGET)/usr/lib
	rm -f $(T_TARGET)/usr/lib/libc++*.dylib $(T_TARGET)/usr/lib/libunwind*.dylib
	rm -f $(T_TARGET)/usr/lib/libc++.a $(T_TARGET)/usr/lib/libc++abi.a \
		$(T_TARGET)/usr/lib/libc++experimental.a $(T_TARGET)/usr/lib/libunwind.a
	rm -f $(T_TARGET)/usr/lib/libSPIRV-Tools.a $(T_TARGET)/usr/lib/libSPIRV-Tools-diff.a \
		$(T_TARGET)/usr/lib/libSPIRV-Tools-link.a $(T_TARGET)/usr/lib/libSPIRV-Tools-lint.a \
		$(T_TARGET)/usr/lib/libSPIRV-Tools-opt.a $(T_TARGET)/usr/lib/libSPIRV-Tools-reduce.a \
		$(T_TARGET)/usr/lib/libSPIRV-Tools-shared.dylib
	find $(T_TARGET)/usr/lib -mindepth 1 -maxdepth 1 -type d ! -name '*.framework' -exec rm -rf {} +
	rm -rf $(T_TARGET)/usr/lib/png.framework
	touch $@

$(T_TARGET)/include_libc: FORCE | $(T_TARGET)
	@mkdir -p $(dir $@)
	rm -rf $@
	cp -rf $(T_INTERMEDIATE)/include_libc $@

# The framework bundles: hand-written Headers + the .tbd link stubs (Foundation,
# AppKit, Metal, QuartzCore, IOSurface, ...) — the -F search half of the sysroot.
$(T_TARGET)/System/Library/Frameworks: FORCE | $(T_TARGET)
	@mkdir -p $(dir $@)
	rm -rf $@
	cp -rf $(T_INTERMEDIATE)/System/Library/Frameworks $@

# Rebuild the install set from scratch: the verbatim usr/lib copy REPLACES the
# per-file ALL_INSTALL_* lists (which were already baked into ALL_TARGETS above).
# No usr/local/include link in the export (the intermediate keeps one for
# hermetic third-party builds): the engine reaches include_libc via
# TARGET_INCLUDE_DIR_LIBC / make/os/darwin.mk -idirafter.
ALL_TARGETS := \
	$(T_TARGET)/lib $(T_TARGET)/usr/include $(T_TARGET)/share/licenses $(T_TARGET)/share/vulkan \
	$(T_TARGET)/usr/lib/libSystem.tbd $(T_TARGET)/include_libc \
	$(T_TARGET)/System/Library/Frameworks $(T_TARGET)/target.mk
endif

ifdef T_SPRT
$(T_TARGET)/usr/lib/libsprt.dylib: $(ALL_TARGETS)
	$(call rule_rm,$@)
	$(MAKE) -j8 -C $(SP_RUNTIME_ROOT) \
		STAPPLER_HOST_FILE=$(T_INTERMEDIATE)/host/host.mk \
		STAPPLER_TARGET_FILE=$(T_TARGET)/target.mk \
		STAPPLER_TARGET=$(SP_TARGET) RELEASE=1
	cp $(SP_RUNTIME_ROOT)/stappler-build/$(SP_TARGET)/release/cc/libsprt.dylib $@

$(T_TARGET)/usr/lib/libsprt.tbd: $(T_TARGET)/usr/lib/libsprt.dylib
	@echo 'Build $@'
	@$(call rule_rm,$@)
	@echo '--- !tapi-tbd' > $@
	@echo 'tbd-version:     4' >> $@
	@echo 'targets:         [ $(SP_APPLE_ARCH) ]' >> $@
	@echo 'flags:           [ not_app_extension_safe ]' >> $@
	@echo "install-name:    '@rpath/libsprt.dylib'" >> $@
	@echo 'current-version: 0' >> $@
	@echo 'compatibility-version: 0' >> $@
	@echo 'exports:' >> $@
	@echo '  - targets:         [ $(SP_APPLE_ARCH) ]' >> $@
	@echo '    symbols:         [' >> $@
	@$(T_INTERMEDIATE)/host/bin/llvm-nm  --extern-only -m $(T_TARGET)/usr/lib/libsprt.dylib | grep --invert-match weak | sed -E 's/.*\).*external ([\$$_0-9a-zA-Z]+).*/        \1,/' >> $@
	@cat functions_$(SP_ARCH).txt >> $@
	@echo '    ]' >> $@
	@echo '    weak-symbols:    [' >> $@
	@$(T_INTERMEDIATE)/host/bin/llvm-nm  --extern-only -m $(T_TARGET)/usr/lib/libsprt.dylib | grep weak | sed -E 's/.*weak.* (_[\$$_0-9a-zA-Z]+).*/        \1,/' >> $@
	@echo '    ]' >> $@
	@echo '...' >> $@

RUNTIME_HEADERS_COPY = cp -r $(SP_RUNTIME_ROOT)/include/* $(T_TARGET)/usr/include; \
	cp -r $(SP_RUNTIME_ROOT)/include_libc/* $(T_TARGET)/usr/include

all: $(T_TARGET)/usr/lib/libsprt.tbd

$(T_TARGET)/runtime.mk: $(T_TARGET)/usr/include $(lastword $(MAKEFILE_LIST))
	$(RUNTIME_HEADERS_COPY)
	@echo 'Build $@'
	@echo 'MODULE_RUNTIME_DEFINED_IN := $$(lastword $$(MAKEFILE_LIST))' > $@
	@echo 'MODULE_RUNTIME_INCLUDES_OBJS := $$(TARGET_SYSROOT)/usr/include/darwin $$(TARGET_SYSROOT)/usr/include/sprt/runtime/geom/glsl' >> $@
	@echo 'MODULE_RUNTIME_SHADERS_INCLUDE := $$(TARGET_SYSROOT)/usr/include/sprt/runtime/geom/glsl' >> $@
	@echo 'MODULE_RUNTIME_GENERAL_LDFLAGS := -lsprt' >> $@
	@echo 'MODULE_RUNTIME_GENERAL_CFLAGS := -DSPRT_SHARED_RUNTIME' >> $@
	@echo 'MODULE_RUNTIME_GENERAL_CXXFLAGS := -DSPRT_SHARED_RUNTIME' >> $@
	@echo 'RUNTIME_INSTALL_LIBRARY := $$(TARGET_SYSROOT)/usr/lib/libsprt.dylib' >> $@
	@echo '$$(call define_module, runtime, MODULE_RUNTIME)' >> $@
	@echo '$$(call define_module, runtime_window, MODULE_RUNTIME_WINDOW)' >> $@

all: $(T_TARGET)/runtime.mk

endif

$(T_TARGET)/release: $(T_TARGET)
	echo "$(GIT_TAG)" > $@
	touch $@

all: $(ALL_TARGETS) $(T_TARGET) $(T_TARGET)/release

.PHONY: all
.DEFAULT_GOAL := all
