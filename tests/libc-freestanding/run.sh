#!/usr/bin/env bash
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

# Builds lfstest and runs it, natively or under qemu-user for another Linux arch.
#
#   run.sh [--target TRIPLE] [--release] [--no-build] [--qemu-cpu MODEL] [-- lfstest arguments...]
#
# Under qemu only the conformance verdict means anything; times do not. --qemu-cpu picks
# the emulated CPU, e.g. max,sve-default-vector-length=64 to run the SVE variant at a
# 512-bit vector length, or neoverse-n1 for a CPU without SVE.

set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ENGINE="$(cd "$HERE/../.." && pwd)"

TARGET=""
BUILD=debug
DO_BUILD=1
QEMU_CPU_ARGS=()
while [[ $# -gt 0 ]]; do
	case "$1" in
		--target) TARGET="$2"; shift ;;
		--target=*) TARGET="${1#--target=}" ;;
		--release) BUILD=release ;;
		--no-build) DO_BUILD=0 ;;
		--qemu-cpu) QEMU_CPU_ARGS=(-cpu "$2"); shift ;;
		--qemu-cpu=*) QEMU_CPU_ARGS=(-cpu "${1#--qemu-cpu=}") ;;
		--) shift; break ;;
		*) break ;;
	esac
	shift
done

HOST_TARGET="${HOST_TARGET:-$(uname -m)-unknown-linux-gnu}"
[[ -n "$TARGET" ]] || TARGET="$HOST_TARGET"

if [[ "$DO_BUILD" == 1 ]]; then
	MAKE_ARGS=(STAPPLER_TARGET="$TARGET")
	[[ "$BUILD" == release ]] && MAKE_ARGS+=(RELEASE=1)
	make -C "$HERE" "${MAKE_ARGS[@]}" -j8 >/dev/null || { echo "build FAILED for $TARGET" >&2; exit 1; }
fi

BIN="$HERE/stappler-build/$TARGET/$BUILD/cc/lfstest"
[[ -x "$BIN" ]] || { echo "missing binary: $BIN" >&2; exit 1; }

if [[ "$TARGET" == "$HOST_TARGET" ]]; then
	exec "$BIN" "$@"
fi

SYSROOT=""
for d in "$ENGINE/toolchains/targets/$TARGET" "$ENGINE/runtime/toolchains/targets/$TARGET"; do
	if [[ -f "$d/target.mk" ]]; then SYSROOT="$d"; break; fi
done
[[ -n "$SYSROOT" ]] || { echo "target $TARGET is not installed" >&2; exit 1; }

QEMU="qemu-${TARGET%%-*}"
command -v "$QEMU" >/dev/null || { echo "$QEMU not found" >&2; exit 1; }

# The loader and libc are looked up by absolute path, so lib and lib64 both link
# to the sysroot's libraries (the same root tests/libc/compare.sh builds).
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
ROOT="$WORK/qemu-root"
mkdir -p "$ROOT/lib" "$ROOT/usr"
ln -s lib "$ROOT/lib64"
ln -s ../lib "$ROOT/usr/lib"
for f in "$SYSROOT"/lib/*.so* "$SYSROOT"/usr/lib/*.so*; do
	[[ -e "$f" ]] && ln -sf "$f" "$ROOT/lib/$(basename "$f")"
done
if [[ "$TARGET" == *-linux-musl && -e "$ROOT/lib/libc.so" ]]; then
	ln -sf "$SYSROOT/lib/libc.so" "$ROOT/lib/ld-musl-${TARGET%%-*}.so.1"
fi

"$QEMU" "${QEMU_CPU_ARGS[@]}" -L "$ROOT" -E LD_LIBRARY_PATH=/lib "$BIN" "$@"
