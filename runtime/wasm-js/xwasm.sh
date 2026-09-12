#!/usr/bin/env bash
# xwasm — build a wasm test, serve it with COOP/COEP (needed for SharedArrayBuffer / threads),
# and open it in the browser. One command to see a test running.
#
#   ./xwasm.sh <test>            build + serve + open
#   ./xwasm.sh <test> --no-open  build + serve, print the URL (don't launch a browser)
#   ./xwasm.sh --list            list available tests
#
# Tests:  libc | thread | bundled | wwin
#
# Toolchain paths auto-detect for a stock installer layout, falling back to the toolchain
# built inside this repository (runtime/toolchains); override with env vars:
#   XENOLITH_WASM_TARGET = wasm32-unknown-unknown (default) or wasm64-unknown-unknown
#   XENOLITH_HOST        = <host toolchain dir> (has bin/make + host.mk)
#   XENOLITH_TARGET      = path to the target.mk of XENOLITH_WASM_TARGET
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
PORT="${XENOLITH_PORT:-8080}"
TRIPLE="${XENOLITH_WASM_TARGET:-wasm32-unknown-unknown}"
case "$TRIPLE" in
	wasm32-unknown-unknown|wasm64-unknown-unknown) ;;
	*) echo "unsupported XENOLITH_WASM_TARGET '$TRIPLE'"; exit 1 ;;
esac

# test -> "make-dir  built.wasm  served.wasm  page.html"
tests_libc="tests/libc         libctest.wasm     libctest.wasm     libc-test.html"
tests_thread="tests/wthread     wthreadtest.wasm  wthreadtest.wasm  wthread.html"
tests_bundled="tests/wasm       wasmtest.wasm     bundled-demo.wasm bundled-demo.html"
tests_wwin="tests/wwin          wwin.wasm         wwin.wasm         wwin.html"

list() { echo "available tests: libc thread bundled wwin"; }

[ "${1:-}" = "--list" ] && { list; exit 0; }
TEST="${1:-libc}"; shift || true
NO_OPEN=0; [ "${1:-}" = "--no-open" ] && NO_OPEN=1

eval "spec=\${tests_${TEST}:-}"
[ -z "$spec" ] && { echo "unknown test '$TEST'"; list; exit 1; }
read -r MK_DIR BUILT SERVED PAGE <<<"$spec"
if [ "$TRIPLE" = wasm64-unknown-unknown ] && [ "$TEST" = wwin ]; then
	echo "wwin needs WebGPU, which is not supported on wasm64 yet (webgpu.mjs assumes 32-bit pointers)"
	exit 1
fi

# --- resolve toolchain -----------------------------------------------------------------
HOST="${XENOLITH_HOST:-}"
if [ -z "$HOST" ]; then
	# H: installer layout name, R: runtime/toolchains/hosts name of the same host
	case "$(uname -s)-$(uname -m)" in
		Darwin-arm64) H=aarch64-apple-macosx; R=$H ;;
		Darwin-x86_64) H=x86_64-apple-macosx; R=$H ;;
		Linux-x86_64) H=x86_64-linux-gnu; R=x86_64-unknown-linux-gnu ;;
		Linux-aarch64) H=aarch64-linux-gnu; R=aarch64-unknown-linux-gnu ;;
		*) H=""; R="" ;;
	esac
	HOST="$HOME/.local/share/xenolith/data/toolchains/hosts/$H"
	[ -x "$HOST/bin/make" ] || HOST="$REPO/runtime/toolchains/hosts/$R"
fi
MAKE="$HOST/bin/make"; HOST_MK="$HOST/host.mk"
[ -x "$MAKE" ] || { echo "host make not found at $MAKE — set XENOLITH_HOST"; exit 1; }

TARGET="${XENOLITH_TARGET:-}"
if [ -z "$TARGET" ]; then
	for c in "$HOME/$TRIPLE/target.mk" "/Users/$(whoami)/$TRIPLE/target.mk" \
			"$REPO/runtime/toolchains/targets/$TRIPLE/target.mk"; do
		[ -f "$c" ] && TARGET="$c" && break
	done
fi
[ -f "$TARGET" ] || { echo "wasm target.mk not found — set XENOLITH_TARGET=/path/to/$TRIPLE/target.mk"; exit 1; }

# --- build -----------------------------------------------------------------------------
echo "▸ building $MK_DIR for $TRIPLE …"
"$MAKE" -C "$REPO/$MK_DIR" STAPPLER_TARGET="$TRIPLE" \
	STAPPLER_HOST_FILE="$HOST_MK" STAPPLER_TARGET_FILE="$TARGET" -j8
cp "$REPO/$MK_DIR/stappler-build/$TRIPLE/debug/cc/$BUILT" "$HERE/$SERVED"
echo "▸ built → $SERVED"

# --- serve (COOP/COEP) + open ----------------------------------------------------------
# kill a previous server on this port
lsof -ti tcp:"$PORT" 2>/dev/null | xargs kill 2>/dev/null || true
( cd "$HERE" && python3 coop-server.py "$PORT" >/tmp/xwasm-server.log 2>&1 & )
sleep 1
URL="http://127.0.0.1:$PORT/$PAGE"
echo "▸ serving $URL  (COOP/COEP on, log: /tmp/xwasm-server.log)"
if [ "$NO_OPEN" = 0 ]; then
	if command -v open >/dev/null; then open "$URL"; elif command -v xdg-open >/dev/null; then xdg-open "$URL"; fi
	echo "▸ opened in browser"
fi
