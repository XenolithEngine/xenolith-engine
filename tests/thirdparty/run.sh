#!/usr/bin/env bash
#
# Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
#
# Build + run driver for the third-party library test suite.
#
# The same sources are built for the Linux host and for x86_64-pc-windows-msvc
# and run on both (the Windows binary under wine), because the shipped libraries
# are separate builds per target: a library can be perfectly healthy on one and
# broken on the other, and only running both shows it.
#
# Usage:
#   ./run.sh [options] [test ...]
#     test...        run only these tests (default: all)
#     --no-build     skip the build step, use the existing binaries
#     --host-only    build/run only the host target (no Windows / wine)
#     --win-only     build/run only x86_64-pc-windows-msvc
#     --network      also run the tests that need the public internet
#     -v|--verbose   print the full output of each run, not just failures
#
# Exit status is 0 only when every target ran with zero failures.

set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HOST_TARGET="x86_64-unknown-linux-gnu"
WIN_TARGET="x86_64-pc-windows-msvc"
OUT="$HERE/stappler-build"
HOST_BIN="$OUT/$HOST_TARGET/debug/cc/thirdpartytest"
WIN_BIN="$OUT/$WIN_TARGET/debug/cc/thirdpartytest.exe"

DO_BUILD=1
DO_HOST=1
DO_WIN=1
VERBOSE=0
ARGS=()

for arg in "$@"; do
	case "$arg" in
		--no-build) DO_BUILD=0 ;;
		--host-only) DO_WIN=0 ;;
		--win-only) DO_HOST=0 ;;
		--network) ARGS+=(--network) ;;
		-v|--verbose) VERBOSE=1 ;;
		-h|--help) sed -n '2,22p' "$0"; exit 0 ;;
		-*) echo "unknown option: $arg" >&2; exit 2 ;;
		*) ARGS+=("$arg") ;;
	esac
done

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

status=0

report() { # <label> <logfile> <exit code>
	local label="$1" log="$2" code="$3"
	local summary
	summary="$(grep -E '^[0-9]+ checks' "$log" | tail -1)"
	printf '%-10s exit=%-3s %s\n' "$label" "$code" "${summary:-<no summary - the run died early>}"
	if [[ "$VERBOSE" == 1 ]]; then
		sed 's/^/    /' "$log"
	elif [[ "$code" != 0 ]]; then
		grep -E '^\[FAIL\]|^====|^       ' "$log" | grep -B1 -A3 '^\[FAIL\]' | sed 's/^/    /'
	fi
	[[ "$code" == 0 ]] || status=1
}

if [[ "$DO_HOST" == 1 ]]; then
	if [[ "$DO_BUILD" == 1 ]]; then
		echo "== building host ($HOST_TARGET) =="
		make -C "$HERE" STAPPLER_TARGET="$HOST_TARGET" -j8 >/dev/null \
			|| { echo "host build FAILED" >&2; exit 1; }
	fi
	[[ -x "$HOST_BIN" ]] || { echo "missing host binary: $HOST_BIN" >&2; exit 1; }
	echo "== running host =="
	LC_ALL=C "$HOST_BIN" "${ARGS[@]+"${ARGS[@]}"}" >"$WORK/host" 2>&1
	report host "$WORK/host" "$?"
fi

if [[ "$DO_WIN" == 1 ]]; then
	if [[ "$DO_BUILD" == 1 ]]; then
		echo "== building windows ($WIN_TARGET) =="
		make -C "$HERE" STAPPLER_TARGET="$WIN_TARGET" -j8 >/dev/null \
			|| { echo "windows build FAILED" >&2; exit 1; }
	fi
	[[ -f "$WIN_BIN" ]] || { echo "missing windows binary: $WIN_BIN" >&2; exit 1; }
	command -v wine >/dev/null || { echo "wine not found (use --host-only)" >&2; exit 1; }
	echo "== running windows (wine) =="
	WINEDEBUG=-all LC_ALL=C wine "$WIN_BIN" "${ARGS[@]+"${ARGS[@]}"}" 2>/dev/null >"$WORK/win"
	report windows "$WORK/win" "$?"
fi

echo "----------------------------------------"
if [[ "$status" == 0 ]]; then
	echo "ALL TARGETS PASSED"
else
	echo "FAILURES - see README.adoc for the known-broken areas"
fi
exit "$status"
