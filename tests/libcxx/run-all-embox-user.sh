#!/bin/bash
# Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
#
# Batch driver for Embox user mode: every tracked scope through
# run-embox-user.sh, one summary line per scope, the same line run-all.sh
# prints:  SCOPE|discovered|pass|compile_fail|link_fail|run_fail|unsupported|unresolved
# Needs a serving pool (see run-embox-user.sh).
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [ "$#" -gt 0 ]; then
  SCOPES=("$@")
else
  SCOPES=(
    containers/associative containers/sequences containers/unord containers/container.adaptors
    containers/views containers/container.requirements algorithms strings utilities iterators
    numerics language.support diagnostics concepts localization input.output ranges time thread
    atomics re experimental depr library modules containers/container.node containers/containers.general
  )
fi
field() { local v; v=$(sed -n "s/^[[:space:]]*$1[[:space:]]*:[[:space:]]*\([0-9][0-9]*\).*/\1/p" <<<"$2" | head -1); echo "${v:-0}"; }
for s in "${SCOPES[@]}"; do
  out="$("$HERE/run-embox-user.sh" "$s" -s 2>&1 || true)"
  if grep -q "^error:" <<<"$out"; then
    echo "$s|$(grep -m1 '^error:' <<<"$out")"
    continue
  fi
  disc=$(sed -n 's/.*Total Discovered Tests: \([0-9]*\).*/\1/p' <<<"$out" | head -1)
  echo "$s|${disc:-0}|$(field Passed "$out")|$(field 'Compile Failed' "$out")|$(field 'Link Failed' "$out")|$(field 'Runtime Failed' "$out")|$(field Unsupported "$out")|$(field Unresolved "$out")"
done
