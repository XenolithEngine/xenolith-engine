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

# The repo carries two release lines: sdk-v* (the engine/SDK) and cli-v* (this tool). "Latest
# release" on GitHub points at the engine line, so the CLI release is found by listing releases
# and taking the first cli-v* tag — the API orders them newest-first.
if [ -n "${XENOLITH_CLI_VERSION:-}" ]; then
	tag=$XENOLITH_CLI_VERSION
	case $tag in
		$TAG_PREFIX*) ;;
		*) die "XENOLITH_CLI_VERSION must start with '$TAG_PREFIX', got '$tag'" ;;
	esac
	say "Using requested release $tag"
else
	tag=$(curl -fsSL "$API/releases?per_page=100" \
		| grep -oE "\"tag_name\": *\"$TAG_PREFIX[^\"]+\"" \
		| head -n 1 \
		| sed 's/.*"\(.*\)"/\1/') \
		|| die "could not list releases from $API"
	[ -n "$tag" ] || die "no '$TAG_PREFIX*' release found in $REPO — has a CLI release been cut?"
	say "Latest CLI release: $tag"
fi

asset="xenolith-cli-$triple.tar.gz"
# A mirror override in the spirit of the CLI's own releaseSourceUrl setting — also what the
# automated test of this script uses (file:// URLs).
base_url="${XENOLITH_RELEASE_BASE:-https://github.com/$REPO/releases/download/$tag}"

tmp=$(mktemp -d "${TMPDIR:-/tmp}/xenolith-install.XXXXXX")
trap 'rm -rf "$tmp"' EXIT

# --- download + verify ----------------------------------------------------------

say "Downloading $asset ..."
curl -fSL "$base_url/$asset" -o "$tmp/$asset" || die "download failed: $base_url/$asset"
curl -fSL "$base_url/$asset.sha256" -o "$tmp/$asset.sha256" || die "download failed: $base_url/$asset.sha256"

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

bin_dir=${XENOLITH_BIN_DIR:-$HOME/.local/bin}
mkdir -p "$bin_dir"

tar -xzf "$tmp/$asset" -C "$tmp"
[ -f "$tmp/xenolith-cli" ] || die "archive did not contain a ./xenolith-cli binary"

# Atomic-ish replace: mv within one filesystem; a concurrent xenolith-cli keeps running (unix).
chmod +x "$tmp/xenolith-cli"
mv -f "$tmp/xenolith-cli" "$bin_dir/xenolith-cli"

# The pre-categories tool was named xenolith-installer-cli; keep the old name working.
ln -sf xenolith-cli "$bin_dir/xenolith-installer-cli"

case ":$PATH:" in
	*":$bin_dir:"*) ;;
	*) say "note: $bin_dir is not in your PATH — add it:
  export PATH=\"$bin_dir:\$PATH\"" ;;
esac

say "Installed:"
"$bin_dir/xenolith-cli" --version
say "Next: xenolith-cli install   # engine + native toolchains"
