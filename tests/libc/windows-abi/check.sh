#!/usr/bin/env bash
#
# Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
#
# Compile-time WINAPI-parity check for the Windows __SPRT_* socket constants.
#
# Parses check.cpp with clang in x86_64-pc-windows-msvc mode against the live
# Windows SDK headers vendored under runtime/toolchains/src/xwin/splat. No code
# is emitted or linked: -fsyntax-only means the exit status is the whole result.
#   0  -> every checked __SPRT_* constant equals its Winsock value
#   !0 -> clang prints the diverging static_assert (constant name in the message)
#
# Usage: ./check.sh [--arch x86_64|aarch64] [-v]
# Requires: clang (any recent version) and a populated splat/ (see
# runtime/toolchains/src.mk :: xwin/splat).

set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/../../.." && pwd)"
SPLAT="$REPO/runtime/toolchains/src/xwin/splat"
SPRT_INCLUDE="$REPO/runtime/include"

ARCH="x86_64"
VERBOSE=0
while [[ $# -gt 0 ]]; do
	case "$1" in
		--arch) ARCH="${2:-x86_64}"; shift ;;
		--arch=*) ARCH="${1#*=}" ;;
		x86_64|aarch64) ARCH="$1" ;;
		-v|--verbose) VERBOSE=1 ;;
		-h|--help) sed -n '2,18p' "$0"; exit 0 ;;
		*) echo "winsock-abi: unknown argument '$1'" >&2; exit 2 ;;
	esac
	shift
done

CLANG="${CLANG:-clang}"
TARGET="$ARCH-pc-windows-msvc"

if ! command -v "$CLANG" >/dev/null 2>&1; then
	echo "winsock-abi: clang not found (set CLANG=...)" >&2; exit 2
fi
if [[ ! -d "$SPLAT/sdk/include/um" ]]; then
	echo "winsock-abi: SKIP - Windows SDK not vendored at $SPLAT" >&2
	echo "             run the xwin splat step (runtime/toolchains/src.mk) first." >&2
	exit 0
fi

CMD=(
	"$CLANG" --target="$TARGET"
	-fsyntax-only -fms-compatibility -fms-extensions
	-Wno-ignored-attributes -Wno-nonportable-include-path -Wno-macro-redefined
	-Wno-invalid-offsetof   # MONITORINFOEXW derives from MONITORINFO (non-standard-layout) in both abi + SDK
	-isystem "$SPLAT/crt/include"
	-isystem "$SPLAT/sdk/include/ucrt"
	-isystem "$SPLAT/sdk/include/shared"
	-isystem "$SPLAT/sdk/include/um"
	-I "$SPRT_INCLUDE"
	# NB: the abi/ headers are include_libc-free, so no -idirafter overlay is needed.
)

# check.cpp / check-types.cpp cover the socket table; check-<abi>.cpp pin each
# wrappers/windows/abi/<name>.h header against the SDK (see abi_check.h).
rc=0
for tu in "$HERE"/check.cpp "$HERE"/check-types.cpp "$HERE"/check-*.cpp; do
	[[ -e "$tu" ]] || continue
	[[ "$VERBOSE" == 1 ]] && printf '%s ' "${CMD[@]}" "$tu" && echo
	"${CMD[@]}" "$tu" || rc=1
done

if [[ "$rc" == 0 ]]; then
	echo "winsock-abi [$TARGET]: PASS - __SPRT_* socket constants and types match the Windows SDK"
	exit 0
else
	echo "winsock-abi [$TARGET]: FAIL - see the static_assert message(s) above" >&2
	exit 1
fi
