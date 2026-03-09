# Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>
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

#@ STAPPLER_BUILD_ROOT << Marks this file as entry point for build system

ifdef OS_MAKE
MAKE := $(OS_MAKE)
endif

BUILD_ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

ifeq ($(or $(STAPPLER_TARGET),$(STAPPLER_BUILD)),)

.DEFAULT_GOAL := host

FORWARD_ARGS := STAPPLER_BUILD=1

ifdef STAPPLER_TARGET
FORWARD_ARGS += STAPPLER_TARGET=$(STAPPLER_TARGET)
endif

host: host-debug
clean: host-debug-clean
install: host-install

host-install:
	@$(MAKE) -f $(LOCAL_MAKEFILE) $(FORWARD_ARGS) install

host-debug:
	@$(MAKE) -f $(LOCAL_MAKEFILE) $(FORWARD_ARGS) all

host-debug-clean:
	@$(MAKE) -f $(LOCAL_MAKEFILE) $(FORWARD_ARGS) clean

host-release:
	@$(MAKE) -f $(LOCAL_MAKEFILE) $(FORWARD_ARGS) RELEASE=1 all

host-release-clean:
	@$(MAKE) -f $(LOCAL_MAKEFILE) $(FORWARD_ARGS) RELEASE=1 clean

host-coverage:
	@$(MAKE) -f $(LOCAL_MAKEFILE) $(FORWARD_ARGS) COVERAGE=1 all

host-coverage-clean:
	@$(MAKE) -f $(LOCAL_MAKEFILE) $(FORWARD_ARGS) COVERAGE=1 clean

host-report:
	@$(MAKE) -f $(LOCAL_MAKEFILE) $(FORWARD_ARGS) COVERAGE=1 report

android: android-debug
android-clean: android-debug-clean

android-export:
	@$(MAKE) -f $(LOCAL_MAKEFILE) $(FORWARD_ARGS) ANDROID_EXPORT=1 STAPPLER_TARGET=unknown-ndk-linux-android android-export
	@$(MAKE) -f $(LOCAL_MAKEFILE) $(FORWARD_ARGS) ANDROID_EXPORT=1 STAPPLER_TARGET=unknown-ndk-linux-android RELEASE=1 android-export

android-debug:
	@$(MAKE) -f $(LOCAL_MAKEFILE) $(FORWARD_ARGS) ANDROID_EXPORT=1 STAPPLER_TARGET=unknown-ndk-linux-android android-export
	@$(MAKE) -f $(LOCAL_MAKEFILE) $(FORWARD_ARGS) STAPPLER_TARGET=unknown-ndk-linux-android all

android-debug-clean:
	@$(MAKE) -f $(LOCAL_MAKEFILE) $(FORWARD_ARGS) STAPPLER_TARGET=unknown-ndk-linux-android clean

android-release:
	@$(MAKE) -f $(LOCAL_MAKEFILE) $(FORWARD_ARGS) ANDROID_EXPORT=1 STAPPLER_TARGET=unknown-ndk-linux-android RELEASE=1 android-export
	@$(MAKE) -f $(LOCAL_MAKEFILE) $(FORWARD_ARGS) STAPPLER_TARGET=unknown-ndk-linux-android RELEASE=1 all

android-release-clean:
	@$(MAKE) -f $(LOCAL_MAKEFILE) $(FORWARD_ARGS) STAPPLER_TARGET=unknown-ndk-linux-android RELEASE=1 clean

.PHONY: clean install
.PHONY: host host-clean host-debug host-debug-clean host-release host-release-clean host-install host-coverage host-report
.PHONY: android android-clean android-export android-debug android-debug-clean android-release android-release-clean

else

ifeq ($(STAPPLER_TARGET),unknown-ndk-linux-android)
include $(BUILD_ROOT)/android-ndk.mk
else
include $(BUILD_ROOT)/host.mk
endif

endif
