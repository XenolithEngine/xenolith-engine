#!/bin/bash
# Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
#
# Build tests/runtime for wasm and run it headlessly under Node, then grade the run.
#
# Usage:
#   tests/runtime/run-wasm.sh                     # every case, one process
#   tests/runtime/run-wasm.sh libc_pthread ...    # the named cases, one process each
#   SPRT_WASM_TARGET=wasm64-unknown-unknown tests/runtime/run-wasm.sh
#
# Grading: most cases print PASS/FAIL and still exit 0, and a trap loses the tail of the
# stdout buffer, so the exit code alone says little. A run fails on a non-zero exit (70 is
# a wasm trap, 124 the timeout - a real hang, not a slow test) or on FAIL / failed /
# [wasm trap] / [E] in the output. The permanent wasm SKIPs (runtime_process,
# runtime_socket, libc_setjmp) report themselves and are not failures.
#
# The runner loads its working directory into the read-only bundle, so every run happens
# in a small scratch directory holding only the probe file libc_wasm64_highmem reads back.
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
TARGET="${SPRT_WASM_TARGET:-wasm32-unknown-unknown}"
RUNNER="${SPRT_WASM_RUNNER:-$ROOT/runtime/wasm-js/run-node.mjs}"
TIMEOUT="${SPRT_RUN_TIMEOUT:-900}"
WASM="$HERE/stappler-build/$TARGET/debug/cc/runtimetest.wasm"

echo "== building tests/runtime ($TARGET) =="
make -C "$HERE" STAPPLER_TARGET="$TARGET" -j"$(nproc)" >/dev/null || { echo "build failed"; exit 1; }

RUNDIR="$(mktemp -d)"
trap 'rm -rf "$RUNDIR"' EXIT
printf 'sprt wasm64 bundle probe\n' > "$RUNDIR/wasm64-bundle.txt"
LOGDIR="$HERE/stappler-build/$TARGET/wasm-runs"
mkdir -p "$LOGDIR"

status=0
run_one() {
	local name="$1" log
	log="$LOGDIR/${name:-all}.log"
	(cd "$RUNDIR" && timeout "$TIMEOUT" node "$RUNNER" "$WASM" runtimetest ${name:+"$name"}) > "$log" 2>&1
	local code=$?
	local bad
	bad=$(grep -cE 'FAIL|failed|wasm trap|\[E\]' "$log")
	if [ "$code" -ne 0 ] || [ "$bad" -ne 0 ]; then
		echo "FAIL  ${name:-<all>}: exit=$code, $bad failure lines (log: $log)"
		grep -E 'FAIL|failed|wasm trap|\[E\]' "$log" | head -20 | sed 's/^/      /'
		status=1
	else
		echo "PASS  ${name:-<all>}: $(grep -c PASS "$log") PASS lines, $(grep -c SKIP "$log") SKIP lines"
	fi
}

if [ "$#" -eq 0 ]; then
	run_one ""
else
	for t in "$@"; do run_one "$t"; done
fi
exit "$status"
