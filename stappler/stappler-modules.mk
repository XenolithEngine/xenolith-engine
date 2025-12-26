# Copyright (c) 2023-2024 Stappler LLC <admin@stappler.dev>
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
STAPPLER_MODULE_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

STAPPLER_VERSION_API := 6
STAPPLER_VERSION_REV := 1
STAPPLER_VERSION_BUILD := $(firstword $(call sp_detect_build_number,$(STAPPLER_MODULE_DIR)))

STAPPLER_CRYPTO_DEFAULT ?= openssl

TOOLKIT_MODULE_LIST += \
	$(STAPPLER_MODULE_DIR)/core/core.mk \
	$(STAPPLER_MODULE_DIR)/bitmap/bitmap.mk \
	$(STAPPLER_MODULE_DIR)/data/data.mk \
	$(STAPPLER_MODULE_DIR)/db/db.mk \
	$(STAPPLER_MODULE_DIR)/event/event.mk \
	$(STAPPLER_MODULE_DIR)/filesystem/filesystem.mk \
	$(STAPPLER_MODULE_DIR)/threads/threads.mk \
	$(STAPPLER_MODULE_DIR)/crypto/crypto.mk \
	$(STAPPLER_MODULE_DIR)/network/network.mk \
	$(STAPPLER_MODULE_DIR)/search/search.mk \
	$(STAPPLER_MODULE_DIR)/sql/sql.mk \
	$(STAPPLER_MODULE_DIR)/threads/threads.mk \
	$(STAPPLER_MODULE_DIR)/geom/geom.mk \
	$(STAPPLER_MODULE_DIR)/font/font.mk \
	$(STAPPLER_MODULE_DIR)/tess/tess.mk \
	$(STAPPLER_MODULE_DIR)/vg/vg.mk \
	$(STAPPLER_MODULE_DIR)/zip/zip.mk \
	$(STAPPLER_MODULE_DIR)/wasm/wasm.mk \
	$(STAPPLER_MODULE_DIR)/makefile/makefile.mk \
	$(STAPPLER_MODULE_DIR)/pug/pug.mk \
	$(STAPPLER_MODULE_DIR)/document/document.mk \
	$(STAPPLER_MODULE_DIR)/layout/layout.mk \
	$(STAPPLER_MODULE_DIR)/experimental/abi/abi.mk \
	$(STAPPLER_MODULE_DIR)/runtime/runtime.mk
