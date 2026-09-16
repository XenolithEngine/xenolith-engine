#!/usr/bin/env bash
#
# Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
#
# Compile-time Darwin ABI parity check for the macOS / macOS+open targets.
#
# Three independent parts, all driven from Linux with a stock clang; nothing is
# linked, nothing is run, and no Apple hardware is involved.
#
#   sprt     the __SPRT_* tables in runtime/include/sprt/c/cross/macos_sprt/**
#            and sprt/c/sys/__sprt_darwin.h, static_asserted against the real
#            Darwin headers (check-*.cpp). Run against BOTH sysroots.
#   sysroot  the +open sysroot itself, against the real SDK: the same probe TU
#            is compiled twice and the emitted values are diffed (probe/*).
#   tbd      every symbol in the +open .tbd link stubs, against the SDK's own
#            tbds -- both that it exists and that the same library owns it.
#
# Usage: ./check.sh [--arch x86_64|aarch64|all] [--sysroot sdk|open|both]
#                   [--only sprt|sysroot|tbd] [--sdk PATH] [-v]
#
# Requires clang, and a macOS SDK to compare against. THE SDK IS NORMALLY
# ABSENT: it is not vendored, not fetched and not redistributable (its licence
# restricts use to Apple hardware). Without it this script prints SKIP and exits
# 0 -- having it is the exception, not the rule. It is looked for, in order:
#
#   1. --sdk PATH / $SP_MACOS_SDK           explicit override (an invalid
#                                           $SP_MACOS_SDK is an error, not a
#                                           silent fallback)
#   2. `xcrun --show-sdk-path`              on a real Mac, the installed SDK
#   3. runtime/toolchains/src/MacOSX.sdk    a copy dropped in by hand
#
# which is the same ladder runtime/toolchains/target-apple/init-target.mk and
# common/configure.mk use to resolve SP_MACOS_SDK for the build itself.

set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/../../.." && pwd)"
SPRT_INCLUDE="$REPO/runtime/include"
TARGETS="$REPO/runtime/toolchains/targets"
OPEN_OVERLAY="$REPO/runtime/toolchains/target-apple/open/sysroot"
OSVER="14.5"

ARCHS="x86_64 aarch64"
SYSROOTS="sdk open"
ONLY="all"
SDK_OVERRIDE=""
VERBOSE=0
while [[ $# -gt 0 ]]; do
	case "$1" in
		--arch) ARCHS="${2:-}"; shift ;;
		--arch=*) ARCHS="${1#*=}" ;;
		--sysroot) SYSROOTS="${2:-}"; shift ;;
		--sysroot=*) SYSROOTS="${1#*=}" ;;
		--sdk) SDK_OVERRIDE="${2:-}"; shift ;;
		--sdk=*) SDK_OVERRIDE="${1#*=}" ;;
		--only) ONLY="${2:-all}"; shift ;;
		--only=*) ONLY="${1#*=}" ;;
		-v|--verbose) VERBOSE=1 ;;
		-h|--help) sed -n '2,32p' "$0"; exit 0 ;;
		*) echo "macos-abi: unknown argument '$1'" >&2; exit 2 ;;
	esac
	shift
done
[[ "$ARCHS" == "all" ]] && ARCHS="x86_64 aarch64"
[[ "$SYSROOTS" == "both" ]] && SYSROOTS="sdk open"

CLANG="${CLANG:-clang}"
if ! command -v "$CLANG" >/dev/null 2>&1; then
	echo "macos-abi: clang not found (set CLANG=...)" >&2; exit 2
fi

# --- locate a macOS SDK to compare against ---------------------------------
# Absence is the normal case on a non-Apple machine; see the header comment.
SDK=""
SDK_ORIGIN=""
if [[ -n "$SDK_OVERRIDE" ]]; then
	SDK="$SDK_OVERRIDE"; SDK_ORIGIN="--sdk"
elif [[ -n "${SP_MACOS_SDK:-}" ]]; then
	# An explicit request is honoured or refused, never silently second-guessed.
	if [[ ! -d "$SP_MACOS_SDK/usr/include/sys" ]]; then
		echo "macos-abi: SP_MACOS_SDK is set to '$SP_MACOS_SDK', which is not a macOS SDK" >&2
		echo "           (expected \$SP_MACOS_SDK/usr/include/sys to exist). Unset it to fall" >&2
		echo "           back to xcrun / runtime/toolchains/src/MacOSX.sdk." >&2
		exit 2
	fi
	SDK="$SP_MACOS_SDK"; SDK_ORIGIN="\$SP_MACOS_SDK"
fi
# Only search when nothing was named explicitly: an explicit choice that turns
# out to be wrong must not be silently replaced by a different SDK.
if [[ -z "$SDK" ]]; then
	if command -v xcrun >/dev/null 2>&1; then
		SDK="$(xcrun --sdk macosx --show-sdk-path 2>/dev/null || true)"
		SDK_ORIGIN="xcrun --sdk macosx --show-sdk-path"
	fi
	if [[ -z "$SDK" || ! -d "$SDK/usr/include/sys" ]]; then
		SDK="$REPO/runtime/toolchains/src/MacOSX.sdk"
		SDK_ORIGIN="runtime/toolchains/src/MacOSX.sdk"
	fi
fi
if [[ ! -d "$SDK/usr/include/sys" ]]; then
	echo "macos-abi: SKIP - no macOS SDK to validate against." >&2
	if [[ -n "$SDK_OVERRIDE" ]]; then
		echo "           --sdk named '$SDK_OVERRIDE', which does not look like a macOS SDK." >&2
	else
		echo "           Looked for: \$SP_MACOS_SDK, then 'xcrun --sdk macosx --show-sdk-path'," >&2
		echo "           then $REPO/runtime/toolchains/src/MacOSX.sdk" >&2
	fi
	echo "           This is expected off an Apple machine: the SDK is neither vendored" >&2
	echo "           nor fetchable, and +open is built precisely so it is not needed." >&2
	echo "           Point SP_MACOS_SDK at an SDK, or drop one in src/, to run the check." >&2
	exit 0
fi
[[ "$VERBOSE" == 1 ]] && echo "macos-abi: SDK = $SDK  (via $SDK_ORIGIN)"

# clang triple arch spelling differs from the toolchain target spelling
triple_arch() { [[ "$1" == "aarch64" ]] && echo arm64 || echo "$1"; }
open_root()   { echo "$TARGETS/$1-apple-macosx+open"; }

# emit the -isysroot / -isystem flags for a (sysroot-kind, arch) pair
sysroot_flags() {
	local kind="$1" arch="$2" root
	if [[ "$kind" == "sdk" ]]; then
		printf '%s\n' -isysroot "$SDK"
	else
		root="$(open_root "$arch")"
		# The +open sysroot puts the SDK-like headers in include_libc, not
		# usr/include, so -isysroot would not find them; -nostdlibinc keeps
		# clang from adding a host /usr/include on top.
		printf '%s\n' -nostdlibinc -isystem "$root/include_libc" \
			-F "$root/System/Library/Frameworks"
	fi
}

rc=0
run() {
	[[ "$VERBOSE" == 1 ]] && printf '%s ' "$@" && echo
	"$@" || rc=1
}

# ===========================================================================
# part 1 -- sprt __SPRT_* tables vs a sysroot (static_assert)
# ===========================================================================
part_sprt() {
	local arch kind tu triple flags
	for arch in $ARCHS; do
		triple="$(triple_arch "$arch")-apple-macosx$OSVER"
		for kind in $SYSROOTS; do
			if [[ "$kind" == "open" && ! -d "$(open_root "$arch")/include_libc" ]]; then
				echo "macos-abi: SKIP sprt/$arch/open - $(open_root "$arch") not installed" >&2
				continue
			fi
			mapfile -t flags < <(sysroot_flags "$kind" "$arch")
			for tu in "$HERE"/check-*.cpp; do
				[[ -e "$tu" ]] || continue
				run "$CLANG" --target="$triple" -fsyntax-only -std=c++20 \
					-Werror=macro-redefined -Wno-invalid-offsetof \
					"${flags[@]}" -I "$SPRT_INCLUDE" "$tu"
			done
			echo "macos-abi: sprt tables vs $kind [$triple] done"
		done
	done
}

# ===========================================================================
# part 2 -- the +open sysroot vs the real SDK (compile twice, diff the values)
# ===========================================================================
# Each probe declares its values as `extern "C" __attribute__((used)) const
# long long` / `const char[]` globals; compiling to LLVM IR and scraping the
# initialisers gives a name->value table with no target execution. A name that
# exists on only one side fails to compile, which is itself the check.
probe_extract() {
	local tu="$1" triple="$2"; shift 2
	"$CLANG" --target="$triple" -w -S -emit-llvm -O0 -o - "$@" "$tu" 2>/dev/null \
		| sed -n -e 's/^@\(abi_[A-Za-z_0-9]*\) = .* i64 \(-\?[0-9]*\).*/\1 \2/p' \
		         -e 's/^@\(enc_[A-Za-z_0-9]*\) = .* c"\([^"]*\)".*/\1 \2/p' \
		| sort
}

part_sysroot() {
	local arch triple tu name tmp sdkf openf
	if command -v python3 >/dev/null 2>&1 && [[ -x "$HERE/gen-probe-frameworks.py" ]]; then
		run python3 "$HERE/gen-probe-frameworks.py" --check
	fi
	local -a oflags
	tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' RETURN
	for arch in $ARCHS; do
		triple="$(triple_arch "$arch")-apple-macosx$OSVER"
		if [[ ! -d "$(open_root "$arch")/include_libc" ]]; then
			echo "macos-abi: SKIP sysroot/$arch - $(open_root "$arch") not installed" >&2
			continue
		fi
		mapfile -t oflags < <(sysroot_flags open "$arch")
		for tu in "$HERE"/probe/probe-*.c "$HERE"/probe/probe-*.mm; do
			[[ -e "$tu" ]] || continue
			name="$(basename "$tu")"
			sdkf="$tmp/$arch.$name.sdk"; openf="$tmp/$arch.$name.open"
			[[ "$VERBOSE" == 1 ]] && echo "probe $name [$triple]"
			# a compile error on either side is a finding in itself
			if ! "$CLANG" --target="$triple" -w -fsyntax-only -isysroot "$SDK" "$tu" 2>"$tmp/err.sdk"; then
				echo "macos-abi: FAIL $name does not compile against the SDK:" >&2
				sed 's/^/    /' "$tmp/err.sdk" >&2; rc=1; continue
			fi
			if ! "$CLANG" --target="$triple" -w -fsyntax-only "${oflags[@]}" "$tu" 2>"$tmp/err.open"; then
				echo "macos-abi: FAIL $name does not compile against $arch+open:" >&2
				sed 's/^/    /' "$tmp/err.open" >&2; rc=1; continue
			fi
			probe_extract "$tu" "$triple" -isysroot "$SDK"   > "$sdkf"
			probe_extract "$tu" "$triple" "${oflags[@]}"     > "$openf"
			if [[ ! -s "$sdkf" ]]; then
				echo "macos-abi: FAIL $name [$triple] produced no values - probe is broken" >&2
				rc=1; continue
			fi
			if ! diff -u --label "SDK/$name" --label "+open/$name" "$sdkf" "$openf"; then
				echo "macos-abi: FAIL $name [$triple] - +open diverges from the SDK above" >&2
				rc=1
			else
				echo "macos-abi: sysroot $name [$triple] OK ($(wc -l < "$sdkf") values)"
			fi
		done
	done
}

# ===========================================================================
# part 3 -- the +open .tbd link stubs vs the SDK's tbds
# ===========================================================================
# The hand-written framework headers are plain git-tracked files copied into the
# sysroot by open-sysroot.mk's `stubs` stamp. Like the stubs they travel
# open/sysroot -> intermediate -> targets and can sit unpublished, so an
# installed sysroot may be built against declarations the overlay has since
# fixed. Diffing the two is cheap and catches it directly, rather than as a
# confusing compile error inside a probe.
part_header_drift() {
	local arch src dst
	src="$OPEN_OVERLAY/System/Library/Frameworks"
	[[ -d "$src" ]] || return
	for arch in $ARCHS; do
		dst="$(open_root "$arch")/System/Library/Frameworks"
		[[ -d "$dst" ]] || continue
		local drift=0 rel
		while IFS= read -r rel; do
			if [[ ! -f "$dst/$rel" ]]; then
				echo "macos-abi: DRIFT $arch+open is missing $rel (present in the tracked overlay)" >&2
				drift=1
			elif ! cmp -s "$src/$rel" "$dst/$rel"; then
				echo "macos-abi: DRIFT $arch+open $rel differs from the tracked overlay" >&2
				drift=1
			fi
		done < <(cd "$src" && find . -name '*.h' | sed 's|^\./||')
		if [[ "$drift" == 1 ]]; then
			echo "           the overlay headers travel open/sysroot -> intermediate -> targets;" >&2
			echo "           re-export the target, or copy the .h files across, to republish them." >&2
			rc=1
		else
			echo "macos-abi: framework headers [$arch+open] match the tracked overlay"
		fi
	done
}

part_tbd() {
	local arch
	if ! command -v python3 >/dev/null 2>&1; then
		echo "macos-abi: SKIP tbd - python3 not found" >&2; return
	fi
	# the git-tracked overlay is the source of truth for the stubs
	run python3 "$HERE/tbd-audit.py" --sdk "$SDK" --stubs "$OPEN_OVERLAY" \
		--exceptions "$HERE/tbd-exceptions.txt" --label "open/sysroot (tracked overlay)"
	# ... and each installed sysroot, which can lag the overlay: the stubs
	# travel open/sysroot -> intermediate -> targets, and nothing re-runs those
	# stages until someone rebuilds the target.
	for arch in $ARCHS; do
		[[ -d "$(open_root "$arch")/usr/lib" ]] || continue
		run python3 "$HERE/tbd-audit.py" --sdk "$SDK" --stubs "$(open_root "$arch")" \
			--exceptions "$HERE/tbd-exceptions.txt" \
			--label "targets/$arch-apple-macosx+open (installed)" \
			--compare-with "$OPEN_OVERLAY"
	done
}

case "$ONLY" in
	all)     part_sprt; part_sysroot; part_header_drift; part_tbd ;;
	sprt)    part_sprt ;;
	sysroot) part_sysroot ;;
	tbd)     part_header_drift; part_tbd ;;
	*) echo "macos-abi: unknown --only '$ONLY' (sprt|sysroot|tbd|all)" >&2; exit 2 ;;
esac

echo
if [[ "$rc" == 0 ]]; then
	echo "macos-abi [$ARCHS / $SYSROOTS]: PASS"
	exit 0
else
	echo "macos-abi [$ARCHS / $SYSROOTS]: FAIL - see the diagnostics above" >&2
	exit 1
fi
