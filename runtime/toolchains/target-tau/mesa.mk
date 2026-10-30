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

# Tau ICDs are shared libraries over libsprt.so, selected by MESA_VARIANT.
# Meson probes through the dll sysroot, so unsupported libc calls fail at link time.

.DEFAULT_GOAL := all

LIBNAME = mesa

include ../common/configure.mk
include lavapipe-options.mk

MESA_VARIANT ?= lavapipe
MESA_SRC ?= $(LIB_SRC_DIR)/mesa
MESA_CROSS := $(SP_INSTALL_PREFIX)/tau-meson-cross.ini
ifeq ($(MESA_VARIANT),venus)
MESA_BUILD := $(SP_INSTALL_PREFIX)/mesa-build-venus
MESA_VARIANT_OPTIONS := $(MESA_VENUS_OPTIONS)
MESA_LIB := src/virtio/vulkan/libvulkan_virtio.so
MESA_ICD := src/virtio/vulkan/virtio_icd.$(SP_ARCH).json
MESA_SONAME := libvulkan_virtio.so
MESA_ICD_NAME := virtio_icd.json
else ifeq ($(MESA_VARIANT),broadcom)
MESA_BUILD := $(SP_INSTALL_PREFIX)/mesa-build-broadcom
MESA_VARIANT_OPTIONS := $(MESA_BROADCOM_OPTIONS)
MESA_LIB := src/broadcom/vulkan/libvulkan_broadcom.so
MESA_ICD := src/broadcom/vulkan/broadcom_icd.$(SP_ARCH).json
MESA_SONAME := libvulkan_broadcom.so
MESA_ICD_NAME := broadcom_icd.json
else
MESA_BUILD := $(SP_INSTALL_PREFIX)/mesa-build
MESA_VARIANT_OPTIONS := $(MESA_OPTIONS)
MESA_LIB := $(MESA_LVP)
MESA_ICD := src/gallium/targets/lavapipe/lvp_icd.$(SP_ARCH).json
MESA_SONAME := libvulkan_lvp.so
MESA_ICD_NAME := lvp_icd.json
endif

# meson lists: one quoted element per flag.
sq := '
comma := ,
empty :=
space := $(empty) $(empty)
meson_list = [$(sq)$(subst $(space),$(sq)$(comma) $(sq),$(strip $(1)))$(sq)]

MESA_TARGET_FLAGS := -resource-dir $(SP_INSTALL_PREFIX)/lib/clang $(SP_OPT)
MESA_C_ARGS := -I$(SP_INSTALL_PREFIX) $(MESA_TARGET_FLAGS) $(CONFIGURE_CMAKE_C_FLAGS_INIT) -D_GNU_SOURCE
# __SPRT_STD_EXTERNAL=1: libc++ is the STL in every C++ unit (tau-llvm-project-include.cmake).
MESA_CPP_ARGS := -I$(SP_INSTALL_PREFIX) $(MESA_TARGET_FLAGS) $(CONFIGURE_CMAKE_CXX_FLAGS_INIT) -D_GNU_SOURCE -D__SPRT_STD_EXTERNAL=1
MESA_LINK_ARGS := -resource-dir $(SP_INSTALL_PREFIX)/lib/clang $(SP_VK_LINK_FLAGS) -Wl,-shared

$(MESA_CROSS): mesa.mk
	@echo "[host_machine]" > $@
	@echo "system = 'tau'" >> $@
	@echo "cpu_family = '$(SP_ARCH)'" >> $@
	@echo "cpu = '$(SP_ARCH)'" >> $@
	@echo "endian = 'little'" >> $@
	@echo "" >> $@
	@echo "[binaries]" >> $@
	@echo "c = '$(SP_CC)'" >> $@
	@echo "cpp = '$(SP_CXX)'" >> $@
	@echo "ar = '$(SP_AR)'" >> $@
	@echo "strip = '$(dir $(SP_AR))llvm-strip'" >> $@
	@echo "pkg-config = 'pkg-config'" >> $@
	@echo "llvm-config = '$(SP_INSTALL_PREFIX)/usr/bin/llvm-config'" >> $@
	@echo "" >> $@
	@echo "[properties]" >> $@
	@echo "needs_exe_wrapper = true" >> $@
	@echo "pkg_config_libdir = '$(SP_INSTALL_PREFIX)/usr/lib/pkgconfig'" >> $@
	@echo "" >> $@
	@echo "[built-in options]" >> $@
	@echo "c_args = $(call meson_list,$(MESA_C_ARGS))" >> $@
	@echo "cpp_args = $(call meson_list,$(MESA_CPP_ARGS))" >> $@
	@echo "c_link_args = $(call meson_list,$(MESA_LINK_ARGS))" >> $@
	@echo "cpp_link_args = $(call meson_list,$(MESA_LINK_ARGS))" >> $@

# The manifest names the library by its soname: dlopen finds it among the module's libraries.
all: $(MESA_CROSS)
	printf '#define MESA_TAU_BUILD_ID "%s"\n' "$$(cat $(MAKE_ROOT)../replacements/mesa/26.2.3-tau/*.patch mesa.mk lavapipe-options.mk $(MESA_CROSS) | sha256sum | cut -d' ' -f1)" > $(SP_INSTALL_PREFIX)/tau-mesa-build-id.h
	meson setup --wipe $(MESA_BUILD) $(MESA_SRC) --cross-file $(MESA_CROSS) \
		--prefix /usr --libdir lib $(MESA_VARIANT_OPTIONS) \
		|| meson setup $(MESA_BUILD) $(MESA_SRC) --cross-file $(MESA_CROSS) \
		--prefix /usr --libdir lib $(MESA_VARIANT_OPTIONS)
	ninja -C $(MESA_BUILD) $(MESA_LIB) $(MESA_ICD)
	mkdir -p $(SP_INSTALL_PREFIX)/usr/lib $(SP_INSTALL_PREFIX)/usr/share/vulkan/icd.d
	cp -f $(MESA_BUILD)/$(MESA_LIB) $(SP_INSTALL_PREFIX)/usr/lib/$(MESA_SONAME)
	sed -e 's|"library_path": "[^"]*"|"library_path": "$(MESA_SONAME)"|' \
		$(MESA_BUILD)/$(MESA_ICD) > $(SP_INSTALL_PREFIX)/usr/share/vulkan/icd.d/$(MESA_ICD_NAME)

.PHONY: all
