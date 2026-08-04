#!/usr/bin/env bash
# Autotest for aux tip-slot / swapchain poison.
# Build, launch via `open` (required on macOS), wait for /tmp/auxui_selftest.status.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
ENGINE="${XENOLITH_ENGINE:-$ROOT}"
APP="$ENGINE/tests/auxui/stappler-build/aarch64-apple-macosx/debug/cc/auxui.app"
LOG="${AUXUI_SELFTEST_LOG:-/tmp/auxui_selftest.log}"
STATUS=/tmp/auxui_selftest.status
TRIPLE="${STAPPLER_TARGET:-aarch64-apple-macosx}"

if ! command -v xenolith-cli >/dev/null 2>&1; then
	echo "xenolith-cli not on PATH" >&2
	exit 127
fi

xenolith-cli build "$ENGINE/tests/auxui" --engine "$ENGINE" --target "$TRIPLE" -j8

# Kill a previous harness instance only (not the user's interactive session if different binary).
pkill -f 'auxui.app/Contents/MacOS/auxui' 2>/dev/null || true
sleep 0.3

rm -f "$STATUS"
: >"$LOG"

AUXUI_SELFTEST=1 open "$APP" --stdout "$LOG" --stderr "$LOG"

deadline=$((SECONDS + 45))
while [[ ! -f "$STATUS" && SECONDS -lt $deadline ]]; do
	sleep 0.25
done

if [[ ! -f "$STATUS" ]]; then
	echo "SELFTEST TIMEOUT — no $STATUS after 45s" >&2
	echo "---- log tail ----" >&2
	tail -n 80 "$LOG" >&2 || true
	exit 2
fi

code="$(tr -d '[:space:]' <"$STATUS")"
echo "AuxSelfTest status=$code"
grep -E 'AuxSelfTest:|firstFrame id=tooltip|native Tooltip|MaterialSwapchainPass' "$LOG" \
	| sed 's/\x1b\[[0-9;]*m//g' || true

if [[ "$code" != "0" ]]; then
	echo "SELFTEST FAILED" >&2
	exit 1
fi

# Belt: greps that must never appear for a green run.
if grep -E 'createWindow begin type=Tooltip|MacosWindow: init type=Tooltip' "$LOG" >/dev/null; then
	echo "SELFTEST FAILED: native Tooltip window in log" >&2
	exit 1
fi
if grep -E 'firstFrame id=tooltip-[^ ]+ success=0' "$LOG" >/dev/null; then
	echo "SELFTEST FAILED: poison firstFrame in log" >&2
	exit 1
fi
if grep -E 'Fail to submit|MaterialSwapchainPass' "$LOG" >/dev/null; then
	echo "SELFTEST FAILED: present submit failure in log" >&2
	exit 1
fi

echo "SELFTEST OK"
exit 0
