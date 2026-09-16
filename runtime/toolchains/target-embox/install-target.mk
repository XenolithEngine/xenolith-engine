# Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
# (same MIT license as the rest of the toolchain trees)

# Copy the built Embox sysroot (imported headers + Embox libs + the generated
# target.mk; compiler-rt/libc++ archives arrive with M2) from intermediate/
# into the installed targets/ tree the rest of the build system consumes.

.DEFAULT_GOAL := all
THIS_FILE := $(lastword $(MAKEFILE_LIST))
MAKE_ROOT := $(dir $(realpath $(THIS_FILE)))
GIT_TAG ?= $(shell git describe --tags --abbrev=0 2>/dev/null)
include ../common/utils/detect-platform.mk
include ../common/utils/llvm-version.mk

T_INTERMEDIATE ?= $(abspath $(MAKE_ROOT))/intermediate/aarch64-embox-none-elf
T_TARGET ?= $(abspath $(MAKE_ROOT))/targets/aarch64-embox-none-elf

$(T_TARGET):
	mkdir -p $(T_TARGET)/usr/lib $(T_TARGET)/share

# Embox libs + startup objects (libc.a, libmm.a, ..., head.o / crt0) from the
# imported sysroot, THEN the cross-built third-party dep archives (libgif,
# libpng, libjpeg, libz, libfreetype, libharfbuzz, ...) from intermediate/usr/lib
# (laid out by the Makefile SP_ARCH inner pass). The third-party set is not in
# sysroot/usr/lib because the sysroot is the raw Embox export (libc/pthread/mm
# only); the deps are cross-built separately against that libc.
$(T_TARGET)/usr/lib: $(T_INTERMEDIATE)/sysroot/usr/lib $(T_INTERMEDIATE)/usr/lib | $(T_TARGET)
	@mkdir -p $@
	cp -af $(T_INTERMEDIATE)/sysroot/usr/lib/*.a $@/ 2>/dev/null || true
	cp -af $(T_INTERMEDIATE)/sysroot/usr/lib/*.o $@/ 2>/dev/null || true
	cp -af $(T_INTERMEDIATE)/usr/lib/*.a $@/ 2>/dev/null || true

# Embox headers (libc, pthread, sched, net, video, ...) from the imported
# sysroot, THEN the cross-built third-party dep headers (gif_lib.h, png.h,
# jpeglib.h, freetype2/, harfbuzz/, ...) from intermediate/usr/include.
$(T_TARGET)/usr/include: $(T_INTERMEDIATE)/sysroot/usr/include $(T_INTERMEDIATE)/usr/include | $(T_TARGET)
	@mkdir -p $(dir $@)
	rm -rf $@
	cp -rf $(T_INTERMEDIATE)/sysroot/usr/include $@
	# Overlay third-party dep headers (some install into subdirs like freetype2/,
	# harfbuzz/, webp/, ...). rsync-style merge: copy each entry, recursing into
	# subdirs but not wiping what the sysroot copy already laid down.
	cp -rf $(T_INTERMEDIATE)/usr/include/. $@/ 2>/dev/null || true

# Mybuild module headers under usr/include do `#include <../../src/…>`.
# With -idirafter <target>/usr/include that resolves to <target>/src/…
# (next to usr/), so the export's src/*.h tree must be installed too.
$(T_TARGET)/src: $(T_INTERMEDIATE)/sysroot/src | $(T_TARGET)
	rm -rf $@
	cp -a $< $@

# resource dir for compiler-rt builtins. Before M2 (no builtins built yet) this
# is a symlink to the host clang resource dir (so <stdarg.h>, <stddef.h>,
# <arm_neon.h>, ... resolve); after M2 it will be a real directory carrying
# our freshly-built builtins plus the host's include/.
#
# SP_RUNTIME_ROOT is the engine runtime/ dir (passed in by the outer Makefile);
# the prebuilt host ships at <engine-root>/toolchains/hosts/$(HOST_ID), i.e.
# one level above SP_RUNTIME_ROOT. Compute it once via $(abspath) instead of a
# relative link so it survives being tarred/moved.
HOST_CLANG_RESOURCE := $(abspath $(SP_RUNTIME_ROOT)/../toolchains/hosts/$(HOST_ID)/lib/clang/$(SP_LLVM_VER))

$(T_TARGET)/lib/clang: | $(T_TARGET)
	@mkdir -p $(dir $@)
	rm -rf $@
	ln -fs $(HOST_CLANG_RESOURCE) $@

# App-facing descriptor produced by init-target.mk.
$(T_TARGET)/target.mk: $(T_INTERMEDIATE)/target.mk | $(T_TARGET)
	cp -af $< $@

$(T_TARGET)/share/licenses: | $(T_TARGET)
	@mkdir -p $(dir $@)
	rm -rf $@
	cp -rf ../licenses $(T_TARGET)/share 2>/dev/null || true

$(T_TARGET)/release: | $(T_TARGET)
	echo "$(GIT_TAG)" > $@

all: $(T_TARGET)/usr/lib $(T_TARGET)/usr/include \
	$(T_TARGET)/src \
	$(T_TARGET)/lib/clang \
	$(T_TARGET)/target.mk $(T_TARGET)/share/licenses $(T_TARGET)/release

.PHONY: all
