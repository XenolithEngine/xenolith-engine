# Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
#
# Shared by compare.sh and bench.sh: ask the binary which rasterizer kernel sets it can
# actually run here. Hard-coding the list would quietly skip a set on one machine and
# invent one on another - it depends on the architecture and, on x86, on the CPU.
#
# The list is logged lazily, on the first rasterized frame, so this has to render.
#
# usage: discover_kernel_sets <binary> <workdir> [client]

discover_kernel_sets() {
	local bin="$1" work="$2"
	local client="${3:-$(dirname "${BASH_SOURCE[0]}")/benchclient.py}"
	local sock="$work/kernelsets.sock" log="$work/kernelsets.log"

	# Retried once: launching an app and talking to its socket is the flakiest thing either
	# script does, and an empty list here is reported as "this binary has no kernels", which
	# sends the reader looking in entirely the wrong place.
	local attempt
	for attempt in 1 2; do
		rm -f "$sock" "$log"
		( timeout -k 3 60 env XENOLITH_INSPECTOR_ADDRESS="unix:$sock" XL_FLAT_QUEUE=1 \
			"$bin" --gapi soft --headless --width 256 --height 256 >"$log" 2>&1 ) &
		local app=$!

		timeout 40 python3 "$client" --address "unix:$sock" --frames 8 --quit >>"$log" 2>&1 || true

		if kill -0 "$app" 2>/dev/null; then kill "$app" 2>/dev/null; fi
		wait "$app" 2>/dev/null || true

		local sets
		sets="$(sed -n 's/.*available kernel sets:\([ a-z0-9]*\).*/\1/p' "$log" | tail -1 | xargs)"
		if [[ -n "$sets" ]]; then
			printf '%s' "$sets"
			return 0
		fi
	done
	return 1
}
