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

# open-sysroot.mk — mix Apple open-source (apple-oss-distributions) headers into
# a "+open" target sysroot so the Xenolith runtime can be built WITHOUT the
# proprietary Xcode macOS SDK.  Structurally analogous to the "+sprt" variant:
# it is a post-install augmentation step invoked by target-apple/Makefile for the
# "*-apple-macosx+open" pseudo-targets.
#
# Tags below are pinned to Apple's macOS 14.5 open-source manifest
# (distribution-macOS @ macos-145); SP_MACOS_VER in the parent Makefile is 14.5.

SHELL := /bin/sh

THIS_FILE := $(lastword $(MAKEFILE_LIST))
MK_DIR    := $(patsubst %/,%,$(dir $(THIS_FILE)))

APPLE_OSS_SRC ?= $(abspath $(MK_DIR)/../src/apple-oss)

# Destination sysroot: supplied by target-apple/Makefile's +open rule. Defaults
# here let this fragment be exercised standalone (make -f open-sysroot.mk).
T_TARGET ?= $(abspath $(MK_DIR)/../targets/x86_64-apple-macosx+open)

ifeq ($(SP_SYSNAME),iOS)
ifdef SP_IOSSIM
SP_XNU_PLATFORM ?= iPhoneSimulator
else
SP_XNU_PLATFORM ?= iPhoneOS
endif
else
SP_XNU_PLATFORM ?= MacOSX
endif

# SDK-like headers (apple-oss + baked overlay + libc++/libunwind from libcxx.mk) live
# in include_libc — same split as the Linux targets: usr/include is reserved for the
# third-party deps' own headers; consumers reach include_libc via -isystem (deps
# builds, toolchain.cmake) or TARGET_INCLUDE_DIR_LIBC/-idirafter (engine modules).
DST_INC  := $(T_TARGET)/include_libc
DST_LIB  := $(T_TARGET)/usr/lib
DST_FW   := $(T_TARGET)/System/Library/Frameworks
OSS_STAMP := $(T_TARGET)/share/apple-oss

OPEN_DIR    := $(abspath $(MK_DIR)/open)
RUNTIME_INC := $(abspath $(MK_DIR)/../../include)
# Baked sysroot overlay (git-tracked, copied verbatim into the target sysroot):
# everything hand-written or generated that mixes into usr/{include,lib} +
# System/Library/Frameworks — the .tbd link stubs, the closed-framework ObjC
# headers (Foundation / UniformTypeIdentifiers as real .framework/Headers bundles),
# the pseudo-system libm headers (math/complex/fenv/tgmath) and the baked MIG
# mach/* headers. Laid out exactly as it lands in the sysroot. Regenerate the
# .tbds via `make bake-stubs`; the MIG headers via gen-mig-headers.sh.
OPEN_SYSROOT := $(OPEN_DIR)/sysroot

OSS_BASE_URL := https://github.com/apple-oss-distributions

# --- pinned component tags (macOS 14.5 open-source manifest) -----------------
OSS_TAG_Libc                 := Libc-1592.100.35
OSS_TAG_libplatform          := libplatform-316.100.10
OSS_TAG_libpthread           := libpthread-519.120.4
OSS_TAG_libdispatch          := libdispatch-1477.100.9
OSS_TAG_libmalloc            := libmalloc-521.120.7
OSS_TAG_copyfile             := copyfile-221.121.1
OSS_TAG_Libinfo              := Libinfo-583.0.1
OSS_TAG_configd              := configd-1300.120.2
OSS_TAG_xnu                  := xnu-10063.121.3
OSS_TAG_dyld                 := dyld-1162
# cctools is NOT in the macOS OS manifest (it's an Xcode developer tool). cctools-1010.6
# ships with Xcode 15.4 (the macOS 14.5 SDK):
OSS_TAG_cctools              := cctools-1010.6
OSS_TAG_objc4                := objc4-912.3
OSS_TAG_CommonCrypto         := CommonCrypto-600028.100.1
OSS_TAG_libiconv             := libiconv-102
OSS_TAG_syslog               := syslog-406
OSS_TAG_libedit              := libedit-62
OSS_TAG_ncurses              := ncurses-71.100.2
OSS_TAG_libclosure           := libclosure-90
OSS_TAG_AvailabilityVersions := AvailabilityVersions-141
OSS_TAG_IOKitUser            := IOKitUser-100076.120.9
OSS_TAG_IOGraphics           := IOGraphics-598
OSS_TAG_ICU                  := ICU-74000.403
OSS_TAG_Security             := Security-61123.121.1
OSS_TAG_CarbonHeaders        := CarbonHeaders-18.1
OSS_SWIFT_CF_BRANCH          := release/6.2.2
OSS_TAG_CF                   := CF-1153.18

# --- source checkouts -------------------------------------------------------

$(APPLE_OSS_SRC)/%:
	@mkdir -p $(APPLE_OSS_SRC)
	rm -rf $@
	git clone --depth 1 --branch $(OSS_TAG_$*) $(OSS_BASE_URL)/$*.git $@

# xnu is enormous; fetch it blobless + sparse and check out only the header dirs
# we consume (an explicit rule overrides the generic pattern rule above).
$(APPLE_OSS_SRC)/xnu:
	@mkdir -p $(APPLE_OSS_SRC)
	rm -rf $@
	git clone --depth 1 --filter=blob:none --sparse --branch $(OSS_TAG_xnu) $(OSS_BASE_URL)/xnu.git $@
	cd $@ && git sparse-checkout set osfmk/mach osfmk/machine osfmk/i386 osfmk/arm osfmk/mach_debug osfmk/kern \
		osfmk/device bsd/sys bsd/machine bsd/i386 bsd/arm bsd/arm64 bsd/arpa bsd/net bsd/netinet bsd/netinet6 \
		bsd/libkern bsd/uuid bsd/bsm libkern/libkern libkern/os iokit/IOKit \
		libsyscall/wrappers libsyscall/os libsyscall/mach/mach EXTERNAL_HEADERS

# Security is a large repo; fetch it blobless + sparse and check out only the
# base/ headers we consume (SecRandom.h / SecBase.h for SecRandomCopyBytes).
$(APPLE_OSS_SRC)/Security:
	@mkdir -p $(APPLE_OSS_SRC)
	rm -rf $@
	git clone --depth 1 --filter=blob:none --sparse --branch $(OSS_TAG_Security) $(OSS_BASE_URL)/Security.git $@
	cd $@ && git sparse-checkout set base header_symlinks OSX/libsecurity_authorization/lib

$(APPLE_OSS_SRC)/cctools:
	@mkdir -p $(APPLE_OSS_SRC)
	rm -rf $@
	git clone --depth 1 --filter=blob:none --sparse --branch $(OSS_TAG_cctools) $(OSS_BASE_URL)/cctools.git $@
	cd $@ && git sparse-checkout set include

$(APPLE_OSS_SRC)/Libinfo:
	@mkdir -p $(APPLE_OSS_SRC)
	rm -rf $@
	git clone --depth 1 --filter=blob:none --sparse --branch $(OSS_TAG_Libinfo) $(OSS_BASE_URL)/Libinfo.git $@
	cd $@ && git sparse-checkout set lookup.subproj gen.subproj

$(APPLE_OSS_SRC)/configd:
	@mkdir -p $(APPLE_OSS_SRC)
	rm -rf $@
	git clone --depth 1 --filter=blob:none --sparse --branch $(OSS_TAG_configd) $(OSS_BASE_URL)/configd.git $@
	cd $@ && git sparse-checkout set SystemConfiguration.fproj

# ICU is large; fetch it blobless + sparse and check out only the public unicode/
# header trees. Pinned to the target-SDK ICU (icucore); the runtime + deps include
# <unicode/*.h> directly.
$(APPLE_OSS_SRC)/ICU:
	@mkdir -p $(APPLE_OSS_SRC)
	rm -rf $@
	git clone --depth 1 --filter=blob:none --sparse --branch $(OSS_TAG_ICU) $(OSS_BASE_URL)/ICU.git $@
	cd $@ && git sparse-checkout set icuSources/common/unicode icuSources/i18n/unicode icuSources/io/unicode

# CoreFoundation source: swift-corelibs-foundation (swiftlang, not apple-oss), the
# maintained CF with modern macros. Blobless + sparse to just the CF headers.
$(APPLE_OSS_SRC)/swift-foundation:
	@mkdir -p $(APPLE_OSS_SRC)
	rm -rf $@
	git clone --depth 1 --filter=blob:none --sparse --branch $(OSS_SWIFT_CF_BRANCH) https://github.com/swiftlang/swift-corelibs-foundation.git $@
	cd $@ && git sparse-checkout set Sources/CoreFoundation

# IOKitUser: the userspace <IOKit/*> headers (IOKitLib.h + its IOTypes/IOKitKeys/
# OSMessageNotification deps at the repo root, plus the graphics.subproj headers).
# blobless + sparse — cone mode keeps root files and the graphics subproject.
$(APPLE_OSS_SRC)/IOKitUser:
	@mkdir -p $(APPLE_OSS_SRC)
	rm -rf $@
	git clone --depth 1 --filter=blob:none --sparse --branch $(OSS_TAG_IOKitUser) $(OSS_BASE_URL)/IOKitUser.git $@
	cd $@ && git sparse-checkout set graphics.subproj

# IOGraphics: source of <IOKit/graphics/IOGraphicsTypes.h> (the display-mode / pixel
# format constants CGDisplayConfiguration + the window backend reference); not in
# IOKitUser. blobless + sparse to just the public IOGraphics headers.
$(APPLE_OSS_SRC)/IOGraphics:
	@mkdir -p $(APPLE_OSS_SRC)
	rm -rf $@
	git clone --depth 1 --filter=blob:none --sparse --branch $(OSS_TAG_IOGraphics) $(OSS_BASE_URL)/IOGraphics.git $@
	cd $@ && git sparse-checkout set IOGraphicsFamily/IOKit/graphics

$(OSS_STAMP):
	@mkdir -p $@

# --- header extraction (one stamp per component) ----------------------------
# Each rule copies only the public header subtree(s) that component owns, mixing
# them into the already-populated $(T_TARGET)/include_libc. Layouts were verified
# against the pinned tags above.

$(OSS_STAMP)/libplatform: | $(APPLE_OSS_SRC)/libplatform $(OSS_STAMP)/xnu $(OSS_STAMP)
	@mkdir -p $(DST_INC)/os $(DST_INC)/libkern
	cp -Rf $(APPLE_OSS_SRC)/libplatform/include/os/.      $(DST_INC)/os/
	cp -Rf $(APPLE_OSS_SRC)/libplatform/include/libkern/. $(DST_INC)/libkern/
	cp -f $(APPLE_OSS_SRC)/libplatform/include/setjmp.h   $(DST_INC)/
	cp -f $(APPLE_OSS_SRC)/libplatform/include/ucontext.h $(DST_INC)/
	@touch $@

# libclosure: the Blocks runtime header (CFRunLoopPerformBlock uses ^{} blocks)
$(OSS_STAMP)/libclosure: | $(APPLE_OSS_SRC)/libclosure $(OSS_STAMP)
	@mkdir -p $(DST_INC)
	cp -f $(APPLE_OSS_SRC)/libclosure/Block.h         $(DST_INC)/Block.h
	cp -f $(APPLE_OSS_SRC)/libclosure/Block_private.h $(DST_INC)/Block_private.h
	@touch $@

$(OSS_STAMP)/CommonCrypto: | $(APPLE_OSS_SRC)/CommonCrypto $(OSS_STAMP)
	@mkdir -p $(DST_INC)/CommonCrypto
	cp -f $(APPLE_OSS_SRC)/CommonCrypto/include/*.h $(DST_INC)/CommonCrypto/
	@touch $@

# libdispatch: dispatch/*  (CoreFoundation headers include <dispatch/dispatch.h>)
$(OSS_STAMP)/libdispatch: | $(APPLE_OSS_SRC)/libdispatch $(OSS_STAMP)
	@mkdir -p $(DST_INC)/dispatch $(DST_INC)/os
	cp -Rf $(APPLE_OSS_SRC)/libdispatch/dispatch/. $(DST_INC)/dispatch/
	-cp -f $(APPLE_OSS_SRC)/libdispatch/os/*.h $(DST_INC)/os/ 2>/dev/null || true
	@touch $@

# NOTE: MIG-generated mach headers (mach/mach_port.h, mach/task.h, ...) are NOT
# produced by a raw copy; only the checked-in headers are mixed in here.
$(OSS_STAMP)/xnu: | $(APPLE_OSS_SRC)/xnu $(OSS_STAMP)
	@mkdir -p $(DST_INC)/mach $(DST_INC)/sys $(DST_INC)/mach-o \
		$(DST_INC)/mach/machine $(DST_INC)/mach/i386 $(DST_INC)/mach/arm $(DST_INC)/mach/arm64
	cp -f $(APPLE_OSS_SRC)/xnu/osfmk/mach/*.h        $(DST_INC)/mach/
	cp -f $(APPLE_OSS_SRC)/xnu/osfmk/mach/machine/*.h $(DST_INC)/mach/machine/
	@# <mach/machine/sdt.h>: the SDK ships an EMPTY guard-only stub (userspace DTrace
	@# probes come from <sys/sdt.h>); xnu's copy is the kernel one whose userspace
	@# branch defines no-op DTRACE_PROBE* macros that would clash with <sys/sdt.h>.
	@printf '#ifndef _MACH_MACHINE_SYS_SDT_H\n#define _MACH_MACHINE_SYS_SDT_H\n#endif /* _MACH_MACHINE_SYS_SDT_H */\n' > $(DST_INC)/mach/machine/sdt.h
	cp -f $(APPLE_OSS_SRC)/xnu/osfmk/mach/i386/*.h    $(DST_INC)/mach/i386/
	@mkdir -p $(DST_INC)/i386 $(DST_INC)/arm $(DST_INC)/machine
	cp -f $(APPLE_OSS_SRC)/xnu/osfmk/i386/eflags.h   $(DST_INC)/i386/
	cp -f $(APPLE_OSS_SRC)/xnu/osfmk/machine/cpu_capabilities.h $(DST_INC)/machine/
	cp -f $(APPLE_OSS_SRC)/xnu/osfmk/i386/cpu_capabilities.h    $(DST_INC)/i386/
	-cp -f $(APPLE_OSS_SRC)/xnu/osfmk/arm/*.h                   $(DST_INC)/arm/ 2>/dev/null || true
	-cp -f $(APPLE_OSS_SRC)/xnu/osfmk/mach/arm/*.h    $(DST_INC)/mach/arm/ 2>/dev/null || true
	-cp -f $(APPLE_OSS_SRC)/xnu/osfmk/mach/arm64/*.h  $(DST_INC)/mach/arm64/ 2>/dev/null || true
	@for h in mach/i386/_structs.h mach/arm/_structs.h; do \
		f=$(DST_INC)/$$h; \
		[ -e "$$f" ] && ! grep -q '<stdint.h>' "$$f" && \
			sed -i 's|#include <machine/types.h>|#include <machine/types.h>\n#include <stdint.h>|' "$$f" || true; \
	done
	cp -f $(APPLE_OSS_SRC)/xnu/libsyscall/mach/mach/vm_page_size.h $(DST_INC)/mach/
	-cp -f $(APPLE_OSS_SRC)/xnu/libsyscall/mach/mach/mach_init.h  $(DST_INC)/mach/ 2>/dev/null || true
	-cp -f $(APPLE_OSS_SRC)/xnu/libsyscall/mach/mach/mach_error.h $(DST_INC)/mach/ 2>/dev/null || true
	-cp -f $(APPLE_OSS_SRC)/xnu/libsyscall/mach/mach/sync.h         $(DST_INC)/mach/ 2>/dev/null || true
	-cp -f $(APPLE_OSS_SRC)/xnu/libsyscall/mach/mach/thread_state.h $(DST_INC)/mach/ 2>/dev/null || true
	-cp -f $(APPLE_OSS_SRC)/xnu/libsyscall/mach/mach/port_obj.h     $(DST_INC)/mach/ 2>/dev/null || true
	-cp -f $(APPLE_OSS_SRC)/xnu/libsyscall/mach/mach/vm_task.h      $(DST_INC)/mach/ 2>/dev/null || true
	-cp -f $(APPLE_OSS_SRC)/xnu/libsyscall/mach/mach/mach_right.h   $(DST_INC)/mach/ 2>/dev/null || true
	cp -f $(APPLE_OSS_SRC)/xnu/libsyscall/mach/mach/mach.h           $(DST_INC)/mach/
	cp -f $(APPLE_OSS_SRC)/xnu/libsyscall/mach/mach/mach_interface.h $(DST_INC)/mach/
	cp -f $(APPLE_OSS_SRC)/xnu/bsd/sys/sysctl.h      $(DST_INC)/sys/
	cp -Rf $(APPLE_OSS_SRC)/xnu/EXTERNAL_HEADERS/mach-o/. $(DST_INC)/mach-o/
	@mkdir -p $(DST_INC)/architecture
	cp -Rf $(APPLE_OSS_SRC)/xnu/EXTERNAL_HEADERS/architecture/. $(DST_INC)/architecture/
	@mkdir -p $(DST_INC)/libkern/i386 $(DST_INC)/libkern/arm $(DST_INC)/libkern/machine
	cp -f $(APPLE_OSS_SRC)/xnu/libkern/libkern/*.h          $(DST_INC)/libkern/
	-cp -f $(APPLE_OSS_SRC)/xnu/libkern/libkern/i386/*.h    $(DST_INC)/libkern/i386/ 2>/dev/null || true
	-cp -f $(APPLE_OSS_SRC)/xnu/libkern/libkern/arm/*.h     $(DST_INC)/libkern/arm/ 2>/dev/null || true
	-cp -f $(APPLE_OSS_SRC)/xnu/libkern/libkern/machine/*.h $(DST_INC)/libkern/machine/ 2>/dev/null || true
	@mkdir -p $(DST_INC)/os
	cp -f $(APPLE_OSS_SRC)/xnu/EXTERNAL_HEADERS/image4/shim/base.h $(DST_INC)/os/base.h
	cp -f $(APPLE_OSS_SRC)/xnu/libkern/os/overflow.h $(DST_INC)/os/
	cp -f $(APPLE_OSS_SRC)/xnu/libkern/os/atomic.h   $(DST_INC)/os/
	cp -f $(APPLE_OSS_SRC)/xnu/libkern/os/log.h      $(DST_INC)/os/
	-cp -f $(APPLE_OSS_SRC)/xnu/libsyscall/os/proc.h $(DST_INC)/os/ 2>/dev/null || true
	cp -f $(APPLE_OSS_SRC)/xnu/libsyscall/wrappers/gethostuuid.h $(DST_INC)/
	cp -f $(APPLE_OSS_SRC)/xnu/libsyscall/wrappers/spawn/spawn.h $(DST_INC)/
	cp -f $(APPLE_OSS_SRC)/xnu/libsyscall/wrappers/libproc/libproc.h $(DST_INC)/
	cp -f $(APPLE_OSS_SRC)/xnu/libsyscall/os/tsd.h $(DST_INC)/os/
	@mkdir -p $(DST_INC)/bsm $(DST_INC)/mach_debug $(DST_INC)/kern
	cp -f $(APPLE_OSS_SRC)/xnu/bsd/bsm/*.h          $(DST_INC)/bsm/
	cp -f $(APPLE_OSS_SRC)/xnu/osfmk/mach_debug/*.h $(DST_INC)/mach_debug/
	cp -f $(APPLE_OSS_SRC)/xnu/osfmk/kern/exc_resource.h \
		$(APPLE_OSS_SRC)/xnu/osfmk/kern/exc_guard.h $(DST_INC)/kern/
	@touch $@

# dyld: mach-o/dyld.h + dyld_priv.h (_dyld_get_image_name etc.). Copied AFTER xnu
# so it augments the same mach-o/ directory rather than being overwritten.
$(OSS_STAMP)/dyld: $(OSS_STAMP)/xnu | $(APPLE_OSS_SRC)/dyld $(OSS_STAMP)
	@mkdir -p $(DST_INC)/mach-o
	cp -f $(APPLE_OSS_SRC)/dyld/include/mach-o/dyld.h      $(DST_INC)/mach-o/
	cp -f $(APPLE_OSS_SRC)/dyld/include/mach-o/dyld_priv.h $(DST_INC)/mach-o/
	@# dyld-1160.6 dyld.h USES DYLD_EXCLAVEKIT_UNAVAILABLE (a newer availability
	@# annotation) but never defines it — only DYLD_DRIVERKIT_UNAVAILABLE. Inject an
	@# empty guarded fallback.
	sed -i 's/^#ifdef __DRIVERKIT_19_0$$/#ifndef DYLD_EXCLAVEKIT_UNAVAILABLE\n#define DYLD_EXCLAVEKIT_UNAVAILABLE\n#endif\n#ifdef __DRIVERKIT_19_0/' $(DST_INC)/mach-o/dyld.h
	cp -f $(APPLE_OSS_SRC)/dyld/include/dlfcn.h           $(DST_INC)/
	@touch $@

# objc4: curated public runtime headers (the repo mixes public + private in
# runtime/; mirror the set Apple's SDK actually ships as <objc/*>).
OBJC_PUBLIC_HDRS := \
	objc.h runtime.h message.h objc-api.h objc-abi.h objc-auto.h \
	objc-exception.h objc-sync.h objc-runtime.h objc-internal.h \
	NSObject.h NSObjCRuntime.h Object.h Protocol.h \
	hashtable.h hashtable2.h maptable.h objc-load.h
$(OSS_STAMP)/objc4: | $(APPLE_OSS_SRC)/objc4 $(OSS_STAMP)
	@mkdir -p $(DST_INC)/objc
	for h in $(OBJC_PUBLIC_HDRS); do \
		cp -f $(APPLE_OSS_SRC)/objc4/runtime/$$h $(DST_INC)/objc/ ; \
	done
	@touch $@

$(OSS_STAMP)/Carbon: | $(APPLE_OSS_SRC)/CarbonHeaders $(OSS_STAMP)
	@mkdir -p $(DST_INC)
	cp -f $(APPLE_OSS_SRC)/CarbonHeaders/MacTypes.h         $(DST_INC)/
	cp -f $(APPLE_OSS_SRC)/CarbonHeaders/ConditionalMacros.h $(DST_INC)/
	cp -f $(APPLE_OSS_SRC)/CarbonHeaders/MacErrors.h        $(DST_INC)/
	cp -f $(APPLE_OSS_SRC)/CarbonHeaders/Endian.h          $(DST_INC)/
	cp -f $(APPLE_OSS_SRC)/CarbonHeaders/AssertMacros.h    $(DST_INC)/
	@touch $@

# CoreFoundation from swift-corelibs-foundation. Depends on libdispatch
# (<dispatch/dispatch.h>) and Carbon (<MacTypes.h>).
$(OSS_STAMP)/CF: $(OSS_STAMP)/libdispatch $(OSS_STAMP)/Carbon | $(APPLE_OSS_SRC)/swift-foundation $(OSS_STAMP)
	@mkdir -p $(DST_INC)/CoreFoundation
	cp -f $(APPLE_OSS_SRC)/swift-foundation/Sources/CoreFoundation/include/*.h $(DST_INC)/CoreFoundation/
	sed -i 's/^#define DEPLOYMENT_RUNTIME_SWIFT 1/#define DEPLOYMENT_RUNTIME_SWIFT 0/' $(DST_INC)/CoreFoundation/CFAvailability.h
	@touch $@

$(OSS_STAMP)/Security: $(OSS_STAMP)/CF | $(APPLE_OSS_SRC)/Security $(OSS_STAMP)
	@mkdir -p $(DST_INC)/Security
	cp -f $(APPLE_OSS_SRC)/Security/base/*.h $(DST_INC)/Security/
	@# Authorization API (lldb Host.mm's root-launch path). Open source, but the
	@# header_symlinks entries point into OSX/libsecurity_authorization/lib —
	@# included in the sparse checkout above.
	cp -f $(APPLE_OSS_SRC)/Security/OSX/libsecurity_authorization/lib/Authorization.h \
		$(APPLE_OSS_SRC)/Security/OSX/libsecurity_authorization/lib/AuthorizationTags.h \
		$(APPLE_OSS_SRC)/Security/OSX/libsecurity_authorization/lib/AuthSession.h \
		$(APPLE_OSS_SRC)/Security/OSX/libsecurity_authorization/lib/AuthorizationDB.h \
		$(DST_INC)/Security/
	@touch $@

$(OSS_STAMP)/ICU: | $(APPLE_OSS_SRC)/ICU $(OSS_STAMP)
	@mkdir -p $(DST_INC)/unicode
	cp -f $(APPLE_OSS_SRC)/ICU/icuSources/common/unicode/*.h $(DST_INC)/unicode/
	cp -f $(APPLE_OSS_SRC)/ICU/icuSources/i18n/unicode/*.h   $(DST_INC)/unicode/
	cp -f $(APPLE_OSS_SRC)/ICU/icuSources/io/unicode/*.h     $(DST_INC)/unicode/
	@touch $@

$(OSS_STAMP)/AvailabilityVersions: | $(APPLE_OSS_SRC)/AvailabilityVersions $(OSS_STAMP)
	@mkdir -p $(DST_INC)/os
	@# The Availability*.h files are TEMPLATES carrying @@AVAILABILITY_MACRO_INTERFACE@@
	@# markers; raw copies leave __API_AVAILABLE/__API_UNAVAILABLE/... UNDEFINED (and
	@# then <os/availability.h> + framework headers break).
	cd $(APPLE_OSS_SRC)/AvailabilityVersions && for t in templates/*.h; do \
		b=`basename $$t`; \
		case $$b in \
			os_availability.h) ./availability --preprocess $$t $(DST_INC)/os/availability.h ;; \
			*)                 ./availability --preprocess $$t $(DST_INC)/$$b ;; \
		esac ; \
	done
	@# The generated <Availability.h> does not pull <os/availability.h> (where the
	@# API_AVAILABLE / API_UNAVAILABLE / SPI_AVAILABLE family lives). Framework
	@# headers (SecBase.h, ...) include only <Availability.h>, so surface it there.
	@# os/availability.h has its own guard and pulls <AvailabilityInternal.h>.
	echo '#include <os/availability.h>' >> $(DST_INC)/Availability.h
	@# _symbol_aliasing.h / _posix_availability.h are Libc-build-generated (not part
	@# of AvailabilityVersions); synthesize them from the version list.
	$(MK_DIR)/gen-availability-shims.sh $(APPLE_OSS_SRC)/AvailabilityVersions/availability.dsl $(DST_INC)
	@touch $@

# Real C/POSIX libc + system headers that the runtime sources compile against:
$(OSS_STAMP)/libc: $(OSS_STAMP)/xnu | $(APPLE_OSS_SRC)/Libc $(APPLE_OSS_SRC)/libpthread $(OSS_STAMP)
	@mkdir -p $(DST_INC)/sys $(DST_INC)/machine $(DST_INC)/i386 $(DST_INC)/arpa $(DST_INC)/net \
		$(DST_INC)/netinet $(DST_INC)/netinet6 $(DST_INC)/libkern $(DST_INC)/uuid $(DST_INC)/pthread
	cp -Rf $(APPLE_OSS_SRC)/Libc/include/.               $(DST_INC)/
	-cp -f $(APPLE_OSS_SRC)/Libc/include/FreeBSD/*.h     $(DST_INC)/ 2>/dev/null || true
	-cp -f $(APPLE_OSS_SRC)/Libc/include/NetBSD/*.h      $(DST_INC)/ 2>/dev/null || true
	-cp -f $(APPLE_OSS_SRC)/Libc/gen/execinfo.h   $(DST_INC)/ 2>/dev/null || true
	-cp -f $(APPLE_OSS_SRC)/Libc/gen/get_compat.h $(DST_INC)/ 2>/dev/null || true
	-cp -f $(APPLE_OSS_SRC)/Libc/locale/xlocale_private.h $(DST_INC)/ 2>/dev/null || true
	@for h in `find $(APPLE_OSS_SRC)/Libc -path '*/FreeBSD/*.h' ! -name private.h ! -name tzfile.h 2>/dev/null`; do cp -f $$h $(DST_INC)/ 2>/dev/null || true; done
	cp -Rf $(APPLE_OSS_SRC)/xnu/bsd/sys/.                 $(DST_INC)/sys/
	@# bake the platform macro right after the include guard (SDK parity — see
	@# the SP_XNU_PLATFORM block at the top of this file)
	sed -i 's/^#define _CDEFS_H_$$/&\n\n#ifndef XNU_PLATFORM_$(SP_XNU_PLATFORM)\n#define XNU_PLATFORM_$(SP_XNU_PLATFORM) 1\n#endif/' $(DST_INC)/sys/cdefs.h
	grep -q 'XNU_PLATFORM_$(SP_XNU_PLATFORM) 1' $(DST_INC)/sys/cdefs.h
	-cp -Rf $(APPLE_OSS_SRC)/xnu/bsd/machine/.           $(DST_INC)/machine/ 2>/dev/null || true
	-cp -Rf $(APPLE_OSS_SRC)/xnu/bsd/i386/.              $(DST_INC)/i386/ 2>/dev/null || true
	-cp -Rf $(APPLE_OSS_SRC)/xnu/bsd/arm/.               $(DST_INC)/arm/ 2>/dev/null || true
	-cp -Rf $(APPLE_OSS_SRC)/xnu/bsd/arm64/.             $(DST_INC)/arm64/ 2>/dev/null || true
	-cp -Rf $(APPLE_OSS_SRC)/xnu/bsd/arpa/.              $(DST_INC)/arpa/ 2>/dev/null || true
	-cp -Rf $(APPLE_OSS_SRC)/xnu/bsd/net/.               $(DST_INC)/net/ 2>/dev/null || true
	-cp -Rf $(APPLE_OSS_SRC)/xnu/bsd/netinet/.           $(DST_INC)/netinet/ 2>/dev/null || true
	-cp -Rf $(APPLE_OSS_SRC)/xnu/bsd/netinet6/.          $(DST_INC)/netinet6/ 2>/dev/null || true
	-cp -Rf $(APPLE_OSS_SRC)/xnu/bsd/libkern/.           $(DST_INC)/libkern/ 2>/dev/null || true
	-cp -Rf $(APPLE_OSS_SRC)/xnu/bsd/uuid/.              $(DST_INC)/uuid/ 2>/dev/null || true
	cp -Rf $(APPLE_OSS_SRC)/libpthread/include/pthread/. $(DST_INC)/pthread/
	-cp -f $(APPLE_OSS_SRC)/libpthread/private/pthread/*_private.h $(DST_INC)/pthread/ 2>/dev/null || true
	-cp -Rf $(APPLE_OSS_SRC)/libpthread/include/sys/.    $(DST_INC)/sys/ 2>/dev/null || true
	cp -f $(APPLE_OSS_SRC)/libpthread/include/pthread/pthread.h $(DST_INC)/pthread.h
	cp -f $(APPLE_OSS_SRC)/libpthread/include/pthread/sched.h   $(DST_INC)/sched.h
	@# apple-oss Libc headers carry `//Begin-Libc ... //End-Libc` blocks marking
	@# LIBC-BUILD-INTERNAL code that Apple's SDK build strips. Notably _ctype.h pulls
	@# the internal "xlocale_private.h" there, which #includes <xlocale.h> with
	@# __DARWIN_XLOCALE_PRIVATE set — that poisons the <xlocale.h> include guard so
	@# the public xlocale/_*.h (strcasecmp_l, ...) never load. Strip those blocks to
	@# match the public SDK header form.
	@for f in `grep -rl '//Begin-Libc' $(DST_INC) 2>/dev/null`; do \
		sed -i '\#//Begin-Libc#,\#//End-Libc#d' "$$f" ; \
	done
	@touch $@

# Libm umbrella subunits: macOS ships no open-source <math.h>/<complex.h>/<fenv.h>/
# <tgmath.h> (Apple's Libm is frozen at 2011), so the C99 surface is provided by
# the pseudo-system headers baked in the open/sysroot overlay (installed by the
# `stubs` stamp). Their prototypes come from the runtime's OWN umbrella subunits
# (sprt/wrappers/libc/*_impl.h — the single source of truth shared with the SPRT
# wrapper); those are NOT baked (they live in the runtime tree), so this stamp
# copies them straight into include_libc beside the baked <math.h> et al. (NOT under
# a sprt/ subtree), and the base headers include them with a plain quoted
# "math_impl.h" — so the sysroot's <math.h>/<complex.h>/<fenv.h> are fully
# self-contained
$(OSS_STAMP)/libm: $(OSS_STAMP)/libc | $(OSS_STAMP)
	@# drop any stale sprt/ copy from a pre-self-contained sysroot build.
	rm -rf $(DST_INC)/sprt
	cp -f $(RUNTIME_INC)/sprt/wrappers/libc/math_impl.h    $(DST_INC)/
	cp -f $(RUNTIME_INC)/sprt/wrappers/libc/complex_impl.h $(DST_INC)/
	cp -f $(RUNTIME_INC)/sprt/wrappers/libc/fenv_impl.h    $(DST_INC)/
	@touch $@

# libmalloc: userspace <malloc/malloc.h> + <malloc/_malloc.h> (the latter is
# pulled by <stdlib.h> for the malloc/calloc/realloc/free prototypes).
$(OSS_STAMP)/libmalloc: | $(APPLE_OSS_SRC)/libmalloc $(OSS_STAMP)
	@mkdir -p $(DST_INC)/malloc
	cp -f $(APPLE_OSS_SRC)/libmalloc/include/malloc/*.h $(DST_INC)/malloc/
	@touch $@

# copyfile: <copyfile.h> — pulled by libc++'s std::filesystem::copy_file (on Apple it
# uses fcopyfile()). The matching _fcopyfile/_copyfile_state_* symbols are carried into
# libSystem.tbd via functions_<arch>.txt.
$(OSS_STAMP)/copyfile: | $(APPLE_OSS_SRC)/copyfile $(OSS_STAMP)
	cp -f $(APPLE_OSS_SRC)/copyfile/copyfile.h $(DST_INC)/
	@touch $@

# Libinfo: <netdb.h> (getaddrinfo/gethostbyname — pulled by mbedtls net_sockets.c, curl,
# and any socket resolver code), <ifaddrs.h> (getifaddrs), + <grp.h>/<pwd.h> (user/group
# db). All byte-identical to the SDK. (The niche DB headers — aliasdb/bootparams/printerdb
# — are left out.)
$(OSS_STAMP)/Libinfo: | $(APPLE_OSS_SRC)/Libinfo $(OSS_STAMP)
	cp -f $(APPLE_OSS_SRC)/Libinfo/lookup.subproj/netdb.h $(DST_INC)/
	cp -f $(APPLE_OSS_SRC)/Libinfo/lookup.subproj/grp.h   $(DST_INC)/
	cp -f $(APPLE_OSS_SRC)/Libinfo/lookup.subproj/pwd.h   $(DST_INC)/
	cp -f $(APPLE_OSS_SRC)/Libinfo/gen.subproj/ifaddrs.h  $(DST_INC)/
	@touch $@

# configd: SystemConfiguration.framework Headers — the minimal SCDynamicStore chain curl's
# macos.c pulls (SCDynamicStoreCopyProxies for system proxy settings). Installed beside the
# baked SystemConfiguration.tbd; the configd source carries some SPI the SDK strips, but the
# public SCDynamicStore* surface curl uses compiles cleanly (deps: CoreFoundation, present).
$(OSS_STAMP)/configd: | $(APPLE_OSS_SRC)/configd $(OSS_STAMP)
	@mkdir -p $(DST_FW)/SystemConfiguration.framework/Headers
	cp -f $(APPLE_OSS_SRC)/configd/SystemConfiguration.fproj/SCDynamicStoreCopySpecific.h $(DST_FW)/SystemConfiguration.framework/Headers/
	cp -f $(APPLE_OSS_SRC)/configd/SystemConfiguration.fproj/SCDynamicStore.h             $(DST_FW)/SystemConfiguration.framework/Headers/
	cp -f $(APPLE_OSS_SRC)/configd/SystemConfiguration.fproj/SCDynamicStoreKey.h          $(DST_FW)/SystemConfiguration.framework/Headers/
	cp -f $(APPLE_OSS_SRC)/configd/SystemConfiguration.fproj/SystemConfiguration.h        $(DST_FW)/SystemConfiguration.framework/Headers/
	@touch $@

# libiconv: <iconv.h> — the public iconv_open/iconv/iconv_close charset-conversion API,
# pulled by libxml2's encoding.c (-DLIBXML2_WITH_ICONV).
$(OSS_STAMP)/libiconv: | $(APPLE_OSS_SRC)/libiconv $(OSS_STAMP)
	cp -f $(APPLE_OSS_SRC)/libiconv/citrus/iconv.h $(DST_INC)/iconv.h
	@touch $@

# syslog: <asl.h> — the Apple System Log client API (used by compiler-rt's
# sanitizers). libsystem_asl ships it under libsystem_asl.tproj/include;
# byte-identical to the SDK copy. Only the public asl.h is installed.
$(OSS_STAMP)/syslog: | $(APPLE_OSS_SRC)/syslog $(OSS_STAMP)
	cp -f $(APPLE_OSS_SRC)/syslog/libsystem_asl.tproj/include/asl.h $(DST_INC)/asl.h
	@touch $@

# libedit: <histedit.h> (lldb line editing) — byte-identical to the SDK copy.
$(OSS_STAMP)/libedit: | $(APPLE_OSS_SRC)/libedit $(OSS_STAMP)
	cp -f $(APPLE_OSS_SRC)/libedit/src/histedit.h $(DST_INC)/
	@touch $@

# ncurses: the terminal headers (lldb's curses GUI + terminfo).
NCURSES_INC := $(APPLE_OSS_SRC)/ncurses/ncurses/include
$(OSS_STAMP)/ncurses: | $(APPLE_OSS_SRC)/ncurses $(OSS_STAMP)
	{ cat $(NCURSES_INC)/curses.head; \
	  AWK=awk sh $(NCURSES_INC)/MKkey_defs.sh $(NCURSES_INC)/Caps; \
	  cat $(NCURSES_INC)/curses.wide $(NCURSES_INC)/curses.tail; } > $(DST_INC)/curses.h
	AWK=awk awk -f $(NCURSES_INC)/MKterm.h.awk $(NCURSES_INC)/Caps > $(DST_INC)/term.h
	sh $(NCURSES_INC)/edit_cfg.sh $(NCURSES_INC)/ncurses_cfg.h $(DST_INC)/term.h
	cp -f $(NCURSES_INC)/tic.h $(NCURSES_INC)/ncurses_dll.h $(NCURSES_INC)/unctrl.h \
		$(NCURSES_INC)/nc_tparm.h $(NCURSES_INC)/termcap.h $(NCURSES_INC)/term_entry.h \
		$(APPLE_OSS_SRC)/ncurses/ncurses/panel/panel.h \
		$(APPLE_OSS_SRC)/ncurses/ncurses/form/form.h \
		$(APPLE_OSS_SRC)/ncurses/ncurses/menu/menu.h \
		$(APPLE_OSS_SRC)/ncurses/ncurses/menu/eti.h \
		$(DST_INC)/
	ln -sf curses.h $(DST_INC)/ncurses.h
	@touch $@

# cctools is the SDK-canonical source for the <mach-o/*> format headers. Two groups:
#   A) headers ONLY cctools has (not xnu/dyld/libunwind), so the SDK's copy comes from
#      cctools: arch.h (cpu-name tables), getsect.h (section lookup), ldsyms.h (pulled by
#      CSU's icplusplus.c), ranlib.h (ar table-of-contents), swap.h (byte-swapping).
#   B) headers xnu ALSO ships in EXTERNAL_HEADERS/mach-o, but whose xnu copy DIVERGES from
#      the SDK while cctools' is byte-identical:  fat.h, nlist.h, stab.h,
#      loader.h, arm64/reloc.h. This stamp is ORDERED AFTER xnu (order-only prereq) so its
#      copies win. (reloc.h, arm/reloc.h, x86_64/reloc.h, fixup-chains.h already match the
#      SDK from xnu, so they stay xnu-sourced.)
$(OSS_STAMP)/cctools: | $(APPLE_OSS_SRC)/cctools $(OSS_STAMP)/xnu $(OSS_STAMP)
	@mkdir -p $(DST_INC)/mach-o/arm64
	@# A) cctools-only mach-o headers
	cp -f $(APPLE_OSS_SRC)/cctools/include/mach-o/arch.h    $(DST_INC)/mach-o/
	cp -f $(APPLE_OSS_SRC)/cctools/include/mach-o/getsect.h $(DST_INC)/mach-o/
	cp -f $(APPLE_OSS_SRC)/cctools/include/mach-o/ldsyms.h  $(DST_INC)/mach-o/
	cp -f $(APPLE_OSS_SRC)/cctools/include/mach-o/ranlib.h  $(DST_INC)/mach-o/
	cp -f $(APPLE_OSS_SRC)/cctools/include/mach-o/swap.h    $(DST_INC)/mach-o/
	@# B) closer-to-SDK than xnu's — overwrite the xnu-staged copies
	cp -f $(APPLE_OSS_SRC)/cctools/include/mach-o/fat.h        $(DST_INC)/mach-o/
	cp -f $(APPLE_OSS_SRC)/cctools/include/mach-o/nlist.h      $(DST_INC)/mach-o/
	cp -f $(APPLE_OSS_SRC)/cctools/include/mach-o/stab.h       $(DST_INC)/mach-o/
	cp -f $(APPLE_OSS_SRC)/cctools/include/mach-o/loader.h     $(DST_INC)/mach-o/
	cp -f $(APPLE_OSS_SRC)/cctools/include/mach-o/arm64/reloc.h $(DST_INC)/mach-o/arm64/
	@touch $@

# IOKit userspace headers (<IOKit/IOKitLib.h> + <IOKit/graphics/*>): the window
# backend's display/framebuffer probing (CGDisplayConfiguration pairs with
# IOGraphicsTypes flags). IOKitUser ships the root <IOKit/*> C headers + the
# graphics.subproj lib headers; IOGraphics ships <IOKit/graphics/IOGraphicsTypes.h>
# (& siblings). CoreFoundation/mach/dispatch deps are already staged above.
# The <IOKit/*> headers live in the IOKit.framework bundle (beside IOKit.tbd, the
# baked overlay stub) — a proper framework, resolved via -F/-isysroot framework
# search. (The hand-written <IOKit/hidsystem/IOLLEvent.h> ships in the overlay's
# IOKit.framework/Headers too.) The non-IOKit deps IOKit/IOTypes.h pulls —
# <device/device_types.h> + <mach/*> — stay under include_libc (they are not
# <IOKit/*> and resolve via the normal include path).
$(OSS_STAMP)/IOKit: | $(APPLE_OSS_SRC)/IOKitUser $(APPLE_OSS_SRC)/IOGraphics $(APPLE_OSS_SRC)/xnu $(OSS_STAMP)
	rm -rf $(DST_INC)/IOKit
	@mkdir -p $(DST_FW)/IOKit.framework/Headers/graphics
	cp -f $(APPLE_OSS_SRC)/xnu/iokit/IOKit/*.h                  $(DST_FW)/IOKit.framework/Headers/
	@mkdir -p $(DST_INC)/device
	cp -f $(APPLE_OSS_SRC)/xnu/osfmk/device/*.h                 $(DST_INC)/device/
	cp -f $(APPLE_OSS_SRC)/IOKitUser/*.h                        $(DST_FW)/IOKit.framework/Headers/
	-cp -f $(APPLE_OSS_SRC)/IOKitUser/graphics.subproj/*.h      $(DST_FW)/IOKit.framework/Headers/graphics/ 2>/dev/null || true
	cp -f $(APPLE_OSS_SRC)/IOGraphics/IOGraphicsFamily/IOKit/graphics/*.h $(DST_FW)/IOKit.framework/Headers/graphics/
	@touch $@

HEADER_STAMPS := \
	$(OSS_STAMP)/libc \
	$(OSS_STAMP)/libm \
	$(OSS_STAMP)/libmalloc \
	$(OSS_STAMP)/copyfile \
	$(OSS_STAMP)/cctools \
	$(OSS_STAMP)/Libinfo \
	$(OSS_STAMP)/configd \
	$(OSS_STAMP)/libiconv \
	$(OSS_STAMP)/syslog \
	$(OSS_STAMP)/libedit \
	$(OSS_STAMP)/ncurses \
	$(OSS_STAMP)/IOKit \
	$(OSS_STAMP)/libplatform \
	$(OSS_STAMP)/libclosure \
	$(OSS_STAMP)/CommonCrypto \
	$(OSS_STAMP)/libdispatch \
	$(OSS_STAMP)/xnu \
	$(OSS_STAMP)/dyld \
	$(OSS_STAMP)/objc4 \
	$(OSS_STAMP)/Carbon \
	$(OSS_STAMP)/CF \
	$(OSS_STAMP)/Security \
	$(OSS_STAMP)/ICU \
	$(OSS_STAMP)/AvailabilityVersions

headers: $(HEADER_STAMPS)

# --- link stubs (.tbd) + closed-framework headers ---------------------------
# The +open sysroot resolves the closed system libraries/frameworks against
# minimal .tbd link stubs. Those stubs — together with the hand-written
# Objective-C headers for the closed frameworks (Foundation,
# UniformTypeIdentifiers) — are BAKED into git under open/sysroot as a
# ready-to-copy sysroot overlay (mirroring the baked MIG headers). A normal build
# just installs that overlay, so it needs neither a freshly-built libsprt nor
# llvm-nm; regenerating the stubs is a separate explicit step (`bake-stubs`).
#
# Each closed framework is a proper bundle: Foundation.framework/Foundation.tbd
# (linker: -framework Foundation) + Foundation.framework/Headers/Foundation.h
# (compiler: #import <Foundation/Foundation.h> resolves via framework search).
#
# The overlay carries stubs ONLY for libraries the platform owns. It must NOT carry
# libc++.tbd / libc++abi.tbd / libunwind.tbd: those are produced by llvm-readtapi from
# the dylibs libcxx.mk cross-builds (see gen-oss-stubs.sh, which classifies the whole
# C++ ABI symbol set as "cxxabi-skip" for exactly this reason). The copy below is
# FORCE'd and unconditional, so an overlay copy of one of them silently overwrites the
# generated stub on every re-run of the +open pipeline — and it does so INVISIBLY,
# because the pipeline's up-to-date marker for the libcxx stage is libc++.tbd
# (target-apple/Makefile), which a stale libc++abi.tbd leaves untouched. The symptom is
# a link that suddenly cannot resolve the canonical C++ ABI (typeinfo/vtable for
# std::exception, __dynamic_cast, __cxa_uncaught_exception[s], std::get_new_handler,
# the builtin typeinfos) against a sysroot that used to link fine.
OPEN_SYSROOT_FILES := $(shell find $(OPEN_SYSROOT) \( -type f -o -type l \) 2>/dev/null)

$(OSS_STAMP)/stubs: $(OPEN_SYSROOT_FILES) | $(OSS_STAMP)
	rm -rf $(DST_INC)/Foundation $(DST_INC)/UniformTypeIdentifiers
	rm -f $(DST_INC)/CoreFoundation/CFCGTypes.h
	cp -Rf $(OPEN_SYSROOT)/. $(T_TARGET)/
	@mkdir -p $(T_TARGET)/usr/local
	ln -sfn ../../include_libc $(T_TARGET)/usr/local/include
	@touch $@

stubs: $(OSS_STAMP)/stubs

all: headers stubs

# --- regeneration ("bake") of the .tbd link stubs --------------------------
# NOT part of a normal build. Re-derives the baked stubs from the reference
# libsprt.dylib's imported symbols UNION the curated per-arch dependency lists
# (functions_x86_64.txt + functions_arm64.txt) + the arch-neutral libm list, and
# writes DUAL-TARGET (x86_64 + arm64) tbds straight into the git overlay
# open/sysroot. The framework Headers/ are left untouched (only *.tbd are
# rewritten). Run after the runtime's set of system-symbol references changes:
#   make -f open-sysroot.mk bake-stubs [LIBSPRT=/path/to/libsprt.dylib]
# The imported-symbol set is stable across builds, so this is rarely needed.
NM       ?= $(TOOLCHAIN_OUTPUT_DIR)/host/bin/llvm-nm
LIBSPRT_X86 := $(abspath $(T_TARGET)/../x86_64-apple-macosx+sprt/usr/lib/libsprt.dylib)
LIBSPRT_ARM := $(abspath $(T_TARGET)/../aarch64-apple-macosx+sprt/usr/lib/libsprt.dylib)
LIBSPRT  := $(wildcard $(LIBSPRT_X86)) $(wildcard $(LIBSPRT_ARM)) $(wildcard $(T_TARGET)/usr/lib/libsprt.dylib)
FUNCTIONS_X86  := $(MK_DIR)/functions_x86_64.txt
FUNCTIONS_ARM  := $(MK_DIR)/functions_arm64.txt
FUNCTIONS_LIBM := $(MK_DIR)/functions_libm.txt

bake-stubs: $(MK_DIR)/gen-oss-stubs.sh
	$(MK_DIR)/gen-oss-stubs.sh '$(LIBSPRT)' $(NM) $(OPEN_SYSROOT) 'x86_64-macos, arm64-macos' \
		$(wildcard $(FUNCTIONS_X86)) $(wildcard $(FUNCTIONS_ARM)) $(wildcard $(FUNCTIONS_LIBM))

.PHONY: all headers stubs bake-stubs
.DEFAULT_GOAL := all
