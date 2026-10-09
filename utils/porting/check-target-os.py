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

"""Where a target OS stands against plan B (docs/articles/*/porting/new-target-os.adoc).

The OS is named the way its directories are named (nuttx, embox, embox_user, wasm, myos):

    utils/porting/check-target-os.py nuttx --triple aarch64-nuttx-none-elf
    utils/porting/check-target-os.py wasm --triple wasm32-unknown-unknown
    utils/porting/check-target-os.py myos --system MyOS --define __MYOS__ --form hosted --like nuttx

Every selection point of the runtime platform layer is checked for the platform macro or for a group
macro that includes it (SPRT_HOSTED_RTOS, SPRT_EMBOX_ANY, ...). It reads the tree only.
"""

import argparse
import glob
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import portlib as pl  # noqa: E402

ABI_FILES = ["config.h", "fcntl.h", "fenv.h", "jmp_buf.h", "signal.h", "syscall.h"]

# (selection file, what it selects, form it applies to: None for both)
SELECTION_POINTS = [
    ("runtime/core/runtime_core_sprt.cpp", "locks (B6b)", None),
    ("runtime/core/runtime_core_defaults.cpp", "clocks, sched, gettid (B6c)", None),
    ("runtime/core/runtime_core_setjmp.cpp", "setjmp/longjmp (B6c)", None),
    ("runtime/core/runtime_core_log.cpp", "log sink (B6c)", None),
    ("runtime/core/pthread/pthread.cc", "threads (B6d)", None),
    ("runtime/libc_wrapper/runtime/SPRuntimeCPlatform.cpp", "weak stubs (B6f)", "hosted"),
    ("runtime/src/platform/SPRuntimePlatform.cpp", "platform info (B6g)", None),
    ("runtime/src/filesystem/SPRuntimeFilesystem.cpp", "filesystem roots (B6g)", None),
    ("runtime/src/dispatch/SPRuntimeDispatchEvent.cpp", "event loop (B6g)", None),
]
MAKE_POINTS = [
    ("runtime/runtime.mk", "runtime link and include order (B6a)", None),
    ("runtime/libc_wrapper/libc-wrapper.mk", "libc_wrapper flags (B6a)", None),
]


class Os:
    def __init__(self, eng, name, macro, system, define):
        self.ident = name.replace("-", "_").lower()
        self.dash = self.ident.replace("_", "-")
        self.macro = (macro or self.ident).upper()
        self.sprt = "SPRT_" + self.macro
        self.system = system
        self.define = define
        self.aliases = pl.platform_aliases(eng, self.sprt)
        # Platforms that share a group macro with this one may share its files too (embox_user uses
        # the embox platform and filesystem layers through SPRT_EMBOX_ANY).
        self.siblings = set()
        text = eng.read("runtime/include/sprt/c/bits/__sprt_def.h") or ""
        for cond, group in re.findall(r"#if ([^\n]*)\n#define (SPRT_[A-Z_]+) 1\n", text):
            if re.search(pl.word(self.sprt), cond):
                for member in re.findall(r"SPRT_([A-Z_]+)", cond):
                    if member != self.macro:
                        self.siblings.add(member.lower())
        if self.system is None:
            self.system = guess_system(eng, self)
        if self.define is None:
            m = re.search(r"#elif ([^\n]*)\n(?:(?!#elif|#else|#endif).*\n)*?#define %s " % self.sprt, text)
            if m:
                names = re.findall(r"defined\(([A-Za-z_0-9]+)\)", m.group(1))
                self.define = names[0] if names else None

    def names(self):
        return {self.ident, self.dash}

    def file_names(self):
        return self.names() | self.siblings | {s.replace("_", "-") for s in self.siblings}

    def code_re(self):
        return re.compile("|".join(pl.word(a) for a in self.aliases))

    def make_re(self):
        tokens = [self.macro, "OSTYPE_IS_" + self.macro] + ([self.system] if self.system else [])
        return re.compile("|".join(pl.word(t) for t in tokens))


def guess_system(eng, o):
    text = eng.read("make/utils/apply-toolchain.mk") or ""
    for system, preset in re.findall(r"ifeq \(\$\(TARGET_SYSTEM\),([^)]+)\)\s*\n\s*include \$\(BUILD_ROOT\)/os/([\w-]+)\.mk", text):
        if preset in o.names():
            return system
    return None


def check_identity(eng, rep, o):
    rep.section("B2", "sprt recognizes the platform")
    rel = "runtime/include/sprt/c/bits/__sprt_def.h"
    text = eng.read(rel) or ""
    rep.check(re.search(r"#define\s+__SPRT_PLATFORM_NAME_%s\s+%s_sprt\b" % (o.macro, o.ident), text),
              "%s: __SPRT_PLATFORM_NAME_%s is %s_sprt" % (rel, o.macro, o.ident))
    ids = dict(re.findall(r"#define\s+__SPRT_PLATFORM_ID_([A-Z0-9_]+)\s+(\d+)", text))
    rep.check(o.macro in ids, "__SPRT_PLATFORM_ID_%s has a number" % o.macro,
              hint="next free: %d" % (max([int(v) for v in ids.values()] or [0]) + 1))
    by_value = {}
    for k, v in ids.items():
        by_value.setdefault(v, []).append(k)
    clashes = ["%s = %s" % (v, ", ".join(n)) for v, n in by_value.items() if len(n) > 1]
    rep.check(not clashes, "platform IDs are unique", details=clashes)
    pos = text.find("#define %s " % o.sprt)
    rep.check(pos >= 0, "%s is defined by the detection chain" % o.sprt,
              hint="add `#elif defined(%s)` with __SPRT_PLATFORM_NAME/__SPRT_PLATFORM_ID/%s"
              % (o.define or "__<OS>__", o.sprt))
    linux = text.find("defined(__linux__)")
    if pos >= 0 and o.ident not in ("linux", "android") and linux >= 0:
        if pos > linux:
            rep.warn("the %s branch comes after defined(__linux__)" % o.sprt,
                     hint="an OS that leaks __linux__ would be detected as Linux; move the branch up")
        else:
            rep.ok("the %s branch is tested before defined(__linux__)" % o.sprt)
    rep.info("compiler macro: %s; group macros: %s" % (o.define or "unknown (--define)",
             ", ".join(o.aliases[1:]) or "none"))
    rep.check(eng.contains("runtime/include/sprt/c/cross/__sprt_errno.h", pl.word(o.sprt)),
              "cross/__sprt_errno.h includes the platform errno.h under %s" % o.sprt, missing=pl.WARN,
              hint="without a branch the platform gets linux_sprt/errno.h; right only for Linux errno numbers")
    gated = [f for f in ("errno.h", "netdb.h", "syslog.h", "termios.h")
             if eng.contains("runtime/include_libc/" + f, o.code_re().pattern)]
    rep.info("include_libc headers that name the platform: %s" % (", ".join(gated) or "none"))


def check_cross(eng, rep, o, like, arches):
    rep.section("B3", "ABI headers cross/%s_sprt" % o.ident)
    base = "runtime/include/sprt/c/cross/%s_sprt" % o.ident
    if not eng.isdir(base):
        rep.fail("%s/ does not exist" % base, hint="start with one-line forwards to linux_sprt")
        return
    files = {f for f in eng.listdir(base) if f.endswith(".h")}
    subdirs = [d for d in eng.listdir(base) if d.endswith("_sprt") and eng.isdir(os.path.join(base, d))]
    ref = "runtime/include/sprt/c/cross/%s_sprt" % like
    if like and eng.isdir(ref) and like != o.ident:
        ref_files = {f for f in eng.listdir(ref) if f.endswith(".h")}
        missing = sorted(ref_files - files)
        rep.check(not missing, "%s/ has the headers %s_sprt has" % (base, like), missing=pl.WARN,
                  details=["missing: " + f for f in missing],
                  hint="a missing header falls back to a generic one; make sure that is intended")
    rep.check(subdirs, "architecture subdirectories: %s" % (", ".join(subdirs) or "none"))
    for d in subdirs:
        lack = [f for f in ABI_FILES if not eng.exists(os.path.join(base, d, f))]
        rep.check(not lack, "%s/%s has the six ABI headers" % (base, d), details=["missing: " + f for f in lack])
    for a in arches:
        rep.check("%s_sprt" % a in subdirs, "%s_sprt exists for the requested architecture" % a)

    broken = []
    for root, _, names in os.walk(eng.path(base)):
        for n in names:
            text = open(os.path.join(root, n), encoding="utf-8", errors="replace").read()
            for inc in re.findall(r'#\s*include\s*[<"](sprt/[^>"]+)[>"]', text):
                if not eng.exists(os.path.join("runtime/include", inc)):
                    broken.append("%s: %s" % (os.path.relpath(os.path.join(root, n), eng.root), inc))
    rep.check(not broken, "every sprt/ include in the cross headers resolves", details=broken)

    gates = sorted(set(re.findall(r"__SPRT_CONFIG_(HAVE_[A-Z0-9_]+)\b",
                                  eng.read("runtime/include/sprt/c/cross/__sprt_config.h") or "")))
    cfg = eng.read(os.path.join(base, "config.h")) or ""
    off = sorted(set(re.findall(r"#define\s+__SPRT_CONFIG_(HAVE_[A-Z0-9_]+)\s+0\b", cfg)))
    on = sorted(set(re.findall(r"#define\s+__SPRT_CONFIG_(HAVE_[A-Z0-9_]+)\s+1\b", cfg)))
    unknown = sorted(set(off + on) - set(gates))
    rep.info("config gates: %d known, %d switched off, %d set to 1 in %s/config.h"
             % (len(gates), len(off), len(on), base), details=["off: " + ", ".join(off)] if off else None)
    rep.check(not unknown, "config.h names only existing gates", missing=pl.WARN,
              details=["unknown: " + g for g in unknown], hint="a misspelled gate silently keeps the default 1")


def check_toolchain(eng, rep, o, form):
    rep.section("B4", "toolchain directory runtime/toolchains/target-%s" % o.dash)
    d = "runtime/toolchains/target-%s" % o.dash
    if not eng.isdir(d):
        rep.fail("%s/ does not exist" % d, hint="copy target-nuttx (hosted) or target-wasm (freestanding)")
    else:
        for f, need in (("Makefile", pl.FAIL), ("init-target.mk", pl.FAIL), ("install-target.mk", pl.FAIL),
                        ("compiler_rt.mk", pl.WARN)):
            rep.check(eng.exists(os.path.join(d, f)), "%s/%s" % (d, f), missing=need)
        if form == "hosted":
            imports = [f for f in eng.listdir(d) if f.startswith("import-")]
            rep.check(imports, "hosted import step (%s)" % (", ".join(imports) or "import-*.mk"), missing=pl.WARN)
        init = eng.read(os.path.join(d, "init-target.mk")) or ""
        if o.system:
            rep.check(re.search(r"TARGET_SYSTEM\s*:=\s*%s\b" % re.escape(o.system), init),
                      "init-target.mk writes TARGET_SYSTEM := %s" % o.system, missing=pl.WARN)
        if o.define:
            rep.check(o.define in init, "init-target.mk passes -D%s in target.mk flags" % o.define, missing=pl.WARN)
    conf = eng.read("runtime/toolchains/common/configure.mk") or ""
    rep.check(re.search(r"^\s*ifdef %s\b" % o.macro, conf, re.M), "common/configure.mk has an `ifdef %s` block" % o.macro,
              missing=pl.WARN, hint="per-OS flags for the cross-built dependencies live only there")
    if o.system:
        rep.check(eng.contains("runtime/toolchains/common/utils/names.mk", r"CONFIGURE_HOST_%s_" % re.escape(o.system)),
                  "names.mk: CONFIGURE_HOST_%s_<arch>" % o.system, missing=pl.WARN)


def check_preset(eng, rep, o):
    rep.section("B5", "make OS preset")
    preset = None
    for n in (o.dash, o.ident):
        if eng.exists("make/os/%s.mk" % n):
            preset = "make/os/%s.mk" % n
    if not rep.check(preset, "make/os/%s.mk exists" % o.dash) == pl.OK:
        return
    if o.system:
        rep.check(eng.contains("make/utils/apply-toolchain.mk", r"ifeq \(\$\(TARGET_SYSTEM\),%s\)" % re.escape(o.system)),
                  "make/utils/apply-toolchain.mk includes the preset for TARGET_SYSTEM %s" % o.system)
    else:
        rep.fail("no TARGET_SYSTEM branch in apply-toolchain.mk includes %s" % preset,
                 hint="else ifeq ($(TARGET_SYSTEM),<System>) include $(BUILD_ROOT)/os/<os>.mk")
    text = eng.read(preset) or ""
    for var in ("OSTYPE_EXEC_SUFFIX", "OSTYPE_LIB_SUFFIX", "OSTYPE_CONFIG_FLAGS", "OSTYPE_GENERAL_CFLAGS",
                "OSTYPE_EXEC_LDFLAGS"):
        rep.check(re.search(r"^\s*%s\s*[:+]?=" % var, text, re.M), "%s sets %s" % (preset, var), missing=pl.WARN)
    rep.check(re.search(r"^\s*%s\s*:=\s*1" % o.macro, text, re.M), "%s ends with `%s := 1`" % (preset, o.macro),
              missing=pl.WARN, hint="module .mk files test it with ifdef %s" % o.macro)


def check_platform_layer(eng, rep, o, form):
    rep.section("B6", "runtime platform layer (%s)" % form)
    code_re, make_re = o.code_re(), o.make_re()
    for rel, what, only in MAKE_POINTS:
        if only and only != form:
            continue
        rep.check(eng.contains(rel, make_re.pattern), "%s: %s names the platform" % (rel, what),
                  missing=pl.WARN if rel.endswith("libc-wrapper.mk") else pl.FAIL)
    for rel, what, only in SELECTION_POINTS:
        if only and only != form:
            continue
        named = eng.contains(rel, code_re.pattern)
        if not named and form == "hosted" and rel.endswith("pthread.cc"):
            rep.info("%s: %s through the generic pthread_native_pthread.cc" % (rel, what))
            continue
        rep.check(named, "%s: %s" % (rel, what), hint="add an #elif for %s (or join a group macro)" % o.sprt)
    core = [n for n in o.file_names() if eng.isdir("runtime/core/" + n)]
    rep.check(core, "runtime/core/%s/ (sprt_lock.cc, emutls.cc, ...)" % o.ident, missing=pl.WARN)
    for kind, pattern in (("platform", "runtime/src/platform/SPRuntimePlatform-%s.cc"),
                          ("filesystem", "runtime/src/filesystem/SPRuntimeFilesystem-%s.cc")):
        found = [pattern % n for n in sorted(o.file_names()) if eng.exists(pattern % n)]
        rep.check(found, "%s file %s" % (kind, found[0] if found else pattern % o.ident), missing=pl.WARN,
                  hint="a group sibling's file may serve this platform; check the selection point above")
    found = [n for n in sorted(o.file_names()) if eng.isdir("runtime/src/dispatch/platform/" + n)]
    rep.check(found, "event loop backend runtime/src/dispatch/platform/%s/" % o.ident, missing=pl.WARN)
    if form == "hosted":
        stubs = [n for n in o.names() if eng.exists("runtime/libc_wrapper/platform/%s/stubs.cc" % n)]
        rep.check(stubs, "runtime/libc_wrapper/platform/%s/stubs.cc" % o.ident, missing=pl.WARN)
    else:
        impl = [n for n in o.names() if eng.isdir("runtime/libc_impl/src/" + n)]
        rep.check(impl, "runtime/libc_impl/src/%s/" % o.ident)
        builtins = sorted(glob.glob(eng.path("runtime/libc_impl/src/builtin_*.cpp")))
        lacking = [os.path.basename(b) for b in builtins if not code_re.search(open(b, errors="replace").read())]
        users = [os.path.basename(b) for b in builtins if "SPRT_WASM" in open(b, errors="replace").read()]
        gap = [b for b in lacking if b in users]
        rep.check(not gap, "builtin_*.cpp that select the wasm backend also select %s" % o.sprt,
                  missing=pl.WARN, details=gap)
        asm = [d for d in eng.listdir("runtime/libc_impl/asm/%s" % (o.system or ""))] if o.system else []
        rep.info("libc_impl/asm/%s: %s" % (o.system, ", ".join(asm) or "none"))


def check_build(eng, rep, o, triple):
    rep.section("B7", "first build and the sysroot")
    if not triple:
        rep.skip("pass --triple to check the sysroot and the built tests")
        return
    d = pl.find_toolchain_dir(eng, "targets", triple)
    if not d:
        dangling = [p for p in pl.toolchain_dirs(eng, "targets", triple) if os.path.islink(p)]
        rep.fail("no sysroot for %s" % triple, details=["dangling link: %s -> %s" % (p, os.readlink(p))
                                                         for p in dangling],
                 hint="build it (B4) and link it into runtime/toolchains/targets/")
    else:
        rep.ok("sysroot: %s" % os.path.relpath(d, eng.root), details=["check it: utils/porting/check-sysroot.py " + triple])
        values, _ = pl.read_toolchain_mk(os.path.join(d, "target.mk"), ["TARGET_SYSTEM"])
        if o.system:
            rep.check(values.get("TARGET_SYSTEM") == o.system, "target.mk TARGET_SYSTEM is %s (found %s)"
                      % (o.system, values.get("TARGET_SYSTEM")))
    arch = pl.parse_triple(triple)["arch"]
    for proj, binary in (("tests/runtime", "runtimetest"), ("tests/libc", "libctest"), ("tests/stappler", "stapplertest"),
                         ("tests/window", "testapp")):
        hits = glob.glob(eng.path(os.path.join(proj, "stappler-build", triple, "*", "cc", binary + "*")))
        hits = [h for h in hits if not h.endswith(".d") and os.path.isfile(h)]
        if hits:
            fmt, found = pl.binary_info(hits[0])
            ok = pl.arch_matches(arch, found)
            rep.check(ok is not False, "%s built: %s (%s, %s)" % (binary, os.path.relpath(hits[0], eng.root), fmt, found))
        else:
            rep.info("%s is not built for %s" % (binary, triple))


def check_window(eng, rep, o):
    rep.section("B9", "window system and graphics")
    has_dir = any(eng.isdir("runtime/window/" + n) for n in o.file_names())
    rep.check(has_dir, "runtime/window/%s/" % o.ident, missing=pl.WARN, hint="--headless works without it")
    rep.check(eng.contains("runtime/window/common/SPRuntimeController.cc", o.code_re().pattern),
              "window/common/SPRuntimeController.cc registers a controller", missing=pl.WARN)
    vk = [n for n in o.names() if eng.isdir("xenolith/backend/vk/platform/" + n)]
    rep.info("Vulkan platform file: %s" % ("xenolith/backend/vk/platform/" + vk[0] if vk else
             "none (software rasterizer with SOFT=1, or headless)"))


def check_xenolith(eng, rep, o):
    rep.section("B10", "xenolith adaptations")
    for rel, what in (("xenolith/application/XLMain.cpp", "entry point"),
                      ("xenolith/application/XLContext.cc", "application event loop engine"),
                      ("xenolith/application/XLAppThread.cc", "app thread event loop engine"),
                      ("xenolith/core/XLCoreObject.cc", "SPIR-V reflection guard")):
        named = eng.contains(rel, o.code_re().pattern)
        rep.info("%s (%s): %s" % (rel, what, "names the platform" if named else "generic path"))


def check_docs(eng, rep, o, triple):
    rep.section("B11", "documentation")
    rep.check(eng.exists("docs/platforms/%s.adoc" % o.dash) or eng.exists("docs/platforms/%s.adoc" % o.ident),
              "docs/platforms/%s.adoc" % o.dash, missing=pl.WARN)
    rep.check(eng.exists("runtime/toolchains/target-%s/README.adoc" % o.dash),
              "runtime/toolchains/target-%s/README.adoc" % o.dash, missing=pl.WARN)
    for rel in ("docs/usage/codestyle/platform/platform-guards.adoc", "docs/usage/codestyle/platform/runtime-libc.adoc",
                "docs/agents/cross-target.md"):
        rep.check(eng.contains(rel, "%s|%s" % (pl.word(o.sprt), pl.word(o.ident))), "%s mentions the platform" % rel,
                  missing=pl.WARN)
    rep.section("B12", "distribution")
    top = eng.read("runtime/toolchains/Makefile")
    if triple:
        rel = triple in (pl.make_list(top, "RELEASE_TARGETS") or [])
        rep.info("RELEASE_TARGETS %s %s" % ("lists" if rel else "does not list", triple),
                 details=None if rel else ["fine when the sysroot cannot be redistributed; document the manual link"])
        rep.info("rule target-%s in runtime/toolchains/Makefile: %s"
                 % (triple, "present" if pl.make_rule_exists(top, "target-" + triple) else "absent (optional)"))


def check_like(eng, rep, o, like, window):
    rep.section("refs", "places that name SPRT_%s with no %s nearby" % (like.upper(), " / ".join(o.aliases)))
    like_tokens = ["SPRT_" + like.upper()]
    missing, _ = pl.scan_refs(eng, like_tokens, o.aliases, window=window)
    files = pl.group_by_file(missing)
    if not files:
        rep.ok("every SPRT_%s branch has the new platform within %d lines" % (like.upper(), window))
        return
    for path, lines in sorted(files.items()):
        rep.info("%s: %d line(s)" % (path, len(lines)), details=["%5d  %s" % (n, t) for n, t in lines[:8]])
    rep.info("candidates, not errors: each is a branch %s took; decide whether the new platform needs it" % like)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("os", help="the OS as its directories are named (nuttx, embox_user, wasm, myos)")
    ap.add_argument("--macro", help="the part after SPRT_ (default: the OS name in upper case)")
    ap.add_argument("--system", help="TARGET_SYSTEM (default: found through make/utils/apply-toolchain.mk)")
    ap.add_argument("--define", help="the compiler macro that identifies the OS (default: from __sprt_def.h)")
    ap.add_argument("--triple", help="a target triple, to check the sysroot and built tests")
    ap.add_argument("--arch", action="append", default=[], help="architecture that must have a cross subdir")
    ap.add_argument("--form", choices=("hosted", "freestanding"), help="default: guessed from the tree")
    ap.add_argument("--like", help="a ported OS to compare with (default: nuttx for hosted, wasm for freestanding)")
    ap.add_argument("--no-refs", action="store_true", help="skip the scan for branches only --like has")
    ap.add_argument("--window", type=int, default=12)
    pl.add_common_args(ap)
    args = ap.parse_args()

    eng = pl.Engine(args.engine)
    o = Os(eng, args.os, args.macro, args.system, args.define)
    form = args.form
    if form is None:
        form = "freestanding" if any(eng.isdir("runtime/libc_impl/src/" + n) for n in o.names()) else "hosted"
    like = (args.like or ("nuttx" if form == "hosted" else "wasm")).replace("-", "_")
    arches = [pl.normalize_arch(a) for a in args.arch]
    if args.triple:
        arches.append(pl.parse_triple(args.triple)["arch"])

    rep = pl.make_report(args, "plan B: target OS %s (%s, compared with %s)" % (o.ident, form, like))
    rep.section("B1", "names")
    rep.info("directories: %s; macro: %s; TARGET_SYSTEM: %s; compiler define: %s"
             % (" / ".join(sorted(o.names())), o.sprt, o.system or "unknown (--system)", o.define or "unknown (--define)"))
    check_identity(eng, rep, o)
    check_cross(eng, rep, o, like, sorted(set(arches)))
    check_toolchain(eng, rep, o, form)
    check_preset(eng, rep, o)
    check_platform_layer(eng, rep, o, form)
    check_build(eng, rep, o, args.triple)
    check_window(eng, rep, o)
    check_xenolith(eng, rep, o)
    check_docs(eng, rep, o, args.triple)
    if not args.no_refs and like != o.ident:
        check_like(eng, rep, o, like, args.window)
    return rep.finish()


if __name__ == "__main__":
    sys.exit(main())
