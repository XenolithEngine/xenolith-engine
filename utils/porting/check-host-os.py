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

"""Where a host OS stands against plan C (docs/articles/*/porting/new-host-os.adoc).

Run it on any machine; it reads the tree. The host is named three ways: what `uname -s` prints,
its triple, and the name of its directories:

    utils/porting/check-host-os.py --uname Darwin --triple aarch64-apple-macosx --os macos
    utils/porting/check-host-os.py --uname FreeBSD --triple x86_64-unknown-freebsd --os freebsd \\
        --uname-arch amd64 --uname-arch arm64

On the new host itself, detect-host.py shows what each detector actually concludes.
"""

import argparse
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import portlib as pl  # noqa: E402


def uname_branch(text, uname):
    """The body of the `ifeq ($(UNAME),<uname>)` branch, or None."""
    m = re.search(r"ifeq \(\$\(UNAME\),%s\)\n(.*?)(?=^else|^endif)" % re.escape(uname), text or "", re.S | re.M)
    return m.group(1) if m else None


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--uname", required=True, help="what `uname -s` prints on the host (FreeBSD)")
    ap.add_argument("--triple", required=True, help="the host triple (x86_64-unknown-freebsd)")
    ap.add_argument("--os", required=True, help="the directory name of the OS (freebsd): host-<os>, <os>_sprt")
    ap.add_argument("--uname-arch", action="append", default=[], help="a `uname -m` spelling that must be "
                    "translated (amd64, arm64); repeatable")
    ap.add_argument("--macro", help="the part after SPRT_ (default: --os in upper case)")
    pl.add_common_args(ap)
    args = ap.parse_args()
    eng = pl.Engine(args.engine)
    tri = pl.parse_triple(args.triple)
    ident = args.os.replace("-", "_").lower()
    sprt = "SPRT_" + (args.macro or ident).upper()
    aliases = pl.platform_aliases(eng, sprt)
    alias_re = "|".join(pl.word(a) for a in aliases)
    suffix = args.triple[len(args.triple.split("-")[0]):]  # -unknown-freebsd
    rep = pl.make_report(args, "plan C: host OS %s (%s)" % (args.uname, args.triple))

    rep.section("C1", "the OS is a target first (plan B)")
    rep.check(eng.contains("runtime/include/sprt/c/bits/__sprt_def.h", r"#define %s\b" % sprt),
              "__sprt_def.h defines %s" % sprt, hint="plan B, step B2")
    rep.check(eng.isdir("runtime/include/sprt/c/cross/%s_sprt" % ident), "cross/%s_sprt exists" % ident, hint="plan B, step B3")
    d = pl.find_toolchain_dir(eng, "targets", args.triple)
    rep.check(d, "target sysroot for %s%s" % (args.triple, " (%s)" % os.path.relpath(d, eng.root) if d else ""),
              hint="the host toolchain is built against it (plan B, step B4)")
    built = os.path.exists(eng.path("tests/runtime/stappler-build/%s/debug/cc/runtimetest" % args.triple))
    rep.check(built, "tests/runtime is built for %s" % args.triple, missing=pl.WARN,
              hint="runtimetest must pass on the OS, runtime_process and runtime_file included (C1)")
    fs = [n for n in (ident, ident.replace("_", "-")) if eng.exists("runtime/src/filesystem/SPRuntimeFilesystem-%s.cc" % n)]
    rep.check(fs or eng.contains("runtime/src/filesystem/SPRuntimeFilesystem.cpp", alias_re)
              or eng.contains("runtime/src/filesystem/SPRuntimeFilesystemPosix.cpp", alias_re),
              "application directories (AppData/AppConfig/AppCache) have a platform layer", missing=pl.WARN,
              hint="xenolith-cli keeps its store there")

    rep.section("C2", "the build system recognizes the host")
    for rel, branch_var in (("make/utils/init-sh.mk", "STAPPLER_HOST"), ("make/utils/init-xlmake.mk", "STAPPLER_HOST"),
                            ("runtime/toolchains/common/utils/detect-platform.mk", "HOST_ID")):
        text = eng.read(rel)
        body = uname_branch(text, args.uname)
        if rep.check(body is not None, "%s: ifeq ($(UNAME),%s)" % (rel, args.uname)) == pl.OK:
            rep.check(suffix in body, "%s: the branch sets %s to <arch>%s" % (rel, branch_var, suffix),
                      missing=pl.WARN, details=[l.strip() for l in body.splitlines() if branch_var in l][:4])
    for a in args.uname_arch:
        target = pl.normalize_arch(a)
        for rel, pattern in (("make/utils/init-sh.mk", r"ifeq \(\$\(STAPPLER_HOST_ARCH\),%s\)" % re.escape(a)),
                             ("runtime/toolchains/common/utils/detect-platform.mk", r"ifeq \(\$\(HOST_ARCH\),%s\)" % re.escape(a)),
                             ("stappler/makefile/SPMakefileBuiltins.cc", r'"%s"' % re.escape(a)),
                             ("utils/installer/core/src/SPITriple.cc", r'"%s"' % re.escape(a)),
                             ("install.sh", r"%s:%s\)" % (re.escape(args.uname), re.escape(a)))):
            rep.check(eng.contains(rel, pattern), "%s translates uname -m `%s` to `%s`" % (rel, a, target),
                      missing=pl.FAIL if rel != "install.sh" else pl.WARN)

    rep.section("C3", "other UNAME branches in the build")
    for rel in pl.iter_source_files(eng, dirs=["make"]):
        text = eng.read(rel) or ""
        names = sorted(set(re.findall(r"ifeq \(\$\(UNAME\),(\w+)\)", text)))
        if names and rel not in ("make/utils/init-sh.mk", "make/utils/init-xlmake.mk"):
            rep.info("%s branches on UNAME for %s%s" % (rel, ", ".join(names),
                     "" if args.uname in names else ": decide whether %s belongs" % args.uname))

    rep.section("C4", "host toolchain build runtime/toolchains/host-%s" % ident.replace("_", "-"))
    base = None
    for n in (ident, ident.replace("_", "-")):
        for sub in ("cross", ""):
            p = os.path.join("runtime/toolchains/host-" + n, sub, "Makefile")
            if eng.exists(p):
                base = p
                break
        if base:
            break
    if rep.check(base, "a host build makefile (host-%s/cross/Makefile)" % ident) == pl.OK:
        text = eng.read(base) or ""
        for what, pattern, need in (("an `out:` goal", r"^out\s*:", pl.FAIL),
                                    ("host.mk generation", r"host\.mk", pl.FAIL),
                                    ("the release stamp", r"/release\b", pl.FAIL),
                                    ("xlmake built against the target sysroot", r"xlmake", pl.WARN),
                                    ("a multi-target LLVM_TARGETS_TO_BUILD", r"LLVM_TARGETS_TO_BUILD=\"[^\"]*;", pl.WARN),
                                    ("DEFAULT_SYSROOT=.. (relocatable clang)", r"DEFAULT_SYSROOT=\.\.", pl.WARN)):
            rep.check(re.search(pattern, text, re.M), "%s: %s" % (base, what), missing=need)
        lic = os.path.join(os.path.dirname(os.path.dirname(base)) if base.endswith("cross/Makefile") else os.path.dirname(base), "licenses")
        rep.check(eng.isdir(lic), "%s/ for share/licenses" % lic, missing=pl.WARN)

    rep.section("C5", "the toolchains Makefile")
    top = eng.read("runtime/toolchains/Makefile") or ""
    rule = re.search(r"^host-cross-%s\s*:([^\n]*)" % re.escape(args.triple), top, re.M)
    if rep.check(rule, "rule host-cross-%s" % args.triple, missing=pl.FAIL) == pl.OK:
        rep.check("target-" + tri["triple"].split("+")[0] in rule.group(1), "it depends on the target-%s sysroot"
                  % tri["base"], missing=pl.WARN, details=[rule.group(0).strip()])
    rep.check(re.search(r"host-cross-all:[^\n]*(?:\\\n[^\n]*)*" + re.escape("host-cross-" + args.triple), top),
              "host-cross-all lists it", missing=pl.WARN)
    rep.check(args.triple in (pl.make_list(top, "RELEASE_HOSTS") or []), "RELEASE_HOSTS lists %s" % args.triple)
    built_host = pl.find_toolchain_dir(eng, "hosts", args.triple)
    rep.section("C6", "the built toolchain")
    if built_host:
        rep.ok("hosts/%s exists: validate with check-toolchain.py %s (and --run on the host)" % (args.triple, args.triple))
    else:
        rep.info("hosts/%s is not built here" % args.triple)

    rep.section("C8", "test scripts")
    rep.check(eng.contains("tests/run-checks.py", r"def host_triple"), "tests/run-checks.py detects the host triple",
              missing=pl.WARN, hint="otherwise export STAPPLER_TARGET=<triple> before running it")
    rep.check(eng.contains("tests/libc/compare.sh", r"HOST_TARGET:-"), "tests/libc/compare.sh takes HOST_TARGET from "
              "the environment", missing=pl.WARN)

    rep.section("C10", "xenolith-cli and the install scripts")
    tri_src = eng.read("utils/installer/core/src/SPITriple.cc") or ""
    native = re.search(r"getNativeOs\(\)\s*\{(.*?)\n\}", tri_src, re.S)
    rep.check(native and re.search(alias_re, native.group(1)), "SPITriple.cc getNativeOs() has a branch for %s"
              % " / ".join(aliases))
    server = re.search(r"getServerOs\([^)]*\)\s*\{(.*?)\n\}", tri_src, re.S)
    rep.check(server and suffix.lstrip("-").split("-")[-1].rstrip("0123456789.") in server.group(1),
              "SPITriple.cc getServerOs() produces %s" % suffix.lstrip("-"), missing=pl.WARN)
    known = re.search(r"kKnownHosts\[\]\s*=\s*\{(.*?)\};", tri_src, re.S)
    rep.check(known and '"%s"' % args.triple in known.group(1), "kKnownHosts lists %s" % args.triple, missing=pl.WARN,
              hint="keep it in sync with RELEASE_HOSTS")
    upd = eng.read("utils/installer/core/src/SPISelfUpdate.cc") or ""
    rep.check(re.search(alias_re, upd) or re.search(pl.word(ident), upd),
              "SPISelfUpdate.cc getCliAssetTriple() knows the OS", missing=pl.WARN)
    rep.check(eng.contains("install.sh", r"%s:" % re.escape(args.uname)), "install.sh maps `%s:<arch>`" % args.uname,
              missing=pl.WARN)
    rep.check(eng.contains(".github/workflows/cli-release.yml", re.escape(args.triple)),
              ".github/workflows/cli-release.yml builds the CLI for %s" % args.triple, missing=pl.WARN)

    rep.section("C11", "release notes and documentation")
    for rel in ("runtime/toolchains/RELEASE_NOTES.md", "runtime/toolchains/README.adoc", "docs/agents/toolchains.md",
                "docs/usage/cli-releases.adoc"):
        rep.check(eng.contains(rel, re.escape(args.triple) + "|" + pl.word(args.uname)), "%s mentions the host" % rel,
                  missing=pl.WARN)
    rep.check(eng.exists("docs/platforms/%s.adoc" % ident.replace("_", "-")), "docs/platforms/%s.adoc" % ident,
              missing=pl.WARN)
    return rep.finish()


if __name__ == "__main__":
    sys.exit(main())
