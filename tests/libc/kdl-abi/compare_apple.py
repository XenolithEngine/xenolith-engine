#!/usr/bin/env python3
# Copyright (c) 2026 Stappler Team <admin@stappler.org>
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

"""Proves the split Apple +open overlay - hand-written API headers and the ABI xlkdl generates from
runtime/kdl/abi/apple-open - is the overlay it was split from, semantically.

    tests/libc/kdl-abi/compare_apple.py [--rev REV | --originals DIR] [--overlay DIR] [--targets DIR]
                                        [--xlkdl BIN] [-j N]

The overlay before the split is runtime/toolchains/target-apple/open/sysroot of `--rev` (or
`--originals`); the split one is the working tree's, with the ABI generated afresh into a copy. For
each configuration - x86_64 and arm64; Objective-C and Objective-C++ for everything, C and C++ for
the include_libc shims - one unit including every hand-written header is compiled against the
installed `<arch>-apple-macosx+open` sysroot of `--targets` with each overlay in front of it. The
macros are compared by name, the declarations and the record layouts as multisets: the split moves
a header's ABI to its top, which changes the order of declarations and nothing else.
"""

import argparse
import concurrent.futures
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import compare  # noqa: E402

REPO = compare.REPO
OVERLAY_REL = os.path.join("runtime", "toolchains", "target-apple", "open", "sysroot")
OVERLAY = os.path.join(REPO, OVERLAY_REL)
MAP = os.path.join(REPO, "runtime", "kdl", "abi", "apple-open.kdl")
CLANG = compare.CLANG
ORIGINALS = "ffe9fede"

FOREIGN = ("include_libc/mach/", "include_libc/sys/syscall.h", "include_libc/__sprt_abi/")
FRAMEWORK = re.compile(r"^System/Library/Frameworks/(\w+)\.framework/Headers/(.+)$")

ARCHES = [("x86_64", "x86_64-apple-macosx14.5"), ("aarch64", "arm64-apple-macosx14.5")]
LANGS = {
    # The framework headers use C23 digit separators (0xffff'0000UL), as Apple's do.
    "objective-c": ["-x", "objective-c", "-std=gnu2x"],
    "objective-c++": ["-x", "objective-c++", "-std=gnu++20"],
    "c": ["-x", "c", "-std=gnu11"],
    "c++": ["-x", "c++", "-std=gnu++20"],
}


def headers(root):
    """The spellings of the hand-written headers of an overlay: (spelling, is a framework header)."""
    out = []
    for d, _, fs in os.walk(root):
        for f in fs:
            if not f.endswith(".h"):
                continue
            rel = os.path.relpath(os.path.join(d, f), root)
            if rel.startswith(FOREIGN):
                continue
            m = FRAMEWORK.match(rel)
            if m:
                out.append(("%s/%s" % (m.group(1), m.group(2)), True))
            elif rel.startswith("include_libc/"):
                out.append((rel[len("include_libc/"):], False))
    return sorted(out)


def flags(arch_target, lang, sysroot):
    return (["--target=" + arch_target] + LANGS[lang] +
            ["-nostdlibinc", "-isystem", os.path.join(sysroot, "include_libc"),
             "-F", os.path.join(sysroot, "System", "Library", "Frameworks"), "-Wno-everything"])


def merged(sysroot, overlay, out):
    """The headers of an installed sysroot with an overlay copied over them, as the build does: a
    framework the overlay has only some headers of keeps the others."""
    for part in ("include_libc", "System"):
        shutil.copytree(os.path.join(sysroot, part), os.path.join(out, part), symlinks=True,
                        ignore=shutil.ignore_patterns("*.tbd", "*.dylib", "*.a"))
    for d, _, fs in os.walk(overlay):
        for f in fs:
            if f.endswith(".tbd"):
                continue
            src = os.path.join(d, f)
            dst = os.path.join(out, os.path.relpath(src, overlay))
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            if os.path.lexists(dst):
                os.remove(dst)
            shutil.copy2(src, dst)
    return out


def run(args, source):
    proc = subprocess.run([CLANG] + args + ["-"], input=source, capture_output=True, text=True)
    return proc.returncode, proc.stdout, proc.stderr


def unit(names):
    return "".join("#include <%s>\n" % n for n in names)


def macros(args, source):
    rc, out, err = run(["-E", "-dM"] + args, source)
    if rc:
        return None, err
    defs = {}
    for line in out.splitlines():
        m = re.match(r"#define (\w+)(\([^)]*\))?(?: (.*))?$", line)
        if m:
            defs[m.group(1)] = (compare.tokens(m.group(2)) if m.group(2) else None, compare.tokens(m.group(3) or ""))
    return defs, None


POINTER = re.compile(r"^0x[0-9a-f]+$")


def unpoint(node):
    """Without the compiler's addresses (`referencedMemberDecl` of an inline body and the like)."""
    if isinstance(node, dict):
        return {k: unpoint(v) for k, v in node.items() if not (isinstance(v, str) and POINTER.match(v))}
    if isinstance(node, list):
        return [unpoint(v) for v in node]
    return node


def decls(args, source):
    rc, out, err = run(["-fsyntax-only", "-Xclang", "-ast-dump=json"] + args, source)
    if rc:
        return None, err
    nodes = compare.unlink([unpoint(compare.strip(n)) for n in json.loads(out).get("inner", [])])
    return sorted(json.dumps(n, sort_keys=True) for n in nodes), None


def layouts(args, source):
    rc, out, err = run(["-fsyntax-only", "-Xclang", "-fdump-record-layouts-complete"] + args, source)
    if rc:
        return None, err
    blocks = [b.strip() for b in compare.unplace(out).split("*** Dumping AST Record Layout") if b.strip()]
    return sorted(blocks), None


def describe(what, a, b):
    if what == "macros":
        return compare.describe_macros(a, b)
    sa, sb = set(a), set(b)
    lines = []
    for x in sorted(sa - sb)[:4]:
        lines.append("    only before: %s" % x[:240])
    for x in sorted(sb - sa)[:4]:
        lines.append("    only after:  %s" % x[:240])
    if not lines:
        lines.append("    the same entries, counted differently")
    return lines


def compare_config(name, a_flags, b_flags, names, guards):
    source = unit(names)
    problems = []
    for what, fn in (("macros", macros), ("decls", decls), ("layouts", layouts)):
        a, ea = fn(a_flags, source)
        b, eb = fn(b_flags, source)
        if a is None or b is None:
            def first_errors(text):
                lines = [ln for ln in text.splitlines() if " error: " in ln or ln.startswith("error:")]
                return lines or text.strip().splitlines()
            ea = "\n".join(first_errors(ea)) if ea else ea
            eb = "\n".join(first_errors(eb)) if eb else eb
            if (a is None) != (b is None):
                err = (ea or eb).strip().splitlines()
                problems.append("  %s: %s does not compile: %s" % (what, "before" if a is None else "after", " / ".join(err[:3])))
            else:
                problems.append("  %s: neither compiles: %s" % (what, " / ".join(ea.strip().splitlines()[:2])))
            continue
        if what == "macros":
            b = {k: v for k, v in b.items() if k not in guards}
        if a != b:
            problems.append("  %s differ" % what)
            problems.extend(describe(what, a, b))
    return name, problems


# Each break changes one thing of the generated ABI and has to be caught.
BREAKS = [
    ("an enum value", "include_libc/__sprt_abi/Metal/MTLPixelFormat.h", "MTLPixelFormatR8Unorm = 10,", "MTLPixelFormatR8Unorm = 11,"),
    ("a field type", "include_libc/__sprt_abi/CoreFoundation/CFCGTypes.h", "CGFloat dx;", "float dx;"),
    ("a macro body", "include_libc/__sprt_abi/TargetConditionals.h", "#define TARGET_OS_OSX 1", "#define TARGET_OS_OSX 0"),
]


def run_all(jobs, j, quiet=False):
    failed = 0
    with concurrent.futures.ThreadPoolExecutor(max_workers=j) as pool:
        for name, problems in pool.map(lambda job: compare_config(*job), jobs):
            if not quiet:
                print("[%s] %s" % (" OK " if not problems else "FAIL", name))
                for line in problems:
                    print(line)
            failed += 1 if problems else 0
    return failed


def extract(rev, out):
    os.makedirs(out, exist_ok=True)
    archive = subprocess.run(["git", "-C", REPO, "archive", rev, OVERLAY_REL], stdout=subprocess.PIPE, check=True)
    subprocess.run(["tar", "-x", "-C", out], input=archive.stdout, check=True)
    return os.path.join(out, OVERLAY_REL)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--rev", default=ORIGINALS)
    ap.add_argument("--originals")
    ap.add_argument("--overlay", default=OVERLAY)
    ap.add_argument("--map", default=MAP)
    ap.add_argument("--targets", default=os.path.join(REPO, "runtime", "toolchains", "targets"))
    ap.add_argument("--xlkdl", default=compare.find_xlkdl())
    ap.add_argument("--self-test", action="store_true")
    ap.add_argument("-j", type=int, default=os.cpu_count() or 4)
    args = ap.parse_args()

    tmp = tempfile.mkdtemp(prefix="kdl-abi-apple-")
    try:
        before = os.path.abspath(args.originals) if args.originals else extract(args.rev, os.path.join(tmp, "before"))
        # The split overlay, its ABI generated afresh: what the descriptions say, not what is committed.
        after = os.path.join(tmp, "after")
        shutil.copytree(args.overlay, after, ignore=shutil.ignore_patterns("*.tbd"))
        shutil.rmtree(os.path.join(after, "include_libc", "__sprt_abi"), ignore_errors=True)
        proc = subprocess.run([args.xlkdl, "abi", "--map", args.map, "--out", os.path.join(after, "include_libc")],
                              capture_output=True, text=True)
        if proc.returncode:
            sys.exit("xlkdl failed:\n" + proc.stderr)
        committed = subprocess.run([args.xlkdl, "abi", "--map", args.map, "--out",
                                    os.path.join(args.overlay, "include_libc"), "--check"], capture_output=True, text=True)
        failed = 0
        ok = committed.returncode == 0
        print("[%s] %s/include_libc/__sprt_abi is what the descriptions generate" % (" OK " if ok else "FAIL",
                                                                                   os.path.relpath(args.overlay, REPO)))
        failed += 0 if ok else 1
        guards = set()
        for d, _, fs in os.walk(os.path.join(after, "include_libc", "__sprt_abi")):
            for f in fs:
                m = re.search(r"^#ifndef (\w+)\n#define \1\n", open(os.path.join(d, f)).read(), re.M)
                if m:
                    guards.add(m.group(1))
        all_names = headers(before)
        if headers(after) != all_names:
            print("[FAIL] the split overlay has other headers: %s" % sorted(set(headers(after)) ^ set(all_names)))
            failed += 1
        jobs = []
        for arch, triple in ARCHES:
            sysroot = os.path.join(args.targets, "%s-apple-macosx+open" % arch)
            if not os.path.isdir(sysroot):
                print("[SKIP] %s not installed" % sysroot)
                continue
            old = merged(sysroot, before, os.path.join(tmp, "%s-before" % arch))
            new = merged(sysroot, after, os.path.join(tmp, "%s-after" % arch))
            for lang in LANGS:
                names = [n for n, fw in all_names if lang.startswith("objective") or not fw]
                jobs.append(("%s %s" % (arch, lang), flags(triple, lang, old), flags(triple, lang, new), names, guards))
        failed += run_all(jobs, args.j)
        print("[%s] %d hand-written headers, %d configurations" % (" OK " if not failed else "FAIL", len(all_names), len(jobs)))
        if args.self_test:
            for what, rel, old, new in BREAKS:
                caught = True
                for arch, triple in ARCHES:
                    path = os.path.join(tmp, "%s-after" % arch, rel)
                    if not os.path.exists(path):
                        continue
                    text = open(path).read()
                    if old not in text:
                        print("[FAIL] self-test: no '%s' in %s" % (old, rel))
                        caught = False
                        break
                    open(path, "w").write(text.replace(old, new, 1))
                if caught:
                    caught = run_all(jobs, args.j, quiet=True) > 0
                for arch, triple in ARCHES:
                    path = os.path.join(tmp, "%s-after" % arch, rel)
                    if os.path.exists(path):
                        open(path, "w").write(open(path).read().replace(new, old, 1))
                print("[%s] self-test: %s is %s" % (" OK " if caught else "FAIL", what, "caught" if caught else "NOT caught"))
                failed += 0 if caught else 1
    finally:
        shutil.rmtree(tmp)
    print("total failures: %d" % failed)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
