#!/usr/bin/env bash
#
# Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
#
# Build the standalone COM smoke test for x86_64-pc-windows-msvc and run it under
# wine. Proves the freestanding Windows COM support layer
# (sprt/wrappers/windows/comdef.h) drives a real COM runtime: _bstr_t over the
# oleaut32 BSTR API, CoInitializeEx, and _com_ptr_t CreateInstance / QueryInterface.
#
# Usage:
#   tests/com/run.sh [--no-build]
#
# Requirements: the windows-msvc runtime toolchain and `wine`.

set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WIN_TARGET="x86_64-pc-windows-msvc"
BIN="$HERE/stappler-build/$WIN_TARGET/debug/cc/comtest.exe"

DO_BUILD=1
for arg in "$@"; do
	case "$arg" in
		--no-build) DO_BUILD=0 ;;
		-h|--help) sed -n '2,16p' "$0"; exit 0 ;;
		*) echo "unknown option: $arg" >&2; exit 2 ;;
	esac
done

if [[ "$DO_BUILD" == 1 ]]; then
	echo "== building COM test ($WIN_TARGET) =="
	make -C "$HERE" STAPPLER_TARGET="$WIN_TARGET" -j8 >/dev/null
fi

[[ -f "$BIN" ]] || { echo "missing windows binary: $BIN" >&2; exit 1; }
command -v wine >/dev/null || { echo "wine not found" >&2; exit 1; }

echo "== running under wine =="
WINEDEBUG=-all LC_ALL=C wine "$BIN"
