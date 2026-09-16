#!/usr/bin/env bash
#
# Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
#
# Rasterizer benchmark: time the pixel loops of the software backend, once per kernel
# set, and print a table with the scalar set as the baseline.
#
# It measures raster::draw and nothing else. A frame-level number would be useless in a
# debug build - everything except the rasterizer module is compiled unoptimized, and the
# scene graph would swamp the only thing an ISA kernel can change. The engine reports the
# rasterization time itself under XL_SOFT_PROFILE=1; this script just drives frames and
# reads the last profile line.
#
# Two settings are not optional and are why the numbers mean anything:
#
#   XL_SOFT_FORCE_FULL_REDRAW=1 - with damage tracking on, a static scene skips its
#     frames outright and every kernel set measures the same zero.
#   SP_RASTER_KERNELS=<set>     - forces the set. The app logs which one it actually
#     used, and this script reads that back rather than trusting the request, because a
#     set that silently fell back would produce a real number for the wrong thing.
#
# Usage:
#   ./bench.sh [options] [case ...]
#     case...        run only these cases (default: all, see CASES below)
#     --no-build     skip the build step, use the existing binary
#     --sets a,b,c   kernel sets to time (default: everything the binary reports)
#     --frames N     frames to render per case (default 240)
#     --size WxH     surface size (default 1920x1080)
#     --tiles WxH    cut each damage region into tiles of this size (default: no cutting)
#     --threads N    fan the tiles out to N threads (default 1). Tiling and threads are
#                    two separate effects, so measure `--tiles WxH` on its own before
#                    adding `--threads`: with both switched on at once neither can be
#                    attributed
#     -l|--list      print the case list and exit
#
# Mpx/s is damage area over time, not shaded pixels over time. The two coincide for the
# cases below because their content is scaled to the surface; for `text` they do not, and
# only its us/frame is meaningful.
#
# Requirements: python3 (for the inspector client).

set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
PROJECT="$ROOT/tests/headless"
TARGET="x86_64-unknown-linux-gnu"
BIN="$PROJECT/stappler-build/$TARGET/debug/cc/headlesstest"

DO_BUILD=1
SETS=""
FRAMES=240
SIZE="1920x1080"
TILES="off"
THREADS=1
SELECT=()

# name|size|invoke specs. `size` is empty to use the harness surface size, or WxH to force
# one for that case alone.
#
#   fill        - a solid quad the size of the surface: the constant-source path
#   sprite      - nearest fetch, 16x16 texture blown up: the magnified case
#   sprite-1to1 - 1024x1024 texture at 1:1 on a 1024x1024 surface: one texel per pixel,
#                 four megabytes of texture, so the fetch actually reaches memory
#   sprite-1to1-hq - the same, bilinear: the case the column cache cannot help, because
#                 x0 changes at every pixel
#   sprite-min  - the same texture minified 2:1: the worst case for the cache
#   sprite-hq   - linear filter, the most arithmetic per pixel
#   alpha       - transparent blending over an already-painted surface
#   text        - the glyph blit
#   grid        - 400 small quads: what costs per primitive rather than per pixel
#                 (triangle setup, the bbox scan, the divide by area) instead of being
#                 hidden under two full-screen triangles
#   mixed       - all of it at once, the closest thing here to a real UI frame
#
# %W% and %H% are substituted with the case's surface size. Without that the scene draws
# fixed 128x200 pixel nodes whatever the surface is, and a throughput kernel would be
# measured almost entirely on per-command overhead. `text` stays as it is: glyphs are
# small by nature, and scaling the font would change which code path runs, not how much.
CASES=(
	'fill||box-size={"width":%W%,"height":%H%} show={"layer":true,"sprite":false,"vector":false,"label":false}'
	'sprite||sprite-size={"width":%W%,"height":%H%} show={"layer":false,"sprite":true,"vector":false,"label":false} filter={"name":"nearest"}'
	'sprite-1to1|1024x1024|sprite-image={"name":"large"} sprite-size={"width":1024,"height":1024} show={"layer":false,"sprite":true,"vector":false,"label":false} filter={"name":"nearest"}'
	'sprite-1to1-hq|1024x1024|sprite-image={"name":"large"} sprite-size={"width":1024,"height":1024} show={"layer":false,"sprite":true,"vector":false,"label":false} filter={"name":"linear"}'
	'sprite-min|512x512|sprite-image={"name":"large"} sprite-size={"width":512,"height":512} show={"layer":false,"sprite":true,"vector":false,"label":false} filter={"name":"nearest"}'
	'sprite-hq||sprite-size={"width":%W%,"height":%H%} show={"layer":false,"sprite":true,"vector":false,"label":false} filter={"name":"linear"} rotate={"angle":22.5}'
	'alpha||box-size={"width":%W%,"height":%H%} sprite-size={"width":%W%,"height":%H%} show={"layer":true,"sprite":true,"vector":true,"label":false} filter={"name":"linear"} alpha={"value":0.55}'
	'text||show={"layer":false,"sprite":false,"vector":false,"label":true}'
	'grid||show={"layer":false,"sprite":false,"vector":false,"label":false} grid={"count":20}'
	'mixed||box-size={"width":%W%,"height":%H%} sprite-size={"width":%W%,"height":%H%} show={"layer":true,"sprite":true,"vector":true,"label":true} filter={"name":"linear"}'
)

case_name() { printf '%s' "${1%%|*}"; }
case_size() { local r="${1#*|}"; printf '%s' "${r%%|*}"; }
case_steps() { printf '%s' "${1#*|*|}"; }

for arg in "$@"; do
	case "${SHIFT_NEXT:-}" in
		sets) SETS="$arg"; SHIFT_NEXT=""; continue ;;
		frames) FRAMES="$arg"; SHIFT_NEXT=""; continue ;;
		size) SIZE="$arg"; SHIFT_NEXT=""; continue ;;
		tiles) TILES="$arg"; SHIFT_NEXT=""; continue ;;
		threads) THREADS="$arg"; SHIFT_NEXT=""; continue ;;
	esac
	case "$arg" in
		--no-build) DO_BUILD=0 ;;
		--sets) SHIFT_NEXT=sets ;;
		--frames) SHIFT_NEXT=frames ;;
		--size) SHIFT_NEXT=size ;;
		--tiles) SHIFT_NEXT=tiles ;;
		--threads) SHIFT_NEXT=threads ;;
		-l|--list) for c in "${CASES[@]}"; do case_name "$c"; echo; done; exit 0 ;;
		-h|--help) sed -n '2,37p' "$0"; exit 0 ;;
		-*) echo "unknown option: $arg" >&2; exit 2 ;;
		*) SELECT+=("$arg") ;;
	esac
done

WIDTH="${SIZE%x*}"
HEIGHT="${SIZE#*x}"

if [[ "$DO_BUILD" == 1 ]]; then
	make -C "$PROJECT" STAPPLER_TARGET="$TARGET" SOFT=1 -j8 >/dev/null || exit 1
fi
[[ -x "$BIN" ]] || { echo "missing binary: $BIN (drop --no-build)" >&2; exit 1; }

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

# shellcheck source=kernelsets.sh
source "$HERE/kernelsets.sh"

# Run one case with one kernel set. Echoes "used_set us_per_frame mpx_per_s", or nothing
# when the run produced no profile line.
run_case() {
	local set="$1" steps="$2" tag="$3" w="$4" h="$5"
	local sock="$WORK/$tag.sock" log="$WORK/$tag.log"
	local invokes=()
	local step
	for step in $steps; do invokes+=(--invoke "$step"); done

	( timeout -k 5 300 env SP_RASTER_KERNELS="$set" XL_SOFT_PROFILE=1 \
		SP_RASTER_TILE="$TILES" SP_RASTER_THREADS="$THREADS" \
		XL_SOFT_FORCE_FULL_REDRAW=1 XL_FLAT_QUEUE=1 \
		XENOLITH_INSPECTOR_ADDRESS="unix:$sock" \
		"$BIN" --gapi soft --headless --width "$w" --height "$h" >"$log" 2>&1 ) &
	local app=$!

	timeout 300 python3 "$HERE/benchclient.py" --address "unix:$sock" \
		"${invokes[@]}" --frames "$FRAMES" --quit >>"$log" 2>&1 || true

	if kill -0 "$app" 2>/dev/null; then
		sleep 1
		kill -0 "$app" 2>/dev/null && kill "$app" 2>/dev/null
	fi
	wait "$app" 2>/dev/null || true

	# What the app actually ran, not what was asked for.
	local used
	used="$(sed -n 's/.*using kernel set: \([a-z0-9]*\).*/\1/p' "$log" | tail -1)"
	[[ -n "$used" ]] || return 1

	local line
	line="$(grep -o 'kernels=[^ ]* .*Mpx/s=[0-9.]*' "$log" | tail -1)"
	[[ -n "$line" ]] || return 1

	# Same discipline as the kernel set: the app reports how many workers actually took part, not
	# how many were asked for. A case whose surface is too small to yield one tile per thread runs
	# with fewer, and the number is still valid - it just is not the number that was requested, so
	# it is reported as what it is rather than under the wrong label.
	local usedThreads
	usedThreads="$(sed -n 's/.*threads=\([0-9.]*\).*/\1/p' <<<"$line")"

	# frames= is reported by the engine, not by us: if the loop produced fewer frames than
	# were asked for, the average is still right but the sample is smaller, and it shows.
	local us mpx frames
	us="$(sed -n 's/.*us\/frame=\([0-9.]*\).*/\1/p' <<<"$line")"
	mpx="$(sed -n 's/.*Mpx\/s=\([0-9.]*\).*/\1/p' <<<"$line")"
	us="$(python3 -c "print(f'{$us:.0f}')" 2>/dev/null || printf '%s' "$us")"
	mpx="$(python3 -c "print(f'{$mpx:.0f}')" 2>/dev/null || printf '%s' "$mpx")"
	frames="$(sed -n 's/.*frames=\([0-9]*\).*/\1/p' <<<"$line")"
	printf '%s %s %s %s %s\n' "$used" "$us" "$mpx" "$frames" "${usedThreads:-?}"
}

if [[ -n "$SETS" ]]; then
	KERNEL_SETS=(${SETS//,/ })
else
	KERNEL_SETS=($(discover_kernel_sets "$BIN" "$WORK"))
fi

[[ ${#KERNEL_SETS[@]} -gt 0 ]] || { echo "no kernel sets reported by $BIN" >&2; exit 1; }

echo "binary : $BIN"
echo "surface: ${WIDTH}x${HEIGHT}, $FRAMES frames, full redraw forced"
echo "tiles  : $TILES, threads: $THREADS"
echo "sets   : ${KERNEL_SETS[*]}"
echo

printf '%-12s' "case"
for set in "${KERNEL_SETS[@]}"; do printf '  %-26s' "$set (us/frame, Mpx/s, n)"; done
echo
printf '%.0s-' $(seq 1 $((12 + ${#KERNEL_SETS[@]} * 28))); echo

declare -A BASE
for entry in "${CASES[@]}"; do
	name="$(case_name "$entry")"
	steps="$(case_steps "$entry")"

	# A case may pin its own surface: `sprite-1to1` only means anything when the surface
	# matches the texture, and Mpx/s only means anything when the drawn area matches the
	# damage area the profile counts.
	caseSize="$(case_size "$entry")"
	caseW="${caseSize:+${caseSize%x*}}"; caseW="${caseW:-$WIDTH}"
	caseH="${caseSize:+${caseSize#*x}}"; caseH="${caseH:-$HEIGHT}"

	steps="${steps//%W%/$caseW}"
	steps="${steps//%H%/$caseH}"

	if [[ ${#SELECT[@]} -gt 0 ]]; then
		found=0
		for s in "${SELECT[@]}"; do [[ "$s" == "$name" ]] && found=1; done
		[[ "$found" == 1 ]] || continue
	fi

	printf '%-12s' "$name"
	for set in "${KERNEL_SETS[@]}"; do
		if result="$(run_case "$set" "$steps" "$name-$set" "$caseW" "$caseH")"; then
			read -r used us mpx nframes usedThreads <<<"$result"
			# Only annotate when the pool could not give what was asked for; the common case
			# stays uncluttered.
			threadNote=""
			if [[ "${usedThreads%%.*}" != "${THREADS%%.*}" ]]; then
				threadNote=" t=${usedThreads%%.*}"
			fi
			if [[ "$used" != "$set" ]]; then
				# Never report a number under the label that was requested rather than run.
				printf '  %-26s' "FELL BACK: $used"
				continue
			fi
			if [[ "$set" == "scalar" ]]; then
				BASE[$name]="$us"
				printf '  %-26s' "$us / $mpx n=$nframes$threadNote"
			else
				base="${BASE[$name]:-}"
				if [[ -n "$base" ]]; then
					speedup="$(python3 -c "print(f'{$base/$us:.2f}x')" 2>/dev/null || echo '?')"
					printf '  %-26s' "$us ($speedup) n=$nframes$threadNote"
				else
					printf '  %-26s' "$us / $mpx n=$nframes$threadNote"
				fi
			fi
		else
			printf '  %-26s' "no data"
		fi
	done
	echo
done
