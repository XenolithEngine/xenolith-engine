#!/bin/sh
# Build the same sources against the static runtime and against sprt.dll, run both under
# Wine, and diff the output.
#
# The interesting part is the tail: the teardown sequence (atexit handlers, static
# destructors, then this image's .CRT pre-terminators and terminators) has to come out in
# the same order either way, even though in the shared build exit() lives in the DLL and
# can only walk the DLL's own terminator sections - the executable's are driven by the
# startup stub instead.
set -e

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)

target=${STAPPLER_TARGET:-x86_64-pc-windows-msvc}
jobs=${JOBS:-8}

run() { # <exe dir>
	(cd "$1" && WINEDEBUG=-all wine ./sprtdlltest.exe 2>/dev/null)
}

echo "--- building static runtime + test"
make -C "$root/runtime" STAPPLER_TARGET="$target" RELEASE=1 -j"$jobs" >/dev/null
make -C "$here" STAPPLER_TARGET="$target" RELEASE=1 SPRT_SHARED=0 -j"$jobs" >/dev/null

echo "--- building shared runtime + test"
make -C "$root/runtime" STAPPLER_TARGET="$target" RELEASE=1 SPRT_SHARED=1 -j"$jobs" >/dev/null
make -C "$here" STAPPLER_TARGET="$target" RELEASE=1 SPRT_SHARED=1 -j"$jobs" >/dev/null

static_dir="$here/stappler-build-static/$target/release/cc"
shared_dir="$here/stappler-build/$target/release/cc"

cp "$root/runtime/stappler-build-shared/$target/release/cc/sprt.dll" "$shared_dir/"

static_out=$(mktemp)
shared_out=$(mktemp)
trap 'rm -f "$static_out" "$shared_out"' EXIT

run "$static_dir" >"$static_out"
static_status=$?
run "$shared_dir" >"$shared_out"
shared_status=$?

echo "--- static (exit=$static_status)"
cat "$static_out"

if [ "$static_status" -ne "$shared_status" ]; then
	echo "EXIT STATUS DIFFERS: static=$static_status shared=$shared_status" >&2
	exit 1
fi

if diff -u "$static_out" "$shared_out"; then
	echo "STATIC AND SHARED IDENTICAL"
else
	echo "OUTPUT DIVERGES" >&2
	exit 1
fi
