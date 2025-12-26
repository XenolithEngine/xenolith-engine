# Copyright (c) 2024 Stappler LLC <admin@stappler.dev>
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

STAPPLER_ROOT ?= ..

LOCAL_LIBRARY := libstappler-xenolith

# force to rebuild if this makefile changed
LOCAL_MAKEFILE := $(lastword $(MAKEFILE_LIST))

LOCAL_OUTDIR := stappler-build

LOCAL_MODULES_PATHS = \
	$(STAPPLER_ROOT)/core/stappler-modules.mk \
	$(STAPPLER_ROOT)/xenolith/xenolith-modules.mk

LOCAL_MODULES ?= \
	xenolith_core \
	xenolith_application \
	xenolith_scene \
	xenolith_font \
	xenolith_backend_vk \
	xenolith_backend_vkgui \
	xenolith_renderer_basic2d \
	xenolith_renderer_material2d \
	xenolith_resources_storage \
	xenolith_resources_network \
	xenolith_resources_assets

LOCAL_EXPORT_MODULES ?= \
	xenolith_application \
	xenolith_backend_vk \
	xenolith_backend_vkgui \
	xenolith_core \
	xenolith_font \
	xenolith_renderer_basic2d \
	xenolith_renderer_material2d \
	xenolith_resources_assets \
	xenolith_resources_network \
	xenolith_resources_storage

include $(STAPPLER_ROOT)/build/make/shared.mk
