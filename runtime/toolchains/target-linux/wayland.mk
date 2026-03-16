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

.DEFAULT_GOAL := all

LIBNAME = wayland

include ../common/configure.mk

CONFIGURE := \
	$(CONFIGURE_CMAKE) \
	-DBUILD_SHARED_LIBS=Off \
	-DLIBXML2_WITH_DEBUG=Off \
	-DLIBXML2_WITH_PROGRAMS=Off \
	-DLIBXML2_WITH_TESTS=Off \
	-DLIBXML2_WITH_TLS=On \

WLP_INSTALL_DIR := $(SP_INSTALL_PREFIX)/usr/include/wayland-protocols
WLP_SRC_DIR := $(LIB_SRC_DIR)/wayland-protocols
PLASMA_SRC_DIR := $(LIB_SRC_DIR)/plasma-wayland-protocols/src/protocols

WAYLAND_LIB := $(SP_INSTALL_PREFIX)/usr/lib/libwayland-client.a

WLP_LIST := $(addprefix $(WLP_INSTALL_DIR)/,\
	viewporter.h \
	viewporter.c \
	xdg-shell.h \
	xdg-shell.c \
	xdg-decoration.h \
	xdg-decoration.c \
	cursor-shape.h \
	cursor-shape.c \
	kde-output-device-v2.h \
	kde-output-device-v2.c \
	kde-output-management-v2.h \
	kde-output-management-v2.c \
	kde-output-order-v1.h \
	kde-output-order-v1.c \
	tablet-v2.h \
	tablet-v2.c \
	)

$(WLP_INSTALL_DIR)/viewporter.h: $(WLP_SRC_DIR)/stable/viewporter/viewporter.xml $(WAYLAND_LIB)
	@$(call rule_mkdir,$(WLP_INSTALL_DIR))
	wayland-scanner client-header $< $@

$(WLP_INSTALL_DIR)/viewporter.c: $(WLP_SRC_DIR)/stable/viewporter/viewporter.xml $(WAYLAND_LIB)
	@$(call rule_mkdir,$(WLP_INSTALL_DIR))
	wayland-scanner private-code $< $@

$(WLP_INSTALL_DIR)/xdg-shell.h: $(WLP_SRC_DIR)/stable/xdg-shell/xdg-shell.xml $(WAYLAND_LIB)
	@$(call rule_mkdir,$(WLP_INSTALL_DIR))
	wayland-scanner client-header $< $@

$(WLP_INSTALL_DIR)/xdg-shell.c: $(WLP_SRC_DIR)/stable/xdg-shell/xdg-shell.xml $(WAYLAND_LIB)
	@$(call rule_mkdir,$(WLP_INSTALL_DIR))
	wayland-scanner private-code $< $@

$(WLP_INSTALL_DIR)/tablet-v2.h: $(WLP_SRC_DIR)/stable/tablet/tablet-v2.xml $(WAYLAND_LIB)
	@$(call rule_mkdir,$(WLP_INSTALL_DIR))
	wayland-scanner client-header $< $@

$(WLP_INSTALL_DIR)/tablet-v2.c: $(WLP_SRC_DIR)/stable/tablet/tablet-v2.xml $(WAYLAND_LIB)
	@$(call rule_mkdir,$(WLP_INSTALL_DIR))
	wayland-scanner private-code $< $@

$(WLP_INSTALL_DIR)/xdg-decoration.h: $(WLP_SRC_DIR)/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml $(WAYLAND_LIB)
	@$(call rule_mkdir,$(WLP_INSTALL_DIR))
	wayland-scanner client-header $< $@

$(WLP_INSTALL_DIR)/xdg-decoration.c: $(WLP_SRC_DIR)/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml $(WAYLAND_LIB)
	@$(call rule_mkdir,$(WLP_INSTALL_DIR))
	wayland-scanner private-code $< $@

$(WLP_INSTALL_DIR)/cursor-shape.h: $(WLP_SRC_DIR)/staging/cursor-shape/cursor-shape-v1.xml $(WAYLAND_LIB)
	@$(call rule_mkdir,$(WLP_INSTALL_DIR))
	wayland-scanner client-header $< $@

$(WLP_INSTALL_DIR)/cursor-shape.c: $(WLP_SRC_DIR)/staging/cursor-shape/cursor-shape-v1.xml $(WAYLAND_LIB)
	@$(call rule_mkdir,$(WLP_INSTALL_DIR))
	wayland-scanner private-code $< $@

$(WLP_INSTALL_DIR)/kde-output-device-v2.h: $(PLASMA_SRC_DIR)/kde-output-device-v2.xml $(WAYLAND_LIB)
	@$(call rule_mkdir,$(WLP_INSTALL_DIR))
	wayland-scanner client-header $< $@

$(WLP_INSTALL_DIR)/kde-output-device-v2.c: $(PLASMA_SRC_DIR)/kde-output-device-v2.xml $(WAYLAND_LIB)
	@$(call rule_mkdir,$(WLP_INSTALL_DIR))
	wayland-scanner private-code $< $@

$(WLP_INSTALL_DIR)/kde-output-management-v2.h: $(PLASMA_SRC_DIR)/kde-output-management-v2.xml $(WAYLAND_LIB)
	@$(call rule_mkdir,$(WLP_INSTALL_DIR))
	wayland-scanner client-header $< $@

$(WLP_INSTALL_DIR)/kde-output-management-v2.c: $(PLASMA_SRC_DIR)/kde-output-management-v2.xml $(WAYLAND_LIB)
	@$(call rule_mkdir,$(WLP_INSTALL_DIR))
	wayland-scanner private-code $< $@

$(WLP_INSTALL_DIR)/kde-output-order-v1.h: $(PLASMA_SRC_DIR)/kde-output-order-v1.xml $(WAYLAND_LIB)
	@$(call rule_mkdir,$(WLP_INSTALL_DIR))
	wayland-scanner client-header $< $@

$(WLP_INSTALL_DIR)/kde-output-order-v1.c: $(PLASMA_SRC_DIR)/kde-output-order-v1.xml $(WAYLAND_LIB)
	@$(call rule_mkdir,$(WLP_INSTALL_DIR))
	wayland-scanner private-code $< $@

$(WAYLAND_LIB): $(lastword $(MAKEFILE_LIST))
	$(call rule_rm,$(LIBNAME))
	$(call rule_mkdir,$(LIBNAME))
	meson setup $(LIBNAME)/ $(LIB_SRC_DIR)/$(LIBNAME) --prefix=$(SP_INSTALL_PREFIX) \
		--cross-file $(SP_INSTALL_PREFIX)/target.ini --pkg-config-path $(SP_INSTALL_PREFIX)/usr/lib/pkgconfig \
		-Dscanner=false -Dtests=false -Ddocumentation=false -Ddtd_validation=false -Ddefault_library=static \
		--libdir $(SP_INSTALL_PREFIX)/usr/lib --includedir $(SP_INSTALL_PREFIX)/usr/include
	ninja -C $(LIBNAME)/ install
	$(call rule_rm,$(SP_INSTALL_PREFIX)/usr/lib/libwayland-server.a)
	$(call rule_rm,$(SP_INSTALL_PREFIX)/usr/include/wayland-server.h)
	$(call rule_rm,$(SP_INSTALL_PREFIX)/usr/include/wayland-server-core.h)
	$(call rule_rm,$(LIBNAME))

all: $(SP_INSTALL_PREFIX)/usr/lib/libwayland-client.a $(WLP_LIST)

.PHONY: all
