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

"""Checks a host toolchain against the hosts/<triple> contract (plan C, steps C4 and C6).

    utils/porting/check-toolchain.py x86_64-unknown-linux-gnu
    utils/porting/check-toolchain.py --all
    utils/porting/check-toolchain.py x86_64-unknown-linux-gnu --run --cross x86_64-pc-windows-msvc

Without --run it only reads the directory. --run executes the tools when they are native to this
machine: it compiles a small C++ program and a shader in a private temporary directory, the way
step C6 asks you to on the new host.
"""

import argparse
import os
import re
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import portlib as pl  # noqa: E402

REQUIRED_TOOLS = ["clang", "clang++", "lld", "ld.lld", "llvm-ar", "glslang", "spirv-link"]
EXPECTED_TOOLS = ["ld64.lld", "lld-link", "wasm-ld", "lldb", "lldb-server", "llvm-objcopy", "llvm-strip", "llvm-rc",
                  "llvm-nm", "spirv-opt", "spirv-val"]
C_PROBE = '#include <stdio.h>\nint main(void) { printf("%d\\n", 1 + 1); return 0; }\n'
SHADER_PROBE = "#version 450\nlayout(location = 0) out vec4 c;\nvoid main() { c = vec4(1.0); }\n"


def resolve(eng, spec):
    if os.path.isdir(spec):
        return os.path.abspath(spec), os.path.basename(os.path.normpath(spec))
    d = pl.find_toolchain_dir(eng, "hosts", spec)
    return d, spec


def tool(bindir, name, windows):
    """The tool's path when it exists; a dangling link counts as missing."""
    for n in ([name + ".exe", name] if windows else [name]):
        p = os.path.join(bindir, n)
        if os.path.exists(p):
            return p
    return None


def run(cmd, cwd, timeout=180):
    try:
        out = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True, timeout=timeout)
        return out.returncode, (out.stdout + out.stderr).strip()
    except (OSError, subprocess.SubprocessError) as e:
        return -1, str(e)


def check_one(eng, rep, spec, do_run, cross):
    root, triple = resolve(eng, spec)
    rep.section(triple, "host toolchain")
    if not root:
        rep.fail("no host toolchain for %s" % spec, hint="hosts/<triple> under toolchains/ or runtime/toolchains/")
        return
    rep.info("location: %s" % root)
    tri = pl.parse_triple(triple)
    windows = tri["os"] == "windows"
    mk = os.path.join(root, "host.mk")
    if rep.check(os.path.isfile(mk), "host.mk exists") != pl.OK:
        return
    values, how = pl.read_toolchain_mk(mk, pl.HOST_MK_VARS)
    if how != "make":
        rep.warn("GNU make not found: host.mk was read as text")
    for name in ("HOST_BINDIR", "HOST_CC", "HOST_CXX", "HOST_AR", "HOST_GLSLANG", "HOST_SPIRV_LINK"):
        v = values.get(name)
        exists = bool(v) and (os.path.isdir(v) if name == "HOST_BINDIR" else os.path.lexists(v))
        rep.check(exists, "%s = %s" % (name, os.path.relpath(v, root) if v else "<empty>"),
                  missing=pl.FAIL if name in ("HOST_BINDIR", "HOST_CC", "HOST_CXX", "HOST_AR") else pl.WARN,
                  hint="the variable must name an existing file inside the toolchain")
    if values.get("HOST_ROOT"):
        rep.check(os.path.realpath(values["HOST_ROOT"]) == os.path.realpath(root), "HOST_ROOT is the toolchain itself",
                  hint="use $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST)))) so the toolchain is relocatable")
    inc = re.findall(r"-idirafter\s+(\S+)", values.get("HOST_GENERAL_CFLAGS", ""))
    if inc:
        ok = os.path.isfile(os.path.join(inc[0], "stddef.h"))
        rep.check(ok, "HOST_GENERAL_CFLAGS adds the compiler headers (%s)" % os.path.relpath(inc[0], root),
                  hint="-idirafter must point at lib/clang/<N>/include of this clang")
    else:
        rep.warn("HOST_GENERAL_CFLAGS has no -idirafter <lib/clang/N/include>",
                 hint="every target.mk moves -resource-dir into the sysroot; the host adds the compiler headers back")

    bindir = values.get("HOST_BINDIR") or os.path.join(root, "bin")
    missing = [t for t in REQUIRED_TOOLS if not tool(bindir, t, windows)]
    rep.check(not missing, "required tools present", details=["missing: " + t for t in missing])
    lacking = [t for t in EXPECTED_TOOLS if not tool(bindir, t, windows)]
    rep.check(not lacking, "the rest of the usual tool set present", missing=pl.WARN, details=["missing: " + t for t in lacking])
    make = [t for t in ("xlmake", "make") if tool(bindir, t, windows)]
    rep.check(make, "build driver: %s" % (", ".join(make) or "none"), hint="ship xlmake (and GNU make where it builds)")
    rep.check("xlmake" in make, "xlmake is in the toolchain", missing=pl.WARN)

    dangling = []
    for dirpath, dirnames, filenames in os.walk(root):
        for n in filenames + dirnames:
            p = os.path.join(dirpath, n)
            if os.path.islink(p) and not os.path.exists(p):
                dangling.append("%s -> %s" % (os.path.relpath(p, root), os.readlink(p)))
            elif os.path.islink(p) and os.path.isabs(os.readlink(p)):
                dangling.append("%s -> %s (absolute)" % (os.path.relpath(p, root), os.readlink(p)))
    rep.check(not dangling, "links are relative and resolve", details=dangling[:30],
              hint="a relocated toolchain must not point outside itself")

    clang = tool(bindir, "clang", windows)
    if clang:
        fmt, found = pl.binary_info(os.path.realpath(clang))
        match = pl.arch_matches(tri["arch"], found)
        expect_fmt = {"windows": "pe", "macosx": "mach-o"}.get(tri["os"], "elf")
        rep.check(match is not False and fmt.startswith(expect_fmt) or fmt == "mach-o-fat",
                  "bin/clang is a %s %s executable" % (fmt, found), hint="the toolchain was built for another host")
    clang_dirs = pl.clang_resource_include(clang) if clang else None
    rep.check(clang_dirs, "lib/clang/<N>/include has the compiler headers")
    release = os.path.join(root, "release")
    if os.path.isfile(release):
        rep.info("release stamp: %s (engine: %s)" % (open(release).read().strip(), eng.git_tag() or "no git tag"))
    else:
        rep.warn("no release stamp")
    if not os.path.isdir(os.path.join(root, "share", "licenses")):
        rep.info("no share/licenses (Windows hosts ship without it)")

    if not do_run:
        return
    native = pl.host_triple()
    if pl.parse_triple(native)["base"] != tri["base"]:
        rep.skip("--run: %s does not run on this %s machine" % (triple, native))
        return
    with tempfile.TemporaryDirectory(prefix="xlport-") as tmp:
        code, text = run([clang, "--version"], tmp)
        rep.check(code == 0, "clang --version: %s" % (text.splitlines()[0] if text else code))
        # The engine brings its own C++ (sprt); a C program is what a bare sysroot can link.
        src = os.path.join(tmp, "t.c")
        open(src, "w").write(C_PROBE)
        sysroot = pl.find_toolchain_dir(eng, "targets", triple)
        args = [clang, src, "-o", os.path.join(tmp, "t")]
        if sysroot:
            tv, _ = pl.read_toolchain_mk(os.path.join(sysroot, "target.mk"), pl.TARGET_MK_VARS)
            incs = [x for k in ("TARGET_INCLUDE_DIR_LIBC", "TARGET_INCLUDE_DIR") if tv.get(k) for x in ("-isystem", tv[k])]
            libs = [x for k in ("TARGET_LIB_DIR_LIBC", "TARGET_LIB_DIR") if tv.get(k) for x in ("-L", tv[k])]
            args = [clang, "--target=" + (tv.get("TARGET_NAME") or triple), "--sysroot=" + sysroot] + \
                tv.get("TARGET_GENERAL_CFLAGS", "").split() + incs + (["-idirafter", clang_dirs] if clang_dirs else []) + \
                [src, "-o", os.path.join(tmp, "t")] + libs
        code, text = run(args, tmp)
        if code == 0:
            code, text = run([os.path.join(tmp, "t")], tmp)
            rep.check(code == 0 and text.strip() == "2", "a C program compiles, links and runs against %s"
                      % ("the target sysroot" if sysroot else "the system"), details=[text][:1])
        else:
            rep.warn("a C program does not build for %s%s" % (triple, " (no sysroot)" if not sysroot else ""),
                     details=text.splitlines()[:6])
        for t in cross:
            code, text = run([clang, "--target=" + t, "-fsyntax-only", "-x", "c", os.devnull], tmp)
            rep.check(code == 0, "clang accepts --target=%s" % t, details=text.splitlines()[:3])
        frag = os.path.join(tmp, "x.frag")
        open(frag, "w").write(SHADER_PROBE)
        gl, link, val = tool(bindir, "glslang", windows), tool(bindir, "spirv-link", windows), tool(bindir, "spirv-val", windows)
        if gl and link:
            code, text = run([gl, "-V", frag, "-o", os.path.join(tmp, "a.spv")], tmp)
            if code == 0:
                code, text = run([link, os.path.join(tmp, "a.spv"), "-o", os.path.join(tmp, "b.spv")], tmp)
            if code == 0 and val:
                code, text = run([val, os.path.join(tmp, "b.spv")], tmp)
            rep.check(code == 0, "glslang -> spirv-link -> spirv-val", details=text.splitlines()[:4])
        xl = tool(bindir, "xlmake", windows)
        if xl:
            code, text = run([xl, "--help"], tmp)
            rep.check(code == 0, "xlmake --help")
    check_deps(rep, root, bindir)


# What an ELF tool may take from the system: the C library and the loader. Everything else must
# come from the toolchain's own lib/, or the toolchain only works where the system happens to match.
SYSTEM_LIBS = re.compile(r"^(linux-vdso|ld-linux|ld-musl|libc\.so|libm\.so|libdl\.so|libpthread\.so|librt\.so|"
                         r"libutil\.so|libresolv\.so|libanl\.so)")


def check_deps(rep, root, bindir):
    import shutil
    ldd = shutil.which("ldd")
    if not ldd:
        rep.skip("ldd not found: dependency check skipped")
        return
    outside = []
    for name in sorted(os.listdir(bindir)):
        p = os.path.join(bindir, name)
        if os.path.islink(p) or not os.path.isfile(p) or not os.access(p, os.X_OK):
            continue
        if pl.binary_info(p)[0] != "elf":
            continue
        code, text = run([ldd, p], root, timeout=30)
        for line in text.splitlines():
            m = re.match(r"\s*(\S+)\s+=>\s+(\S+)", line)
            if not m or SYSTEM_LIBS.match(os.path.basename(m.group(1))):
                continue
            target = m.group(2)
            if target == "not" or not os.path.realpath(target).startswith(os.path.realpath(root)):
                outside.append("%s: %s => %s" % (name, m.group(1), "NOT FOUND" if target == "not" else target))
    rep.check(not outside, "tools load only system libc and the toolchain's own libraries", details=outside,
              hint="ship the library in lib/ or link it statically")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("toolchain", nargs="*", help="a host triple or a toolchain directory")
    ap.add_argument("--all", action="store_true", help="every toolchain under toolchains/hosts and runtime/toolchains/hosts")
    ap.add_argument("--run", action="store_true", help="also run the tools when they are native to this machine")
    ap.add_argument("--cross", action="append", default=[], help="with --run: a target triple clang must accept")
    pl.add_common_args(ap)
    args = ap.parse_args()
    eng = pl.Engine(args.engine)
    specs = list(args.toolchain)
    if args.all:
        seen = set()
        for base in ("toolchains/hosts", "runtime/toolchains/hosts"):
            for n in eng.listdir(base):
                if n not in seen and eng.isdir(os.path.join(base, n)):
                    seen.add(n)
                    specs.append(n)
    if not specs:
        specs = [pl.host_triple()]
    rep = pl.make_report(args, "host toolchains")
    for s in specs:
        check_one(eng, rep, s, args.run, args.cross)
    return rep.finish()


if __name__ == "__main__":
    sys.exit(main())
