#!/usr/bin/env bash
# Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
# SPDX-License-Identifier: MIT
#
# build-nuttx-export.sh — reproduce a Xenolith-ready nuttx-export package from
# a NuttX source tree. This script captures every NuttX-side work-around the
# target-nuttx integration needs so they are not lost:
#
#   * the Xenolith LLVM toolchain is used as the NuttX target compiler
#     (CONFIG_ARM64_TOOLCHAIN_CLANG), with the system gcc kept as HOSTCC
#     (NuttX builds its own host tools via `cc`, which must NOT resolve to the
#      cross-clang or the build dies at <sys/utsname.h>);
#   * `./tools/configure.sh` alone fails on `'$BINDIR/arch/dummy/Kconfig' not
#     found` because arch/dummy/Kconfig is a make-target generated on the first
#     real `make` invocation — a single `make olddefconfig` after configure
#     settles the file and .config;
#   * the stock arm64 defconfigs ship with options that the Xenolith runtime
#     needs flipped (TLS_NELEM>0 for pthread_key, the C++ EH runtime on, env
#     support on). We apply them via kconfig-tweak between configure and export.
#
# Inputs (env):
#   NUTTX_DIR      required — path to a NuttX source checkout (master ~87260499
#                  or newer). apps/ must sit as its sibling (NuttX refuses
#                  apps/ inside its own tree).
#   NUTTX_CONFIG   required — <board>:<config> (e.g. qemu-armv8a:nsh_gicv2,
#                  raspberrypi-4b:fb, rv-virt:nsh64). Must ship a defconfig
#                  with CONFIG_ARCH_TOOLCHAIN_CLANG=y / CONFIG_ARM64_TOOLCHAIN_CLANG=y
#                  (or equivalent for the arch).
#   XENOLITH_HOST_TOOLCHAIN  optional — path to the Xenolith LLVM bin dir
#                  (the one containing clang/clang++/ld.lld/llvm-ar). If unset,
#                  the script tries the conventional locations.
#   OUTPUT_DIR     optional — where to drop nuttx-export-<ver>.tar.gz
#                  (default: $NUTTX_DIR).
#
# Output: prints the absolute path to the produced nuttx-export-<ver>.tar.gz.
#
# Usage:
#   NUTTX_DIR=~/code/nuttx NUTTX_CONFIG=qemu-armv8a:nsh_gicv2 \
#       ./build-nuttx-export.sh
#
# Then feed the resulting tarball to the target-nuttx driver:
#   make -C runtime/toolchains target-aarch64-nuttx-none-elf \
#       NUTTX_EXPORT=/path/to/nuttx-export-<ver>.tar.gz

set -euo pipefail

: "${NUTTX_DIR:?NUTTX_DIR is required (path to a NuttX source tree)}"
: "${NUTTX_CONFIG:?NUTTX_CONFIG is required (e.g. qemu-armv8a:nsh_gicv2)}"
NUTTX_DIR="$(readlink -f "$NUTTX_DIR")"
OUTPUT_DIR="${OUTPUT_DIR:-$NUTTX_DIR}"

# Locate the Xenolith LLVM toolchain. Try explicit env first, then conventional
# paths inside an installed SDK / repo checkout, then fail.
XENOLITH_HOST_TOOLCHAIN="${XENOLITH_HOST_TOOLCHAIN:-}"
if [ -z "$XENOLITH_HOST_TOOLCHAIN" ]; then
    for candidate in \
        "${XENOLITH_ENGINE:-/xenolith-engine}/toolchains/hosts/$(uname -m)-unknown-linux-gnu/bin" \
        "$HOME/.local/share/xenolith/data/engines/master/toolchains/hosts/$(uname -m)-unknown-linux-gnu/bin" \
        "$(xenolith-installer-cli detect 2>/dev/null | head -1)"; do
        if [ -x "$candidate/clang" ]; then
            XENOLITH_HOST_TOOLCHAIN="$candidate"
            break
        fi
    done
fi
[ -x "$XENOLITH_HOST_TOOLCHAIN/clang" ] || {
    echo "error: Xenolith LLVM clang not found. Set XENOLITH_HOST_TOOLCHAIN to the bin/ dir." >&2
    exit 1
}

# CRITICAL PATH layout: system /usr/bin FIRST (so cc/gcc for HOSTCC stay system),
# Xenolith toolchain LAST (clang/clang++/ld.lld/llvm-ar for the target). There
# is no /usr/bin/clang on a stock Ubuntu image, so `clang` resolves to Xenolith.
export PATH="/usr/bin:/bin:/usr/local/bin:$HOME/.local/bin:$XENOLITH_HOST_TOOLCHAIN"
echo "info: HOSTCC=$(command -v cc)  CLANG=$(command -v clang)  LD.LLD=$(command -v ld.lld)"

# Sanity: make sure kconfig-tweak (kconfiglib) is reachable. NuttX uses it in
# configure.sh / sethost.sh; we also call it ourselves below.
if ! command -v kconfig-tweak >/dev/null 2>&1; then
    echo "error: kconfig-tweak not on PATH. Install kconfiglib (e.g. \`pip install kconfiglib\`)." >&2
    exit 1
fi
command -v unzip >/dev/null 2>&1 || { echo "error: \`unzip\` required (NuttX downloads dtc.zip)." >&2; exit 1; }
command -v dtc  >/dev/null 2>&1 || { echo "warn: \`dtc\` (device-tree-compiler) recommended." >&2; }

cd "$NUTTX_DIR"

echo "info: distclean + configure $NUTTX_CONFIG"
make distclean >/dev/null 2>&1 || true
# configure.sh exits non-zero with `'$BINDIR/arch/dummy/Kconfig' not found`
# (KconfigError) but has already done the file-copy / defconfig placement work
# by that point. The dummy/Kconfig file is a make-target produced on the first
# real `make`, which olddefconfig triggers below. So tolerate the exit code.
./tools/configure.sh -l "$NUTTX_CONFIG" || true

# `olddefconfig` triggers the make rule that creates arch/dummy/Kconfig, then
# re-runs genconfig successfully and settles .config. It is non-interactive —
# feed /dev/null as stdin (an earlier `yes "" | ...` form tripped SIGPIPE 141
# under `set -o pipefail` when olddefconfig closed its stdin early).
echo "info: olddefconfig (settles arch/dummy/Kconfig + .config)"
make olddefconfig </dev/null >/dev/null

# Apply the CONFIG_* overrides the Xenolith runtime needs. These are the
# deltas between the stock NuttX arm64 defconfigs and what the runtime
# expects; see docs/platforms/nuttx.adoc §"Required CONFIG_.setconfig overrides".
echo "info: applying Xenolith CONFIG_* overrides"
# pthread_key_* slots (arm64 has no __thread; runtime uses pthread keys for TLS).
kconfig-tweak --file .config --set-val CONFIG_TLS_NELEM 8        || true
kconfig-tweak --file .config --enable  CONFIG_LIBCXX             || true
kconfig-tweak --file .config --enable  CONFIG_LIBCXXABI          || true
kconfig-tweak --file .config --enable  CONFIG_CXX_EXCEPTION      || true
# Inverted-convention opt-out: keep environment support on.
kconfig-tweak --file .config --disable CONFIG_DISABLE_ENVIRON    || true
# Re-settle after the overrides.
make olddefconfig </dev/null >/dev/null

echo "info: make export"
make export

# NuttX writes nuttx-export[-<CONFIG_VERSION_STRING>].tar.gz into $NUTTX_DIR.
TARBALL="$(ls -1 "$NUTTX_DIR"/nuttx-export*.tar.gz 2>/dev/null | head -1 || true)"
[ -n "$TARBALL" ] || { echo "error: make export did not produce nuttx-export-*.tar.gz" >&2; exit 1; }

# Move to OUTPUT_DIR if requested elsewhere.
if [ "$(readlink -f "$OUTPUT_DIR")" != "$NUTTX_DIR" ]; then
    mv "$TARBALL" "$OUTPUT_DIR/"
    TARBALL="$OUTPUT_DIR/$(basename "$TARBALL")"
fi

echo "ok: $TARBALL"
echo "$TARBALL"
