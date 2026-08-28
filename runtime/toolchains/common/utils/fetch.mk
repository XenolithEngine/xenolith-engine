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
#
# The make-side interface to the source fetcher. src.mk declares what each
# dependency is; everything about *how* it is downloaded and verified lives here
# and in common/utils/fetch.sh (POSIX) / fetch.ps1 (Windows).
#
# A dependency is declared as a block of variables named after it, followed by a
# one-line recipe. For an archive:
#
#     zlib_URL    := https://www.zlib.net/zlib-1.3.2.tar.gz
#     zlib_SHA256 := bb329a0a...
#     zlib_SIG    := .asc          # suffix appended to _URL, or a full URL
#     zlib_KEY    := zlib          # keys/zlib.asc
#
#     $(SRC_ROOT)/zlib: | prepare
#     	$(call sp_fetch_tar,zlib)
#
# and for a git checkout:
#
#     libpng_REPO   := https://github.com/pnggroup/libpng.git
#     libpng_TAG    := v1.6.58
#     libpng_COMMIT := 3061454d980de7d53608f594194cfac722721d2a
#
#     $(SRC_ROOT)/libpng: | prepare
#     	$(call sp_fetch_clone,libpng)
#
# Recognised variables, all optional except _URL / _REPO:
#
#   <name>_URL         archive or file to download
#   <name>_SHA256      pinned SHA-256; a mismatch aborts the build
#   <name>_SIG         detached OpenPGP signature: ".asc"/".sig" suffix or full URL
#   <name>_KEY         basename in keys/ of the public key that signature must match
#   <name>_STRIP       leading path components to drop when unpacking (default 1)
#   <name>_REPO        git remote
#   <name>_TAG         tag or branch to clone
#   <name>_COMMIT      commit the checkout must land on; catches a moved tag
#   <name>_DEPTH       clone depth (ignored when only _COMMIT is given)
#   <name>_SUBMODULES  non-empty to clone submodules
#
# Set SP_REQUIRE_SIGNATURES=1 to turn every soft spot - a source with no
# signature, a missing pinned key, a host with no gpg - into a hard error. That
# is the setting for release and CI builds; see keys/README.adoc.

ifndef SP_FETCH_INCLUDED
SP_FETCH_INCLUDED := 1

SP_FETCH_DIR  := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
SP_KEYS_DIR   ?= $(abspath $(SP_FETCH_DIR)../../keys)
SP_TMP_DIR    ?= $(TMP_DIR)

SP_REQUIRE_SIGNATURES ?= 0

# The fetcher reads these from the environment rather than the command line:
# they are the same for every dependency and would only add noise to each call.
export SP_KEYS_DIR
export SP_TMP_DIR
export SP_REQUIRE_SIGNATURES

ifeq ($(findstring Windows,$(OS)),Windows)
# PowerShell parameters take a single dash and match case-insensitively, so the
# same option names work on both hosts.
SP_FETCH_CMD := powershell -NoProfile -ExecutionPolicy Bypass -File "$(SP_FETCH_DIR)fetch.ps1"
SP_DASH      := -
else
SP_FETCH_CMD := sh "$(SP_FETCH_DIR)fetch.sh"
SP_DASH      := --
endif

# Emit an option only when it has a value, so an undeclared _SIG or _COMMIT
# simply disables that check instead of passing an empty string down.
sp_opt = $(if $(strip $(2)),$(SP_DASH)$(1) "$(strip $(2))")
sp_swt = $(if $(strip $(2)),$(SP_DASH)$(1))

# $(1) = dependency name, $(2) = destination (defaults to $(SRC_ROOT)/$(1))
sp_dest = $(if $(strip $(2)),$(strip $(2)),$(SRC_ROOT)/$(1))

sp_fetch_common = $(call sp_opt,name,$(1)) $(call sp_opt,url,$($(1)_URL)) \
	$(call sp_opt,dest,$(call sp_dest,$(1),$(2))) \
	$(call sp_opt,sha256,$($(1)_SHA256)) \
	$(call sp_opt,sig,$($(1)_SIG)) \
	$(call sp_opt,key,$($(1)_KEY))

sp_fetch_tar  = $(SP_FETCH_CMD) tar $(call sp_fetch_common,$(1),$(2)) $(call sp_opt,strip,$($(1)_STRIP))
sp_fetch_zip  = $(SP_FETCH_CMD) zip $(call sp_fetch_common,$(1),$(2)) $(call sp_opt,strip,$($(1)_STRIP))
sp_fetch_file = $(SP_FETCH_CMD) file $(call sp_fetch_common,$(1),$(2))

sp_fetch_clone = $(SP_FETCH_CMD) clone $(call sp_opt,name,$(1)) \
	$(call sp_opt,repo,$($(1)_REPO)) \
	$(call sp_opt,dest,$(call sp_dest,$(1),$(2))) \
	$(call sp_opt,tag,$($(1)_TAG)) \
	$(call sp_opt,commit,$($(1)_COMMIT)) \
	$(call sp_opt,depth,$($(1)_DEPTH)) \
	$(call sp_swt,submodules,$($(1)_SUBMODULES))

# Patching an unpacked tarball. `git -C` instead of `cd ... &&` because the
# Windows host runs recipes through PowerShell 5.1, which has no `&&`, and a `;`
# there would let a failed cd fall through into a patch applied in the wrong tree.
# $(1) = dependency name, $(2) = path under replacements/
sp_patch = git -C "$(SRC_ROOT)/$(1)" apply -p1 "$(LIBS_MAKE_ROOT)replacements/$(2)"

endif
