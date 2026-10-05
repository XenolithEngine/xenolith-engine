#!/bin/sh
# Xenolith SDK installer — the curl|sh entry point:
#
#   curl -fsSL https://raw.githubusercontent.com/XenolithEngine/xenolith-engine/master/install.sh | sh
#
# Downloads the newest xenolith-cli release (tags "cli-v*", which never take the repo's "latest"
# pointer — that stays on the engine's sdk-v* releases), verifies its checksum, and installs it
# into ~/.local/bin (override with XENOLITH_BIN_DIR). A specific version can be requested with
# XENOLITH_CLI_VERSION=cli-v0.2.0.
#
# Running it again updates: an installed release that is already the newest is left alone, and an
# older one is replaced. XENOLITH_CLI_FORCE=1 reinstalls regardless. An installed CLI can also do
# this itself: xenolith-cli self-update.
#
# POSIX sh on purpose: it must run on macOS and bare Linux images alike.

set -eu

REPO="XenolithEngine/xenolith-engine"
API="https://api.github.com/repos/$REPO"
TAG_PREFIX="cli-v"

say() { printf '%s\n' "$*"; }
die() { printf 'install.sh: %s\n' "$*" >&2; exit 1; }

need() { command -v "$1" >/dev/null 2>&1 || die "'$1' is required but not found in PATH"; }
need curl
need tar

# --- platform -> asset triple --------------------------------------------------

os=$(uname -s)
arch=$(uname -m)
case "$os:$arch" in
	Darwin:arm64) triple=aarch64-apple-macosx ;;
	Darwin:x86_64) triple=x86_64-apple-macosx ;;
	Linux:x86_64) triple=x86_64-unknown-linux-musl ;;
	Linux:aarch64) triple=aarch64-unknown-linux-musl ;;
	*)
		die "no prebuilt xenolith-cli for $os $arch.
Download it from https://github.com/$REPO/releases (assets named xenolith-cli-<triple>.tar.gz),
or build from source: git clone --recursive https://github.com/$REPO.git, see docs/agents/."
		;;
esac

# --- pick the release -----------------------------------------------------------

# The larger of two dotted versions.
version_max() {
	printf '%s\n%s\n' "$1" "$2" | sort -t. -k1,1n -k2,2n -k3,3n -k4,4n | tail -n 1
}

# The repo carries two release lines: sdk-v* (the engine/SDK) and cli-v* (this tool). "Latest
# release" on GitHub points at the engine line, so the CLI release is found by listing releases
# and taking the highest cli-v* version that is not a prerelease. The API orders releases by
# creation date, which a fix to an older line would break.
if [ -n "${XENOLITH_CLI_VERSION:-}" ]; then
	tag=$XENOLITH_CLI_VERSION
	case $tag in
		$TAG_PREFIX*) ;;
		*) die "XENOLITH_CLI_VERSION must start with '$TAG_PREFIX', got '$tag'" ;;
	esac
	say "Using requested release $tag"
else
	releases=$(curl -fsSL "$API/releases?per_page=100") \
		|| die "could not list releases from $API (network error or GitHub API rate limit)"
	# "tag_name" precedes "prerelease" in every release object.
	tag=$(printf '%s\n' "$releases" | tr ',' '\n' | awk -v prefix="$TAG_PREFIX" '
		/"tag_name":/ { sub(/.*"tag_name": *"/, ""); sub(/".*/, ""); tag = $0; next }
		/"prerelease":/ {
			if ($0 ~ /false/ && index(tag, prefix) == 1 && substr(tag, length(prefix) + 1) ~ /^[0-9]+(\.[0-9]+)*$/)
				print substr(tag, length(prefix) + 1)
			tag = ""
		}' | sort -t. -k1,1n -k2,2n -k3,3n -k4,4n | tail -n 1)
	[ -z "$tag" ] || tag="$TAG_PREFIX$tag"
	[ -n "$tag" ] || die "no '$TAG_PREFIX*' release found in $REPO — has a CLI release been cut?"
	say "Latest CLI release: $tag"
fi

# --- compare with what is installed -----------------------------------------------

bin_dir=${XENOLITH_BIN_DIR:-$HOME/.local/bin}

installed=
if [ -x "$bin_dir/xenolith-cli" ]; then
	installed=$("$bin_dir/xenolith-cli" --version 2>/dev/null | awk '$1 == "xenolith-cli" { print $2; exit }') || installed=
fi

case $installed in
	"") ;;
	$TAG_PREFIX[0-9]*)
		have=${installed#"$TAG_PREFIX"}
		want=${tag#"$TAG_PREFIX"}
		if [ -n "${XENOLITH_CLI_FORCE:-}" ]; then
			say "Reinstalling over $installed"
		elif [ "$have" = "$want" ]; then
			say "xenolith-cli $installed is already installed in $bin_dir"
			exit 0
		elif [ "$(version_max "$have" "$want")" = "$have" ]; then
			if [ -z "${XENOLITH_CLI_VERSION:-}" ]; then
				say "The installed $installed is newer than the latest release $tag; nothing to do."
				say "(XENOLITH_CLI_FORCE=1 replaces it anyway.)"
				exit 0
			fi
			say "Downgrading $installed -> $tag"
		else
			say "Updating $installed -> $tag"
		fi
		;;
	*) say "Replacing a local build ($installed) with $tag" ;;
esac

asset="xenolith-cli-$triple.tar.gz"
# A mirror override in the spirit of the CLI's own releaseSourceUrl setting — also what the
# automated test of this script uses (file:// URLs).
base_url="${XENOLITH_RELEASE_BASE:-https://github.com/$REPO/releases/download/$tag}"

tmp=$(mktemp -d "${TMPDIR:-/tmp}/xenolith-install.XXXXXX")
trap 'rm -rf "$tmp"' EXIT

# --- download + verify ----------------------------------------------------------

say "Downloading $asset ..."
curl -fsSL "$base_url/$asset" -o "$tmp/$asset" || die "download failed: $base_url/$asset"
curl -fsSL "$base_url/$asset.sha256" -o "$tmp/$asset.sha256" || die "download failed: $base_url/$asset.sha256"

# The sidecar is "<hex>  <name>" (sha256sum format); only the hash is compared.
expected=$(awk '{print $1}' "$tmp/$asset.sha256")
case "$(uname -s)" in
	Darwin) actual=$(shasum -a 256 "$tmp/$asset" | awk '{print $1}') ;;
	*) actual=$(sha256sum "$tmp/$asset" | awk '{print $1}') ;;
esac
[ "$actual" = "$expected" ] || die "checksum mismatch:
  expected $expected
  got      $actual"

# --- install --------------------------------------------------------------------

mkdir -p "$bin_dir"

tar -xzf "$tmp/$asset" -C "$tmp"
[ -f "$tmp/xenolith-cli" ] || die "archive did not contain a ./xenolith-cli binary"

chmod +x "$tmp/xenolith-cli"

# Run the new binary before it replaces a working one.
"$tmp/xenolith-cli" --version >/dev/null 2>&1 \
	|| die "the downloaded xenolith-cli does not run on this machine ($os $arch); nothing was replaced"

# Atomic-ish replace: mv within one filesystem; a concurrent xenolith-cli keeps running (unix).
mv -f "$tmp/xenolith-cli" "$bin_dir/xenolith-cli"

# The pre-categories tool was named xenolith-installer-cli; keep the old name working.
ln -sf xenolith-cli "$bin_dir/xenolith-installer-cli"

case ":$PATH:" in
	*":$bin_dir:"*)
		found=$(command -v xenolith-cli 2>/dev/null || true)
		if [ -n "$found" ] && [ "$found" != "$bin_dir/xenolith-cli" ]; then
			say "note: $found comes first in your PATH and shadows the one installed here"
		fi
		;;
	*) say "note: $bin_dir is not in your PATH — add it:
  export PATH=\"$bin_dir:\$PATH\"" ;;
esac

say "Installed:"
"$bin_dir/xenolith-cli" --version
say "Next: xenolith-cli install   # engine + native toolchains"
