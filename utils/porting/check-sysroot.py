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

"""Checks a target sysroot against the target.mk contract (plan B, step B4).

    utils/porting/check-sysroot.py x86_64-unknown-linux-gnu
    utils/porting/check-sysroot.py runtime/toolchains/target-nuttx/targets/aarch64-nuttx-none-elf
    utils/porting/check-sysroot.py --all

A triple is looked up where the build system looks: toolchains/targets/ first, then
runtime/toolchains/targets/. The sysroot is only read.
"""

import argparse
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import portlib as pl  # noqa: E402

LIB_EXT = (".a", ".so", ".lib", ".dylib", ".o", ".obj", ".tbd")
RUNTIME_LIBS = [("compiler-rt builtins", r"(lib)?clang_rt\.builtins.*\.(a|lib)$|^libclang_rt\.(osx|ios|iossim)\.a$"),
                ("libc++abi", r"lib(c\+\+abi)\.(a|so|dylib|tbd)$|^c\+\+abi\.lib$"),
                ("libunwind", r"lib(unwind)\.(a|so|dylib|tbd)$|^unwind\.lib$"),
                ("runtime libsprt", r"^libsprt\.a$|^sprt\.(lib|dll)$")]


def resolve(eng, spec):
    if os.path.isdir(spec):
        return os.path.abspath(spec), os.path.basename(os.path.normpath(spec))
    for d in pl.toolchain_dirs(eng, "targets", spec):
        if os.path.isdir(d):
            return d, spec
    return None, spec


def check_one(eng, rep, spec, sample):
    root, triple = resolve(eng, spec)
    rep.section(triple, "sysroot")
    if root is None:
        dangling = [p for p in pl.toolchain_dirs(eng, "targets", spec) if os.path.islink(p)]
        rep.fail("no sysroot for %s" % spec, details=["dangling link %s -> %s" % (p, os.readlink(p))
                                                      for p in dangling],
                 hint="build it with runtime/toolchains, or install it with xenolith-cli")
        return
    rep.info("location: %s%s" % (root, " (link to %s)" % os.path.realpath(root) if os.path.islink(root) else ""))
    mk = os.path.join(root, "target.mk")
    if not rep.check(os.path.isfile(mk), "target.mk exists") == pl.OK:
        return
    values, how = pl.read_toolchain_mk(mk, pl.TARGET_MK_VARS)
    if how != "make":
        rep.warn("GNU make not found: target.mk was read as text, conditionals were ignored")
    multi_abi = values.get("TARGET_SYSTEM") == "Android-NDK"  # one sysroot for every NDK ABI
    for name in ("TARGET_SYSROOT", "TARGET_SYSTEM", "TARGET_NAME"):
        rep.check(values.get(name) or (multi_abi and name == "TARGET_NAME"),
                  "%s = %s" % (name, values.get(name) or "<empty>"), hint="required by make/utils/defaults.mk")
    sysroot = values.get("TARGET_SYSROOT", "")
    if sysroot:
        rep.check(os.path.realpath(sysroot) == os.path.realpath(root), "TARGET_SYSROOT points at the sysroot itself",
                  hint="use $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST)))) so the sysroot is relocatable")
    system = values.get("TARGET_SYSTEM")
    apply = eng.read("make/utils/apply-toolchain.mk") or ""
    if system:
        branch = re.search(r"ifeq \(\$\(TARGET_SYSTEM\),%s\)\s*\n\s*include \$\(BUILD_ROOT\)/os/([\w-]+)\.mk"
                           % re.escape(system), apply)
        rep.check(branch, "apply-toolchain.mk knows TARGET_SYSTEM %s%s" % (system, " -> make/os/%s.mk"
                  % branch.group(1) if branch else ""), hint="plan B, step B5")
    tri = pl.parse_triple(triple)
    arch = values.get("TARGET_ARCH") or tri["arch"]
    if values.get("TARGET_ARCH"):
        rep.check(pl.normalize_arch(values["TARGET_ARCH"]) == tri["arch"] or tri["arch"] == "unknown",
                  "TARGET_ARCH %s matches the triple" % values["TARGET_ARCH"], missing=pl.WARN)
    flags = " ".join(values.get(k, "") for k in ("TARGET_GENERAL_CFLAGS", "TARGET_GENERAL_CXXFLAGS"))
    rd = re.search(r"-resource-dir\s+(\S+)", flags)
    if rd:
        rep.check(os.path.isdir(rd.group(1)), "-resource-dir %s exists" % os.path.relpath(rd.group(1), root))
    for name in ("TARGET_INCLUDE_DIR", "TARGET_INCLUDE_DIR_LIBC", "TARGET_LIB_DIR", "TARGET_LIB_DIR_LIBC"):
        v = values.get(name)
        if v:
            rep.check(os.path.isdir(v), "%s exists (%s)" % (name, os.path.relpath(v, root)), missing=pl.WARN)
    defines = re.findall(r"-D(__[A-Za-z0-9_]+__)\b", flags)
    if defines:
        sflags = values.get("TARGET_GENERAL_SFLAGS", "")
        lacking = [d for d in defines if d not in sflags]
        rep.check(not lacking or not sflags and system in ("Linux", "Android", "Windows", "Darwin", "MacOS", "iOS", "WASM"),
                  "platform defines %s reach the assembler (TARGET_GENERAL_SFLAGS)" % ", ".join(defines),
                  missing=pl.WARN, details=["missing in SFLAGS: " + d for d in lacking])

    release = os.path.join(root, "release")
    if os.path.isfile(release):
        stamp = open(release).read().strip()
        tag = eng.git_tag()
        rep.info("release stamp: %s (engine: %s)" % (stamp, tag or "no git tag"))
    else:
        rep.warn("no release stamp", hint="install-target.mk writes $(GIT_TAG) to <sysroot>/release")

    dangling, libs = [], []
    for dirpath, dirnames, filenames in os.walk(root, followlinks=False):
        for n in filenames + dirnames:
            p = os.path.join(dirpath, n)
            if os.path.islink(p) and not os.path.exists(p):
                dangling.append("%s -> %s" % (os.path.relpath(p, root), os.readlink(p)))
        for n in filenames:
            if n.endswith(LIB_EXT) or re.search(r"\.so(\.\d+)*$", n):
                libs.append(os.path.join(dirpath, n))
    rep.check(not dangling, "no dangling links inside the sysroot", missing=pl.WARN, details=dangling[:30])
    rep.info("%d libraries and objects" % len(libs))
    names = {os.path.basename(p): p for p in libs}
    # Windows has the MS C++ ABI in sprt; Apple links the system libc++abi and unwinder.
    system_abi = system in ("Windows", "Darwin", "MacOS", "iOS")
    for what, pattern in RUNTIME_LIBS:
        hits = sorted(n for n in names if re.search(pattern, n))
        optional = what == "runtime libsprt" or (system_abi and what in ("libc++abi", "libunwind"))
        status = pl.OK if hits else (pl.INFO if optional else pl.WARN)
        rep.add(status, "%s: %s" % (what, ", ".join(os.path.relpath(names[h], root) for h in hits[:3]) if hits
                                     else "not found"))

    if multi_abi or tri["arch"] == "unknown":
        rep.info("several architectures in one sysroot: machine check skipped")
        return
    wrong, unknown, checked = [], 0, 0
    for p in sorted(libs)[:sample] if sample else sorted(libs):
        if not os.path.isfile(p) or p.endswith(".tbd"):
            continue
        fmt, found = pl.binary_info(p)
        checked += 1
        match = pl.arch_matches(arch, found)
        if match is False:
            wrong.append("%s: %s %s" % (os.path.relpath(p, root), fmt, found))
        elif match is None:
            unknown += 1
    rep.check(not wrong, "%d binaries are for %s%s" % (checked - len(wrong) - unknown, arch,
              " (%d without a readable machine, e.g. LLVM bitcode or linker scripts)" % unknown if unknown else ""),
              details=wrong[:30], hint="a library built for another architecture links into nothing")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("sysroot", nargs="*", help="a triple or a sysroot directory")
    ap.add_argument("--all", action="store_true", help="every sysroot under toolchains/targets and runtime/toolchains/targets")
    ap.add_argument("--sample", type=int, default=0, help="check the machine of at most N libraries (default: all)")
    pl.add_common_args(ap)
    args = ap.parse_args()
    eng = pl.Engine(args.engine)
    specs = list(args.sysroot)
    if args.all:
        seen = set()
        for base in ("toolchains/targets", "runtime/toolchains/targets"):
            for n in eng.listdir(base):
                if n not in seen and (eng.isdir(os.path.join(base, n)) or os.path.islink(eng.path(os.path.join(base, n)))) \
                        and "stale" not in n:
                    seen.add(n)
                    specs.append(n)
    if not specs:
        ap.error("name a triple, a directory, or pass --all")
    rep = pl.make_report(args, "target sysroots")
    for s in specs:
        check_one(eng, rep, s, args.sample)
    return rep.finish()


if __name__ == "__main__":
    sys.exit(main())
