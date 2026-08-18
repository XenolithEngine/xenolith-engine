#!/bin/sh
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
# Source fetcher for src.mk (POSIX branch; the PowerShell branch is fetch.ps1).
#
# Every download is checked three ways, in this order, and any failure aborts the
# build instead of leaving a half-written tree behind:
#
#   1. the transfer itself must succeed (HTTP status, retries, non-empty result) -
#      a proxy error page saved as "curl-8.21.0.tar.xz" no longer reaches tar;
#   2. the SHA-256 must equal the value pinned in src.mk - this is the check that
#      actually protects a normal build, because the expected value lives in our
#      git history rather than on the server we just downloaded from;
#   3. where upstream publishes a detached OpenPGP signature, it is verified
#      against the matching pinned public key in toolchains/keys/. That check is
#      what makes step 2's pin trustworthy at the moment somebody bumps a version.
#
# The signature check is best-effort by default: without gpg installed it warns
# and continues, because the SHA-256 pin still holds. Set SP_REQUIRE_SIGNATURES=1
# (release and CI builds) to turn a missing gpg, a missing key or an unsigned
# source into a hard error.
#
# Usage:
#   fetch.sh tar   --name N --url U --dest D [--sha256 H] [--sig S] [--key K] [--strip N]
#   fetch.sh zip   --name N --url U --dest D [--sha256 H] [--sig S] [--key K]
#   fetch.sh file  --name N --url U --dest F [--sha256 H] [--sig S] [--key K]
#   fetch.sh clone --name N --repo U --dest D [--tag T] [--commit C] [--submodules] [--depth N]
#
# --sig is either a suffix appended to the source URL (".asc", ".sig") or a full URL.
# --key names a file in SP_KEYS_DIR without the .asc extension.

set -u

SP_TMP_DIR="${SP_TMP_DIR:-}"
SP_KEYS_DIR="${SP_KEYS_DIR:-}"
SP_REQUIRE_SIGNATURES="${SP_REQUIRE_SIGNATURES:-0}"

me="fetch"

die() { echo "$me: $NAME: $*" >&2; exit 1; }
note() { echo "$me: $NAME: $*" >&2; }

# --------------------------------------------------------------------------
# argument parsing
# --------------------------------------------------------------------------

MODE="${1:-}"
[ -n "$MODE" ] || { echo "$me: no mode given" >&2; exit 1; }
shift

NAME=""; URL=""; REPO=""; DEST=""; SHA256=""; SIG=""; KEY=""
STRIP=1; TAG=""; COMMIT=""; SUBMODULES=0; DEPTH=""

while [ $# -gt 0 ]; do
	case "$1" in
		--name)       NAME="$2"; shift 2 ;;
		--url)        URL="$2"; shift 2 ;;
		--repo)       REPO="$2"; shift 2 ;;
		--dest)       DEST="$2"; shift 2 ;;
		--sha256)     SHA256="$2"; shift 2 ;;
		--sig)        SIG="$2"; shift 2 ;;
		--key)        KEY="$2"; shift 2 ;;
		--strip)      STRIP="$2"; shift 2 ;;
		--tag)        TAG="$2"; shift 2 ;;
		--commit)     COMMIT="$2"; shift 2 ;;
		--depth)      DEPTH="$2"; shift 2 ;;
		--submodules) SUBMODULES=1; shift ;;
		*) echo "$me: unknown option '$1'" >&2; exit 1 ;;
	esac
done

[ -n "$NAME" ] || { echo "$me: --name is required" >&2; exit 1; }
[ -n "$DEST" ] || die "--dest is required"
[ -n "$SP_TMP_DIR" ] || die "SP_TMP_DIR is not set"

WORK="$SP_TMP_DIR/$NAME"

# --------------------------------------------------------------------------
# tool discovery
# --------------------------------------------------------------------------

have() { command -v "$1" >/dev/null 2>&1; }

download() { # url outfile
	_u="$1"; _o="$2"
	rm -f "$_o"
	if have curl; then
		curl --fail --silent --show-error --location \
			--retry 3 --retry-delay 2 --connect-timeout 30 \
			--output "$_o" "$_u" || return 1
	elif have wget; then
		wget --quiet --tries=3 --timeout=30 -O "$_o" "$_u" || return 1
	else
		die "neither curl nor wget is available"
	fi
	# A server that answers 200 with an empty body, or a transfer truncated to
	# nothing, would otherwise surface much later as a confusing tar error.
	[ -s "$_o" ] || return 1
	return 0
}

sha256_of() { # file
	if have sha256sum; then sha256sum "$1" | cut -d' ' -f1
	elif have shasum; then shasum -a 256 "$1" | cut -d' ' -f1
	elif have openssl; then openssl dgst -sha256 "$1" | sed 's/.*= *//'
	else return 1
	fi
}

# --------------------------------------------------------------------------
# verification
# --------------------------------------------------------------------------

check_sha256() { # file
	[ -n "$SHA256" ] || {
		note "WARNING: no SHA-256 pinned; the download is unverified"
		return 0
	}
	_got=$(sha256_of "$1") || die "no SHA-256 tool (sha256sum/shasum/openssl) available"
	# Normalise case so a pin pasted from an upper-case listing still matches.
	_got=$(echo "$_got" | tr 'A-F' 'a-f')
	_want=$(echo "$SHA256" | tr 'A-F' 'a-f')
	if [ "$_got" != "$_want" ]; then
		rm -f "$1"
		echo "$me: $NAME: SHA-256 MISMATCH - refusing to use this download" >&2
		echo "    url      $URL" >&2
		echo "    expected $_want" >&2
		echo "    actual   $_got" >&2
		echo "  Either the upstream file was replaced (re-check the release and the" >&2
		echo "  signature by hand before touching the pin) or the transfer was tampered" >&2
		echo "  with. Do not 'fix' this by pasting the new hash without checking." >&2
		exit 1
	fi
}

check_signature() { # file
	[ -n "$SIG" ] || {
		if [ "$SP_REQUIRE_SIGNATURES" = "1" ]; then
			die "SP_REQUIRE_SIGNATURES=1 but this source publishes no signature"
		fi
		return 0
	}

	case "$SIG" in
		http://*|https://*) _sigurl="$SIG" ;;
		*)                  _sigurl="$URL$SIG" ;;
	esac
	_keyfile="$SP_KEYS_DIR/$KEY.asc"

	if [ -z "$KEY" ] || [ ! -f "$_keyfile" ]; then
		if [ "$SP_REQUIRE_SIGNATURES" = "1" ]; then
			die "no pinned key for a signed source (expected $_keyfile)"
		fi
		note "WARNING: signature published but no pinned key; skipping"
		return 0
	fi

	if ! have gpg; then
		if [ "$SP_REQUIRE_SIGNATURES" = "1" ]; then
			die "SP_REQUIRE_SIGNATURES=1 but gpg is not installed"
		fi
		note "WARNING: gpg not installed; signature not checked (SHA-256 pin still enforced)"
		return 0
	fi

	_sigfile="$WORK/$(basename "$1").sigfile"
	download "$_sigurl" "$_sigfile" || die "could not download the signature $_sigurl"

	# A throwaway keyring holding nothing but the pinned key: gpg exits 0 only for
	# a good signature made by that one key, and the user's own keyring - and
	# whatever it happens to trust - never enters into it.
	_gnupg="$WORK/gnupg"
	rm -rf "$_gnupg"
	mkdir -p "$_gnupg"
	chmod 700 "$_gnupg"
	_ring="$_gnupg/pinned.gpg"

	if ! GNUPGHOME="$_gnupg" gpg --batch --quiet --no-default-keyring \
			--keyring "$_ring" --import "$_keyfile" >/dev/null 2>&1; then
		die "could not import the pinned key $_keyfile"
	fi

	# Exit status only - gpg's human-readable output is localised, so grepping it
	# for "Good signature" silently passes everything under a non-English locale.
	if GNUPGHOME="$_gnupg" gpg --batch --quiet --no-default-keyring \
			--keyring "$_ring" --trust-model always \
			--verify "$_sigfile" "$1" >/dev/null 2>&1; then
		note "OpenPGP signature OK (key $KEY)"
	else
		echo "$me: $NAME: BAD OpenPGP SIGNATURE" >&2
		echo "    file $URL" >&2
		echo "    sig  $_sigurl" >&2
		echo "    key  $_keyfile" >&2
		echo "  Either upstream rotated the signing key (verify the new fingerprint" >&2
		echo "  against the project's own site, then update toolchains/keys/) or this" >&2
		echo "  download is not what upstream published." >&2
		GNUPGHOME="$_gnupg" gpg --batch --no-default-keyring --keyring "$_ring" \
			--verify "$_sigfile" "$1" >&2 2>&1 || true
		rm -rf "$_gnupg"
		exit 1
	fi
	rm -rf "$_gnupg"
}

fetch_and_verify() { # -> echoes the path of the verified file
	mkdir -p "$WORK" || die "could not create $WORK"
	_out="$WORK/$(basename "$URL")"
	download "$URL" "$_out" || die "download failed: $URL"
	check_sha256 "$_out"
	check_signature "$_out"
	echo "$_out"
}

# Replace DEST with STAGE in one step, so an interrupted run can never leave a
# partially extracted tree that a later make would treat as a finished checkout.
install_tree() { # stagedir
	rm -rf "$DEST" || die "could not remove the previous $DEST"
	mkdir -p "$(dirname "$DEST")"
	mv -f "$1" "$DEST" || die "could not move the unpacked tree into $DEST"
	touch "$DEST" 2>/dev/null || true
}

# --------------------------------------------------------------------------
# modes
# --------------------------------------------------------------------------

do_tar() {
	[ -n "$URL" ] || die "--url is required"
	_file=$(fetch_and_verify) || exit 1

	# Listing the archive before unpacking turns a truncated or mislabelled file
	# into one clear error rather than a half-extracted directory.
	tar -tf "$_file" >/dev/null 2>&1 || die "not a readable archive: $_file"

	_stage="$WORK/unpack"
	rm -rf "$_stage"
	mkdir -p "$_stage"
	tar -xf "$_file" --strip-components="$STRIP" -C "$_stage" \
		|| die "could not unpack $_file"
	install_tree "$_stage"
	rm -f "$_file"
}

do_zip() {
	[ -n "$URL" ] || die "--url is required"
	have unzip || die "unzip is not installed"
	_file=$(fetch_and_verify) || exit 1

	unzip -tqq "$_file" >/dev/null 2>&1 || die "not a readable zip archive: $_file"

	_stage="$WORK/unzip"
	rm -rf "$_stage"
	mkdir -p "$_stage"
	unzip -qq "$_file" -d "$_stage" || die "could not unpack $_file"

	# Same effect as tar --strip-components=1, without naming the versioned
	# top-level directory (sqlite-amalgamation-NNNNNNN) anywhere in the makefile.
	if [ "$STRIP" = "1" ]; then
		_n=$(ls -A "$_stage" | wc -l)
		_top=$(ls -A "$_stage" | head -1)
		if [ "$_n" = "1" ] && [ -d "$_stage/$_top" ]; then
			_stage="$_stage/$_top"
		else
			die "expected exactly one top-level directory in the archive, found $_n"
		fi
	fi
	install_tree "$_stage"
	rm -f "$_file"
}

do_file() {
	[ -n "$URL" ] || die "--url is required"
	_file=$(fetch_and_verify) || exit 1
	mkdir -p "$(dirname "$DEST")"
	mv -f "$_file" "$DEST" || die "could not move the file into $DEST"
}

# Submodules are updated after the working tree is on the pinned commit, so they
# land on the revisions that commit records. No --depth here: the pinned submodule
# revision is not necessarily the tip of anything, and a shallow submodule fetch
# fails when it is not. `clone --recurse-submodules --depth N` behaved the same
# way - shallowness applies to submodules only with --shallow-submodules.
update_submodules() { # $1 = tree, $2 = what it is pinned to (for the error message)
	[ "$SUBMODULES" = "1" ] || return 0
	git -C "$1" submodule update --init --recursive --quiet \
		|| die "could not update submodules at $2"
}

do_clone() {
	[ -n "$REPO" ] || die "--repo is required"
	have git || die "git is not installed"

	_stage="$WORK/clone"
	rm -rf "$_stage"
	mkdir -p "$(dirname "$_stage")"

	if [ -n "$TAG" ]; then
		# Fetch the tag rather than `clone --branch <tag>`. clone --branch hands
		# git's clone path the ref's own object, and for an annotated tag that
		# object is the tag, not a commit, so git prints
		#
		#     warning: refs/tags/v1.6.58 fdc7185... is not a commit!
		#
		# for most of the sources here. The resulting clone is correct - git peels
		# the tag for HEAD anyway - but the warning is alarming and means nothing.
		# Fetching the tag explicitly does not go through that path. The refspec
		# copies the tag into the new repository too, so `git describe` still works
		# in the unpacked tree the way it did before; some upstream build scripts
		# read it for their version string.
		git init --quiet "$_stage" || die "git init failed"
		git -C "$_stage" remote add origin "$REPO" || die "could not add the remote"

		if [ -n "$DEPTH" ]; then
			_fetch="fetch --quiet --depth $DEPTH"
		else
			_fetch="fetch --quiet"
		fi

		# shellcheck disable=SC2086
		git -C "$_stage" $_fetch origin "refs/tags/$TAG:refs/tags/$TAG" 2>/dev/null \
			|| git -C "$_stage" $_fetch origin "refs/heads/$TAG:refs/remotes/origin/$TAG" 2>/dev/null \
			|| git -C "$_stage" $_fetch origin "$TAG" \
			|| die "could not fetch '$TAG' from $REPO"

		git -c advice.detachedHead=false -C "$_stage" checkout --quiet --detach FETCH_HEAD \
			|| die "could not check out '$TAG'"
	else
		# No tag to name: a plain clone does not go through the path that warns.
		# A bare commit pin needs the history, so --depth is dropped there.
		set -- -c advice.detachedHead=false clone --quiet
		if [ -n "$DEPTH" ] && [ -z "$COMMIT" ]; then set -- "$@" --depth "$DEPTH"; fi
		git "$@" "$REPO" "$_stage" || die "git clone failed: $REPO"

		if [ -n "$COMMIT" ]; then
			git -c advice.detachedHead=false -C "$_stage" checkout --quiet "$COMMIT" \
				|| die "pinned commit $COMMIT is not in $REPO"
		fi
	fi

	# Only meaningful for the tag path: in the bare-commit path the checkout above
	# already named the commit, so there is nothing left to disagree with.
	if [ -n "$COMMIT" ] && [ -n "$TAG" ]; then
		_head=$(git -C "$_stage" rev-parse HEAD 2>/dev/null) || die "could not read HEAD"
		if [ "$_head" != "$COMMIT" ]; then
			# A tag is a mutable pointer; upstream re-tagging is exactly the event
			# this catches, and it must not pass silently.
			echo "$me: $NAME: TAG MOVED - refusing this clone" >&2
			echo "    repo   $REPO" >&2
			echo "    tag    $TAG" >&2
			echo "    pinned $COMMIT" >&2
			echo "    actual $_head" >&2
			echo "  Review what upstream changed under the tag before updating the pin." >&2
			rm -rf "$_stage"
			exit 1
		fi
	fi

	update_submodules "$_stage" "${COMMIT:-$TAG}"

	install_tree "$_stage"
}

case "$MODE" in
	tar)   do_tar ;;
	zip)   do_zip ;;
	file)  do_file ;;
	clone) do_clone ;;
	*) echo "$me: unknown mode '$MODE'" >&2; exit 1 ;;
esac
