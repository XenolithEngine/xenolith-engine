#!/usr/bin/env bash
#
# Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
#
# Build + run + diff driver for the freestanding-libc behavioural test suite.
#
# The same sources (c/*.cpp) are compiled twice: once for the Linux/glibc host
# (the reference) and once for x86_64-pc-windows-msvc (the freestanding
# runtime/libc_impl). Every test prints deterministic output; this script runs
# each test individually on both targets and diffs the two outputs. A function
# whose Windows behaviour matches Linux produces an IDENTICAL line; any divergence
# (or a crash / nonzero exit) is reported with the offending diff.
#
# Tests are run one at a time (via `libctest <name>`) rather than as a single
# full run so that a crash in one test cannot truncate the buffered output of the
# others, and so each function group can be diffed in isolation.
#
# Usage:
#   ./compare.sh [options] [test ...]
#     test...        run only these tests (default: all, via `libctest --list`)
#     --no-build     skip the build step, use existing binaries
#     --host-only    build/run only the host target (no Windows / wine)
#     -v|--verbose   print the full diff for every diverging test
#
# Requirements: a working `make` toolchain for both targets and `wine` to run the
# Windows binary on a non-Windows host.

set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HOST_TARGET="x86_64-unknown-linux-gnu"
WIN_TARGET="x86_64-pc-windows-msvc"
OUT="$HERE/stappler-build"
HOST_BIN="$OUT/$HOST_TARGET/debug/cc/libctest"
WIN_BIN="$OUT/$WIN_TARGET/debug/cc/libctest.exe"

DO_BUILD=1
HOST_ONLY=0
VERBOSE=0
SELECT=()

for arg in "$@"; do
	case "$arg" in
		--no-build) DO_BUILD=0 ;;
		--host-only) HOST_ONLY=1 ;;
		-v|--verbose) VERBOSE=1 ;;
		-h|--help) sed -n '2,30p' "$0"; exit 0 ;;
		-*) echo "unknown option: $arg" >&2; exit 2 ;;
		*) SELECT+=("$arg") ;;
	esac
done

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

note() { printf '%s\n' "$*"; }

if [[ "$DO_BUILD" == 1 ]]; then
	note "== building host ($HOST_TARGET) =="
	make -C "$HERE" STAPPLER_TARGET="$HOST_TARGET" -j8 >/dev/null || { echo "host build FAILED" >&2; exit 1; }
	if [[ "$HOST_ONLY" == 0 ]]; then
		note "== building windows ($WIN_TARGET) =="
		make -C "$HERE" STAPPLER_TARGET="$WIN_TARGET" -j8 >/dev/null || { echo "windows build FAILED" >&2; exit 1; }
	fi
fi

[[ -x "$HOST_BIN" ]] || { echo "missing host binary: $HOST_BIN" >&2; exit 1; }
if [[ "$HOST_ONLY" == 0 ]]; then
	[[ -f "$WIN_BIN" ]] || { echo "missing windows binary: $WIN_BIN" >&2; exit 1; }
	command -v wine >/dev/null || { echo "wine not found (use --host-only)" >&2; exit 1; }
fi

run_host() { LC_ALL=C "$HOST_BIN" "$1" 2>/dev/null; }
run_win()  { WINEDEBUG=-all LC_ALL=C wine "$WIN_BIN" "$1" 2>/dev/null; }

# Determine the test list.
if [[ "${#SELECT[@]}" -gt 0 ]]; then
	TESTS=("${SELECT[@]}")
else
	mapfile -t TESTS < <(run_host --list)
fi

pass=0; fail=0; failed_names=()
for t in "${TESTS[@]}"; do
	run_host "$t" >"$WORK/h" ; he=$?
	if [[ "$HOST_ONLY" == 1 ]]; then
		printf '%-18s host_exit=%s\n' "$t" "$he"
		continue
	fi
	run_win "$t" >"$WORK/w" ; we=$?
	if diff -q "$WORK/h" "$WORK/w" >/dev/null 2>&1 && [[ "$he" == 0 && "$we" == 0 ]]; then
		printf '%-18s OK   (host_exit=%s win_exit=%s)\n' "$t" "$he" "$we"
		pass=$((pass+1))
	else
		n=$(diff "$WORK/h" "$WORK/w" | grep -c '^[<>]')
		printf '%-18s FAIL (host_exit=%s win_exit=%s, %s diff lines)\n' "$t" "$he" "$we" "$n"
		fail=$((fail+1)); failed_names+=("$t")
		if [[ "$VERBOSE" == 1 ]]; then
			diff -u "$WORK/h" "$WORK/w" | sed 's/^/    /'
		fi
	fi
done

echo "----------------------------------------"
if [[ "$HOST_ONLY" == 1 ]]; then
	echo "host-only run complete (${#TESTS[@]} tests)"
	exit 0
fi
echo "identical: $pass   diverging: $fail   (of ${#TESTS[@]})"
if [[ "$fail" -gt 0 ]]; then
	echo "diverging tests: ${failed_names[*]}"
	echo "re-run with -v to see diffs, or: diff <(LC_ALL=C $HOST_BIN <name>) <(WINEDEBUG=-all LC_ALL=C wine $WIN_BIN <name>)"
	exit 1
fi
echo "ALL TESTS IDENTICAL"
