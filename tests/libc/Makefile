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

# Standalone test suite that checks the freestanding libc implementation
# (runtime/libc_impl) for behavioural identity with the host libc.
#
# The same sources are compiled for the host (Linux/glibc) and for
# x86_64-pc-windows-msvc (the freestanding libc_impl). Every test prints
# deterministic output so the two runs can be diffed for identity.
# See compare.sh for the build+run+diff driver.

# force to rebuild if this makefile changed
LOCAL_MAKEFILE := $(lastword $(MAKEFILE_LIST))

STAPPLER_BUILD_ROOT ?= $(dir $(LOCAL_MAKEFILE))../../make

LOCAL_OUTDIR := $(dir $(LOCAL_MAKEFILE))stappler-build
LOCAL_EXECUTABLE := libctest

LOCAL_PRIVATE_INCLUDE_PCH :=

LOCAL_MODULES_PATHS =

LOCAL_MODULES ?= \
	runtime_libc_wrapper \
	runtime

LOCAL_ROOT = $(abspath $(dir $(LOCAL_MAKEFILE)))

LOCAL_SRCS_DIRS := c
LOCAL_SRCS_OBJS :=

LOCAL_INCLUDES_DIRS :=
LOCAL_INCLUDES_OBJS :=

LOCAL_MAIN := main.cpp

include $(STAPPLER_BUILD_ROOT)/universal.mk

#
# Cross-target build helpers
#
# "Available" targets are the ones for which a toolchain description is actually
# installed in this checkout, i.e. every <root>/[runtime/]toolchains/targets/*/target.mk.
# The list is therefore discovered, not hardcoded: a target appears here as soon as
# its toolchain is built/unpacked (e.g. wasm32-unknown-unknown once target-wasm is
# installed) and disappears when it is not available.
#
#   make list-targets                          # print the discovered target list
#   make all-targets                           # build the suite for every target
#   make -k all-targets                        # ... without stopping on first failure
#   make all-targets -j8                       # targets build in parallel
#   make all-targets RELEASE=1                 # command-line vars are forwarded
#   make all-targets-clean                     # clean every target
#   make target-x86_64-pc-windows-msvc         # build a single target
#   make all-targets TARGETS="a b"             # restrict the list explicitly
#   make all-targets TARGETS_EXCLUDE="aarch64-apple-ios"
#
# Note that cross-targets are only compiled here, not run: compare.sh/wine drive
# execution for the targets that can actually be executed on this host.

# Only meaningful in the dispatch pass (no STAPPLER_TARGET/STAPPLER_BUILD yet);
# inside a per-target sub-make universal.mk has already switched to host.mk.
ifeq ($(or $(STAPPLER_TARGET),$(STAPPLER_BUILD)),)

LOCAL_STAPPLER_ROOT := $(abspath $(STAPPLER_BUILD_ROOT)/..)

LOCAL_TARGET_FILES := \
	$(wildcard $(LOCAL_STAPPLER_ROOT)/toolchains/targets/*/target.mk) \
	$(wildcard $(LOCAL_STAPPLER_ROOT)/runtime/toolchains/targets/*/target.mk)

TARGETS_EXCLUDE ?= unknown-ndk-linux-android %+dll %+sprt %-ios %-ios-simulator

TARGETS ?= $(sort $(foreach f,$(LOCAL_TARGET_FILES),$(notdir $(patsubst %/,%,$(dir $(f))))))
TARGETS := $(filter-out $(TARGETS_EXCLUDE),$(TARGETS))

LOCAL_TARGET_GOALS := $(addprefix target-,$(TARGETS))
LOCAL_TARGET_CLEAN_GOALS := $(addsuffix -clean,$(LOCAL_TARGET_GOALS))

$(LOCAL_TARGET_GOALS): target-%:
	@$(MAKE) -f $(LOCAL_MAKEFILE) $(FORWARD_ARGS) STAPPLER_TARGET=$* all

$(LOCAL_TARGET_CLEAN_GOALS): target-%-clean:
	@$(MAKE) -f $(LOCAL_MAKEFILE) $(FORWARD_ARGS) STAPPLER_TARGET=$* clean

all-targets: $(LOCAL_TARGET_GOALS)

all-targets-clean: $(LOCAL_TARGET_CLEAN_GOALS)

list-targets:
	$(info Available targets ($(words $(TARGETS))):)$(foreach t,$(TARGETS),$(info - $(t)))

.PHONY: all-targets all-targets-clean list-targets
.PHONY: $(LOCAL_TARGET_GOALS) $(LOCAL_TARGET_CLEAN_GOALS)

endif
