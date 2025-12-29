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

ifndef STAPPLER_TARGET

.DEFAULT_GOAL := host

# Загружаем конфигурацию разделяемого окружения ОС
-include $(BUILD_ROOT)/shared-config.mk

include $(BUILD_ROOT)/utils/detect-platform.mk

host: host-debug
clean: host-debug-clean
install: host-install

host-install:
	@$(MAKE) -f $(LOCAL_MAKEFILE) $(call sp_detect_platform,host) install

host-debug:
	@$(MAKE) -f $(LOCAL_MAKEFILE) $(call sp_detect_platform,host) all

host-debug-clean:
	@$(MAKE) -f $(LOCAL_MAKEFILE) $(call sp_detect_platform,host) clean

host-release:
	@$(MAKE) -f $(LOCAL_MAKEFILE) $(call sp_detect_platform,host) RELEASE=1

host-release-clean:
	@$(MAKE) -f $(LOCAL_MAKEFILE) $(call sp_detect_platform,host) RELEASE=1 clean

host-coverage:
	@$(MAKE) -f $(LOCAL_MAKEFILE) $(call sp_detect_platform,host) COVERAGE=1 all

host-coverage-clean:
	@$(MAKE) -f $(LOCAL_MAKEFILE) $(call sp_detect_platform,host) COVERAGE=1 clean

host-report:
	@$(MAKE) -f $(LOCAL_MAKEFILE) $(call sp_detect_platform,host) COVERAGE=1 report

android: android-debug
android-clean: android-debug-clean

android-export:
	@$(MAKE) -f $(LOCAL_MAKEFILE) ANDROID_EXPORT=1 $(call sp_detect_platform,android) android-export
	@$(MAKE) -f $(LOCAL_MAKEFILE) ANDROID_EXPORT=1 RELEASE=1 $(call sp_detect_platform,android) android-export

android-debug:
	@$(MAKE) -f $(LOCAL_MAKEFILE) ANDROID_EXPORT=1 $(call sp_detect_platform,android) android-export
	@$(MAKE) -f $(LOCAL_MAKEFILE) $(call sp_detect_platform,android) all

android-debug-clean:
	@$(MAKE) $(call sp_detect_platform,android) clean

android-release:
	@$(MAKE) -f $(LOCAL_MAKEFILE) ANDROID_EXPORT=1 $(call sp_detect_platform,android) android-export
	@$(MAKE) -f $(LOCAL_MAKEFILE) $(call sp_detect_platform,android) RELEASE=1 all

android-release-clean:
	@$(MAKE) -f $(LOCAL_MAKEFILE) $(call sp_detect_platform,android) RELEASE=1 clean

ios: ios-debug
ios-clean: ios-debug-clean

ios-export:
	@$(MAKE) -f $(LOCAL_MAKEFILE) $(call sp_detect_platform,ios) IOS_ARCH=export ios-export

mac-export:
	@$(MAKE) -f $(LOCAL_MAKEFILE) $(call sp_detect_platform,host) RELEASE=1 mac-export

xwin: xwin-debug
xwin-clean: xwin-debug-clean

xwin-debug:
	@$(MAKE) -f $(LOCAL_MAKEFILE) $(call sp_detect_platform,xwin) all

xwin-debug-clean:
	@$(MAKE) -f $(LOCAL_MAKEFILE) $(call sp_detect_platform,xwin) clean

xwin-release:
	@$(MAKE) -f $(LOCAL_MAKEFILE) $(call sp_detect_platform,xwin) RELEASE=1 all

xwin-release-clean:
	@$(MAKE) -f $(LOCAL_MAKEFILE) $(call sp_detect_platform,xwin) RELEASE=1 clean

xwin-all:
	@$(MAKE) -f $(LOCAL_MAKEFILE) $(call sp_detect_platform,xwin) xwin
	@$(MAKE) -f $(LOCAL_MAKEFILE) $(call sp_detect_platform,xwin) RELEASE=1 xwin

xwin-all-clean:
	@$(MAKE) -f $(LOCAL_MAKEFILE) $(call sp_detect_platform,xwin) xwin-clean
	@$(MAKE) -f $(LOCAL_MAKEFILE) $(call sp_detect_platform,xwin) RELEASE=1 xwin-clean

.PHONY: clean install
.PHONY: host host-clean host-debug host-debug-clean host-release host-release-clean host-install host-coverage host-report
.PHONY: android android-clean android-export android-debug android-debug-clean android-release android-release-clean
.PHONY: ios ios-clean ios-debug ios-debug-clean ios-release ios-release-clean
.PHONY: xwin xwin-clean xwin-debug xwin-debug-clean xwin-release xwin-release-clean xwin-all xwin-all-clean

else

ifeq ($(STAPPLER_TARGET),host)
include $(BUILD_ROOT)/host.mk
else ifeq ($(STAPPLER_TARGET),android)
include $(BUILD_ROOT)/android.mk
else ifeq ($(STAPPLER_TARGET),xwin)
include $(BUILD_ROOT)/xwin.mk
endif

endif
