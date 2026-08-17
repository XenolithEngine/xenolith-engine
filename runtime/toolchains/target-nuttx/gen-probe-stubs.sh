#!/bin/sh
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
#
# Generate libprobe-stubs.a — weak placeholder definitions for every symbol that
# the staged NuttX archives reference but none of them defines.
#
# Why it exists: a cmake feature probe (check_function_exists / check_symbol_exists)
# is only meaningful if it LINKS; a compile-only probe reports every function as
# present, because the test source merely declares `char FUNC();`. But a NuttX
# probe executable cannot link on its own — the flat image is closed by board code,
# by the linker script (_stext/_sinit/g_idle_topstack/...), and by the engine's
# libc_impl module, which supplies the part of the sprt umbrella surface NuttX libc
# lacks (complex math, wide-char, regex, locale, fenv, setjmp/longjmp). None of
# those are in the toolchain sysroot, so every probe would fail and report absent.
#
# The set is derived, not hand-listed: undefined-minus-defined over the whole
# staged archive set. That closure is exactly "resolved later, at the final image
# link", so stubbing it lets a probe link succeed while an ACTUALLY missing symbol
# — one nothing references and nothing defines — still fails, which is the answer
# the probe is asking for. The stubs are weak, so a real definition anywhere on the
# link line always wins; and nothing links this archive except probes.
#
# libapps.a is excluded on purpose: it is built from the board's app list, so it
# carries the application's own symbols (xenolith_main and whatever libc_impl the
# engine already linked into it). Feeding that back into the probe closure would
# make the answers depend on which app the export happened to be built with.
#
# Usage: gen-probe-stubs.sh <nm> <cc> <ar> <out.a> <linked>... -- <ref-only>...
#
# Each entry is an archive or a directory of *.a. The two groups are NOT
# interchangeable:
#
#   <linked>   archives that are ON the probe link line (see
#              SP_NUTTX_PROBE_LDFLAGS in common/configure.mk): the NuttX sysroot
#              plus libsprt.a. Only these can DEFINE a symbol for a probe, so
#              only these subtract.
#   <ref-only> archives a probe may pull a reference from but that are not on
#              the link line — the staged deps. libcrypto referencing `stderr`
#              is the standing example: the reference only shows up if the dep
#              archives are scanned, but libcrypto itself is not on the link
#              line and must not be allowed to satisfy anything.
#
# Getting that split wrong is not theoretical: libsqlite3.a carries a static
# `readlink`, which as a *defined* symbol cancelled the stub for the real one
# and broke every curl probe with "undefined symbol: readlink".

set -e

NM="$1"
CC="$2"
AR="$3"
OUT="$4"
shift 4

if [ -z "$OUT" ] || [ $# -eq 0 ]; then
	echo "usage: $0 <nm> <cc> <ar> <out.a> <linked>... -- <ref-only>..." >&2
	exit 1
fi

WORK="${OUT}.d"
rm -rf "$WORK"
mkdir -p "$WORK"

# Expand entries (each an archive or a directory of *.a) into an archive list.
collect() {
	__out=""
	for e in $1; do
		if [ -d "$e" ]; then
			for f in "$e"/*.a; do
				case "$f" in
				*/libapps.a) continue ;;
				*/libprobe-stubs.a) continue ;;
				*'*.a') continue ;;
				esac
				__out="$__out $f"
			done
		elif [ -f "$e" ]; then
			__out="$__out $e"
		fi
	done
	echo "$__out"
}

LINKED_SPEC=""
REFONLY_SPEC=""
seen_sep=0
for a in "$@"; do
	if [ "$a" = "--" ]; then
		seen_sep=1
		continue
	fi
	if [ "$seen_sep" = "0" ]; then
		LINKED_SPEC="$LINKED_SPEC $a"
	else
		REFONLY_SPEC="$REFONLY_SPEC $a"
	fi
done

LINKED=$(collect "$LINKED_SPEC")
REFONLY=$(collect "$REFONLY_SPEC")

# `U` only: `w` (weak undefined) needs no definition to link.
"$NM" --undefined-only $LINKED $REFONLY 2>/dev/null \
	| awk '$1 == "U" { print $2 }' | sort -u > "$WORK/undef"
# --extern-only: a file-local definition (nm prints its type letter lowercase)
# cannot satisfy another object's reference, so it must not cancel a stub.
"$NM" --defined-only --extern-only $LINKED 2>/dev/null \
	| awk 'NF >= 3 { print $3 }' | sort -u > "$WORK/def"
comm -23 "$WORK/undef" "$WORK/def" > "$WORK/stub"

{
	echo "/* Generated by gen-probe-stubs.sh - do not edit. */"
	# One shape for both kinds of reference: a call resolves against a data
	# address just as well as a load does, and the probe binary is never run.
	# It has to be the DATA shape rather than an empty function, though —
	# stdout/stderr and friends are loaded with LDR, and aarch64 rejects
	# R_AARCH64_LDST64_ABS_LO12_NC against a byte-aligned code symbol
	# ("improper alignment for relocation"), which would fail the link for a
	# reason that has nothing to do with what the probe asked.
	echo "#define SPRT_PROBE_STUB(s) __attribute__((weak, aligned(16))) char s[16];"
	while read -r sym; do
		[ -n "$sym" ] || continue
		echo "SPRT_PROBE_STUB($sym)"
	done < "$WORK/stub"
} > "$WORK/probe-stubs.c"

# $CC unquoted: the caller passes the compiler plus its --target/-resource-dir.
$CC -c -O0 -ffreestanding -nostdinc -o "$WORK/probe-stubs.o" "$WORK/probe-stubs.c"
rm -f "$OUT"
"$AR" rcs "$OUT" "$WORK/probe-stubs.o"

echo "probe-stubs: $(wc -l < "$WORK/stub") symbols -> $OUT"
