# Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
# (same MIT license as the rest of the toolchain trees)

# Import a NuttX `make export` package into this target's sysroot and extract
# the per-board architecture flags + feature CONFIG_* values the downstream
# init-target.mk needs to generate target.mk / toolchain.cmake.
#
# Inputs (set by the caller — see target-nuttx/Makefile):
#   NUTTX_EXPORT=<path>      a prebuilt nuttx-export[-<ver>].tar.gz (preferred)
#   NUTTX_DIR=<path>         a NuttX source tree (we run `make export` here)
#   NUTTX_CONFIG=<b>:<c>     board:config to build the export from (required w/ NUTTX_DIR)
#   NUTTX_BOARD=<board>      optional explicit board override
#   TOOLCHAIN_OUTPUT_DIR=<path>  per-triple intermediate dir (set by outer Makefile)
#
# Outputs (in $(TOOLCHAIN_OUTPUT_DIR)):
#   nuttx-export/            the unpacked / built export tree
#     include/  libs/  startup/  scripts/  arch/  tools/  .config  scripts/Make.defs
#   sysroot/usr/include      NuttX headers (incl. libc, pthread, sched, net...)
#   sysroot/usr/lib          NuttX static libraries (libc.a, libmm.a, libsched.a, ...)
#   sysroot/usr/include_libc alias include path used by the rest of the toolchain layout
#   nuttx-arch-flags.mk      extracted ARCHCPUFLAGS / ARCHCFLAGS / ARCHCXXFLAGS / LDFLAGS
#   nuttx-config.mk          feature CONFIG_* values turned into make variables
#
# This step does NOT compile anything — it is a layout/extraction pass. The
# compiled LLVM runtimes arrive in M2; the NuttX kernel+apps image is produced
# by the Xenolith OS layer (xenolith-os), not here.

.DEFAULT_GOAL := all
THIS_FILE := $(lastword $(MAKEFILE_LIST))
include $(dir $(THIS_FILE))../common/utils/detect-platform.mk
include $(dir $(THIS_FILE))../common/utils/init-shell.mk

OUT := $(TOOLCHAIN_OUTPUT_DIR)
EXPORT_DIR := $(OUT)/nuttx-export
SYSROOT := $(OUT)/sysroot
ARCH_FLAGS_FILE := $(OUT)/nuttx-arch-flags.mk
CONFIG_FILE := $(OUT)/nuttx-config.mk

# --- 1. Acquire the export tree --------------------------------------------

# A prebuilt tarball wins over a from-source build (faster + reproducible).
$(EXPORT_DIR)/.stamp-unpacked:
ifeq ($(NUTTX_EXPORT),)
# Build the export package from NUTTX_DIR + NUTTX_CONFIG on the spot. We use
# NuttX's own ./tools/configure.sh + make export. The board:config is split on
# the first colon. NB: this requires a working NuttX build host (kconfiglib,
# the baremetal toolchain that matches the board's arch, qemu for some configs);
# the prebuilt-tarball path is the recommended one for CI.
	@echo "Building nuttx-export from source: $(NUTTX_DIR) [$(NUTTX_CONFIG)]"
	$(call rule_rm,$(EXPORT_DIR))
	mkdir -p $(EXPORT_DIR)
	cd "$(NUTTX_DIR)" && ./tools/configure.sh "$(NUTTX_CONFIG)" && $(MAKE) export
	cp -f "$(NUTTX_DIR)"/nuttx-export*.tar.gz $(EXPORT_DIR)/ || \
		cp -rf "$(NUTTX_DIR)/nuttx-export" "$(EXPORT_DIR)/unpacked"
	cd $(EXPORT_DIR) && for pkg in nuttx-export*.tar.gz; do \
		[ -e "$$pkg" ] && tar -xzf "$$pkg" && break; done || true
	[ -d $(EXPORT_DIR)/nuttx-export ] || mv $(EXPORT_DIR)/unpacked $(EXPORT_DIR)/nuttx-export
else
	@echo "Unpacking nuttx-export: $(NUTTX_EXPORT)"
	$(call rule_rm,$(EXPORT_DIR))
	mkdir -p $(EXPORT_DIR)
	cd $(EXPORT_DIR) && tar -xzf "$(abspath $(NUTTX_EXPORT))"
	# `make export` produces nuttx-export[-<ver>].tar.gz unpacking to nuttx-export[-<ver>].
	# Normalise to a single canonical nuttx-export/ name.
	cd $(EXPORT_DIR) && mv nuttx-export* nuttx-export || true
endif
	@touch $@

# --- 2. Lay out headers + libraries as a sysroot ---------------------------

# The export tree has include/, libs/, startup/, scripts/Make.defs, .config.
# We expose them as usr/include + usr/lib so init-target.mk can write the
# standard toolchain.cmake/target.mk contract (TARGET_INCLUDE_DIR_LIBC etc).
$(SYSROOT)/usr/include: $(EXPORT_DIR)/.stamp-unpacked
	@mkdir -p $(dir $@)
	rm -rf $@
	# NuttX headers: libc, sched, pthread, net, video, nx, fs, ... The export
	# include/ already follows symlinks to per-arch chip/board headers.
	cp -rf $(EXPORT_DIR)/nuttx-export/include $@
	# arch/ holds per-arch chip + os headers (sched/*, pthread/*, ...). Pull
	# them in under usr/include/arch so -isystem usr/include resolves them.
	[ -d $(EXPORT_DIR)/nuttx-export/arch ] && \
		cp -rf $(EXPORT_DIR)/nuttx-export/arch $@/arch || true

# Mirror include path used by the rest of the toolchain layout (TARGET_INCLUDE_DIR_LIBC).
$(SYSROOT)/usr/include_libc: $(SYSROOT)/usr/include
	@mkdir -p $(dir $@)
	rm -rf $@
	ln -sf ../usr/include $@

# NuttX flat-build libs: libc, libmm, libsched, libdrivers, libboards, libarch,
# libfs, libbinfmt, libm, libnet (if CONFIG_NET), libnx (if CONFIG_NX), libxx
# (if CONFIG_HAVE_CXX), libapps. Copy whatever the export produced; the linker
# picks up only what it needs.
$(SYSROOT)/usr/lib: $(EXPORT_DIR)/.stamp-unpacked
	@mkdir -p $@
	cp -af $(EXPORT_DIR)/nuttx-export/libs/*.a $@/ 2>/dev/null || true
	# startup objects (crt0 / head) live under startup/ — keep them next to libs
	# so the image link can find them via -L<sysroot>/usr/lib.
	cp -af $(EXPORT_DIR)/nuttx-export/startup/* $@/ 2>/dev/null || true

# --- 3. Extract arch flags + feature CONFIG_* ------------------------------

# scripts/Make.defs in the export package records the exact CC/CXX/CROSSDEV,
# ARCHCPUFLAGS (-mcpu/-march/-mfpu/-mfloat-abi/-mabi), ARCHCFLAGS, ARCHCXXFLAGS,
# LDFLAGS the NuttX build used. We turn the bits the toolchain.cmake/target.mk
# needs into a small include file rather than re-deriving them.
#
# We are intentionally tolerant of missing fields: the export's Make.defs
# shape has historically drifted between NuttX releases; pin the version in
# target-nuttx/README.adoc and verify after upgrading.
$(ARCH_FLAGS_FILE): $(EXPORT_DIR)/.stamp-unpacked
	@echo "Extracting arch flags from $(EXPORT_DIR)/nuttx-export/scripts/Make.defs"
	@{ \
		echo '# Auto-generated by target-nuttx/import-export.mk'; \
		echo '# Do not edit; regenerate by rebuilding this target.'; \
		echo; \
		# `make export` writes Make.defs as `VAR <spaces> = value` (no `export' prefix, \
		# arbitrary whitespace around `="). extract() reads one field by name and strips \
		# everything up to and including the first `=' on the matched line. \
		defs="$(EXPORT_DIR)/nuttx-export/scripts/Make.defs"; \
		extract() { grep -m1 -E "^[[:space:]]*$$1[[:space:]]*[+:?]?=" $$defs | sed -E 's/^[^=]*=//; s/^[[:space:]]*//; s/^"(.*)"$$/\1/'; }; \
		echo "NUTTX_ARCH := $$(extract NUTTX_ARCH)"; \
		echo "NUTTX_ARCH_CHIP := $$(extract NUTTX_ARCH_CHIP)"; \
		echo "NUTTX_BOARD := $$(extract NUTTX_BOARD)"; \
		echo "NUTTX_BUILD := $$(extract NUTTX_BUILD)"; \
		echo "NUTTX_CROSSDEV := $$(extract CROSSDEV)"; \
		echo "NUTTX_CC := $$(extract CC)"; \
		echo "NUTTX_CXX := $$(extract CXX)"; \
		echo "NUTTX_ARCHCFLAGS := $$(extract ARCHCFLAGS)"; \
		echo "NUTTX_ARCHCPUFLAGS := $$(extract ARCHCPUFLAGS)"; \
		echo "NUTTX_ARCHCXXFLAGS := $$(extract ARCHCXXFLAGS)"; \
		echo "NUTTX_ARCHOPTIMIZATION := $$(extract ARCHOPTIMIZATION)"; \
		echo "NUTTX_LDFLAGS := $$(extract LDFLAGS)"; \
		echo "NUTTX_LDELFFLAGS := $$(extract LDELFFLAGS)"; \
		echo "NUTTX_LDLIBS := $$(extract LDLIBS)"; \
		echo "NUTTX_LLVM_ARCHTYPE := $$(extract LLVM_ARCHTYPE)"; \
		echo "NUTTX_LLVM_CPUTYPE := $$(extract LLVM_CPUTYPE)"; \
		echo "NUTTX_LLVM_ABITYPE := $$(extract LLVM_ABITYPE)"; \
	} > $@
	@cat $@

# Turn interesting CONFIG_* keys from the export's .config into make variables
# that init-target.mk / runtime can branch on (CONFIG_NET, CONFIG_LIBM, ...).
# This is the bridge that lets one export package describe its own capabilities.
$(CONFIG_FILE): $(EXPORT_DIR)/.stamp-unpacked
	@echo "Extracting CONFIG_* from $(EXPORT_DIR)/nuttx-export/.config"
	@{ \
		echo '# Auto-generated by target-nuttx/import-export.mk'; \
		echo '# Selected CONFIG_* values from the NuttX .config.'; \
		echo; \
		for sym in CONFIG_NET CONFIG_LIBM CONFIG_BUILD_FLAT CONFIG_BUILD_PROTECTED \
		           CONFIG_BUILD_KERNEL CONFIG_HAVE_CXX CONFIG_LIBCXX CONFIG_LIBCXXABI \
		           CONFIG_LIBCXXNONE CONFIG_LIBMINIABI CONFIG_CXX_EXCEPTION \
		           CONFIG_FS_MAP CONFIG_DISABLE_ENVIRON \
		           CONFIG_SCHED_THREAD_LOCAL CONFIG_TLS_ALIGNED \
		           CONFIG_TLS_NELEM CONFIG_TLS_TASK_NELEM CONFIG_TLS_NCLEANUP \
		           CONFIG_ARCH_ARM64 CONFIG_ARCH_TOOLCHAIN_CLANG \
		           CONFIG_ARCH_CHIP_BCM2711 CONFIG_ARCH_CHIP_RP2040 \
		           CONFIG_BCM2711_FRAMEBUFFER CONFIG_VIDEO_FB CONFIG_NX; do \
			val=$$(grep -m1 "^$${sym}=" $(EXPORT_DIR)/nuttx-export/.config | cut -d= -f2-); \
			[ -n "$$val" ] && echo "NUTTX_$${sym} := $$val" || true; \
		done; \
	} > $@
	@cat $@

# --- 4. Aggregate ----------------------------------------------------------

all: $(SYSROOT)/usr/include $(SYSROOT)/usr/include_libc $(SYSROOT)/usr/lib \
	$(ARCH_FLAGS_FILE) $(CONFIG_FILE)

.PHONY: all
