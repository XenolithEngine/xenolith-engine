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

MODULE_STAPPLER_DB_DEFINED_IN := $(TOOLKIT_MODULE_PATH)
MODULE_STAPPLER_DB_PRIVATE_INCLUDE_PCH := SPCommon.h
MODULE_STAPPLER_DB_LIBS := -l:libsqlite3.a
MODULE_STAPPLER_DB_FLAGS :=
MODULE_STAPPLER_DB_SRCS_DIRS := $(STAPPLER_MODULE_DIR)/db
MODULE_STAPPLER_DB_SRCS_OBJS := 
MODULE_STAPPLER_DB_INCLUDES_DIRS := $(STAPPLER_MODULE_DIR)/db
MODULE_STAPPLER_DB_INCLUDES_OBJS :=
MODULE_STAPPLER_DB_DEPENDS_ON := stappler_search stappler_sql stappler_filesystem stappler_crypto
MODULE_STAPPLER_DB_GENERAL_CXXFLAGS :=
MODULE_STAPPLER_DB_GENERAL_LDFLAGS :=
MODULE_STAPPLER_DB_SHARED_CONSUME := \
	stappler_sql

MODULE_STAPPLER_DB_SHARED_DEPENDS_ON := stappler_bitmap

ifdef MACOS
# Use platform version
MODULE_STAPPLER_DB_LIBS :=
MODULE_STAPPLER_DB_GENERAL_CXXFLAGS += -DSTAPPLER_SQLITE_LINKED
MODULE_STAPPLER_DB_GENERAL_LDFLAGS += -lsqlite3
endif

#spec

MODULE_STAPPLER_DB_SHARED_SPEC_SUMMARY := libstappler database interface

define MODULE_STAPPLER_DB_SHARED_SPEC_DESCRIPTION
Module libstappler-db is an interface for database storage (SQlite or PostgreSQL)
Implements complex strongly-typed versionized database schemes:
- Automatic and virtual fields supports
- Views
- Access control by user role
- Per-field compression
endef

# module name resolution
$(call define_module, stappler_db, MODULE_STAPPLER_DB)
