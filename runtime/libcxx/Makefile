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

# Vendoring pipeline for the libc++ port (see README.adoc).
#
# The vendored upstream files under include/ and src/libcxx/ are baked into the
# repository and committed. This Makefile only exists to REGENERATE them from the
# vendored llvm-project tree and (re)apply the minimal patches in patch/. It is not
# part of the normal build; `make regen` is run by hand when bumping the upstream
# libc++ revision. The default target is deliberately a no-op.
#
# include/ is a full byte-for-byte mirror of upstream libcxx/include, MINUS the C++03
# frozen headers (__cxx03/) and the CMake generation templates (*.in, CMakeLists.txt).
# The build-time configuration those templates would produce (__config_site,
# __assertion_handler) is supplied instead by the sprt overlay in ../include_libc/cxx,
# which precedes include/ on the search path and retargets libc++ onto the sprt libc.
# There is no hand-written shim layer inside include/ anymore — the port is achieved
# through the overlay, so the whole header tree is mirrored wholesale.

THIS_DIR     := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
LLVM         := $(THIS_DIR)/../toolchains/src/llvm-project
UPSTREAM     := $(LLVM)/libcxx
LIBC_UPSTREAM:= $(LLVM)/libc
PATCH_DIR    := $(THIS_DIR)/patch

# Verbatim upstream out-of-line TUs copied into src/libcxx/ from $(UPSTREAM)/src/.
# They are #included by the src/SPRTCxx*.cpp wrappers (compiled by libcxx.mk), never
# scanned as standalone units. This list is explicit: only the subsystems whose ABI
# the runtime actually provides are built.
VENDOR_SOURCES := \
	regex.cpp \
	memory_resource.cpp \
	valarray.cpp \
	random.cpp \
	chrono.cpp \
	system_error.cpp \
	error_category.cpp \
	string.cpp \
	hash.cpp \
	memory.cpp \
	locale.cpp \
	ios.cpp \
	ios.instantiations.cpp \
	ostream.cpp \
	fstream.cpp \
	strstream.cpp \
	charconv.cpp \
	mutex.cpp \
	mutex_destructor.cpp \
	condition_variable.cpp \
	condition_variable_destructor.cpp \
	shared_mutex.cpp \
	thread.cpp \
	call_once.cpp \
	future.cpp \
	bind.cpp \
	new_helpers.cpp \
	verbose_abort.cpp \
	iostream.cpp \
	algorithm.cpp

# Out-of-line TUs that live in an upstream subdirectory (filesystem/, ryu/); copied
# preserving that subpath under src/libcxx/. Wrapped by SPRTCxxFilesystem.cpp (the
# filesystem group) and SPRTCxxCharconv.cpp (the ryu float backend).
VENDOR_SOURCES_SUBDIR := \
	filesystem/directory_entry.cpp \
	filesystem/directory_iterator.cpp \
	filesystem/filesystem_clock.cpp \
	filesystem/filesystem_error.cpp \
	filesystem/int128_builtins.cpp \
	filesystem/operations.cpp \
	filesystem/path.cpp \
	ryu/d2s.cpp \
	ryu/f2s.cpp \
	ryu/d2fixed.cpp

# Headers sitting next to a vendored TU (included by relative path, e.g.
# memory_resource.cpp -> memory_resource_init_helper.h; ostream.cpp -> std_stream.h).
# Copied flat into src/libcxx/.
VENDOR_SRC_HEADERS := \
	memory_resource_init_helper.h \
	std_stream.h \
	iostream_init.h

# filesystem/ private headers pulled by the filesystem TUs via #include "error.h" etc.
# Copied into src/libcxx/filesystem/.
VENDOR_FS_HEADERS := \
	error.h \
	file_descriptor.h \
	format_string.h \
	path_parser.h \
	posix_compat.h \
	time_utils.h

# The whole upstream src/include/ tree (atomic_support.h, sso_allocator.h, ryu/*,
# from/to_chars_floating_point.h, ...) is mirrored wholesale into src/libcxx/include/,
# same as the header tree above -- individual listing is no longer maintained.

# llvm-libc float-conversion closure (shared/ + src/__support + a few hdr/,include/)
# pulled by charconv's from/to_chars_floating_point.h. The exact transitive set is
# compile-derived and committed as a manifest (paths relative to the llvm-project root);
# copied into src/libcxx/libc-shared/ preserving the libc/-relative subpath.
LIBC_SHARED_MANIFEST := $(THIS_DIR)/src/libcxx/libc-shared.manifest

.PHONY: all regen check-upstream clean-vendored

all:
	@echo "libcxx port: vendored files are committed; run 'make regen' to refresh from upstream."

regen: check-upstream
	@echo "== mirroring upstream include/ (except __cxx03 + cmake templates) =="
	@rm -rf "$(THIS_DIR)/include"
	@mkdir -p "$(THIS_DIR)/include"
	@( cd "$(UPSTREAM)/include" && tar \
		--exclude='__cxx03' \
		--exclude='CMakeLists.txt' \
		--exclude='*.in' \
		-cf - . ) | ( cd "$(THIS_DIR)/include" && tar -xf - )
	@echo "== mirroring upstream src/include/ (ryu, float-conv, sso, atomic_support) =="
	@rm -rf "$(THIS_DIR)/src/libcxx/include"
	@mkdir -p "$(THIS_DIR)/src/libcxx/include"
	@( cd "$(UPSTREAM)/src/include" && tar -cf - . ) | ( cd "$(THIS_DIR)/src/libcxx/include" && tar -xf - )
	@echo "== copying $(words $(VENDOR_SOURCES)) TUs + $(words $(VENDOR_SOURCES_SUBDIR)) subdir-TUs + headers =="
	@mkdir -p "$(THIS_DIR)/src/libcxx/filesystem" "$(THIS_DIR)/src/libcxx/ryu"
	@$(foreach s,$(VENDOR_SOURCES), \
		cp "$(UPSTREAM)/src/$(s)" "$(THIS_DIR)/src/libcxx/$(s)" && ) true
	@$(foreach s,$(VENDOR_SOURCES_SUBDIR), \
		cp "$(UPSTREAM)/src/$(s)" "$(THIS_DIR)/src/libcxx/$(s)" && ) true
	@$(foreach s,$(VENDOR_SRC_HEADERS), \
		cp "$(UPSTREAM)/src/$(s)" "$(THIS_DIR)/src/libcxx/$(s)" && ) true
	@$(foreach s,$(VENDOR_FS_HEADERS), \
		cp "$(UPSTREAM)/src/filesystem/$(s)" "$(THIS_DIR)/src/libcxx/filesystem/$(s)" && ) true
	@echo "== vendoring llvm-libc float closure ($(shell wc -l < $(LIBC_SHARED_MANIFEST)) headers) =="
	@rm -rf "$(THIS_DIR)/src/libcxx/libc-shared"
	@while read -r rel; do \
		sub="$${rel#libc/}"; \
		mkdir -p "$(THIS_DIR)/src/libcxx/libc-shared/$$(dirname "$$sub")"; \
		cp "$(LLVM)/$$rel" "$(THIS_DIR)/src/libcxx/libc-shared/$$sub"; \
	done < "$(LIBC_SHARED_MANIFEST)"
	@if ls "$(PATCH_DIR)"/*.patch >/dev/null 2>&1; then \
		echo "== applying patches =="; \
		for p in "$(PATCH_DIR)"/*.patch; do \
			echo "  git apply $$p"; \
			cd "$(THIS_DIR)" && git apply -p1 "$$p"; \
		done; \
	else \
		echo "== no patches (the port is overlay-only; patch/ is empty) =="; \
	fi
	@echo "== regen done =="

check-upstream:
	@test -d "$(UPSTREAM)/include" || { \
		echo "ERROR: vendored llvm-project not found at $(UPSTREAM)."; \
		echo "Run the toolchain source unpack (runtime/toolchains) first."; exit 1; }
	@test -d "$(LIBC_UPSTREAM)/shared" || { \
		echo "ERROR: vendored llvm-libc not found at $(LIBC_UPSTREAM) (needed for the"; \
		echo "charconv float closure). Run the toolchain source unpack first."; exit 1; }

# Remove the baked vendored files (the mirrored include/ tree, the vendored TUs and
# their private headers, and the llvm-libc float closure under src/libcxx/).
clean-vendored:
	@rm -rf "$(THIS_DIR)/include"
	@$(foreach s,$(VENDOR_SOURCES),rm -f "$(THIS_DIR)/src/libcxx/$(s)";)
	@$(foreach s,$(VENDOR_SOURCES_SUBDIR),rm -f "$(THIS_DIR)/src/libcxx/$(s)";)
	@$(foreach s,$(VENDOR_SRC_HEADERS),rm -f "$(THIS_DIR)/src/libcxx/$(s)";)
	@rm -rf "$(THIS_DIR)/src/libcxx/include" "$(THIS_DIR)/src/libcxx/filesystem" \
		"$(THIS_DIR)/src/libcxx/ryu" "$(THIS_DIR)/src/libcxx/libc-shared"
	@echo "removed vendored files."
