# Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
# Copyright (c) 2025 Stappler Team <admin@stappler.org>
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

MODULE_XENOLITH_APPLICATION_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_XENOLITH_APPLICATION_PRECOMPILED_HEADERS :=
MODULE_XENOLITH_APPLICATION_SRCS_DIRS :=
MODULE_XENOLITH_APPLICATION_SRCS_OBJS := \
	$(XENOLITH_MODULE_DIR)/application/XLApplication.scu.cpp \
	$(XENOLITH_MODULE_DIR)/application/XLPlatform.scu.cpp \
	$(XENOLITH_MODULE_DIR)/application/XLDirector.scu.cpp \
	$(XENOLITH_MODULE_DIR)/application/XLNodes.scu.cpp
MODULE_XENOLITH_APPLICATION_INCLUDES_DIRS := 
MODULE_XENOLITH_APPLICATION_INCLUDES_OBJS := \
	$(XENOLITH_MODULE_DIR)/application \
	$(XENOLITH_MODULE_DIR)/application/actions \
	$(XENOLITH_MODULE_DIR)/application/director \
	$(XENOLITH_MODULE_DIR)/application/input \
	$(XENOLITH_MODULE_DIR)/application/resources \
	$(XENOLITH_MODULE_DIR)/application/nodes

MODULE_XENOLITH_APPLICATION_DEPENDS_ON := xenolith_core xenolith_font stappler_data

ifndef LOCAL_MAIN
MODULE_XENOLITH_APPLICATION_DEPENDS_ON += xenolith_application_main
endif

ifdef LINUX
MODULE_XENOLITH_APPLICATION_DBUS_CFLAGS := $(shell pkg-config --cflags dbus-1)
MODULE_XENOLITH_APPLICATION_GENERAL_CFLAGS += $(MODULE_XENOLITH_APPLICATION_DBUS_CFLAGS)
MODULE_XENOLITH_APPLICATION_GENERAL_CXXFLAGS += $(MODULE_XENOLITH_APPLICATION_DBUS_CFLAGS)
endif

ifdef WIN32
MODULE_XENOLITH_APPLICATION_LIBS += -lshell32 -lOleAut32 -lWinhttp -lShcore -lGdi32 -lDxva2 -lSetupAPI -lUxTheme -lWindowsApp -lDwmapi
ifdef RELEASE
MODULE_XENOLITH_APPLICATION_EXEC_LDFLAGS += -Wl,/subsystem:windows -Wl,/ENTRY:mainCRTStartup
endif
endif

ifdef MACOS

MODULE_XENOLITH_APPLICATION_GENERAL_LDFLAGS += -framework Cocoa -framework Network -framework Quartz -framework IOKit -framework MetalKit

endif

#spec

MODULE_XENOLITH_APPLICATION_SHARED_SPEC_SUMMARY := Xenolith general application interface

define MODULE_XENOLITH_APPLICATION_SHARED_SPEC_DESCRIPTION
Module libxenolith-application implements interface for application main loop:
- Application-level resources
- Event handling
endef

# module name resolution
MODULE_xenolith_application := MODULE_XENOLITH_APPLICATION

MODULE_XENOLITH_APPLICATION_MAIN_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_XENOLITH_APPLICATION_MAIN_PRECOMPILED_HEADERS :=
MODULE_XENOLITH_APPLICATION_MAIN_SRCS_DIRS := 
MODULE_XENOLITH_APPLICATION_MAIN_SRCS_OBJS := $(XENOLITH_MODULE_DIR)/application/XLMain.cpp
MODULE_XENOLITH_APPLICATION_MAIN_INCLUDES_DIRS :=
MODULE_XENOLITH_APPLICATION_MAIN_INCLUDES_OBJS :=

MODULE_XENOLITH_APPLICATION_MAIN_DEPENDS_ON := stappler_core

#spec

MODULE_XENOLITH_APPLICATION_MAIN_SHARED_SPEC_SUMMARY := Xenolith default entry point implementation

define MODULE_XENOLITH_APPLICATION_MAIN_SHARED_SPEC_DESCRIPTION
Module libxenolith-application-main contains default entry point for an applications without `main` definition
endef

# module name resolution
MODULE_xenolith_application_main := MODULE_XENOLITH_APPLICATION_MAIN
