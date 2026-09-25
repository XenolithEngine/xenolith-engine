#!/usr/bin/env python3
"""The test protocol of the engine, as a program: what a change owes, and how much of it at once.

    tests/run-checks.py                  # FAST - what the working tree's diff can break
    tests/run-checks.py console          # the console harnesses alone, no window at all
    tests/run-checks.py suite window     # every headless window check
    tests/run-checks.py full             # THE GATE: every console harness and every window check
    tests/run-checks.py --list           # print the plan and run nothing

WHY THIS EXISTS, AND WHAT IT COSTS. Measured on a 16-core Linux host on 2026-09-13, debug builds,
nothing else running:

    runtimetest 12 s, stapplertest 4 s, libctest / localetest / uilayouttest under a second
    gittest 19 s
    the 29 window checks   761 s at -j1, 226 s at -j4, 136 s at -j8 (1631 checks)

So the console harnesses are a coffee-free twenty seconds and the window suite is thirteen minutes
sequentially - which is why nobody runs it after an edit, and why a stand rots. What the tiers do is
name the middle: the checks whose subject the diff touched.

HOW A CHANGE IS MAPPED. The console harnesses go by directory (`OWES` below - the same table
`docs/agents/test-projects.md` states in prose). The window checks go by NAME: this repository names
each script after the widget it drives, so `XLUiSlider.cc` selects `slider-check.py`. The file name
is split on camel case rather than searched as a string, because a substring search answers
`text-input-check` for `XLContext.cc`. It is a heuristic and it is declared as one: a `xenolith/`
change that names no widget still gets `WINDOW_SMOKE` - five cheap scripts that between them touch
layout, style, hit-testing, the canvas and hotkeys - and a path that matches nothing at all widens
the plan to everything. `full` is what a commit is gated on regardless.

A WINDOW CHECK WITH ITS OWN BINARY. `particles-check.py` drives `examples/window/particles`, not
`testapp` (`WINDOW_BINARIES`): it is selected by name like the rest (`XL2dParticleSystem.cc`,
`XL2dVkParticlePass.cc`), and also by a change under the example itself (`EXAMPLE_CHECKS`), which is
otherwise outside the plan. An unbuilt example skips it the way an unbuilt harness is skipped.

PARALLELISM. Every window check starts its own `testapp` on its own unix socket (`/tmp/xl-*.sock`,
overridable through `XENOLITH_INSPECTOR_SOCK`), so two of them share nothing but the machine.
Measured: 761 s at `-j1`, 226 s at `-j4`, 136 s at `-j8`, GREEN in all three - contention costs job
time (761 s of it becomes 932 s at `-j8`) and no correctness. `-j4` is the default and `-j8` is what
the gate can be run at on an idle machine; `-j1` is what a suspicious failure is re-run at.

WHAT IS NOT HERE. `tests/window/xcb-side-check.py` drives a REAL X11 session with XTEST and takes
the keyboard focus, so it fails whenever another window holds it: run it by hand
(`XL_TEST_DISPLAY=:1 tests/window/xcb-side-check.py`) after touching XcbWindow's key handling.
`markdown-perf-check.py` MEASURES rather than checks, and cannot run at all as things stand - it
regenerates its corpus with a `gen-big-md.py` that is not in the repository. `tests/com`,
`tests/wwin`, `tests/wthread`, `tests/mtl`, `tests/auxui` and `tests/wasm` are cross-target harnesses
(Windows under wine, macOS, wasm) and belong to `docs/agents/cross-target.md`, not to an iteration
loop on Linux.

`computetest` needs a Vulkan device and is not in the `console` tier; `xenolith/core` and
`xenolith/backend/vk` owe it. On a host with no loader or no device it prints SKIP, counts 0 checks
and exits 0 - a skip is not a pass, so read the `device` line before calling the GPU path covered.

The exit status is the number of RED jobs, and every FAIL line of every harness is reprinted at the
end: a runner that reports the count and throws away which check failed is worse than no runner.
"""

import argparse
import os
import re
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TARGET = os.environ.get("STAPPLER_TARGET", "x86_64-unknown-linux-gnu")
BUILD = os.environ.get("XL_CHECK_BUILD", "debug")

# The console harnesses: (project, binary, arguments, is it in the `console` tier?).
# `tesstest` is two goldens and neither alone is the check - geometry can move without the digest
# moving and the other way round (docs/agents/test-projects.md) - so both are listed.
CLI = [
    ("tests/runtime", "runtimetest", [], True),
    ("tests/libc", "libctest", [], True),
    ("tests/locale", "localetest", [], True),
    ("tests/uilayout", "uilayouttest", [], True),
    ("tests/stappler", "stapplertest", [], True),
    ("tests/particles", "particlestest", [], True),
    ("tests/tess", "tesstest", ["golden"], False),
    ("tests/tess", "tesstest", ["raster-golden"], False),
    ("tests/git", "gittest", [], False),
    ("tests/thirdparty", "thirdpartytest", [], False),
    ("tests/remote", "remotetest", [], False),
    ("tests/compute", "computetest", [], False),
]

# Which console harnesses a directory owes. First match wins, so the specific paths lead.
OWES = [
    ("stappler/tess", ["tesstest"]),
    ("stappler/vg", ["tesstest", "stapplertest"]),
    ("runtime/libc_impl", ["libctest", "runtimetest"]),
    ("runtime", ["runtimetest", "libctest"]),
    ("stappler", ["stapplertest"]),
    ("xenolith/font", ["localetest", "stapplertest"]),
    ("xenolith/renderer/ui/layout", ["uilayouttest"]),
    ("xenolith/renderer/basic2d/particle", ["particlestest"]),
    ("xenolith/renderer/basic2d/glsl", ["particlestest"]),
    ("xenolith/backend/vk", ["computetest"]),
    ("xenolith/core", ["computetest"]),
    ("xenolith", []),
]

# What a `xenolith/` change gets when it names no widget: five scripts that are cheap and between
# them touch the layout, the stylesheet, hit-testing, the canvas and the hotkey registry.
# 39 s of job time, ~250 assertions.
WINDOW_SMOKE = ["style-check.py", "geometry-check.py", "scale9-check.py", "canvas-check.py",
                "hotkey-check.py"]

# Measured at -j1 (2026-09-13). ORDER ONLY - longest first, so the tail of a parallel run is not one
# 66-second script everybody else waits for. A name missing from it is scheduled as an average one.
COST = {
    "window/slider-check.py": 66, "window/menu-check.py": 66, "window/inline-edit-check.py": 61,
    "window/chip-check.py": 58, "window/picker-check.py": 51, "window/scrollbar-check.py": 40,
    "window/context-menu-check.py": 36, "window/vector-check.py": 34, "window/color-check.py": 34,
    "window/table-reorder-check.py": 33, "window/select-check.py": 32,
    "window/selection-check.py": 32, "window/remote-check.py": 31, "window/text-input-check.py": 31,
    "window/number-check.py": 29, "window/tooltip-check.py": 27, "window/markdown-check.py": 25,
    "window/hit-test-check.py": 20, "window/accordion-check.py": 17, "window/form-check.py": 15,
    "window/drag-check.py": 12, "window/canvas-check.py": 11, "window/text-undo-check.py": 11,
    "window/hotkey-check.py": 9, "window/style-check.py": 8, "window/geometry-check.py": 8,
    "window/panel-check.py": 4, "window/clipboard-check.py": 4, "window/scale9-check.py": 3,
    "window/render-level-check.py": 4, "window/overflow-check.py": 3,
    "window/particles-check.py": 42, "window/remote-example-check.py": 24,
    "window/remote-window-check.py": 20, "window/virtual-window-check.py": 88,
    "window/damage-check.py": 21, "window/remote-render-check.py": 33,
    "gittest": 19, "computetest": 6, "runtimetest": 12, "stapplertest": 4, "libctest": 1, "localetest": 1,
    "uilayouttest": 1, "particlestest": 1,
}

# Window checks that start a binary other than tests/window's testapp: (project, binary). The runner
# passes the binary as the script's argument and skips the script when it is not built.
WINDOW_BINARIES = {
    "particles-check.py": ("examples/window/particles", "particles"),
}

# `examples/` is outside the plan, except the examples a window check drives
EXAMPLE_CHECKS = [
    ("examples/window/particles", ["particles-check.py"]),
    ("examples/window/dndtree", ["remote-example-check.py"]),
    ("examples/window/form", ["remote-example-check.py"]),
    ("examples/window/dock", ["remote-example-check.py"]),
]

# Scripts that are not checks, or cannot be part of an automated run - see the docstring.
NOT_CHECKS = {"markdown-perf-check.py"}
MANUAL = {"xcb-side-check.py"}


def window_scripts():
    d = os.path.join(ROOT, "tests/window")
    return sorted(f for f in os.listdir(d)
                  if f.endswith("-check.py") and f not in NOT_CHECKS and f not in MANUAL)


def binary(proj, name):
    return os.path.join(ROOT, proj, "stappler-build", TARGET, BUILD, "cc", name)


def changed_paths(rev=None):
    if rev:
        out = subprocess.run(["git", "diff", "--name-only", rev], cwd=ROOT,
                             capture_output=True, text=True).stdout.splitlines()
        return [l.strip() for l in out if l.strip()]
    out = subprocess.run(["git", "status", "--porcelain=1"], cwd=ROOT,
                         capture_output=True, text=True).stdout.splitlines()
    return [l[3:].strip().split(" -> ")[-1] for l in out if l.strip()]


def select_window(paths):
    """Window checks whose NAME matches a changed file's name - the declared heuristic.

    A file name is split on camel case rather than searched as a string: a substring search answers
    `text-input-check.py` for `XLContext.cc` ("context" contains "text"), and a plan with four wrong
    scripts in it is one nobody reads. Tokens match by PREFIX either way, so `inline-edit` still
    catches `XLUiInlineEditor`; a two-word name also matches the joined form, because the source is
    `XLUiContextMenu.cc` and not `XLUiContext_Menu.cc`."""
    tokens, joined = set(), []
    for p in paths:
        stem = os.path.splitext(os.path.basename(p))[0]
        tokens |= {t.lower() for t in re.findall(r"[A-Z]+(?![a-z])|[A-Z][a-z]+|[a-z]+|\d+", stem)}
        joined.append(stem.lower())
    out = []
    for s in window_scripts():
        subject = s[:-len("-check.py")]
        parts = [p for p in subject.split("-") if len(p) > 3]
        if subject.replace("-", "") in " ".join(joined):
            out.append(s)
        elif any(t.startswith(p) or p.startswith(t) for p in parts for t in tokens if len(t) > 3):
            out.append(s)
    return out


class Job:
    def __init__(self, name, argv, cwd=ROOT, timeout=1800, requires=None):
        self.name, self.argv, self.cwd, self.timeout = name, argv, cwd, timeout
        # the binary the job cannot run without: the harness itself, or a window check's own app
        self.requires = requires if requires else (None if argv[0] == sys.executable else argv[0])
        self.cost = COST.get(name, 10)
        self.secs, self.rc, self.checks, self.fails = 0.0, None, None, []

    def run(self):
        t0 = time.time()
        try:
            p = subprocess.run(self.argv, cwd=self.cwd, capture_output=True, text=True,
                               errors="replace", timeout=self.timeout)
            self.rc, text = p.returncode, p.stdout + p.stderr
        except subprocess.TimeoutExpired:
            self.rc, text = -9, f"TIMEOUT after {self.timeout}s"
        self.secs = time.time() - t0
        for m in re.finditer(r"(\d+) checks?, (\d+) failures?", text):
            self.checks = int(m.group(1))
        if self.checks is None:  # studiotest and the console harnesses count by `[ OK ]` line
            n = text.count("[ OK ]")
            self.checks = n or None
        self.fails = [l.rstrip() for l in text.splitlines()
                      if re.search(r"\bFAIL\b|\[ FAIL|Traceback|SystemExit|TIMEOUT", l)][:20]
        if self.rc != 0 and not self.fails:
            # A job can fail without a line that says FAIL - a harness that exits on a signal, a
            # script that raises where nothing prints. The tail is then the only evidence there is.
            self.fails = ["(no FAIL line - the last lines of the output:)"] + \
                         [l.rstrip() for l in text.splitlines()[-15:]]
        tally = "" if self.checks is None else f"{self.checks:>5} checks"
        print(f"  {'ok  ' if self.rc == 0 else 'RED '} {self.name:<40} {self.secs:6.1f}s {tally}",
              flush=True)
        return self


def cli_jobs(names=None, console_only=False):
    jobs = []
    for proj, name, args, in_console in CLI:
        if console_only and not in_console:
            continue
        if names is not None and name not in names:
            continue
        label = name + (" " + " ".join(args) if args else "")
        jobs.append(Job(label, [binary(proj, name)] + args, cwd=os.path.join(ROOT, proj)))
    return jobs


def window_jobs(scripts):
    jobs = []
    for s in scripts:
        argv = [sys.executable, os.path.join(ROOT, "tests/window", s)]
        requires = None
        if s in WINDOW_BINARIES:
            requires = binary(*WINDOW_BINARIES[s])
            argv.append(requires)
        jobs.append(Job("window/" + s, argv, requires=requires))
    return jobs


def dedup(jobs):
    seen, out = set(), []
    for j in jobs:
        if j.name not in seen:
            seen.add(j.name)
            out.append(j)
    return out


def plan(args):
    if args.tier == "console":
        return cli_jobs(console_only=True), "the console harnesses, no window"
    if args.tier == "full":
        return cli_jobs() + window_jobs(window_scripts()), "the gate"
    if args.tier == "suite":
        jobs = []
        for n in args.names:
            if n in ("window", "gui"):
                jobs += window_jobs(window_scripts())
            elif n in ("cli", "console"):
                jobs += cli_jobs()
            else:
                jobs += cli_jobs(names={n, n + "test"})
        if not jobs:
            sys.exit("suite: name `window`, `cli`, or a harness (runtime, libc, stappler, tess, ...)")
        return dedup(jobs), " ".join(args.names)

    all_paths = changed_paths(args.since)
    examples = sorted({s for prefix, names in EXAMPLE_CHECKS for s in names
                       if any(p.startswith(prefix + "/") for p in all_paths)})
    paths = [p for p in all_paths
             if not p.startswith(("docs/", "examples/")) and not p.endswith(".md")]
    if not paths:
        if examples:
            return (dedup(cli_jobs(console_only=True) + window_jobs(examples)),
                    f"only examples changed - the console harnesses and {', '.join(examples)}")
        return cli_jobs(console_only=True), "the working tree is clean - the console harnesses only"
    owed, wide = set(), None
    for p in paths:
        for prefix, names in OWES:
            if p.startswith(prefix + "/"):
                owed |= set(names)
                break
        else:
            if not p.startswith(("tests/", "make/", "toolchains/", "utils/")):
                wide = p
    if wide:
        return (cli_jobs() + window_jobs(window_scripts()),
                f"{wide} maps to nothing - running everything")
    scripts = select_window(paths)
    if not scripts and any(p.startswith("xenolith/") for p in paths):
        scripts = WINDOW_SMOKE
    scripts = scripts + [s for s in examples if s not in scripts]
    jobs = dedup(cli_jobs(console_only=True) + cli_jobs(names=owed) + window_jobs(scripts))
    return jobs, f"{len(paths)} changed paths -> {len(jobs)} jobs"


def stale(kill):
    out = subprocess.run(["ps", "-eo", "pid,args"], capture_output=True, text=True).stdout
    # This checkout's binaries only, by full path: another checkout on the same machine runs its own
    # checks, and a bare "cc/testapp" would kill them in the middle.
    apps = [binary("tests/window", "testapp")] + \
        [binary(proj, name) for proj, name in WINDOW_BINARIES.values()]
    found = [l for l in out.splitlines() if "--headless" in l and any(a in l for a in apps)]
    if kill:
        for l in found:
            try:
                os.kill(int(l.split()[0]), 9)
            except (ValueError, ProcessLookupError, PermissionError):
                pass
    return found


def main():
    # Line-buffered, so the runner's own lines and a subprocess's (`make`, under --build) interleave
    # in the order they happened rather than in the order the pipe flushed them.
    sys.stdout.reconfigure(line_buffering=True)
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("tier", nargs="?", default="fast", choices=["fast", "console", "suite", "full"])
    ap.add_argument("names", nargs="*", help="for `suite`: window, cli, or a harness name")
    ap.add_argument("-j", "--jobs", type=int, default=int(os.environ.get("XL_CHECK_JOBS", 4)))
    ap.add_argument("--since", metavar="REV", help="select from `git diff REV`, not the working tree")
    ap.add_argument("--only", action="append", default=[], metavar="TEXT",
                    help="keep only the jobs whose name contains TEXT")
    ap.add_argument("--list", action="store_true", help="print the plan and run nothing")
    ap.add_argument("--keep-stale", action="store_true", help="do not kill leftover testapps")
    args = ap.parse_args()

    jobs, note = plan(args)
    if args.only:
        jobs = [j for j in jobs if any(o in j.name for o in args.only)]
    print(f"* {note}")
    if args.list:
        for j in sorted(jobs, key=lambda j: -j.cost):
            print(f"  {j.name:<40} ~{j.cost}s")
        print(f"  ({len(jobs)} jobs, ~{sum(j.cost for j in jobs)}s of work)")
        return 0

    missing = [j for j in jobs if j.requires and not os.path.exists(j.requires)]
    for j in missing:
        print(f"! not built, skipped: {j.name} ({os.path.relpath(j.requires, ROOT)})")
    jobs = [j for j in jobs if j not in missing]
    if not jobs:
        print("nothing to run")
        return 0

    if not args.keep_stale:
        left = stale(kill=True)
        if left:
            print(f"* killed {len(left)} leftover testapp(s)")

    print(f"* {len(jobs)} jobs, -j{args.jobs}")
    t0 = time.time()
    with ThreadPoolExecutor(max_workers=max(1, args.jobs)) as ex:
        list(ex.map(lambda j: j.run(), sorted(jobs, key=lambda j: -j.cost)))
    wall = time.time() - t0

    red = [j for j in jobs if j.rc != 0]
    print(f"\n{len(jobs)} jobs, {sum(j.checks or 0 for j in jobs)} checks, {len(red)} red, "
          f"{wall:.1f}s wall, {sum(j.secs for j in jobs):.1f}s of job time")
    for j in red:
        print(f"\nRED {j.name} (exit {j.rc})")
        for l in j.fails:
            print("    " + l)
    left = stale(kill=False)
    if left:
        print(f"\n! {len(left)} testapp process(es) still alive - a script died before `quit`")
    if red:
        print("\nRe-run a red job alone before believing it:  tests/run-checks.py "
              f"{args.tier} --only {red[0].name.split('/')[-1]} -j1")
    return len(red)


if __name__ == "__main__":
    sys.exit(main())
