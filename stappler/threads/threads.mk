# Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>
# Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
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

MODULE_STAPPLER_THREADS_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_STAPPLER_THREADS_LIBS :=
MODULE_STAPPLER_THREADS_SRCS_DIRS := $(STAPPLER_MODULE_DIR)/threads
MODULE_STAPPLER_THREADS_SRCS_OBJS :=
MODULE_STAPPLER_THREADS_INCLUDES_DIRS :=
MODULE_STAPPLER_THREADS_INCLUDES_OBJS := $(STAPPLER_MODULE_DIR)/threads
MODULE_STAPPLER_THREADS_DEPENDS_ON := stappler_core

ifdef LINUX
MODULE_STAPPLER_THREADS_LIBS += -lpthread
endif

ifdef MACOS_ARCH
MODULE_STAPPLER_THREADS_LIBS += -framework Foundation
endif

# module name resolution
$(call define_module, stappler_threads, MODULE_STAPPLER_THREADS)
