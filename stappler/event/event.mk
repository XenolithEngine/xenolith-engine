# Copyright (c) 2025 Stappler LLC <admin@stappler.dev>
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

MODULE_STAPPLER_EVENT_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_STAPPLER_EVENT_SRCS_DIRS := $(STAPPLER_MODULE_DIR)/event
MODULE_STAPPLER_EVENT_SRCS_OBJS :=
MODULE_STAPPLER_EVENT_INCLUDES_DIRS :=
MODULE_STAPPLER_EVENT_INCLUDES_OBJS := $(STAPPLER_MODULE_DIR)/event
MODULE_STAPPLER_EVENT_DEPENDS_ON := stappler_filesystem stappler_threads
MODULE_STAPPLER_EVENT_LIBS :=

ifdef WIN32
MODULE_STAPPLER_EVENT_LIBS += -lntdll -lUser32
endif

MODULE_STAPPLER_EVENT_SHARED_SPEC_SUMMARY := libstappler event loop module

define MODULE_STAPPLER_EVENT_SHARED_SPEC_DESCRIPTION
Module libstappler-event is a cross-platform low-level event loop
In general, it provides:
- async timers
- cross-thread function calls
- way to associate fd/HANDLE events with callback
endef

# module name resolution
$(call define_module, stappler_event, MODULE_STAPPLER_EVENT)
