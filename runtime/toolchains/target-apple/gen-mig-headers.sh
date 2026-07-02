#!/bin/sh
# Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
#
# gen-mig-headers.sh SDK XNU_OSFMK OUTDIR [ARCH]
#
# Generate the MIG-derived <mach/*.h> user + server headers that mach_interface.h
# pulls in but that are NOT checked into any apple-oss source (they are produced
# from the .defs by Apple's `mig` at OS-build time). These are BAKED INTO GIT under
# OUTDIR/mach (installed into the +open sysroot by open-sysroot.mk), so a normal
# +open build needs neither `mig` nor the Xcode SDK — only regenerating them does.
#
#   SDK        a real macOS SDK sysroot (for the C type headers the .defs reference,
#              and most of the .defs themselves) — passed to mig via -isysroot
#   XNU_OSFMK  apple-oss xnu osfmk/ dir (source of upl.defs, absent from the SDK)
#   OUTDIR     destination; headers land in OUTDIR/mach
#   ARCH       mig -arch value (default x86_64; the user headers are prototype-only
#              and arch-neutral, so one arch serves both x86_64 and arm64)
#
# Requires: mig + migcom on PATH (/usr/local/bin here).

set -eu

SDK="$1"; XNU="$2"; OUT="$3"; ARCH="${4:-x86_64}"
mkdir -p "$OUT/mach"
tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT

INC="-isysroot $SDK -I$SDK/usr/include -I$XNU"

# Resolve a .defs by name: prefer the SDK copy, fall back to xnu osfmk/mach.
resolve() {
	if   [ -f "$SDK/usr/include/mach/$1" ]; then echo "$SDK/usr/include/mach/$1"
	elif [ -f "$XNU/mach/$1" ];             then echo "$XNU/mach/$1"
	else echo "gen-mig-headers: missing defs $1" >&2; exit 1
	fi
}

# user routine header (-header); discard the user/server stubs + server header.
user() {
	mig -arch "$ARCH" $INC -header "$OUT/mach/$2" \
		-user "$tmp/u.c" -server "$tmp/s.c" -sheader "$tmp/sh.h" "$(resolve "$1")"
	echo "  user   mach/$2"
}

# libsyscall-variant user header: -DLIBSYSCALL_INTERFACE selects the underscore-prefixed
# routine variants the SDK ships (e.g. _mach_make_memory_entry in mach_vm.h, which the
# plain build would emit as mach_make_memory_entry and clash with vm_map.h). This is how
# Apple generates the shipped libsyscall <mach/*> user headers. Used for the newer vm/
# voucher/memory-entry interfaces added below.
user_ls() {
	mig -arch "$ARCH" -DLIBSYSCALL_INTERFACE $INC -header "$OUT/mach/$2" \
		-user "$tmp/u.c" -server "$tmp/s.c" -sheader "$tmp/sh.h" "$(resolve "$1")"
	echo "  user*  mach/$2"
}

# server routine header (-sheader); discard the rest.
server() {
	mig -arch "$ARCH" $INC -header "$tmp/h.h" \
		-user "$tmp/u.c" -server "$tmp/s.c" -sheader "$OUT/mach/$2" "$(resolve "$1")"
	echo "  server mach/$2"
}

# --- user headers (mach_interface.h client side) ---
user clock.defs          clock.h
user clock_priv.defs     clock_priv.h
user host_priv.defs      host_priv.h
user host_security.defs  host_security.h
user processor.defs      processor.h
user processor_set.defs  processor_set.h
# These five defs carry `#if !KERNEL && !LIBSYSCALL_INTERFACE → userprefix _kernelrpc_;`
# — a plain build renames the routines (mach_port_deallocate → _kernelrpc_...), which is
# NOT what the SDK ships (consumers like LLVM's Threading.inc call the plain names).
# Generate them the way Apple does the shipped user headers: with LIBSYSCALL_INTERFACE.
user_ls mach_host.defs    mach_host.h
user_ls mach_port.defs    mach_port.h
user_ls task.defs         task.h
user_ls thread_act.defs   thread_act.h
user_ls vm_map.defs       vm_map.h
# newer vm / voucher / memory-entry user interfaces the SDK ships (from mach_vm.defs &c.)
# — libsyscall variants (see user_ls); mach_vm.h's _mach_make_memory_entry must use the
# underscore form or it clashes with vm_map.h.
user_ls mach_vm.defs      mach_vm.h
user_ls mach_voucher.defs mach_voucher.h
user_ls memory_entry.defs memory_entry.h
# exc.defs user side (client exception_raise routines) — the SDK ships mach/exc.h beside
# mach/exc_server.h; the two don't collide (different routine names).
user exc.defs             exc.h

# The SDK's <mach/mach_host.h> (alone among the MIG headers) additionally pulls in
# <mach/mach_init.h> — the source of mach_host_self()/mach_task_self()/the host_page_size()
# wrapper. MIG itself does not emit that include, so inject it here to match the SDK
# (e.g. MoltenVK's MVKOSExtensions.mm includes only <mach/mach_host.h> yet uses all three).
sed -i 's|#include <mach/port.h>|#include <mach/port.h>\n#include <mach/mach_init.h>|' "$OUT/mach/mach_host.h"
echo "  patch  mach/mach_host.h (+#include <mach/mach_init.h>)"
# NOTE: mach_eventlink.defs NOT generated — its header needs <mach/mach_eventlink_types.h>
# (mach_eventlink_*_option_t), which +open doesn't ship; niche real-time-sync API.
# NOTE: clock_reply.defs user header (mach/clock_reply.h) NOT generated — its
# clock_alarm_reply clashes with clock_reply_server.h, which our (xnu-derived)
# mach_interface.h pulls in (the real SDK's mach_interface.h does NOT include the server
# headers). clock alarm replies are a niche API; the server side is kept.
# NOTE: mach/upl.h is intentionally NOT generated — upl.defs uses the kernel-only
# upl_t type, and the real macOS SDK ships neither mach/upl.h nor an <mach/upl.h>
# include in mach_interface.h (open-sysroot.mk strips that kernel-only include).

# --- server headers (the *_server.h set) ---
server clock_reply.defs  clock_reply_server.h
server exc.defs          exc_server.h
server mach_exc.defs     mach_exc_server.h
server notify.defs       notify_server.h

echo "gen-mig-headers: wrote 20 headers to $OUT/mach"
