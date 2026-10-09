#!/usr/bin/env python3
# Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

"""Compares the numbers in sprt's cross headers with the target's real libc (plans A3 and B3).

    utils/porting/check-abi.py --target x86_64-unknown-linux-gnu
    utils/porting/check-abi.py --target riscv64-unknown-linux-musl --show syscall,socket

The build proves most of these numbers one at a time: runtime_libc_wrapper has a static_assert per
name, and the first wrong one stops the compile. This script preprocesses both sides with the host
clang and lists every difference at once. It also covers what no static_assert does: the system
call numbers that futex, epoll_pwait2, io_uring, inotify and pidfd_open are called with.

Only the preprocessor runs; nothing is compiled or linked. A freestanding target (sprt is the libc)
has nothing to compare against and is skipped.
"""

import argparse
import glob
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import portlib as pl  # noqa: E402

NATIVE_HEADERS = ["errno.h", "fcntl.h", "signal.h", "unistd.h", "sys/types.h", "sys/stat.h", "sys/mman.h",
                  "sys/socket.h", "sys/un.h", "netinet/in.h", "netinet/tcp.h", "netdb.h", "poll.h", "time.h",
                  "fenv.h", "langinfo.h", "locale.h", "stdio.h", "stdlib.h", "dirent.h", "setjmp.h", "math.h",
                  "sys/syscall.h", "sys/epoll.h", "sys/eventfd.h", "sys/timerfd.h", "sys/inotify.h",
                  "sys/signalfd.h", "sys/wait.h", "sys/resource.h", "sys/ioctl.h", "termios.h", "syslog.h",
                  "fnmatch.h", "glob.h", "regex.h", "sys/statvfs.h", "sys/file.h", "sched.h", "pthread.h"]

FREESTANDING = {"Windows", "WASM", "EmboxUser", "TauUser"}

GROUPS = [("syscall", r"^SYSCALL_"), ("errno", r"^E[A-Z0-9]+$"), ("fcntl", r"^(O_|F_|AT_|FD_|LOCK_|SEEK_|POSIX_FADV)"),
          ("signal", r"^(SIG|_NSIG|SA_|SI_|SS_|MINSIGSTKSZ|NSIG)"),
          ("socket", r"^(SO_|SOL_|AF_|PF_|SOCK_|MSG_|SHUT_|IP|TCP_|SCM_|INADDR|AI_|NI_|EAI_)"),
          ("mman", r"^(MAP_|PROT_|MS_|MADV_|MCL_|MFD_|MREMAP_|POSIX_MADV)"), ("fenv", r"^FE_"),
          ("sysconf", r"^(_SC_|_PC_|_CS_)"), ("poll", r"^(POLL|EPOLL)"), ("clock", r"^(CLOCK_|TIMER_)"),
          ("stat", r"^(S_I|UTIME_|DT_)"), ("other", r".")]

# Meta macros of the sprt headers themselves, not numbers of the platform ABI.
META = re.compile(r"^(CONFIG_|ID\b|ID_|PLATFORM_|ARCH_|CROSS_|API|GLOBAL|LOCAL|NOEXCEPT|HAVE_|C_FUNC|"
                  r"INLINE|NOTICE|UNAVAILABLE|DEPRECATED|RESTRICT|.*_H_?$)")


def group_of(name):
    for g, pattern in GROUPS:
        if re.search(pattern, name):
            return g
    return "other"


def native_candidates(short):
    if short.startswith("SYSCALL_"):
        n = short[len("SYSCALL_"):]
        return ["__NR_" + n, "SYS_" + n]
    if short == "_NSIG":
        return ["_NSIG", "NSIG"]
    return [short]


def asserted_names(eng):
    """Where each __SPRT_<name> is compared with the native name in a static_assert: {name: "file:line"}.

    The asserts sit under #if guards that record known exceptions, which this script cannot evaluate,
    so a difference is reported with the place to read rather than as a certain build failure.
    """
    names = {}
    for path in sorted(glob.glob(eng.path("runtime/**/*.c*"), recursive=True)):
        if "/toolchains/" in path or "/musl-libc/" in path:
            continue
        try:
            text = open(path, encoding="utf-8", errors="replace").read()
        except OSError:
            continue
        for m in re.finditer(r"static_assert\((.*?)(?:,\s*\"[^\"]*\")?\);", text, re.S):
            line = text.count("\n", 0, m.start()) + 1
            where = "%s:%d" % (os.path.relpath(path, eng.root), line)
            for n in re.findall(r"__SPRT_([A-Za-z0-9_]+)", m.group(1)):
                names.setdefault(n, where)
    return names


def used_syscalls(eng):
    used = set()
    for path in glob.glob(eng.path("runtime/**/*.c*"), recursive=True):
        if "/toolchains/" in path or "/musl-libc/" in path:
            continue
        try:
            used.update(re.findall(r"__SPRT_SYSCALL_([a-z0-9_]+)", open(path, errors="replace").read()))
        except OSError:
            pass
    return used


def preprocess_union(clang, args, headers):
    """Macros of all headers that preprocess; falls back to one header at a time on an error."""
    src = "".join("#include <%s>\n" % h for h in headers)
    macros, err = pl.clang_macros(clang, args, src)
    if not err:
        return macros, headers, []
    merged, ok, bad = {}, [], []
    for h in headers:
        m, e = pl.clang_macros(clang, args, "#include <%s>\n" % h)
        if e:
            bad.append(h)
        else:
            ok.append(h)
            merged.update(m)
    return merged, ok, bad


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--target", required=True, help="target triple with a built sysroot")
    ap.add_argument("--sysroot", help="sysroot directory (default: looked up like the build does)")
    ap.add_argument("--show", default="", help="comma-separated groups to list in full (syscall,errno,fcntl,signal,"
                    "socket,mman,fenv,sysconf,poll,clock,stat,other); differences are always listed")
    pl.add_common_args(ap)
    args = ap.parse_args()
    eng = pl.Engine(args.engine)
    rep = pl.make_report(args, "sprt cross headers vs the libc of %s" % args.target)

    rep.section("setup", "toolchain and sysroot")
    sysroot = os.path.abspath(args.sysroot) if args.sysroot else pl.find_toolchain_dir(eng, "targets", args.target)
    if not sysroot or not os.path.isfile(os.path.join(sysroot, "target.mk")):
        rep.fail("no sysroot with a target.mk for %s" % args.target, hint="build it, or pass --sysroot")
        return rep.finish()
    clang = pl.find_host_clang(eng)
    if not clang:
        rep.fail("no host clang", hint="install a host toolchain (xenolith-cli install)")
        return rep.finish()
    res = pl.clang_resource_include(clang)
    values, _ = pl.read_toolchain_mk(os.path.join(sysroot, "target.mk"), pl.TARGET_MK_VARS)
    system = values.get("TARGET_SYSTEM", "")
    name = values.get("TARGET_NAME") or args.target
    rep.info("sysroot %s; TARGET_SYSTEM %s; clang --target=%s" % (sysroot, system, name))
    if system in FREESTANDING:
        rep.skip("%s is freestanding: sprt is the libc there and has no native numbers to match" % system)
        return rep.finish()

    cflags = values.get("TARGET_GENERAL_CFLAGS", "").split()
    defines = [f for f in cflags if f.startswith("-D")]
    if pl.parse_triple(args.target)["env"].startswith("musl") and \
            eng.contains("make/os/linux.mk", r"__SPRT_LINUX_MUSL"):
        defines.append("-D__SPRT_LINUX_MUSL=1")
    os_dirs = [x for k in ("TARGET_INCLUDE_DIR_LIBC", "TARGET_INCLUDE_DIR") if values.get(k) for x in ("-idirafter", values[k])]
    sprt_args = ["--target=" + name, "-nostdinc", "-I", eng.path("runtime/include")] + \
        (["-isystem", res] if res else []) + defines + os_dirs
    cross = sorted(os.path.basename(p) for p in glob.glob(eng.path("runtime/include/sprt/c/cross/__sprt_*.h")))
    sprt, ok_h, bad_h = preprocess_union(clang, sprt_args, ["sprt/c/cross/" + h for h in cross])
    rep.check(not bad_h, "sprt cross headers preprocess for this target (%d of %d)" % (len(ok_h), len(cross)),
              details=["failed: " + h for h in bad_h])

    native_args = ["--target=" + name, "--sysroot=" + sysroot] + [f for f in cflags if f != "-nostdinc"] + \
        [x for k in ("TARGET_INCLUDE_DIR_LIBC", "TARGET_INCLUDE_DIR") if values.get(k) for x in ("-isystem", values[k])] + \
        (["-idirafter", res] if res else [])
    native, ok_n, bad_n = preprocess_union(clang, native_args, NATIVE_HEADERS)
    if not ok_n:
        rep.fail("none of the libc headers preprocess against the sysroot", details=bad_n[:5])
        return rep.finish()
    rep.info("native headers: %d preprocess, %d absent on this OS" % (len(ok_n), len(bad_n)),
             details=["absent: " + ", ".join(bad_n)] if bad_n else None)

    asserted = asserted_names(eng)
    used = used_syscalls(eng)
    show = {g.strip() for g in args.show.split(",") if g.strip()}
    stats = {}
    diffs = {}
    listing = {}
    for full in sorted(sprt):
        if not full.startswith("__SPRT_"):
            continue
        short = full[len("__SPRT_"):]
        if META.match(short):
            continue
        sv = pl.eval_macro(full, sprt)
        if sv is None:
            continue
        g = group_of(short)
        st = stats.setdefault(g, {"same": 0, "diff": 0, "sprt-only": 0, "opaque": 0})
        cand = [c for c in native_candidates(short) if c in native]
        if not cand:
            st["sprt-only"] += 1
            if g in show:
                listing.setdefault(g, []).append("%-32s sprt %-10s native -" % (short, sv))
            continue
        nv = pl.eval_macro(cand[0], native)
        if nv is None:
            st["opaque"] += 1
            continue
        if nv == sv:
            st["same"] += 1
            if g in show:
                listing.setdefault(g, []).append("%-32s %s" % (short, sv))
            continue
        st["diff"] += 1
        if g == "syscall":
            severity = pl.FAIL if short[len("SYSCALL_"):] in used else pl.WARN
            why = "called by the runtime" if severity == pl.FAIL else "not called by the runtime today"
        else:
            severity = pl.WARN
            why = ("asserted at %s: read its #if guard" % asserted[short]) if short in asserted else \
                "no static_assert checks it"
        diffs.setdefault(g, []).append((severity, "%-28s sprt %-12s native %-12s (%s)" % (short, sv, nv, why)))

    rep.section("compare", "numbers by group")
    for g, _ in GROUPS:
        if g not in stats:
            continue
        st = stats[g]
        entries = diffs.get(g, [])
        severity = pl.FAIL if any(s == pl.FAIL for s, _ in entries) else (pl.WARN if entries else pl.OK)
        rep.add(severity, "%-8s %d equal, %d differ, %d only in sprt, %d not evaluable"
                % (g, st["same"], st["diff"], st["sprt-only"], st["opaque"]),
                details=[t for _, t in sorted(entries, key=lambda e: e[0] != pl.FAIL)] + listing.get(g, []),
                hint="a guarded static_assert records a deliberate exception; anything else is fixed in the "
                     "cross header, keyed on the libc (as __SPRT_LINUX_MUSL) when glibc and musl disagree"
                if entries else None)
    if "syscall" not in stats:
        rep.info("no __SPRT_SYSCALL_* numbers on this platform (or no native __NR_* to compare with)")
    rep.info("`only in sprt` is expected for names the OS lacks (the plan puts them at 0x1000 and up)")
    return rep.finish()


if __name__ == "__main__":
    sys.exit(main())
