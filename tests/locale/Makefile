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

# force to rebuild if this makefile changed
LOCAL_MAKEFILE := $(lastword $(MAKEFILE_LIST))

STAPPLER_BUILD_ROOT ?= $(dir $(LOCAL_MAKEFILE))../../make

LOCAL_OUTDIR := $(dir $(LOCAL_MAKEFILE))stappler-build
LOCAL_EXECUTABLE := localetest

LOCAL_MODULES_PATHS = stappler/stappler-modules.mk xenolith/xenolith-modules.mk

# Only the locale module and what it needs. `locale::` owns no GPU objects and builds no scene, so
# these tests need neither a backend nor a window - which is what makes a string table assertable at
# all. A Label is exercised through `LabelBase`, which is a plain class and not a Node.
LOCAL_MODULES := xenolith_font

LOCAL_ROOT = $(dir $(LOCAL_MAKEFILE))
LOCAL_SRCS_DIRS := locale
LOCAL_MAIN := main.cpp

APPCONFIG_APP_NAME := LocaleTest
APPCONFIG_BUNDLE_NAME := org.stappler.LocaleTest

include $(STAPPLER_BUILD_ROOT)/universal.mk
