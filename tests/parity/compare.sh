#!/usr/bin/env bash
#
# Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
#
# Backend-parity driver: render the same scene with two gAPI backends and diff the
# screenshots pixel by pixel.
#
# One binary (tests/headless, built with SOFT=1) can run either backend, so there is
# no two-build dance: the script launches `headlesstest --headless --gapi <api>`
# twice, drives each through the inspector socket (invoke -> frame -> screenshot),
# and compares the PNGs. Both sides are forced onto the flat render queue with
# XL_FLAT_QUEUE=1 - the software backend is always served the flat queue, so a
# Vulkan reference built with the default (shadow/depth) queue would be comparing
# two different frame graphs.
#
# The reference is Vulkan and the subject is soft, unless --reference/--subject say
# otherwise. Passing a directory to --baseline instead compares a backend against a
# stored run of itself, which is how a refactor of the shared renderer is checked
# for regressions.
#
# Usage:
#   ./compare.sh [options] [case ...]
#     case...            run only these cases (default: all, see CASES below)
#     --no-build         skip the build step, use the existing binary
#     --reference API    backend to use as reference (default vulkan)
#     --subject API      backend to compare against it (default soft)
#     --save DIR         keep the reference PNGs in DIR (implies no comparison)
#     --baseline DIR     compare the reference backend against PNGs saved in DIR
#     --size WxH         surface size (default 640x480)
#     --glyph-paths      compare soft against itself with the glyph blit disabled: the
#                        blit must reproduce a nearest texture fetch exactly, and this
#                        is the check for that (always exact)
#     --windowed         compare soft in a real window against soft headless. Needs a
#                        session (SP_SESSION_TYPE / XDG_SESSION_TYPE); always exact -
#                        see the note below
#     --kernels          compare every rasterizer kernel set this CPU can run against the
#                        scalar one. Same backend, same scene, same memory - only the
#                        number of pixels a loop handles at a time differs, so this is
#                        always exact too
#     --kernel-set NAME  just that one set against scalar (what --kernels runs per set)
#     --damage           compare a full repaint against the damage-driven one. Same
#                        backend, same scene - only how much of the surface gets
#                        rasterized differs, so it is always exact. A case where damage
#                        never engaged is reported as vacuous rather than counted as
#                        proof, and the gate fails if none of them engaged
#     --tiles [WxH]      compare rasterization cut into tiles against the untiled one. Same
#                        backend, same scene, same memory - only how the work is divided
#                        differs, so it is always exact. Default 128x128
#     --threads N        with --tiles, fan the tiles out to N threads (default 1, which
#                        measures the cut alone). The picture must not depend on how the
#                        scheduler happened to hand tiles out, so run it more than once
#     -v|--verbose       show app logs of a failing case and write a diff map
#     -l|--list          print the case list and exit
#
# Requirements: python3 with numpy and Pillow; a Vulkan ICD for the reference runs.

set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
PROJECT="$ROOT/tests/headless"
TARGET="x86_64-unknown-linux-gnu"
BIN="$PROJECT/stappler-build/$TARGET/debug/cc/headlesstest"

DO_BUILD=1
REFERENCE="vulkan"
SUBJECT="soft"
SAVE=""
BASELINE=""
SIZE="640x480"
VERBOSE=0
GLYPH_PATHS=0
WINDOWED=0
KERNEL_SET=""
ALL_KERNELS=0
DAMAGE=0
TILES=""
THREADS=1
REF_ENV=""
SUB_ENV=""
SELECT=()

# Each case is: name | tolerance-mode | invoke specs (space separated, NAME=JSON).
#
# `tolerant` accepts a difference of one step in a channel - the smallest an 8-bit
# channel can express - and nothing more, however many pixels carry it. Two steps is
# a real divergence no matter how rare, so there is no share-of-pixels allowance.
#
# `exact` demands bit equality, and is used where there is no implementation freedom
# to hide behind: solid fills, and nearest-filtered axis-aligned sampling.
#
# Text is tolerant, and it is worth being precise about which half of it is
# approximate. The glyph FETCH is exact by construction: a Label is normalized
# (XL2dLabel.cc, init -> setNormalized(true)), so VertexPlan::applyNormalized
# rebuilds its model matrix as identity plus a *floored* translation - rotation and
# scale dropped, the scale having gone into the font size - and the glyph therefore
# lands on integer pixels at 1:1, where a blit and a nearest fetch are the same
# thing. `--glyph-paths` proves that half bit for bit.
#
# What is left is the BLEND, and it is a last-bit difference on partially covered
# pixels only. Blending the "more correct" way - source kept in float, rounded once
# at the end - was tried and measured *further* from the hardware (966 differing
# pixels instead of 20 on `alpha`), so the 8-bit quantize-then-blend form this
# backend uses is the better model of what a GPU actually does, not a shortcut.
#
# --windowed is a different question and takes no tolerance at all. It renders the
# same scene twice with the SAME backend, once presenting through the window system's
# own buffers (wl_shm, X SHM) and once into an ordinary bitmap. The rasterizer, the
# command list and the target stride are identical; only where the pixels live
# differs. So the four things that can go wrong - format/swizzle, stride, stale
# pixels from a damage bug, a mixed-up slot index - are exactly the four this
# milestone can introduce, and any of them shows up as a differing pixel. One would
# be a defect, not noise.
CASES=(
	'layer|exact|show={"layer":true,"sprite":false,"vector":false,"label":false}'
	'sprite-nearest|exact|show={"layer":false,"sprite":true,"vector":false,"label":false} filter={"name":"nearest"}'
	'sprite-linear|tolerant|show={"layer":false,"sprite":true,"vector":false,"label":false} filter={"name":"linear"}'
	'sprite-rotated|tolerant|show={"layer":false,"sprite":true,"vector":false,"label":false} filter={"name":"linear"} rotate={"angle":22.5}'
	'vector|tolerant|show={"layer":false,"sprite":false,"vector":true,"label":false}'
	'vector-dashed|tolerant|show={"layer":false,"sprite":false,"vector":true,"label":false} stroke={"width":6,"cap":"butt","dash":[18,12]}'
	'vector-dotted|tolerant|show={"layer":false,"sprite":false,"vector":true,"label":false} stroke={"width":6,"cap":"round","dash":[0,14]}'
	'alpha|tolerant|show={"layer":true,"sprite":true,"vector":true,"label":false} filter={"name":"linear"} alpha={"value":0.55}'
	'clip|exact|show={"layer":false,"sprite":true,"vector":false,"label":false} filter={"name":"nearest"} clip={"enabled":true}'
	'label|tolerant|show={"layer":false,"sprite":false,"vector":false,"label":true}'
	'label-scaled|tolerant|show={"layer":false,"sprite":false,"vector":false,"label":true} font-size={"value":38}'
	'text-update|tolerant|show={"layer":false,"sprite":false,"vector":false,"label":true} text={"value":"first-pass"} @frame=4 text={"value":"Второй-проход-42"}'
	'label-underline|tolerant|show={"layer":false,"sprite":false,"vector":false,"label":true} underline={"enabled":true}'
)

case_name() { printf '%s' "${1%%|*}"; }
case_mode() { local r="${1#*|}"; printf '%s' "${r%%|*}"; }
case_steps() { printf '%s' "${1#*|*|}"; }

for arg in "$@"; do
	case "${SHIFT_NEXT:-}" in
		reference) REFERENCE="$arg"; SHIFT_NEXT=""; continue ;;
		subject) SUBJECT="$arg"; SHIFT_NEXT=""; continue ;;
		save) SAVE="$arg"; SHIFT_NEXT=""; continue ;;
		baseline) BASELINE="$arg"; SHIFT_NEXT=""; continue ;;
		size) SIZE="$arg"; SHIFT_NEXT=""; continue ;;
		kernelset) KERNEL_SET="$arg"; SHIFT_NEXT=""; continue ;;
		threads) THREADS="$arg"; SHIFT_NEXT=""; continue ;;
	esac
	case "$arg" in
		--no-build) DO_BUILD=0 ;;
		--reference) SHIFT_NEXT=reference ;;
		--subject) SHIFT_NEXT=subject ;;
		--save) SHIFT_NEXT=save ;;
		--baseline) SHIFT_NEXT=baseline ;;
		--size) SHIFT_NEXT=size ;;
		--glyph-paths) GLYPH_PATHS=1 ;;
		--windowed) WINDOWED=1 ;;
		--kernels) ALL_KERNELS=1 ;;
		--kernel-set) SHIFT_NEXT=kernelset ;;
		--damage) DAMAGE=1 ;;
		--tiles) TILES="128x128" ;;
		--tiles=*) TILES="${arg#--tiles=}" ;;
		--threads) SHIFT_NEXT=threads ;;
		-v|--verbose) VERBOSE=1 ;;
		-l|--list) for c in "${CASES[@]}"; do case_name "$c"; echo; done; exit 0 ;;
		-h|--help) sed -n '2,44p' "$0"; exit 0 ;;
		-*) echo "unknown option: $arg" >&2; exit 2 ;;
		*) SELECT+=("$arg") ;;
	esac
done
[[ -z "${SHIFT_NEXT:-}" ]] || { echo "missing value for --$SHIFT_NEXT" >&2; exit 2; }

WIDTH="${SIZE%%x*}"
HEIGHT="${SIZE##*x}"

if [[ "$GLYPH_PATHS" == 1 ]]; then
	REFERENCE="soft"
	SUBJECT="soft"
	SUB_ENV="XL_SOFT_GLYPH_SAMPLING=1"
fi

# One kernel set against the scalar one. Same backend, same scene, same target - the only
# difference is how many pixels a loop handles at a time, and that is not allowed to change
# a single byte. So there is no tolerance here at all, by the same argument as --windowed.
if [[ -n "$KERNEL_SET" ]]; then
	REFERENCE="soft"
	SUBJECT="soft"
	REF_ENV="SP_RASTER_KERNELS=scalar"
	SUB_ENV="SP_RASTER_KERNELS=$KERNEL_SET"
fi

# A full repaint against the damage-driven one. Both runs draw the same scene with the same
# backend and the same kernels; the only difference is that one rasterizes the whole surface
# every frame and the other only the regions that changed. A pixel the damage path leaves
# alone was written by an earlier frame, so agreement here is the statement that partial
# redraw composes with itself - and disagreement is a real defect, not a rounding choice.
#
# Both sides carry XL_SOFT_DAMAGE_LOG=1: without it a silently disabled damage path renders
# the same picture as a working one, and the gate would pass by measuring nothing.
if [[ "$DAMAGE" == 1 ]]; then
	REFERENCE="soft"
	SUBJECT="soft"
	REF_ENV="XL_SOFT_FORCE_FULL_REDRAW=1 XL_SOFT_DAMAGE_LOG=1"
	SUB_ENV="XL_SOFT_DAMAGE_LOG=1"
fi

# Tiled rasterization against untiled. A tile is nothing but a smaller clip rectangle, and the
# rasterizer is not allowed to care: the pixels it writes have to depend on which column and row
# they are in, not on where the run that covered them happened to start. That property does not
# come for free - the interpolation anchor exists to provide it - so this is the gate that holds
# it, and it takes no tolerance at all.
#
# Both sides carry XL_SOFT_PROFILE=1 so the run can be asked afterwards how many tiles it really
# produced. A tiling that quietly did not engage would compare a run against itself and pass.
if [[ -n "$TILES" ]]; then
	REFERENCE="soft"
	SUBJECT="soft"
	REF_ENV="SP_RASTER_TILE=off SP_RASTER_THREADS=1 XL_SOFT_PROFILE=1"
	SUB_ENV="SP_RASTER_TILE=$TILES SP_RASTER_THREADS=$THREADS XL_SOFT_PROFILE=1"
fi

# Every set the binary can run here, one after another. Re-invokes this script rather than
# looping inside it: a run is per-set anyway, and the per-set verdict is what matters.
if [[ "$ALL_KERNELS" == 1 ]]; then
	if [[ "$DO_BUILD" == 1 ]]; then
		make -C "$PROJECT" STAPPLER_TARGET="$TARGET" SOFT=1 -j8 >/dev/null || exit 1
	fi
	[[ -x "$BIN" ]] || { echo "missing binary: $BIN (drop --no-build)" >&2; exit 1; }

	WORK_SETS="$(mktemp -d)"
	trap 'rm -rf "$WORK_SETS"' EXIT
	# shellcheck source=kernelsets.sh
	source "$HERE/kernelsets.sh"
	sets="$(discover_kernel_sets "$BIN" "$WORK_SETS")"
	[[ -n "$sets" ]] || { echo "no kernel sets reported by $BIN" >&2; exit 1; }

	rc=0
	for set in $sets; do
		[[ "$set" == "scalar" ]] && continue
		echo "== kernel set: $set vs scalar =="
		"$0" --no-build --kernel-set "$set" ${SELECT[@]+"${SELECT[@]}"} || rc=1
	done
	[[ "$rc" == 0 ]] && echo "ALL KERNEL SETS MATCH SCALAR"
	exit $rc
fi

if [[ "$WINDOWED" == 1 ]]; then
	REFERENCE="soft"
	SUBJECT="soft"
	[[ -n "${SP_SESSION_TYPE:-}${XDG_SESSION_TYPE:-}" ]] \
		|| { echo "--windowed needs a session: set SP_SESSION_TYPE=wayland or =x11" >&2; exit 2; }
fi

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

note() { printf '%s\n' "$*"; }

if [[ "$DO_BUILD" == 1 ]]; then
	note "== building $PROJECT (SOFT=1, $TARGET) =="
	make -C "$PROJECT" STAPPLER_TARGET="$TARGET" SOFT=1 -j8 >/dev/null \
		|| { echo "build FAILED" >&2; exit 1; }
fi
[[ -x "$BIN" ]] || { echo "missing binary: $BIN (drop --no-build)" >&2; exit 1; }

python3 -c 'import numpy, PIL' 2>/dev/null \
	|| { echo "python3 needs numpy and Pillow" >&2; exit 2; }

# Render one case with one backend into $4. Returns nonzero if the app or the client
# failed; the app log is left in $WORK/<tag>.log either way.
#
# `mode` is headless or windowed. Windowed drops --headless, so the frame is presented
# through the window system's own buffers - which is the whole point of the --windowed
# check - and the surface size then becomes the WM's decision, not ours.
render() {
	local mode="$1" api="$2" steps="$3" out="$4" tag="$5" extraEnv="${6:-}"
	local w="${7:-$WIDTH}" h="${8:-$HEIGHT}" density="${9:-}"
	local sock="$WORK/$tag.sock" log="$WORK/$tag.log"
	local invokes=()
	local step
	for step in $steps; do invokes+=(--invoke "$step"); done

	local appArgs=(--gapi "$api" --width "$w" --height "$h")
	[[ "$mode" == headless ]] && appArgs+=(--headless)
	[[ -n "$density" ]] && appArgs+=(--density "$density")

	env $extraEnv XENOLITH_INSPECTOR_ADDRESS="unix:$sock" XL_FLAT_QUEUE=1 \
		"$BIN" "${appArgs[@]}" >"$log" 2>&1 &
	local app=$!

	local status=0
	python3 "$HERE/xlclient.py" --address "unix:$sock" --out "$out" \
		"${invokes[@]}" --quit >>"$log" 2>&1 || status=$?

	# --quit exits the process on its own; only step in if it did not
	if kill -0 "$app" 2>/dev/null; then
		sleep 1
		kill -0 "$app" 2>/dev/null && kill "$app" 2>/dev/null
	fi
	wait "$app" 2>/dev/null || true

	return $status
}

if [[ "${#SELECT[@]}" -gt 0 ]]; then
	SELECTED=()
	for want in "${SELECT[@]}"; do
		found=""
		for c in "${CASES[@]}"; do
			[[ "$(case_name "$c")" == "$want" ]] && { SELECTED+=("$c"); found=1; break; }
		done
		[[ -n "$found" ]] || { echo "unknown case: $want (see --list)" >&2; exit 2; }
	done
	CASES=("${SELECTED[@]}")
fi

if [[ -n "$SAVE" ]]; then
	mkdir -p "$SAVE" || exit 2
	note "== saving $REFERENCE baseline to $SAVE (${WIDTH}x${HEIGHT}) =="
fi

pass=0; fail=0; failed_names=(); damage_engaged=0
for c in "${CASES[@]}"; do
	name="$(case_name "$c")"
	mode="$(case_mode "$c")"
	steps="$(case_steps "$c")"
	damage_note=""
	tiles_note=""

	if [[ "$GLYPH_PATHS" == 1 ]]; then
		# the blit and the sampler must agree bit for bit; nothing here is approximate
		mode="exact"
	fi

	# A kernel set is byte-identical to scalar everywhere except where it samples a
	# texture: there the scalar quantizes through double and a vector kernel through
	# float, because four doubles in a register where eight floats fit throws away half
	# the reason to vectorize. One step in a channel is what that can cost - the same
	# tolerance these cases already carry against Vulkan, for the same reason. Two steps
	# is a defect of the kernel, not a reason to widen the gate.
	#
	# The allowance is headroom, not slack: as of M4.5-tex nothing actually uses it, and
	# the report still prints the real count, so "0 differing pixels" stays visible.
	if [[ -n "$KERNEL_SET" ]]; then
		case "$name" in
			sprite-*|clip|alpha) mode="tolerant" ;;
			*) mode="exact" ;;
		esac
	fi

	if [[ "$DAMAGE" == 1 || -n "$TILES" ]]; then
		mode="exact"
	fi

	ref="$WORK/ref-$name.png"
	act="$WORK/act-$name.png"

	if [[ "$WINDOWED" == 1 ]]; then
		# Windowed first, because it is the run whose size we do not get to choose: the WM
		# decides, and the surface may also be scaled by output density. The headless
		# reference is then rendered at exactly that buffer size, with the density that
		# reproduces the same logical layout - otherwise imgdiff refuses on a size
		# mismatch and the real question never gets asked.
		if ! render windowed "$SUBJECT" "$steps" "$act" "act-$name" "$SUB_ENV"; then
			printf '%-16s FAIL (%s did not render in a window)\n' "$name" "$SUBJECT"
			fail=$((fail+1)); failed_names+=("$name")
			[[ "$VERBOSE" == 1 ]] && sed 's/^/    /' "$WORK/act-$name.log"
			continue
		fi

		bufSize="$(python3 -c 'import sys;from PIL import Image
with Image.open(sys.argv[1]) as i: print("%d %d" % i.size)' "$act")"
		bufW="${bufSize%% *}"; bufH="${bufSize##* }"

		if (( bufW % WIDTH != 0 || bufH % HEIGHT != 0 || bufW / WIDTH != bufH / HEIGHT )); then
			printf '%-16s FAIL (window is %sx%s, not an integer scale of %sx%s)\n' \
					"$name" "$bufW" "$bufH" "$WIDTH" "$HEIGHT"
			fail=$((fail+1)); failed_names+=("$name")
			continue
		fi

		if ! render headless "$SUBJECT" "$steps" "$ref" "ref-$name" "$SUB_ENV" \
				"$bufW" "$bufH" "$((bufW / WIDTH))"; then
			printf '%-16s FAIL (%s did not render headless)\n' "$name" "$SUBJECT"
			fail=$((fail+1)); failed_names+=("$name")
			[[ "$VERBOSE" == 1 ]] && sed 's/^/    /' "$WORK/ref-$name.log"
			continue
		fi
	elif ! render headless "$REFERENCE" "$steps" "$ref" "ref-$name" "$REF_ENV"; then
		printf '%-16s FAIL (%s did not render)\n' "$name" "$REFERENCE"
		fail=$((fail+1)); failed_names+=("$name")
		[[ "$VERBOSE" == 1 ]] && sed 's/^/    /' "$WORK/ref-$name.log"
		continue
	fi

	if [[ -n "$SAVE" ]]; then
		cp "$ref" "$SAVE/$name.png"
		printf '%-16s saved\n' "$name"
		pass=$((pass+1))
		continue
	fi

	if [[ "$WINDOWED" == 1 ]]; then
		# Same rasterizer, same command list; only the destination memory differs. There is
		# no implementation freedom left to spend a tolerance on.
		mode="exact"
		subject_label="windowed"
	elif [[ -n "$BASELINE" ]]; then
		act="$BASELINE/$name.png"
		[[ -f "$act" ]] || { printf '%-16s FAIL (no baseline %s)\n' "$name" "$act"
			fail=$((fail+1)); failed_names+=("$name"); continue; }
		# a backend compared against a stored run of itself must be bit-identical
		mode="exact"
		subject_label="baseline"
	else
		subject_label="$SUBJECT"
		if ! render headless "$SUBJECT" "$steps" "$act" "act-$name" "$SUB_ENV"; then
			printf '%-16s FAIL (%s did not render)\n' "$name" "$SUBJECT"
			fail=$((fail+1)); failed_names+=("$name")
			[[ "$VERBOSE" == 1 ]] && sed 's/^/    /' "$WORK/act-$name.log"
			continue
		fi

		# A requested kernel set that quietly fell back would make this a scalar-vs-scalar
		# comparison, which passes and proves nothing. The app says which set it ran.
		if [[ -n "$KERNEL_SET" ]]; then
			used="$(sed -n 's/.*using kernel set: \([a-z0-9]*\).*/\1/p' "$WORK/act-$name.log" \
					| tail -1)"
			if [[ "$used" != "$KERNEL_SET" ]]; then
				printf '%-16s FAIL (asked for %s, ran %s)\n' "$name" "$KERNEL_SET" "${used:-none}"
				fail=$((fail+1)); failed_names+=("$name")
				continue
			fi
			subject_label="$KERNEL_SET"
		fi

		# A tiling that fell back to one tile per region, or a thread count the pool could not
		# supply, would make this a run against itself. The app reports both, so ask it.
		if [[ -n "$TILES" ]]; then
			subject_label="tiles"
			line="$(grep -o 'threads=[0-9]* .*tiles/frame=[0-9.]*' "$WORK/act-$name.log" | tail -1)"
			gotTiles="$(sed -n 's/.*tiles\/frame=\([0-9.]*\).*/\1/p' <<<"$line")"
			gotThreads="$(sed -n 's/.*threads=\([0-9]*\).*/\1/p' <<<"$line")"
			if [[ -z "$gotTiles" ]] || (( $(python3 -c "print(1 if ${gotTiles:-0} > 1.0 else 0)") == 0 )); then
				printf '%-16s FAIL (asked for %s tiles, got %s per frame)\n' \
						"$name" "$TILES" "${gotTiles:-none}"
				fail=$((fail+1)); failed_names+=("$name")
				continue
			fi
			if [[ "$gotThreads" != "$THREADS" ]]; then
				printf '%-16s FAIL (asked for %s threads, ran %s)\n' \
						"$name" "$THREADS" "${gotThreads:-none}"
				fail=$((fail+1)); failed_names+=("$name")
				continue
			fi
			tiles_note=" [$gotTiles tiles/frame, $gotThreads thread(s)]"
		fi

		# A case whose scene never changes gets one full repaint per swapchain image and then
		# nothing at all - the frames are skipped outright. That case proves nothing about the
		# damage path, and saying so is the difference between a gate and a green light.
		if [[ "$DAMAGE" == 1 ]]; then
			subject_label="damage"
			engaged="$(sed -n 's/.*damage: repainting \([0-9]*\) region(s), \([0-9]*\)%.*/\1r \2%/p' \
					"$WORK/act-$name.log" | sort -u -k2 | head -1)"
			if [[ -n "$engaged" ]]; then
				damage_engaged=$((damage_engaged+1))
				damage_note=" [engaged: $engaged]"
			else
				damage_note=" [vacuous: damage never engaged]"
			fi
		fi
	fi

	diffargs=()
	[[ "$mode" == "exact" ]] && diffargs+=(--exact)
	[[ "$VERBOSE" == 1 ]] && diffargs+=(--out-diff "$WORK/diff-$name.png")

	if summary="$(python3 "$HERE/imgdiff.py" "$ref" "$act" "${diffargs[@]}" 2>&1)"; then
		printf '%-16s OK   [%s] %s%s\n' "$name" "$mode" "$summary" "$damage_note$tiles_note"
		pass=$((pass+1))
	else
		printf '%-16s FAIL [%s] %s%s\n' "$name" "$mode" "$summary" "$damage_note$tiles_note"
		fail=$((fail+1)); failed_names+=("$name")
		if [[ "$VERBOSE" == 1 ]]; then
			cp "$ref" "/tmp/parity-$name-$REFERENCE.png" 2>/dev/null
			cp "$act" "/tmp/parity-$name-$subject_label.png" 2>/dev/null
			cp "$WORK/diff-$name.png" "/tmp/parity-$name-diff.png" 2>/dev/null
			echo "    images kept: /tmp/parity-$name-{$REFERENCE,$subject_label,diff}.png"
		fi
	fi
done

echo "----------------------------------------"
if [[ -n "$SAVE" ]]; then
	echo "saved $pass case(s) to $SAVE"
	[[ "$fail" -gt 0 ]] && { echo "failed: ${failed_names[*]}"; exit 1; }
	exit 0
fi
echo "matching: $pass   diverging: $fail   (of ${#CASES[@]})"
if [[ "$DAMAGE" == 1 ]]; then
	echo "cases where damage actually engaged: $damage_engaged (of ${#CASES[@]})"
	if [[ "$damage_engaged" == 0 ]]; then
		echo "VACUOUS: not one case took the partial-redraw path, so nothing was compared"
		exit 1
	fi
fi
if [[ "$fail" -gt 0 ]]; then
	echo "diverging cases: ${failed_names[*]}"
	echo "re-run with -v to keep the images and see the app log"
	exit 1
fi
echo "ALL CASES MATCH"
