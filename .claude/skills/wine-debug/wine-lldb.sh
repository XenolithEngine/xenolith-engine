#!/usr/bin/env bash
#
# wine-lldb.sh — debug a Windows PE (.exe) running under wine, from native linux lldb.
#
#   linux lldb ──gdb-remote──▶ wine lldb-server.exe (platform mode) ──▶ debuggee (wine)
#
# Use it to run a Windows test build under wine and, when it faults (access
# violation, stack overflow, assert), get an automatic backtrace + registers —
# without a Windows machine.
#
# Program stdout/stderr appear on the lldb-SERVER console (captured to a log and
# reprinted at the end). Debugger I/O (breakpoints, frames, bt) is in the client.
#
# Usage:
#   wine-lldb.sh <target.exe> [options]
#   wine-lldb.sh <target.exe>                     # run to exit; on crash → bt + registers
#   wine-lldb.sh <target.exe> -b <symbol>         # break at symbol, then run
#   wine-lldb.sh <target.exe> -s <cmds.lldb>      # run your own lldb command file
#   wine-lldb.sh <target.exe> -- arg1 arg2        # pass args to the program
#   wine-lldb.sh --restart-server                 # kill+restart the platform server
#
# Options:
#   -b, --break <sym>   set a breakpoint (repeatable)
#   -s, --source <file> lldb command file run after target-create, instead of a bare `run`
#   -p, --port <n>      platform-server port (default 1234)
#   -o, --stop-at-entry stop at entry instead of running
#       --restart-server  force a fresh lldb-server before connecting
#   -- <args...>        everything after -- is passed to the debuggee
set -u

# --- locate the repo & toolchain (script lives at <root>/.claude/skills/wine-debug/) ---
SELF="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SELF/../../.." && pwd)"
# The host toolchain lives in one of two places; toolchains/hosts wins over
# runtime/toolchains/hosts. Override with $XENOLITH_HOSTS.
LLDB_SERVER=""
for h in "${XENOLITH_HOSTS:-}" "$ROOT/toolchains/hosts" "$ROOT/runtime/toolchains/hosts"; do
  [ -n "$h" ] || continue
  if [ -f "$h/x86_64-pc-windows-msvc/bin/lldb-server.exe" ]; then
    LLDB_SERVER="$h/x86_64-pc-windows-msvc/bin/lldb-server.exe"; break
  fi
done
LLDB="${LLDB:-lldb}"                     # native linux lldb

# --- args ---
PORT=1234
TARGET=""
SRCFILE=""
STOP_AT_ENTRY=0
RESTART=0
declare -a BREAKS=()
declare -a PROG_ARGS=()
while [ $# -gt 0 ]; do
  case "$1" in
    -b|--break)   BREAKS+=("$2"); shift 2 ;;
    -s|--source)  SRCFILE="$2"; shift 2 ;;
    -p|--port)    PORT="$2"; shift 2 ;;
    -o|--stop-at-entry) STOP_AT_ENTRY=1; shift ;;
    --restart-server)   RESTART=1; shift ;;
    --)           shift; PROG_ARGS=("$@"); break ;;
    -h|--help)    sed -n '2,40p' "${BASH_SOURCE[0]}"; exit 0 ;;
    -*)           echo "unknown option: $1" >&2; exit 2 ;;
    *)            TARGET="$1"; shift ;;
  esac
done

[ -x "$(command -v "$LLDB" 2>/dev/null)" ] || { echo "native lldb not found (set \$LLDB)" >&2; exit 1; }
[ -n "$LLDB_SERVER" ] || { echo "lldb-server.exe not found under toolchains/hosts or runtime/toolchains/hosts (set \$XENOLITH_HOSTS)" >&2; exit 1; }

SRVLOG="/tmp/wine-lldb-server-$PORT.log"

server_up() { ss -tln 2>/dev/null | grep -q ":$PORT "; }

start_server() {
  echo "### starting lldb-server.exe on :$PORT  (log: $SRVLOG)"
  WINEDEBUG="${WINEDEBUG:--all}" nohup wine "$LLDB_SERVER" platform --server \
      --listen "*:$PORT" >"$SRVLOG" 2>&1 &
  for _ in $(seq 1 20); do server_up && return 0; sleep 0.5; done
  echo "### server failed to come up — see $SRVLOG" >&2; return 1
}

if [ "$RESTART" = 1 ]; then pkill -f "lldb-server.exe platform" 2>/dev/null; sleep 1; fi
server_up || start_server || exit 1
[ -n "$TARGET" ] || { echo "### server ready on :$PORT (no target given)"; exit 0; }
[ -f "$TARGET" ] || { echo "target not found: $TARGET" >&2; exit 1; }
TARGET="$(cd "$(dirname "$TARGET")" && pwd)/$(basename "$TARGET")"   # absolutize

# --- remember where the server log is now, so we print only THIS run's output ---
LOG_OFFSET=$( [ -f "$SRVLOG" ] && wc -c <"$SRVLOG" || echo 0 )

# --- build the lldb command list ---
declare -a O=()
O+=(-o "platform select remote-linux")
O+=(-o "platform connect connect://localhost:$PORT")
O+=(-o "target create $TARGET")
[ ${#PROG_ARGS[@]} -gt 0 ] && O+=(-o "settings set target.run-args ${PROG_ARGS[*]}")
for b in "${BREAKS[@]}"; do O+=(-o "breakpoint set --name $b"); done
# -s is a SETUP file (breakpoints / settings) sourced before the run — do NOT put
# run/continue in it, or a crash there won't trigger the on-crash (-k) backtrace.
[ -n "$SRCFILE" ] && O+=(-o "command source $SRCFILE")
if [ "$STOP_AT_ENTRY" = 1 ]; then
  O+=(-o "process launch --stop-at-entry")
else
  O+=(-o "run")
fi
# on-crash (-k): fires only if the inferior faults; harmless on clean exit.
# bt is depth-capped so a stack overflow (infinite recursion) doesn't dump forever.
O+=(-k "bt all")
O+=(-k "thread backtrace -c 60")
O+=(-k "register read")
O+=(-k "thread list")
O+=(-k "quit")

echo "### lldb session (target: $TARGET)"
"$LLDB" --batch "${O[@]}"
RC=$?

echo
echo "### ---- program console output (this run, from wine server) ----"
tail -c "+$((LOG_OFFSET + 1))" "$SRVLOG" 2>/dev/null \
  | grep -av -E "libEGL|pci id for fd|dri2 screen|fixme:|lldb-server|^Connection established|^Disconnected|^Launched '"
echo "### ---- end program output (lldb rc=$RC) ----"
exit $RC
