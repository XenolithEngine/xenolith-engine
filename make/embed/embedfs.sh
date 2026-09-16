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

# BundleFS generator for the GNU-make build path (see make/embed/apply.mk).
#
# Turns a directory into a translation unit that stappler_filesystem serves under
# FileCategory::Embedded. The engine-native path uses xlmake's in-process $(EMBED)
# directive instead; this script exists so the tree still builds with plain GNU make,
# and its output must stay byte-for-byte identical to xlmake's for an uncompressed
# bundle (utils/xlmake writes the same layout from SPMakefileEmbed.cc).
#
# Only POSIX tools are used: find, sort, od, awk, wc. `od -An -v -tx1` is in coreutils
# and busybox alike, unlike xxd which ships with vim.
#
# Usage: embedfs.sh <out.cpp> <bundle-name> <src-dir> [compress]
#
# Limitation: compression is not implemented here — LZ4/brotli are out of reach for a
# shell script. A bundle asking for it is written uncompressed with a warning; the
# format records compression per entry, so the result is valid either way and only the
# size saving is lost. Build with xlmake to get compression.
#
# Limitation: file names containing a newline or a tab are not supported (the pipeline
# is line-based) and are reported as an error rather than silently skipped.

set -e

if [ $# -lt 3 ]; then
	echo "usage: embedfs.sh <out.cpp> <bundle-name> <src-dir> [compress]" >&2
	exit 1
fi

OUT=$1
NAME=$2
DIR=$3
COMPRESS=${4:-0}

if [ ! -d "$DIR" ]; then
	echo "embedfs: not a directory: $DIR" >&2
	exit 1
fi

if [ "$COMPRESS" != "0" ] && [ -n "$COMPRESS" ]; then
	echo "embedfs: warning: compression is unavailable in the sh fallback;" \
		"writing bundle '$NAME' uncompressed (build with xlmake for compression)" >&2
fi

# C identifier suffix derived from the bundle name
IDENT=`printf '%s' "$NAME" | LC_ALL=C sed 's/[^A-Za-z0-9_]/_/g'`

TMPDIR_EMBED=`mktemp -d`
trap 'rm -rf "$TMPDIR_EMBED"' EXIT INT TERM

LIST="$TMPDIR_EMBED/list"
BODY="$TMPDIR_EMBED/body"
ENTRIES="$TMPDIR_EMBED/entries"

# Entry order is part of the format: bytewise, except that '/' sorts below every other
# character, which keeps a directory's subtree contiguous (see comparePath in
# SPFilesystemEmbedded.h). Substituting '/' with \001 and sorting the result reproduces it
# exactly; the substitution is reversed afterwards, so nothing but the order is affected.
# (A file name containing a literal \001 byte would survive the round-trip incorrectly —
# such names do not occur in practice and are not supported.)
( cd "$DIR" && find -L . \( -type d -o -type f \) -print ) \
	| LC_ALL=C sed 's|^\./||' \
	| LC_ALL=C grep -v '^\.$' \
	| LC_ALL=C awk '
		{
			if (index($0, "\t") > 0) {
				printf("embedfs: file name with a tab is not supported: %s\n", $0) > "/dev/stderr"
				exit 1
			}
			gsub("/", "\001")
			print
		}' \
	| LC_ALL=C sort \
	| LC_ALL=C awk '{ gsub("\001", "/"); print }' > "$LIST"

# Newest mtime over the content, in seconds. Reported by stat() as mtime/ctime/atime, and
# deliberately content-derived rather than "now" so the output is reproducible. GNU and BSD
# spell `stat` differently; an unknown platform simply gets 0.
newest_mtime() {
	max=0
	while IFS= read -r rel; do
		[ -f "$DIR/$rel" ] || continue
		m=`stat -c %Y "$DIR/$rel" 2>/dev/null || stat -f %m "$DIR/$rel" 2>/dev/null || echo 0`
		[ -n "$m" ] || m=0
		if [ "$m" -gt "$max" ]; then max=$m; fi
	done < "$LIST"
	echo "$max"
}

MTIME=`newest_mtime`
BUILD_TIME=`expr "$MTIME" \* 1000000`

# Escape a path for a C string literal. Anything outside printable ASCII (and the two
# characters that would end or escape the literal) becomes a 3-digit octal escape, which —
# unlike \x — can not swallow the character that follows it.
escape_path() {
	printf '%s' "$1" | LC_ALL=C od -An -v -tu1 | LC_ALL=C awk '
		{
			for (i = 1; i <= NF; ++i) {
				c = $i + 0
				if (c == 34) { printf("\\\"") }
				else if (c == 92) { printf("\\\\") }
				else if (c >= 32 && c < 127) { printf("%c", c) }
				else { printf("\\%03o", c) }
			}
		}'
}

: > "$BODY"
: > "$ENTRIES"

OFFSET=0
COUNT=0

while IFS= read -r REL; do
	COUNT=`expr $COUNT + 1`
	ESCAPED=`escape_path "$REL"`
	LEN=`printf '%s' "$REL" | LC_ALL=C wc -c | tr -d ' '`

	if [ -d "$DIR/$REL" ]; then
		printf '\t{ "%s", %s, EntryFlags::Dir, 0, 0, 0 },\n' "$ESCAPED" "$LEN" >> "$ENTRIES"
		continue
	fi

	SIZE=`LC_ALL=C wc -c < "$DIR/$REL" | tr -d ' '`
	printf '\t{ "%s", %s, EntryFlags::None, %s, %s, %s },\n' \
		"$ESCAPED" "$LEN" "$OFFSET" "$SIZE" "$SIZE" >> "$ENTRIES"

	if [ "$SIZE" -gt 0 ]; then
		# one hex byte per line; the whole stream is re-flowed into rows once, below
		LC_ALL=C od -An -v -tx1 < "$DIR/$REL" \
			| LC_ALL=C tr -s ' ' '\n' \
			| LC_ALL=C grep -v '^$' >> "$BODY"
	fi

	OFFSET=`expr $OFFSET + $SIZE`
done < "$LIST"

# 16 bytes per row, tab-indented, single space between values
LC_ALL=C awk '
	{
		if ((NR - 1) % 16 == 0) {
			if (NR > 1) { printf("\n") }
			printf("\t")
		} else {
			printf(" ")
		}
		printf("0x%s,", $1)
	}
	END { if (NR > 0) { printf("\n") } }' "$BODY" > "$BODY.rows"

{
	echo "/**"
	echo " Autogenerated by BundleFS. Do not edit."
	echo ""
	echo " Bundle: $NAME"
	echo " **/"
	echo "///@ SP_EXCLUDE"
	echo ""
	echo '#include "SPFilesystemEmbedded.h"'
	echo '#include "SPSharedModule.h"'
	echo ""
	echo "namespace STAPPLER_VERSIONIZED stappler::filesystem::embedded {"
	echo ""
	echo "alignas(16) static const uint8_t s_data_$IDENT[] = {"
	if [ "$OFFSET" -gt 0 ]; then
		cat "$BODY.rows"
	else
		echo "	0x00,"
	fi
	echo "};"
	echo ""
	echo "static const Entry s_entries_$IDENT[] = {"
	if [ "$COUNT" -gt 0 ]; then
		cat "$ENTRIES"
	else
		echo '	{ "", 0, EntryFlags::Dir, 0, 0, 0 },'
	fi
	echo "};"
	echo ""
	echo "static const Bundle s_bundle_$IDENT = {"
	echo "	BundleVersion,"
	echo "	\"$NAME\","
	echo "	s_entries_$IDENT,"
	echo "	$COUNT,"
	echo "	s_data_$IDENT,"
	echo "	$OFFSET,"
	echo "	$BUILD_TIME,"
	echo "};"
	echo ""
	echo "SP_USED static SharedExtension s_extension_$IDENT(BundleModuleName, \"$NAME\","
	echo "		&s_bundle_$IDENT);"
	echo ""
	echo "} // namespace stappler::filesystem::embedded"
} > "$OUT"
