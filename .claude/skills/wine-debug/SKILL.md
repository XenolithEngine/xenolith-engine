---
name: wine-debug
description: >-
  Debug a Windows (.exe) build under wine from native linux lldb — catch a crash,
  access violation, or stack overflow and get a source-level backtrace, registers,
  and locals, all on Linux without a Windows machine. Use when a Windows/MSVC
  cross-build (tests/libc, tests/window, any x86_64-pc-windows-msvc target) faults,
  hangs, or misbehaves under wine and you need to find *where* and *why* — or
  whenever you'd reach for a debugger on a Windows binary here.
---

# Debugging a Windows PE under wine with lldb

The repo ships an LLDB that runs under wine. Native Linux `lldb` drives it over the
gdb-remote protocol, so you get a real source-level debugger on a Windows `.exe`
**without leaving Linux**:

```
linux lldb ──gdb-remote──▶ wine lldb-server.exe (platform mode) ──▶ debuggee (wine)
```

Debug info comes from the `.pdb` next to the `.exe`, so breakpoints by name, C++
symbol demangling, source lines, inlined frames, `frame variable`, and backtraces
all work. Windows exceptions are translated (access violation `0xc0000005`,
stack overflow `0xc00000fd`, etc.), so a fault stops the debugger instead of
vanishing.

## Quick start — use the driver script

`wine-debug/wine-lldb.sh` starts the platform server (once; it survives many
sessions), connects, runs the program, and **on a crash automatically prints a
backtrace + registers**. On a clean exit it just prints the program's output.

```bash
S=.claude/skills/wine-debug/wine-lldb.sh
EXE=tests/libc/stappler-build/x86_64-pc-windows-msvc/debug/cc/libctest.exe

"$S" "$EXE"                 # run to exit or crash; auto-backtrace on fault
"$S" "$EXE" -b some_func    # break at a symbol, then run (repeatable)
"$S" "$EXE" -o              # stop at entry (then it's on you to drive lldb)
"$S" "$EXE" -- arg1 arg2    # pass args to the program
"$S" "$EXE" -s setup.lldb   # source a SETUP file (breakpoints/settings) before the run
"$S" --restart-server       # kill + relaunch the wine platform server
```

The script prints the whole lldb session, then a `program console output` section
(see the stdout note below). `rc=` at the end is lldb's exit code.

## Reading a crash

On a fault the script emits `stop reason = Exception 0x…` followed by `bt all`,
`thread backtrace -c 60`, `register read`, `thread list`. To interpret:

| Exception code | Meaning | Typical cause |
|---|---|---|
| `0xc0000005` | access violation | null / dangling / OOB pointer, bad `rip` |
| `0xc00000fd` | **stack overflow** | unbounded recursion, huge stack object |
| `0xc000001d` | illegal instruction | corrupt code ptr, bad call target |
| `0x80000003` | breakpoint (int3) | hit a `__debugbreak` / assert / trap |

**Stack overflow** shows the same deep frame repeating; `thread backtrace -c 60`
caps the dump so infinite recursion doesn't scroll forever — look for the cycle
of frames and the function that re-enters itself. The faulting `rsp` near the
bottom of the thread's stack region confirms exhaustion.

## Manual recipe (full control)

When you need custom stepping, watchpoints, or expression evaluation, drive lldb
yourself. Start the server (`lldb-server.exe` lives under the host toolchain —
see "Toolchain location" below):

```bash
wine toolchains/hosts/x86_64-pc-windows-msvc/bin/lldb-server.exe \
     platform --server --listen "*:1234"      # leave running in the background
```

Then, in native `lldb`:

```
platform select remote-linux
platform connect connect://localhost:1234
target create <abs-path-to>.exe
breakpoint set --name <symbol>
run                        # or: process launch --stop-at-entry
# on stop:
bt                         # backtrace
frame select 1             # pick the frame that actually has your locals
frame variable             # or: frame variable <name>
```

For a batch run that auto-backtraces on crash, pass the run as a direct `-o`
command and attach on-crash (`-k`) commands:

```bash
lldb --batch \
  -o "platform select remote-linux" \
  -o "platform connect connect://localhost:1234" \
  -o "target create <abs>.exe" \
  -o "run" \
  -k "bt all" -k "register read" -k "quit"
```

## Gotchas (learned the hard way)

- **Program stdout/stderr go to the lldb-SERVER console, not the lldb client.**
  The script captures the server log and reprints *only this run's* output at the
  end. Driving lldb manually, watch the server's terminal/log for program output.
- **`--batch` aborts on the first command error.** A failing `frame variable`
  (e.g. the var isn't in the current frame) kills the rest of the batch. Select
  the right frame first — a breakpoint often lands in an *inlined* frame #0 where
  your locals aren't in scope; the real frame is #1 (`frame select 1`).
- **`-k` (on-crash) only fires for crashes during direct `-o` commands.** A crash
  *inside* a `command source`d file is not treated as a batch crash and won't
  auto-backtrace. So a `-s` file is for **setup only** (breakpoints, `settings
  set …`) — never put `run`/`continue` in it; let the script own the run.
- **The platform server survives across client sessions** — start it once and
  reconnect repeatedly; only `--restart-server` if it wedged. It listens via
  `wineserver`; check with `ss -tln | grep :1234`.
- **Always give `target create` an absolute path.** The remote working dir is a
  wine `Z:\…` path; relative paths resolve against surprising places.
- Ignore the `libEGL … dri2 screen` / `fixme:` wine noise on startup — filtered
  out by the script.

## Toolchain location

The wine-hosted `lldb-server.exe` (and `clang`, `lld`, etc.) live under a host
toolchain dir that sits in **one of two places**, checked in this priority order:

1. `toolchains/hosts/x86_64-pc-windows-msvc/bin/` *(preferred — wins if present)*
2. `runtime/toolchains/hosts/x86_64-pc-windows-msvc/bin/` *(fallback)*

The driver script probes both and uses the first that exists. Override the search
with `XENOLITH_HOSTS=/path/to/hosts` if your layout differs.

## Building the target first

The `.exe` + `.pdb` must exist. Build the Windows target via the `xenolith-build`
skill — CLI preferred (`xenolith-cli build tests/libc --engine <engine>
--target x86_64-pc-windows-msvc`), raw `make -C tests/libc
STAPPLER_TARGET=x86_64-pc-windows-msvc` only if the CLI is missing. A debug build
(default, not `--release` / `RELEASE=1`) gives the richest line info.
