#!/usr/bin/env bash
#
# Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
#
# Build + run + diff driver for the freestanding-libc behavioural test suite.
#
# The same sources (c/*.cpp) are compiled twice: once for the Linux/glibc host
# (the reference) and once for a second target - x86_64-pc-windows-msvc (the
# freestanding runtime/libc_impl) by default. Every test prints deterministic
# output; this script runs each test individually on both targets and diffs the
# two outputs. A function whose behaviour on the target matches the host produces
# an IDENTICAL line; any divergence (or a crash / nonzero exit) is reported with
# the offending diff.
#
# The second target is run by what its triple calls for: a *-windows-msvc binary
# under wine, a Linux binary of another arch under qemu-user (`qemu-<arch>`). For
# the latter the script lays out a root for `qemu -L` from the target's sysroot,
# so the binary's /lib64/ld-linux-*.so or /lib/ld-musl-*.so resolves there.
#
# Tests are run one at a time (via `libctest <name>`) rather than as a single
# full run so that a crash in one test cannot truncate the buffered output of the
# others, and so each function group can be diffed in isolation.
#
# Usage:
#   ./compare.sh [options] [test ...]
#     test...        run only these tests (default: all, via `libctest --list`)
#     --no-build     skip the build step, use existing binaries
#     --host-only    build/run only the host target (no second target)
#     --target T     the second target triple (default: x86_64-pc-windows-msvc),
#                    e.g. loongarch64-unknown-linux-gnu
#     --qemu-lib D   extra directory whose libraries go into the qemu root's lib
#                    dir (e.g. one holding the target's libgcc_s.so.1, which
#                    glibc's pthread_exit dlopen()s)
#     --qemu-locale D  LOCPATH for the guest: a directory holding a compiled
#                    C.UTF-8 (glibc 2.35+ does not build it into libc, and the
#                    host's locales are not visible to the target's glibc); the
#                    multibyte/uchar tests need it
#     -v|--verbose   print the full diff for every diverging test
#
# Requirements: a working `make` toolchain for both targets, and `wine` for a
# Windows target or `qemu-<arch>` (qemu-user) for a Linux one of another arch.

set -u
# a crashing target under qemu-user would otherwise leave qemu_*.core in the cwd
ulimit -c 0

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ENGINE="$(cd "$HERE/../.." && pwd)"
HOST_TARGET="x86_64-unknown-linux-gnu"
TGT_TARGET="x86_64-pc-windows-msvc"
OUT="$HERE/stappler-build"
HOST_BIN="$OUT/$HOST_TARGET/debug/cc/libctest"

DO_BUILD=1
HOST_ONLY=0
VERBOSE=0
QEMU_LIBS=()
QEMU_ENV=()
SELECT=()

while [[ $# -gt 0 ]]; do
	case "$1" in
		--no-build) DO_BUILD=0 ;;
		--host-only) HOST_ONLY=1 ;;
		--target) TGT_TARGET="$2"; shift ;;
		--target=*) TGT_TARGET="${1#--target=}" ;;
		--qemu-lib) QEMU_LIBS+=("$2"); shift ;;
		--qemu-lib=*) QEMU_LIBS+=("${1#--qemu-lib=}") ;;
		--qemu-locale) QEMU_ENV+=(-E "LOCPATH=$(cd "$2" && pwd)"); shift ;;
		--qemu-locale=*) QEMU_ENV+=(-E "LOCPATH=$(cd "${1#--qemu-locale=}" && pwd)") ;;
		-v|--verbose) VERBOSE=1 ;;
		-h|--help) sed -n '2,40p' "$0"; exit 0 ;;
		-*) echo "unknown option: $1" >&2; exit 2 ;;
		*) SELECT+=("$1") ;;
	esac
	shift
done

case "$TGT_TARGET" in
	*-windows-msvc) TGT_KIND=wine; TGT_BIN="$OUT/$TGT_TARGET/debug/cc/libctest.exe" ;;
	*-linux-*) TGT_KIND=qemu; TGT_BIN="$OUT/$TGT_TARGET/debug/cc/libctest" ;;
	*) echo "don't know how to run $TGT_TARGET" >&2; exit 2 ;;
esac

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

note() { printf '%s\n' "$*"; }

if [[ "$DO_BUILD" == 1 ]]; then
	note "== building host ($HOST_TARGET) =="
	make -C "$HERE" STAPPLER_TARGET="$HOST_TARGET" -j8 >/dev/null || { echo "host build FAILED" >&2; exit 1; }
	if [[ "$HOST_ONLY" == 0 ]]; then
		note "== building target ($TGT_TARGET) =="
		make -C "$HERE" STAPPLER_TARGET="$TGT_TARGET" -j8 >/dev/null || { echo "target build FAILED" >&2; exit 1; }
	fi
fi

# A root for `qemu -L`: the dynamic loader and libc are looked up by absolute
# path (/lib64/ld-linux-loongarch-lp64d.so.1, /lib/ld-musl-loongarch64.so.1), so
# both lib and lib64 hold links to the sysroot's libraries, the same lookup order
# as make/utils/defaults.mk finds the target in.
make_qemu_root() {
	local sysroot="" d f
	for d in "$ENGINE/toolchains/targets/$TGT_TARGET" "$ENGINE/runtime/toolchains/targets/$TGT_TARGET"; do
		if [[ -f "$d/target.mk" ]]; then sysroot="$d"; break; fi
	done
	[[ -n "$sysroot" ]] || { echo "target $TGT_TARGET is not installed" >&2; return 1; }
	QEMU_ROOT="$WORK/qemu-root"
	mkdir -p "$QEMU_ROOT/lib" "$QEMU_ROOT/usr"
	ln -s lib "$QEMU_ROOT/lib64"
	ln -s ../lib "$QEMU_ROOT/usr/lib"
	for f in "$sysroot"/lib/*.so* "$sysroot"/usr/lib/*.so*; do
		[[ -e "$f" ]] && ln -sf "$f" "$QEMU_ROOT/lib/$(basename "$f")"
	done
	for d in "${QEMU_LIBS[@]}"; do
		for f in "$d"/*.so*; do
			[[ -e "$f" ]] && ln -sf "$(cd "$(dirname "$f")" && pwd)/$(basename "$f")" "$QEMU_ROOT/lib/$(basename "$f")"
		done
	done
	# musl's dynamic loader is libc.so itself; the sysroot does not always carry the
	# ld-musl-<arch>.so.1 name the binary asks for
	if [[ "$TGT_TARGET" == *-linux-musl && -e "$QEMU_ROOT/lib/libc.so" ]]; then
		ln -sf "$sysroot/lib/libc.so" "$QEMU_ROOT/lib/ld-musl-${TGT_TARGET%%-*}.so.1"
	fi
}

[[ -x "$HOST_BIN" ]] || { echo "missing host binary: $HOST_BIN" >&2; exit 1; }
if [[ "$HOST_ONLY" == 0 ]]; then
	[[ -f "$TGT_BIN" ]] || { echo "missing target binary: $TGT_BIN" >&2; exit 1; }
	if [[ "$TGT_KIND" == wine ]]; then
		command -v wine >/dev/null || { echo "wine not found (use --host-only)" >&2; exit 1; }
		TGT_RUN=(env WINEDEBUG=-all wine "$TGT_BIN")
	else
		QEMU="qemu-${TGT_TARGET%%-*}"
		command -v "$QEMU" >/dev/null || { echo "$QEMU not found (use --host-only)" >&2; exit 1; }
		make_qemu_root || exit 1
		# The sysroot's glibc was configured with the build machine's absolute prefix,
		# so its built-in search path is not /lib64: dlopen("libgcc_s.so.1") from
		# pthread_exit would miss the root. Point the guest loader at it.
		TGT_RUN=("$QEMU" -L "$QEMU_ROOT" -E LD_LIBRARY_PATH=/lib "${QEMU_ENV[@]}" "$TGT_BIN")
	fi
fi

run_host() { LC_ALL=C "$HOST_BIN" "$1" 2>/dev/null; }
run_tgt()  { LC_ALL=C "${TGT_RUN[@]}" "$1" 2>/dev/null; }

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
	run_tgt "$t" >"$WORK/w" ; we=$?
	if diff -q "$WORK/h" "$WORK/w" >/dev/null 2>&1 && [[ "$he" == 0 && "$we" == 0 ]]; then
		printf '%-18s OK   (host_exit=%s tgt_exit=%s)\n' "$t" "$he" "$we"
		pass=$((pass+1))
	else
		n=$(diff "$WORK/h" "$WORK/w" | grep -c '^[<>]')
		printf '%-18s FAIL (host_exit=%s tgt_exit=%s, %s diff lines)\n' "$t" "$he" "$we" "$n"
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
	echo "re-run with -v to see diffs, or: diff <(LC_ALL=C $HOST_BIN <name>) <(LC_ALL=C ${TGT_RUN[*]} <name>)"
	exit 1
fi
echo "ALL TESTS IDENTICAL"
