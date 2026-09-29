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

# Mesa's Vulkan drivers for the Embox target, linked into the flat EL1 image:
#   MESA_VARIANT=lavapipe  libvulkan_lvp.a, the software driver on llvmpipe
#                          (xenolith-os docs/EMBOX-LAVAPIPE.md, B3);
#   MESA_VARIANT=venus     libvulkan_virtio.a, Venus over the kernel's
#                          virtio-gpu driver (xl_vgpu.h in the Embox export),
#                          no LLVM (xenolith-os docs/EMBOX-VENUS.md, B1).
# Each has a build tree of its own, so switching does not rebuild the other.
#
# Mesa builds with meson; there is no Embox meson cross file anywhere else, so
# this one is generated here from the very flags the cmake deps get
# (common/configure.mk, EMBOX): sprt's include_libc first, the libc++ port for
# C++, the Embox headers last, emulated TLS. system = 'embox' keeps Mesa off its
# Linux paths (KMS/DRM, /proc, futex); the Embox patches in
# replacements/mesa/<ver>-embox teach it the rest. meson's own probes link
# against libsprt.a and the Embox libc with --no-undefined, as curl's do.
#
# The build tree stays (intermediate/.../mesa-build).

.DEFAULT_GOAL := all

LIBNAME = mesa

include ../common/configure.mk

MESA_SRC ?= $(LIB_SRC_DIR)/mesa
MESA_VARIANT ?= lavapipe
MESA_BUILD := $(SP_INSTALL_PREFIX)/mesa-build$(if $(filter venus,$(MESA_VARIANT)),-venus)
MESA_CROSS := $(SP_INSTALL_PREFIX)/embox-meson-cross.ini
HOST_BIN := $(SP_INSTALL_PREFIX)/host/bin

# meson lists: one quoted element per flag.
sq := '
comma := ,
empty :=
space := $(empty) $(empty)
meson_list = [$(sq)$(subst $(space),$(sq)$(comma) $(sq),$(strip $(1)))$(sq)]

MESA_TARGET_FLAGS := --target=$(SP_ARCH_TARGET_CLANG) -resource-dir $(SP_INSTALL_PREFIX)/lib/clang $(SP_OPT)
MESA_C_ARGS := $(MESA_TARGET_FLAGS) $(CONFIGURE_CMAKE_C_FLAGS_INIT) -D_GNU_SOURCE
# __SPRT_STD_EXTERNAL=1: libc++ is the STL in every C++ unit here; see
# embox-llvm-project-include.cmake for why sprt cannot always tell by itself.
MESA_CPP_ARGS := $(MESA_TARGET_FLAGS) $(CONFIGURE_CMAKE_CXX_FLAGS_INIT) -D_GNU_SOURCE -D__SPRT_STD_EXTERNAL=1
# The probe link line of common/configure.mk (SP_EMBOX_PROBE_LDFLAGS) without its
# --start-group: meson wraps the libraries of every link in a group of its own,
# and ld.lld refuses a nested one. The libraries are listed twice instead.
P := $(SP_INSTALL_PREFIX)
MESA_PROBE_LIBS := -lsprt -lc -lm -lc++abi -lunwind -lsme_stub -lprobe-stubs
MESA_LINK_ARGS := --target=$(SP_ARCH_TARGET_CLANG) -resource-dir $(P)/lib/clang \
	-nodefaultlibs -nostartfiles -L$(P)/usr/lib -L$(P)/sysroot/usr/lib \
	$(MESA_PROBE_LIBS) $(MESA_PROBE_LIBS) \
	$(P)/sysroot/usr/lib/libclang_rt.builtins-aarch64.a \
	-Wl,--no-dependent-libraries -Wl,--no-undefined -Wl,-u,main -Wl,-e,main

MESA_OPTIONS := \
	--buildtype release \
	-Db_ndebug=true \
	-Ddefault_library=static \
	-Dcpp_std=gnu++20 \
	-Dplatforms= \
	-Dcpp_rtti=false \
	-Dglx=disabled \
	-Degl=disabled \
	-Dgbm=disabled \
	-Dgles1=disabled \
	-Dgles2=disabled \
	-Dopengl=false \
	-Dvulkan-layers= \
	-Dtools= \
	-Dshader-cache=disabled \
	-Dzlib=disabled \
	-Dzstd=disabled \
	-Dexpat=disabled \
	-Dxmlconfig=disabled \
	-Dlibunwind=disabled \
	-Dvalgrind=disabled \
	-Ddisplay-info=disabled \
	-Dspirv-tools=disabled \
	-Dbuild-tests=false \
	-Dvideo-codecs= \
	-Dperfetto=false

ifeq ($(MESA_VARIANT),venus)
MESA_OPTIONS += \
	-Dvulkan-drivers=virtio \
	-Dgallium-drivers= \
	-Dllvm=disabled
MESA_LIB_BUILT := src/virtio/vulkan/libvulkan_virtio.a
else
MESA_OPTIONS += \
	-Dvulkan-drivers=swrast \
	-Dgallium-drivers=llvmpipe \
	-Dllvm=enabled \
	-Dshared-llvm=disabled \
	-Ddraw-use-llvm=true \
	-Dllvm-orcjit=false
MESA_LIB_BUILT := src/gallium/targets/lavapipe/libvulkan_lvp.a
endif

$(MESA_CROSS): mesa.mk
	@echo "[host_machine]" > $@
	@echo "system = 'embox'" >> $@
	@echo "cpu_family = 'aarch64'" >> $@
	@echo "cpu = 'cortex-a72'" >> $@
	@echo "endian = 'little'" >> $@
	@echo "" >> $@
	@echo "[binaries]" >> $@
	@echo "c = '$(HOST_BIN)/clang'" >> $@
	@echo "cpp = '$(HOST_BIN)/clang++'" >> $@
	@echo "ar = '$(HOST_BIN)/llvm-ar'" >> $@
	@echo "strip = '$(HOST_BIN)/llvm-strip'" >> $@
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

all: $(MESA_CROSS)
	meson setup --wipe $(MESA_BUILD) $(MESA_SRC) --cross-file $(MESA_CROSS) \
		--prefix $(SP_INSTALL_PREFIX)/usr --libdir lib --includedir include \
		$(MESA_OPTIONS) \
		|| meson setup $(MESA_BUILD) $(MESA_SRC) --cross-file $(MESA_CROSS) \
		--prefix $(SP_INSTALL_PREFIX)/usr --libdir lib --includedir include \
		$(MESA_OPTIONS)
	ninja -C $(MESA_BUILD) $(MESA_LIB_BUILT)
	cp -f $(MESA_BUILD)/$(MESA_LIB_BUILT) $(SP_INSTALL_PREFIX)/usr/lib/$(notdir $(MESA_LIB_BUILT))

.PHONY: all
