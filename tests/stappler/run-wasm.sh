#!/bin/bash
# Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
#
# Build tests/stappler for wasm and run it headlessly under Node.
#
# Usage:
#   tests/stappler/run-wasm.sh                   # every case, one process
#   tests/stappler/run-wasm.sh zip css ...       # the named cases, one process each
#   SPRT_WASM_TARGET=wasm64-unknown-unknown tests/stappler/run-wasm.sh
#
# The embedded-filesystem cases compare each bundled file with its original under
# resources/, read relative to the working directory - natively that is tests/stappler.
# The Node runner serves its launch directory as the read-only bundle, so every run happens
# in a scratch directory holding a copy of resources/ and nothing else (launching from
# tests/stappler itself would drag the whole build tree into the bundle).
#
# A run fails on a non-zero exit (the suite exits with its failure count; 70 is a wasm trap,
# 124 the timeout) or on [FAIL] / [wasm trap] in the output.
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
TARGET="${SPRT_WASM_TARGET:-wasm32-unknown-unknown}"
RUNNER="${SPRT_WASM_RUNNER:-$ROOT/runtime/wasm-js/run-node.mjs}"
TIMEOUT="${SPRT_RUN_TIMEOUT:-900}"
WASM="$HERE/stappler-build/$TARGET/debug/cc/stapplertest.wasm"

echo "== building tests/stappler ($TARGET) =="
make -C "$HERE" STAPPLER_TARGET="$TARGET" -j"$(nproc)" >/dev/null || { echo "build failed"; exit 1; }

RUNDIR="$(mktemp -d)"
trap 'rm -rf "$RUNDIR"' EXIT
cp -r "$HERE/resources" "$RUNDIR/resources"
LOGDIR="$HERE/stappler-build/$TARGET/wasm-runs"
mkdir -p "$LOGDIR"

status=0
run_one() {
	local name="$1" log
	log="$LOGDIR/${name:-all}.log"
	(cd "$RUNDIR" && timeout "$TIMEOUT" node "$RUNNER" "$WASM" stapplertest ${name:+"$name"}) \
		> "$log" 2>&1 < /dev/null
	local code=$?
	local bad
	bad=$(grep -cE '\[FAIL\]|wasm trap' "$log")
	if [ "$code" -ne 0 ] || [ "$bad" -ne 0 ]; then
		echo "FAIL  ${name:-<all>}: exit=$code, $bad failure lines (log: $log)"
		grep -E '\[FAIL\]|wasm trap' "$log" | head -20 | sed 's/^/      /'
		status=1
	else
		echo "PASS  ${name:-<all>}: $(grep -c '\[ OK \]' "$log") checks"
	fi
}

if [ "$#" -eq 0 ]; then
	run_one ""
else
	for t in "$@"; do run_one "$t"; done
fi
exit "$status"
