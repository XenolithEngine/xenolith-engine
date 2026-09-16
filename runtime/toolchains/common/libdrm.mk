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

# libdrm is the only SHARED library in the common set of Linux targets.
#
# It cannot be static: libdrm owns process state (the table of open devices, the
# driver version cache) and is linked by the GPU driver at the same time. A copy
# compiled into the application plus a copy inside mesa would give two
# independent states over the very same fds — the same class of problem as two
# C++ ABI runtimes in one process. Distributions ship only libdrm.so for exactly
# this reason.
#
# The vendor wrappers (libdrm_intel/radeon/amdgpu/...) are disabled entirely.
# They are not part of the core ABI: they are helpers over one driver's ioctls
# that mesa needs for specific hardware. Building them pulls in pciaccess and
# atomic primitives, and the set depends on the architecture — the common target
# has no use for that, and board specifics are built by whoever builds the
# driver. The libdrm core (xf86drm.h/xf86drmMode.h plus the UAPI copies under
# libdrm/) is vendor-independent and identical on every architecture.
#
# On the build system: meson rather than cmake, because libdrm has no
# CMakeLists of its own. The cross file is taken as-is (target.ini next to the
# sysroot): meson itself expands @DIRNAME@ in it to the file's directory, so the
# same ini works both in the build tree and in the laid-out target. wayland.mk
# is arranged the same way.

.DEFAULT_GOAL := all

LIBNAME = libdrm

include ../common/configure.mk

LIBDRM_LIB := $(SP_INSTALL_PREFIX)/usr/lib/libdrm.so.2

# The tests need cairo and the ability to run target binaries, the man pages
# need docbook; the udev rules install into the system /lib/udev and have no
# business in a sysroot.
CONFIGURE_MESON := \
	--cross-file $(SP_INSTALL_PREFIX)/target.ini \
	--pkg-config-path $(SP_INSTALL_PREFIX)/usr/lib/pkgconfig \
	--prefix=$(SP_INSTALL_PREFIX) \
	--libdir $(SP_INSTALL_PREFIX)/usr/lib \
	--includedir $(SP_INSTALL_PREFIX)/usr/include \
	--buildtype release \
	-Ddefault_library=shared \
	-Dtests=false \
	-Dinstall-test-programs=false \
	-Dcairo-tests=disabled \
	-Dman-pages=disabled \
	-Dvalgrind=disabled \
	-Dudev=false \
	-Dintel=disabled \
	-Dradeon=disabled \
	-Damdgpu=disabled \
	-Dnouveau=disabled \
	-Dvmwgfx=disabled \
	-Domap=disabled \
	-Dexynos=disabled \
	-Dfreedreno=disabled \
	-Dtegra=disabled \
	-Dvc4=disabled \
	-Detnaviv=disabled

all:
	$(call rule_rm,$(LIBNAME))
	$(call rule_mkdir,$(LIBNAME))
	meson setup $(LIBNAME)/ $(LIB_SRC_DIR)/$(LIBNAME) $(CONFIGURE_MESON)
	ninja -C $(LIBNAME)/ install
	$(call rule_rm,$(LIBNAME))

.PHONY: all
