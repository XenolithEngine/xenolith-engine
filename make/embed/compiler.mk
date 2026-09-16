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

# BundleFS: turn a directory of resources into a translation unit, so that the files ship
# inside the binary and are read through FileCategory::Embedded.
#
# Declared by a project as LOCAL_EMBED_DIRS / LOCAL_EMBED_COMPRESSED_DIRS, or by a module as
# MODULE_<X>_EMBED_DIRS / MODULE_<X>_EMBED_COMPRESSED_DIRS. See make/MODULES.md.

BUILD_EMBED_OUTDIR := $(BUILD_С_OUTDIR)/embed

# $(1) - bundle directory
#
# The mount name of a bundle is the directory's own name, so "resources/style.css" addresses the
# same file whether it comes from an on-disk resources/ directory or from the embedded copy —
# switching an app over is a change of FileCategory and nothing else.
sp_embed_name = $(notdir $(patsubst %/,%,$(1)))

# $(1) - bundle directory
sp_embed_source = $(BUILD_EMBED_OUTDIR)/stappler-embed-$(call sp_embed_name,$(1)).cpp

# $(1) - list of bundle directories
sp_embed_sources = $(foreach dir,$(1),$(call sp_embed_source,$(dir)))

# $(1) - list of directories, $(2) - root for relative ones
sp_embed_resolve = $(realpath \
	$(call sp_list_abspaths,$(1)) \
	$(call sp_add_root,$(2),$(call sp_list_relpaths,$(1))))

# $(1) - bundle directory
#
# Everything below the directory is a prerequisite of its TU. The directories themselves are
# listed too: adding or deleting a file does not touch any remaining file, but it does bump the
# mtime of the directory holding it, so that is what catches such a change.
#
# The outer $(wildcard) is a filter, not a glob. GNU make has no way to express a path containing
# a space — $(wildcard) has already split "a b.txt" into two words by the time we see it, and
# feeding those to a rule fails the build outright. Passing the list back through $(wildcard)
# drops exactly those bogus words, since neither half names a real file. Such a file is still
# embedded correctly (the generator reads the directory itself, not this list); it just does not
# act as a rebuild trigger on its own — the mtime of its directory does. Under xlmake, where a
# space inside a path is carried as a placeholder rather than a separator, nothing is dropped.
sp_embed_deps = $(wildcard $(call sp_find_dirs_recursive,$(1)) $(call sp_find_recursive,$(1),*))
