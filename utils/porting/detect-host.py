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

"""Asks every place that detects the host what it thinks this machine is (plan C, steps C1-C3).

The host is worked out independently by make/utils/init-sh.mk (GNU make), make/utils/init-xlmake.mk
(xlmake), runtime/toolchains/common/utils/detect-platform.mk (toolchain builds) and xenolith-cli.
When they disagree, one launch mode silently picks another toolchain. Run this on the machine
being made a host:

    utils/porting/detect-host.py
    utils/porting/detect-host.py --xlmake /path/to/xlmake --cli /path/to/xenolith-cli

It evaluates the detection makefiles in a private temporary directory and runs `xenolith-cli
detect`; it builds nothing.
"""

import argparse
import glob
import os
import re
import shutil
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import portlib as pl  # noqa: E402


def find_binary(eng, explicit, name, build_dirs):
    if explicit:
        return explicit if os.path.exists(explicit) else None
    triple = pl.host_triple()
    for kind_dir in pl.toolchain_dirs(eng, "hosts", triple):
        p = os.path.join(kind_dir, "bin", name)
        if os.path.exists(p):
            return p
    for d in build_dirs:
        hits = sorted(glob.glob(eng.path(os.path.join(d, "stappler-build", "*", "*", "cc", name))),
                      key=os.path.getmtime, reverse=True)
        if hits:
            return hits[0]
    return shutil.which(name)


def run(cmd, cwd=None, timeout=60):
    try:
        out = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout, cwd=cwd)
        return out.returncode, out.stdout, out.stderr
    except (OSError, subprocess.SubprocessError) as e:
        return -1, "", str(e)


def eval_xlmake(xlmake, eng, names):
    with tempfile.TemporaryDirectory(prefix="xlport-") as tmp:
        mk = os.path.join(tmp, "eval.mk")
        with open(mk, "w") as f:
            f.write("BUILD_ROOT := %s\nprint_verbose =\ninclude $(BUILD_ROOT)/utils/init-xlmake.mk\nall: ; @:\n"
                    % eng.path("make"))
        cmd = [xlmake, "-i", "-f", mk] + [x for n in names for x in ("-V", n)]
        code, out, err = run(cmd, cwd=tmp)
    if code != 0:
        return None, err.strip().splitlines()[-3:]
    values = {}
    for line in out.splitlines():
        m = re.match(r"^([A-Z_]+) = (.*)$", line)
        if m:
            values[m.group(1)] = "" if m.group(2) == "<undefined>" else m.group(2)
    return values, None


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--xlmake", help="xlmake binary (default: host toolchain, local build or PATH)")
    ap.add_argument("--cli", help="xenolith-cli binary (default: local build or PATH)")
    pl.add_common_args(ap)
    args = ap.parse_args()
    eng = pl.Engine(args.engine)
    rep = pl.make_report(args, "host detection on this machine")

    rep.section("C1", "what the machine says")
    u = os.uname()
    expected = pl.host_triple()
    rep.info("uname -s %s, uname -m %s; expected triple %s" % (u.sysname, u.machine, expected))
    if u.machine != pl.normalize_arch(u.machine):
        rep.info("`%s` must be translated to `%s` everywhere the host is detected" % (u.machine, pl.normalize_arch(u.machine)))

    rep.section("C2", "what the detectors say")
    results = {}
    make = pl.make_program()
    if make:
        out = subprocess.run([make, "--version"], capture_output=True, text=True).stdout.splitlines()[0]
        ver = re.search(r"(\d+)\.(\d+)", out)
        rep.check(ver and (int(ver.group(1)), int(ver.group(2))) >= (4, 1), "%s (%s)" % (out, make),
                  hint="the build needs GNU make 4.1 or newer; BSD make does not work")
        sh = pl.eval_make_vars([eng.path("make/utils/verbose.mk"), eng.path("make/utils/init-sh.mk")],
                               ["STAPPLER_HOST", "ANDROID_HOST", "UNAME"], extra=["BUILD_ROOT := " + eng.path("make")])
        if sh is None:
            rep.fail("make/utils/init-sh.mk does not evaluate here", hint="its `else` branch errors on an unknown OS")
        else:
            results["init-sh.mk (GNU make)"] = sh["STAPPLER_HOST"]
            rep.info("init-sh.mk: STAPPLER_HOST=%s ANDROID_HOST=%s" % (sh["STAPPLER_HOST"], sh["ANDROID_HOST"]))
        dp = pl.eval_make_vars([eng.path("runtime/toolchains/common/utils/detect-platform.mk")],
                               ["HOST_ID", "HOST_TOOLCHAIN", "HOST_ANDROID"])
        if dp is None:
            rep.fail("runtime/toolchains/common/utils/detect-platform.mk does not evaluate here",
                     hint="add a branch for this OS (plan C, C2)")
        else:
            results["detect-platform.mk (toolchain builds)"] = dp["HOST_ID"]
            tc = dp["HOST_TOOLCHAIN"]
            rep.info("detect-platform.mk: HOST_ID=%s HOST_TOOLCHAIN=%s" % (dp["HOST_ID"], tc or "<none>"))
            if tc:
                rep.check(eng.isdir("runtime/toolchains/" + tc), "native host build directory runtime/toolchains/%s exists" % tc)
            else:
                rep.info("no native host build here: the host toolchain is installed or cross-built on Linux")
    else:
        rep.fail("GNU make not found (gmake or make)", hint="xenolith-cli builds with its own xlmake; raw builds need GNU make 4.1+")

    xlmake = find_binary(eng, args.xlmake, "xlmake", ["utils/xlmake"])
    if xlmake:
        values, err = eval_xlmake(xlmake, eng, ["STAPPLER_HOST", "ANDROID_HOST", "XL_UNAME_MACHINE", "XL_GLIBC_VERSION"])
        if values is None:
            rep.fail("xlmake could not evaluate make/utils/init-xlmake.mk (%s)" % xlmake, details=err)
        else:
            results["init-xlmake.mk (xlmake)"] = values.get("STAPPLER_HOST", "")
            rep.info("init-xlmake.mk: STAPPLER_HOST=%s XL_UNAME_MACHINE=%s XL_GLIBC_VERSION=%s"
                     % (values.get("STAPPLER_HOST"), values.get("XL_UNAME_MACHINE"), values.get("XL_GLIBC_VERSION") or "-"))
    else:
        rep.skip("no xlmake found (--xlmake)")

    cli = find_binary(eng, args.cli, "xenolith-cli", ["utils/installer/cli"])
    if cli:
        env = dict(os.environ)
        env.pop("STAPPLER_HOST", None)
        try:
            out = subprocess.run([cli, "detect"], capture_output=True, text=True, timeout=60, env=env)
            lines = [l for l in out.stdout.splitlines() if l.strip() and not l.startswith("[")]
            if out.returncode == 0 and lines:
                triple = lines[-1].split()[0]
                results["xenolith-cli detect"] = triple
                rep.info("xenolith-cli detect: %s" % lines[-1])
            else:
                rep.fail("xenolith-cli detect failed", details=(out.stdout + out.stderr).splitlines()[-3:])
        except (OSError, subprocess.SubprocessError) as e:
            rep.fail("xenolith-cli could not run: %s" % e)
    else:
        rep.skip("no xenolith-cli found (--cli)")

    rep.section("C2", "agreement")
    values = set(results.values())
    rep.check(len(values) == 1, "all detectors agree on %s" % (" / ".join(sorted(values)) or "nothing"),
              details=["%-40s %s" % (k, v) for k, v in results.items()],
              hint="fix the detector that disagrees: plan C lists every place")
    if values:
        rep.check(expected in values, "the detected triple matches uname (%s)" % expected, missing=pl.WARN)
    triple = sorted(values)[0] if len(values) == 1 else expected

    rep.section("C6", "toolchains for %s" % triple)
    host_dir = pl.find_toolchain_dir(eng, "hosts", triple)
    rep.check(host_dir, "host toolchain hosts/%s%s" % (triple, " at " + os.path.relpath(host_dir, eng.root) if host_dir else ""),
              hint="install it (xenolith-cli install) or build it (make -C runtime/toolchains host-cross-%s)" % triple)
    tgt_dir = pl.find_toolchain_dir(eng, "targets", triple)
    rep.check(tgt_dir, "target sysroot targets/%s (building for this machine itself)" % triple, missing=pl.WARN)
    if host_dir:
        rep.info("validate it: utils/porting/check-toolchain.py %s --run" % triple)

    rep.section("C3", "tools the build expects on the host")
    for tool, why, need in (("sh", "recipes in sh mode", pl.FAIL), ("git", "build number from git describe", pl.WARN),
                            ("tar", "xenolith-cli unpacks toolchains with tar -xf", pl.FAIL),
                            ("xz", "toolchain archives are .tar.xz (bsdtar handles it without xz)", pl.INFO),
                            ("curl", "install.sh downloads with curl", pl.WARN),
                            ("cmake", "only for building toolchains from source", pl.INFO),
                            ("ninja", "only for building toolchains from source", pl.INFO)):
        p = shutil.which(tool)
        rep.check(p, "%s: %s (%s)" % (tool, p or "not found", why), missing=need)
    tar = shutil.which("tar")
    if tar:
        code, out, err = run([tar, "--version"])
        rep.info("tar: %s" % ((out or err).splitlines() or ["?"])[0])
    root = eng.root
    rep.check(" " not in root, "engine path has no spaces (%s)" % root, hint="make cannot quote paths with spaces")
    probe = os.path.join(root, "README.md")
    if os.path.exists(probe):
        rep.check(not os.path.exists(os.path.join(root, "readme.MD")), "the file system is case-sensitive",
                  missing=pl.INFO, hint="fine on Windows (patched make); elsewhere the build assumes case")
    return rep.finish()


if __name__ == "__main__":
    sys.exit(main())
