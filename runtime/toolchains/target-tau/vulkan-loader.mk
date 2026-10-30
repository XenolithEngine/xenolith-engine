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

# The Khronos Vulkan loader for Tau: libvulkan.so.1, without window systems. It finds drivers
# as on Linux, by the manifests in the data directories (RT-A-VulkanLoader-1 of xenolith-tau):
# /usr/share/vulkan/icd.d, where a module sees the files of the libraries linked into it.

.DEFAULT_GOAL := all

LIBNAME = vulkan-loader

include ../common/configure.mk

LOADER_BUILD := $(SP_INSTALL_PREFIX)/vulkan-loader-build

all:
	cmake -G Ninja -S $(LIB_SRC_DIR)/vulkan-loader -B $(LOADER_BUILD) $(CONFIGURE_CMAKE) \
		-DBUILD_SHARED_LIBS=ON \
		-DBUILD_TESTS=OFF \
		-DBUILD_WSI_XCB_SUPPORT=OFF \
		-DBUILD_WSI_XLIB_SUPPORT=OFF \
		-DBUILD_WSI_XLIB_XRANDR_SUPPORT=OFF \
		-DBUILD_WSI_WAYLAND_SUPPORT=OFF \
		-DBUILD_WSI_DIRECTFB_SUPPORT=OFF \
		-DSYSCONFDIR=/etc \
		-DFALLBACK_CONFIG_DIRS=/etc/xdg \
		-DFALLBACK_DATA_DIRS=/usr/local/share:/usr/share
	cmake --build $(LOADER_BUILD) --parallel
	mkdir -p $(SP_INSTALL_PREFIX)/usr/lib
	cp -f $$(realpath $(LOADER_BUILD)/loader/libvulkan.so.1) $(SP_INSTALL_PREFIX)/usr/lib/libvulkan.so.1

.PHONY: all
