# Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
# (same MIT license as the rest of the toolchain trees)

# Copy the built NuttX sysroot (imported headers + NuttX libs + the generated
# target.mk; compiler-rt/libc++ archives arrive with M2) from intermediate/
# into the installed targets/ tree the rest of the build system consumes.

.DEFAULT_GOAL := all
THIS_FILE := $(lastword $(MAKEFILE_LIST))
MAKE_ROOT := $(dir $(realpath $(THIS_FILE)))
GIT_TAG ?= $(shell git describe --tags --abbrev=0 2>/dev/null)
include ../common/utils/detect-platform.mk

T_INTERMEDIATE ?= $(abspath $(MAKE_ROOT))/intermediate/aarch64-nuttx-none-elf
T_TARGET ?= $(abspath $(MAKE_ROOT))/targets/aarch64-nuttx-none-elf

$(T_TARGET):
	mkdir -p $(T_TARGET)/usr/lib $(T_TARGET)/share

# NuttX libs + startup objects (libc.a, libmm.a, ..., head.o / crt0).
$(T_TARGET)/usr/lib: $(T_INTERMEDIATE)/sysroot/usr/lib | $(T_TARGET)
	@mkdir -p $@
	cp -af $</*.a $@/ 2>/dev/null || true
	cp -af $</*.o $@/ 2>/dev/null || true

# NuttX headers (libc, pthread, sched, net, video, ...).
$(T_TARGET)/usr/include: $(T_INTERMEDIATE)/sysroot/usr/include | $(T_TARGET)
	@mkdir -p $(dir $@)
	rm -rf $@
	cp -rf $< $@

# resource dir for compiler-rt builtins. Before M2 (no builtins built yet) this
# is a symlink to the host clang resource dir (so <stdarg.h>, <stddef.h>,
# <arm_neon.h>, ... resolve); after M2 it will be a real directory carrying
# our freshly-built builtins plus the host's include/.
#
# SP_RUNTIME_ROOT is the engine runtime/ dir (passed in by the outer Makefile);
# the prebuilt host ships at <engine-root>/toolchains/hosts/$(HOST_ID), i.e.
# one level above SP_RUNTIME_ROOT. Compute it once via $(abspath) instead of a
# relative link so it survives being tarred/moved.
HOST_CLANG_RESOURCE := $(abspath $(SP_RUNTIME_ROOT)/../toolchains/hosts/$(HOST_ID)/lib/clang/21)

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
	$(T_TARGET)/lib/clang \
	$(T_TARGET)/target.mk $(T_TARGET)/share/licenses $(T_TARGET)/release

.PHONY: all
