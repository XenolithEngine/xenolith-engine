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

# current dir
XENOLITH_MODULE_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

XENOLITH_VERSION_API := 3
XENOLITH_VERSION_REV := 1
XENOLITH_VERSION_BUILD := $(firstword $(call sp_detect_build_number,$(XENOLITH_MODULE_DIR)))

TOOLKIT_MODULE_LIST += \
	$(XENOLITH_MODULE_DIR)/core/core.mk \
	$(XENOLITH_MODULE_DIR)/application/application.mk \
	$(XENOLITH_MODULE_DIR)/font/font.mk \
	$(XENOLITH_MODULE_DIR)/backend/vk/vk.mk \
	$(XENOLITH_MODULE_DIR)/renderer/basic2d/basic2d.mk \
	$(XENOLITH_MODULE_DIR)/renderer/material2d/material2d.mk \
	$(XENOLITH_MODULE_DIR)/renderer/richtext/richtext.mk \
	$(XENOLITH_MODULE_DIR)/renderer/simpleui/simpleui.mk \
	$(XENOLITH_MODULE_DIR)/resources/storage/storage.mk \
	$(XENOLITH_MODULE_DIR)/resources/network/network.mk \
	$(XENOLITH_MODULE_DIR)/resources/assets/assets.mk
