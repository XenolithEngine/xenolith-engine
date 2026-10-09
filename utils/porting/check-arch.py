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

"""Where a CPU architecture stands against plan A (docs/articles/*/porting/new-arch.adoc).

Checks every place the plan names, step by step, and reports what is there and what is missing.
Run it for an architecture the engine already has to see a complete port:

    utils/porting/check-arch.py riscv64
    utils/porting/check-arch.py loongarch64 --like riscv64
    utils/porting/check-arch.py newarch64 --llvm NewArch --kernel newarch --android

It reads the tree and the built sysroots; it never edits or builds anything.
"""

import argparse
import os
import re
import shutil
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import portlib as pl  # noqa: E402

ABI_FILES = ["config.h", "fcntl.h", "fenv.h", "jmp_buf.h", "signal.h", "syscall.h"]


def defined_names(text):
    return set(re.findall(r"^\s*#\s*define\s+([A-Za-z_][A-Za-z0-9_]*)", text or "", re.M))


def check_sprt_def(eng, rep, arch, upper):
    rep.section("A2", "sprt recognizes the architecture")
    rel = "runtime/include/sprt/c/bits/__sprt_def.h"
    text = eng.read(rel) or ""
    rep.check(re.search(r"#define\s+__SPRT_ARCH_NAME_%s\s+%s_sprt\b" % (upper, re.escape(arch)), text),
              "%s: __SPRT_ARCH_NAME_%s is %s_sprt" % (rel, upper, arch),
              hint="add `#define __SPRT_ARCH_NAME_%s %s_sprt` next to the other architectures" % (upper, arch))
    ids = dict(re.findall(r"#define\s+__SPRT_ARCH_ID_([A-Z0-9_]+)\s+(\d+)", text))
    rep.check(upper in ids, "%s: __SPRT_ARCH_ID_%s has a number" % (rel, upper),
              hint="the next free number is %d" % (max([int(v) for v in ids.values()] or [0]) + 1))
    dup = {}
    for name, value in ids.items():
        dup.setdefault(value, []).append(name)
    clashes = ["%s = %s" % (v, ", ".join(n)) for v, n in dup.items() if len(n) > 1]
    rep.check(not clashes, "architecture IDs are unique", details=clashes)

    # Every branch of the detection chain must name and number the same architecture: a copied branch
    # that keeps its neighbour's name silently picks up the neighbour's ABI headers.
    branch_re = re.compile(r"#\s*(?:el)?if\s+([^\n]*)\n((?:(?!#\s*(?:elif|else|endif)).*\n)*?)"
                           r"\s*#define\s+__SPRT_ARCH_NAME\s+__SPRT_ARCH_NAME_([A-Z0-9_]+)\s*\n"
                           r"\s*#define\s+__SPRT_ARCH_ID\s+__SPRT_ARCH_ID_([A-Z0-9_]+)")
    branches = branch_re.findall(text)
    mismatched = ["#if %s -> NAME_%s, ID_%s" % (c.strip(), n, i) for c, _, n, i in branches if n != i]
    rep.check(not mismatched, "every detection branch uses one architecture for NAME and ID",
              hint="a branch maps its macros to another architecture's headers", details=mismatched)
    targets = {}
    for c, _, n, i in branches:
        targets.setdefault(n, []).append(c.strip())
    twice = ["%s <- %s" % (n, " | ".join(cs)) for n, cs in targets.items() if len(cs) > 1]
    rep.check(not twice, "no two detection branches select the same architecture",
              hint="a branch copied from its neighbour kept the neighbour's name", details=twice)
    stem = re.sub(r"\d+$", "", arch).lower()
    foreign = [(c.strip(), n) for c, _, n, i in branches if stem in c.lower() and n != upper
               and not n.startswith(stem.upper())]
    if foreign:
        rep.fail("a branch that tests for %s selects another architecture" % stem,
                 details=["#if %s -> %s" % f for f in foreign])
    mine = [(c.strip(), n) for c, _, n, i in branches if n == upper or i == upper]
    if mine:
        rep.ok("detection branch for %s: #if %s" % (upper, mine[0][0]))
        lower_cond = mine[0][0].lower()
        if arch.startswith("riscv") or arch.startswith("loongarch") or arch.endswith("64"):
            if not re.search(r"64|xlen|grlen|lp64", lower_cond) and arch not in ("x86_64", "aarch64"):
                rep.warn("the branch does not test the word size", hint="architectures with 32- and 64-bit "
                         "variants need a size test, as `__riscv_xlen == 64`")
    else:
        rep.fail("no `#elif defined(...)` branch defines __SPRT_ARCH_NAME_%s" % upper,
                 hint="add a branch to the chain that ends in #error \"Unknown arch\"")

    rel = "xenolith/remote/XLRemotePeerInfo.h"
    h = eng.read(rel) or ""
    enum = re.search(r"enum class OsArch[^{]*\{([^}]*)\}", h)
    names = {n.lower(): v for n, v in re.findall(r"(\w+)\s*=\s*(\d+)", enum.group(1))} if enum else {}
    key = arch.replace("_", "").lower()
    found = [n for n in names if n.replace("_", "") == key]
    rep.check(found, "%s: OsArch has an entry for %s" % (rel, arch),
              hint="OsArch mirrors __SPRT_ARCH_ID_* and is part of the remote protocol")
    if found and upper in ids:
        rep.check(names[found[0]] == ids[upper], "OsArch value equals __SPRT_ARCH_ID_%s (%s)" % (upper, ids[upper]))
    rep.check(eng.contains("xenolith/remote/XLRemotePeerInfo.cc", r"__SPRT_ARCH_ID_%s\b" % upper),
              "XLRemotePeerInfo.cc static_assert covers __SPRT_ARCH_ID_%s" % upper, missing=pl.WARN)


def check_abi_headers(eng, rep, arch, like, oses):
    rep.section("A3", "ABI headers <os>_sprt/%s_sprt" % arch)
    for os_dir in oses:
        rel = "runtime/include/sprt/c/cross/%s/%s_sprt" % (os_dir, arch)
        if not eng.isdir(rel):
            rep.fail("%s/ does not exist" % rel, hint="copy %s/%s_sprt and correct the numbers" % (os_dir, like))
            continue
        missing = [f for f in ABI_FILES if not eng.exists(os.path.join(rel, f))]
        rep.check(not missing, "%s/ has the six ABI headers" % rel, details=["missing: " + f for f in missing])
        ref = "runtime/include/sprt/c/cross/%s/%s_sprt" % (os_dir, like)
        if like == arch or not eng.isdir(ref):
            continue
        for f in ABI_FILES[1:]:
            mine, theirs = defined_names(eng.read(os.path.join(rel, f))), defined_names(eng.read(os.path.join(ref, f)))
            if not mine and not theirs:
                continue
            gone, extra = sorted(theirs - mine), sorted(mine - theirs)
            if gone or extra:
                details = ["only in %s: %s" % (like, ", ".join(gone))] if gone else []
                details += ["only in %s: %s" % (arch, ", ".join(extra))] if extra else []
                rep.info("%s/%s differs from %s by %d names" % (rel, f, like, len(gone) + len(extra)),
                         details=details)
    linux = eng.read("runtime/include/sprt/c/cross/linux_sprt/sockdef.h") or ""
    if re.search(r"__SPRT_PF_MAX", linux):
        mentions = bool(re.search(r"__SPRT_ARCH_ID_%s\b" % arch.upper(), linux))
        rep.info("linux_sprt/sockdef.h: __SPRT_PF_MAX %s" % ("names this architecture" if mentions else
                 "uses the default for this architecture"),
                 hint=None)
    rep.info("the numbers are proved by building tests/runtime (A6) and by check-abi.py --target <triple>")


def check_toolchain_lists(eng, rep, arch, llvm):
    rep.section("A4", "shared toolchain lists")
    names = eng.read("runtime/toolchains/common/utils/names.mk") or ""
    rep.check(re.search(r"^CONFIGURE_PROC_%s\s*:=" % re.escape(arch), names, re.M),
              "names.mk: CONFIGURE_PROC_%s" % arch, hint="CONFIGURE_PROC_%s := %s" % (arch, arch))
    rep.check(re.search(r"^CONFIGURE_HOST_Linux_%s\s*:=" % re.escape(arch), names, re.M),
              "names.mk: CONFIGURE_HOST_Linux_%s" % arch, hint="CONFIGURE_HOST_Linux_%s := %s-linux-gnu" % (arch, arch))
    if not llvm:
        rep.skip("LLVM backend name unknown: pass --llvm <Name> to check LLVM_TARGETS_TO_BUILD")
        return
    lists = []
    for rel in pl.iter_source_files(eng, dirs=["runtime/toolchains"]):
        text = eng.read(rel) or ""
        for m in re.finditer(r"LLVM_TARGETS_TO_BUILD=\"?([A-Za-z0-9;]+)\"?", text):
            if ";" in m.group(1):
                line = text.count("\n", 0, m.start()) + 1
                lists.append((rel, line, m.group(1).split(";")))
    lacking = ["%s:%d  %s" % (r, l, ";".join(v)) for r, l, v in lists if llvm not in v]
    rep.check(lists and not lacking, "%d multi-target LLVM_TARGETS_TO_BUILD lists include %s"
              % (len(lists) - len(lacking), llvm), hint="every released clang must be able to target the "
              "architecture", details=lacking)
    for rel, why in (("runtime/toolchains/common/libjpeg-turbo.mk", "SIMD assembly (WITH_SIMD)"),
                     ("runtime/toolchains/common/wamr.mk", "WAMR_BUILD_TARGET (stappler_wasm)")):
        has = eng.contains(rel, pl.word(arch))
        rep.info("%s: %s %s" % (rel, why, "names this architecture" if has else
                 "does not name this architecture - check that it builds"))


def check_linux_sysroots(eng, rep, arch, kernel):
    rep.section("A5", "Linux target sysroots")
    tl = eng.read("runtime/toolchains/target-linux/Makefile")
    for var in ("GNU_ARCHS", "MUSL_ARCHS"):
        values = pl.make_list(tl, var) or []
        rep.check(arch in values, "target-linux/Makefile: %s contains %s" % (var, arch),
                  hint="%s := %s %s" % (var, " ".join(values), arch))
    glibc = eng.read("runtime/toolchains/target-linux/glibc/Makefile")
    m = re.search(r"ifeq \(\$\(SP_ARCH_TARGET_LINUX\),%s\)\n((?:(?!\s*else|\s*endif).*\n)*)" % re.escape(arch), glibc or "")
    if rep.check(m, "target-linux/glibc/Makefile: architecture block", hint="set the kernel ARCH name, "
                 "SP_LDSO_NAME and the glibc version that first has the port") == pl.OK:
        block = m.group(1)
        rep.check("SP_LDSO_NAME" in block, "glibc block sets SP_LDSO_NAME", missing=pl.WARN)
        if kernel and kernel != arch:
            rep.check(re.search(r"SP_ARCH_TARGET_LINUX\s*:=\s*%s\b" % re.escape(kernel), block),
                      "glibc block maps %s to kernel ARCH=%s" % (arch, kernel), missing=pl.WARN)
    musl = eng.read("runtime/toolchains/target-linux/musl/Makefile")
    rep.check(re.search(r"ifeq \(\$\(SP_ARCH_TARGET\),%s\)" % re.escape(arch), musl or ""),
              "target-linux/musl/Makefile: architecture block", hint="SP_ARCH_KERNEL and SP_ARCH_LLVM")
    rep.check(eng.contains("runtime/toolchains/target-linux/openssl.mk", r"ifeq \(\$\(SP_ARCH\),%s\)" % re.escape(arch)),
              "target-linux/openssl.mk: OPENSSL_TARGET for %s" % arch,
              hint="the configuration name from OpenSSL Configurations/10-main.conf")
    top = eng.read("runtime/toolchains/Makefile")
    for libc in ("gnu", "musl"):
        t = "%s-unknown-linux-%s" % (arch, libc)
        rep.check(pl.make_rule_exists(top, "target-" + t), "runtime/toolchains/Makefile: rule target-" + t)
        d = pl.find_toolchain_dir(eng, "targets", t)
        if d and os.path.exists(os.path.join(d, "target.mk")):
            rep.ok("sysroot built: " + os.path.relpath(d, eng.root),
                   details=["check it with: utils/porting/check-sysroot.py " + t])
        else:
            rep.info("sysroot %s is not built yet (make -C runtime/toolchains target-%s)" % (t, t))


def check_engine_build(eng, rep, arch, bits):
    rep.section("A6", "first engine build")
    if bits == 32:
        ilp32 = eng.read("make/os/linux.mk") or ""
        rep.check(re.search(pl.word(arch), ilp32), "make/os/linux.mk: the ILP32 list names %s" % arch,
                  hint="32-bit architectures change the mangled size_t of the exported operator new/delete")
    comp = eng.read("make/c/compiler.mk") or ""
    if "gsplit-dwarf" in comp:
        rep.info("make/c/compiler.mk uses -gsplit-dwarf in debug builds%s" % (
            " (excluded for this architecture)" if re.search(pl.word(arch), comp) else
            "; exclude the architecture if its linker relaxes instructions and a debug link fails"))
    for libc in ("gnu", "musl"):
        t = "%s-unknown-linux-%s" % (arch, libc)
        for proj, binary in (("tests/runtime", "runtimetest"), ("tests/libc", "libctest"),
                             ("tests/stappler", "stapplertest")):
            p = eng.path(os.path.join(proj, "stappler-build", t, "debug", "cc", binary))
            if os.path.exists(p):
                fmt, found = pl.binary_info(p)
                rep.check(pl.arch_matches(arch, found), "%s built for %s (%s, %s)" % (binary, t, fmt, found))
            else:
                rep.info("%s is not built for %s (make -C %s STAPPLER_TARGET=%s)" % (binary, t, proj, t))


def check_run(eng, rep, arch):
    rep.section("A7", "running under qemu-user")
    q = shutil.which("qemu-" + arch)
    rep.check(q, "qemu-%s is on PATH" % arch, missing=pl.WARN, hint="install qemu-user to run the tests")
    cmp = eng.read("tests/libc/compare.sh") or ""
    rep.check("--target" in cmp, "tests/libc/compare.sh can run a foreign Linux target (--target)",
              missing=pl.WARN, hint="the qemu mode came with commit 5be3d759 (loongarch64)")
    rep.check("HOST_TARGET:-" in cmp, "tests/libc/compare.sh takes HOST_TARGET from the environment",
              missing=pl.INFO)


def check_simd(eng, rep, arch, upper):
    rep.section("A8", "SIMD and CPU-aware code (optional)")
    gost = eng.read("stappler/core/string/SPGost3411-2012.cc") or ""
    if "__BYTE_ORDER__" in gost:
        rep.ok("GOST 34.11 decides byte order by __BYTE_ORDER__")
    else:
        rep.warn("GOST 34.11 guesses byte order from a list of architectures",
                 hint="an architecture missing from the list is treated as big-endian; fixed in 5be3d759")
    simd = eng.read("runtime/include/sprt/runtime/geom/simd_attr.h") or ""
    rep.info("geometry SIMD: %s" % ("simd_attr.h selects a backend for this architecture"
             if re.search(r"__SPRT_ARCH_ID_%s\b|%s" % (upper, pl.word(arch)), simd)
             else "no dedicated backend; SSE through SIMDe or the scalar backend"))
    attr = eng.read("stappler/raster/SPRasterAttr.h") or ""
    rep.info("raster kernels: %s" % ("SPRasterAttr.h has a flag for this architecture"
             if re.search(r"__SPRT_ARCH_ID_%s\b" % upper, attr) else "portable SWAR and scalar kernels"))
    plat = eng.read("runtime/src/platform/SPRuntimePlatform-posix.cc") or ""
    rep.info("cycle counter rdtsc(): %s" % ("implemented" if re.search(r"__SPRT_ARCH_ID_%s\b|%s"
             % (upper, pl.word(arch)), plat) else "SP_HAS_RDTSC 0 for this architecture"))


def check_host(eng, rep, arch, release_hosts):
    rep.section("A9", "a toolchain that runs on %s (optional)" % arch)
    for d in ("host-linux-glibc/cross", "host-linux-musl/cross"):
        rel = "runtime/toolchains/%s/Makefile" % d
        rep.check(eng.contains(rel, r"ifeq \(\$\(SP_ARCH_HOST\),%s\)" % re.escape(arch)),
                  "%s: SP_ARCH_HOST block" % rel, missing=pl.WARN)
    rep.check(eng.contains("runtime/toolchains/host-linux-musl/musl.mk", r"ifeq \(\$\(SP_ARCH\),%s\)" % re.escape(arch)),
              "host-linux-musl/musl.mk: architecture block", missing=pl.WARN)
    top = eng.read("runtime/toolchains/Makefile")
    all_cross = pl.make_list(top, "host-cross-all") or []
    for libc in ("gnu", "musl"):
        t = "%s-unknown-linux-%s" % (arch, libc)
        rep.check(pl.make_rule_exists(top, "host-cross-" + t), "rule host-cross-" + t, missing=pl.WARN)
        listed = re.search(r"host-cross-all:[^\n]*(?:\\\n[^\n]*)*" + re.escape("host-cross-" + t), top or "")
        rep.check(listed, "host-cross-all lists host-cross-" + t, missing=pl.WARN)
        d = pl.find_toolchain_dir(eng, "hosts", t)
        if d:
            rep.info("host toolchain present: %s (check-toolchain.py %s)" % (os.path.relpath(d, eng.root), t))
    del all_cross, release_hosts


def check_other_os(eng, rep, arch, android, windows):
    if android:
        rep.section("A10", "Android on %s" % arch)
        rep.check(eng.isdir("runtime/include/sprt/c/cross/android_sprt/%s_sprt" % arch),
                  "android_sprt/%s_sprt exists" % arch)
        for rel in ("runtime/toolchains/target-android/Makefile", "runtime/toolchains/target-android/install-target-ndk.mk",
                    "runtime/toolchains/target-android/openssl.mk", "make/os/android-ndk.mk"):
            rep.check(eng.contains(rel, pl.word(arch)), "%s names %s" % (rel, arch))
        rep.check(eng.contains("runtime/toolchains/common/utils/names.mk", r"CONFIGURE_HOST_Android_%s\b" % re.escape(arch)),
                  "names.mk: CONFIGURE_HOST_Android_%s" % arch)
    if windows:
        rep.section("A11", "Windows on %s" % arch)
        rep.check(eng.isdir("runtime/include/sprt/c/cross/windows_sprt/%s_sprt" % arch),
                  "windows_sprt/%s_sprt exists" % arch)
        rep.check(eng.isdir("runtime/libc_impl/asm/Windows/%s" % arch), "libc_impl/asm/Windows/%s exists" % arch,
                  hint="chkstk.s (with _alloca_probe) and fenv.s")
        rep.check(eng.contains("runtime/toolchains/target-windows/Makefile", pl.word(arch)),
                  "target-windows/Makefile names %s" % arch)
        rep.check(eng.contains("runtime/sprt-shared-windows.mk", r"SPRT_LIB_MACHINE"), "sprt-shared-windows.mk "
                  "sets SPRT_LIB_MACHINE (check the value for %s)" % arch, missing=pl.WARN)


def check_release(eng, rep, arch, upper, host_built):
    rep.section("A12", "release, installer and documentation")
    top = eng.read("runtime/toolchains/Makefile")
    targets = pl.make_list(top, "RELEASE_TARGETS") or []
    hosts = pl.make_list(top, "RELEASE_HOSTS") or []
    for libc in ("gnu", "musl"):
        t = "%s-unknown-linux-%s" % (arch, libc)
        rep.check(t in targets, "RELEASE_TARGETS lists " + t)
        rep.check(t in hosts, "RELEASE_HOSTS lists " + t, missing=pl.FAIL if host_built else pl.INFO)
    if eng.exists("runtime/toolchains/check-release.py"):
        rep.check(eng.contains("runtime/toolchains/check-release.py", pl.word(arch)),
                  "check-release.py knows %s (triple_arch, ELF_MACHINES)" % arch)
    tri = eng.read("utils/installer/core/src/SPITriple.cc") or ""
    server = re.search(r"getServerArch\([^)]*\)\s*\{(.*?)\n\}", tri, re.S)
    native = re.search(r"getNativeArch\(\)\s*\{(.*?)\n\}", tri, re.S)
    rep.check(server and re.search(r'"%s"' % re.escape(arch), server.group(1)),
              "SPITriple.cc getServerArch() accepts %s" % arch, missing=pl.WARN)
    rep.check(native and "__SPRT_ARCH_ID_%s" % upper in native.group(1),
              "SPITriple.cc getNativeArch() returns %s" % arch, missing=pl.WARN)
    rep.check(eng.contains("install.sh", r":%s\)" % re.escape(arch)), "install.sh maps Linux:%s" % arch,
              missing=pl.WARN, hint="only when a CLI asset is published for the architecture")
    rep.check(eng.contains(".github/workflows/cli-release.yml", pl.word(arch)),
              ".github/workflows/cli-release.yml builds a CLI for %s" % arch, missing=pl.WARN)
    for rel in ("docs/articles/ru/general/support.adoc", "runtime/toolchains/README.adoc",
                "runtime/toolchains/RELEASE_NOTES.md", "docs/platforms/linux.adoc", "README.md"):
        rep.check(eng.contains(rel, pl.word(arch)), "%s mentions %s" % (rel, arch), missing=pl.WARN)


def check_like(eng, rep, arch, like, window):
    rep.section("refs", "places that name %s with no %s nearby" % (like, arch))
    like_tokens = sorted({like, like.upper(), "__SPRT_ARCH_ID_" + like.upper()})
    new_tokens = sorted({arch, arch.upper()})
    missing, _ = pl.scan_refs(eng, like_tokens, new_tokens, window=window)
    files = pl.group_by_file(missing)
    if not files:
        rep.ok("every mention of %s has %s within %d lines" % (like, arch, window))
        return
    for path, lines in sorted(files.items()):
        rep.info("%s: %d line(s)" % (path, len(lines)), details=["%5d  %s" % (n, t) for n, t in lines[:8]])
    rep.info("these are candidates, not errors: review each list and decide whether %s belongs there" % arch)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("arch", help="architecture name as it appears in triples (loongarch64)")
    ap.add_argument("--like", default=None, help="a supported architecture to compare with (default: riscv64, "
                    "or aarch64 when checking riscv64)")
    ap.add_argument("--llvm", default=None, help="LLVM backend name for LLVM_TARGETS_TO_BUILD (LoongArch)")
    ap.add_argument("--kernel", default=None, help="Linux kernel ARCH= name (loongarch)")
    ap.add_argument("--bits", type=int, choices=(32, 64), default=None, help="word size (default: from the name)")
    ap.add_argument("--android", action="store_true", help="also check plan step A10 (Android)")
    ap.add_argument("--windows", action="store_true", help="also check plan step A11 (Windows)")
    ap.add_argument("--host", action="store_true", help="treat step A9 (host toolchain) as required")
    ap.add_argument("--no-refs", action="store_true", help="skip the scan for lists that name --like only")
    ap.add_argument("--window", type=int, default=12, help="lines around a reference that count as `nearby`")
    pl.add_common_args(ap)
    args = ap.parse_args()

    eng = pl.Engine(args.engine)
    arch = pl.normalize_arch(args.arch)
    upper = arch.upper()
    known = pl.KNOWN_ARCHES.get(arch, (None, None, None))
    llvm = args.llvm or known[0]
    kernel = args.kernel or known[1]
    bits = args.bits or known[2] or (64 if "64" in arch else 32)
    like = args.like or ("aarch64" if arch == "riscv64" else "riscv64")
    oses = ["linux_sprt"] + (["android_sprt"] if args.android else []) + (["windows_sprt"] if args.windows else [])

    rep = pl.make_report(args, "plan A: architecture %s (compared with %s)" % (arch, like))
    rep.section("A1", "prerequisites")
    rep.info("LLVM backend: %s; kernel ARCH: %s; %d-bit" % (llvm or "unknown (--llvm)", kernel or "unknown (--kernel)", bits))
    clang = pl.find_host_clang(eng)
    if clang:
        macros, err = pl.clang_macros(clang, ["--target=%s-unknown-linux-gnu" % arch], "")
        if err or not macros:
            rep.fail("the host clang does not know %s-unknown-linux-gnu" % arch,
                     hint="add the LLVM backend (A4) and rebuild the host toolchain", details=err.splitlines()[:3])
        else:
            order = macros.get("__BYTE_ORDER__")
            rep.ok("host clang targets %s-unknown-linux-gnu (%s)" % (arch, os.path.relpath(clang, eng.root)
                   if clang.startswith(eng.root) else clang))
            rep.check(order == "__ORDER_LITTLE_ENDIAN__", "the architecture is little-endian", missing=pl.WARN,
                      hint="big-endian targets are untested; target.ini writes endian = 'little'")
            ptr = macros.get("__SIZEOF_POINTER__")
            rep.check(ptr == str(bits // 8), "pointer size is %s bytes" % ptr, missing=pl.WARN)
    else:
        rep.skip("no host clang found to probe the triple")

    check_sprt_def(eng, rep, arch, upper)
    check_abi_headers(eng, rep, arch, like, oses)
    check_toolchain_lists(eng, rep, arch, llvm)
    check_linux_sysroots(eng, rep, arch, kernel)
    check_engine_build(eng, rep, arch, bits)
    check_run(eng, rep, arch)
    check_simd(eng, rep, arch, upper)
    check_host(eng, rep, arch, None)
    if not args.host:
        for item in rep.sections[-1]["items"]:
            if item["status"] == pl.FAIL:
                item["status"] = pl.WARN
    check_other_os(eng, rep, arch, args.android, args.windows)
    check_release(eng, rep, arch, upper, args.host)
    if not args.no_refs and like != arch:
        check_like(eng, rep, arch, like, args.window)
    return rep.finish()


if __name__ == "__main__":
    sys.exit(main())
