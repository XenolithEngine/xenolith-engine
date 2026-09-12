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
LOCAL_EXECUTABLE := uilayouttest

LOCAL_MODULES_PATHS = stappler/stappler-modules.mk xenolith/xenolith-modules.mk

# The layout backends and the CSS resolver, and nothing that draws. `LayoutSystem::apply()` is a
# public entry point that needs only its owner Node, so a tree can be built, laid out and measured
# with no Director, no backend and no window - which is what makes a placement assertable as a
# number instead of as a screenshot.
LOCAL_MODULES := xenolith_renderer_ui

LOCAL_ROOT = $(dir $(LOCAL_MAKEFILE))
LOCAL_SRCS_DIRS := layout
LOCAL_MAIN := main.cpp

APPCONFIG_APP_NAME := UiLayoutTest
APPCONFIG_BUNDLE_NAME := org.stappler.UiLayoutTest

include $(STAPPLER_BUILD_ROOT)/universal.mk
