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

"""Proves the headers xlkdl generates from runtime/kdl/abi are the hand-written ones, semantically.

The hand-written headers are runtime/include of the git revision ORIGINALS (or `--originals DIR`);
the generated ones are under sprt/gen, `sprt/c/bits/x.h` as `sprt/gen/c/bits/x.h`. Every generated
header is written into an overlay include root. For each configuration of a set's matrix, a unit
including every header of the set is compiled twice - by its old paths against the original tree,
and by its sprt/gen paths with the overlay before runtime/include - and three things are compared:

  macros   `clang -E -dM`: every macro defined, bodies compared token by token
  decls    `-ast-dump=json`: every declaration in order, ids, positions and tag ownership dropped
  layouts  `-fdump-record-layouts-complete`: the layout of every record

When the units differ, or the original does not compile, each header is compiled on its own to say
which one it is; a header whose original does not compile has to fail with the overlay too. A header
no configuration reaches (e2k128, files no dispatcher includes) is compiled on its own after its
configuration's base headers.

    tests/libc/kdl-abi/compare.py [--xlkdl <binary>] [--originals DIR | --rev REV] [--set windows|libc]
                                  [--inline] [--self-test] [-j N] [-v]

It also checks that runtime/include/sprt/gen is what the descriptions generate, file for file.
`--inline` generates with every block written in place. `--self-test` breaks the overlay (a field
type, a guard, a macro body, an arch fragment, a block, an override, a platform fragment) and passes only when every
break is caught, then breaks the descriptions eight ways and expects xlkdl to refuse each with its
place. CLANG names the compiler (default: clang).
"""

import argparse
import concurrent.futures
import difflib
import glob
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
INCLUDE = os.path.join(REPO, "runtime", "include")
STUBS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "stubs")
MAP = os.path.join(REPO, "runtime", "kdl", "abi", "sprt.kdl")
API_MAP = os.path.join(REPO, "runtime", "kdl", "api", "sprt-api.kdl")
CLANG = os.environ.get("CLANG", "clang")

# The last revision with the hand-written headers in runtime/include; set by main().
ORIGINALS = "ffe9fede"
ORIGINAL = None

GEN = "sprt/gen/"

# The include guards of the generated headers the hand-written tree did not have: the only macros
# such a header adds by being a header of its own.
NEW_GUARDS = set()

MSVC = ["-fms-extensions", "-fms-compatibility"]
LANGS = [("c", ["-x", "c", "-std=c11"]), ("c++", ["-x", "c++", "-std=c++20"])]


def windows_matrix():
    out = []
    for target, extra in [
        ("x86_64-pc-windows-msvc", MSVC),
        ("aarch64-pc-windows-msvc", MSVC),
        ("x86_64-unknown-linux-gnu", []),
    ]:
        for lang, flags in LANGS:
            defines = [[], ["-DUNICODE"]]
            if target.startswith("aarch64"):
                defines.append(["-D_M_ARM64EC"])
            for d in defines:
                name = " ".join([target, lang] + [x[2:] for x in d])
                out.append((name, ["--target=" + target] + extra + flags + d))
    return out


# The platform each configuration selects through bits/__sprt_def.h; e2k has no clang target and is
# selected by its predefines on a target of the same pointer size.
LIBC_TARGETS = [
    ("linux-x86_64", ["--target=x86_64-unknown-linux-gnu"]),
    ("linux-aarch64", ["--target=aarch64-unknown-linux-gnu"]),
    ("linux-riscv64", ["--target=riscv64-unknown-linux-gnu"]),
    ("linux-loongarch64", ["--target=loongarch64-unknown-linux-gnu"]),
    ("linux-e2k", ["--target=x86_64-unknown-linux-gnu", "-U__x86_64__", "-D__e2k__"]),
    ("linux-e2k32", ["--target=i686-unknown-linux-gnu", "-U__i386__", "-D__e2k__"]),
    ("android-aarch64", ["--target=aarch64-linux-android24"]),
    ("android-arm", ["--target=armv7a-linux-androideabi24"]),
    ("android-x86", ["--target=i686-linux-android24"]),
    ("android-x86_64", ["--target=x86_64-linux-android24"]),
    ("macos-x86_64", ["--target=x86_64-apple-macosx12"]),
    ("macos-arm64", ["--target=arm64-apple-macosx12"]),
    ("ios-arm64", ["--target=arm64-apple-ios15"]),
    ("wasm32", ["--target=wasm32-unknown-unknown"]),
    ("wasm64", ["--target=wasm64-unknown-unknown"]),
    ("windows-x86_64", ["--target=x86_64-pc-windows-msvc"] + MSVC),
    ("windows-aarch64", ["--target=aarch64-pc-windows-msvc"] + MSVC),
    ("embox-aarch64", ["--target=aarch64-none-elf", "-D__EMBOX__"]),
    ("embox_user-aarch64", ["--target=aarch64-none-elf", "-D__EMBOX_USER__"]),
    ("nuttx-aarch64", ["--target=aarch64-none-elf", "-D__NuttX__"]),
]


def libc_matrix():
    out = []
    for target, flags in LIBC_TARGETS:
        for lang, lflags in LANGS:
            for build in ([], ["-D__SPRT_BUILD=1"]):
                name = " ".join([target, lang] + (["__SPRT_BUILD"] if build else []))
                out.append((name, flags + lflags + build))
    return out


SETS = {
    "windows": ("sprt/gen/wrappers/windows/abi/", windows_matrix),
    "libc": ("sprt/gen/c/", libc_matrix),
}

TOKEN = re.compile(r"""[A-Za-z_$][\w$]*|\.?\d[\w.']*|"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'|->|##|\S""")


def tokens(text):
    return " ".join(TOKEN.findall(text))


def run(args, source):
    proc = subprocess.run([CLANG] + args + ["-"], input=source, capture_output=True, text=True)
    return proc.returncode, proc.stdout, proc.stderr


def roots(overlay):
    out = ["-I", STUBS]
    if overlay:
        return out + ["-I", overlay, "-I", INCLUDE]
    return out + ["-I", ORIGINAL]


def source_of(header):
    """The path a generated header had when it was written by hand."""
    return "sprt/" + header[len(GEN):] if header.startswith(GEN) else header


def unit(headers, overlay=None):
    return "".join("#include <%s>\n" % (h if overlay else source_of(h)) for h in headers)


def macros(flags, overlay, source):
    rc, out, err = run(["-E", "-dM"] + flags + roots(overlay), source)
    if rc != 0:
        return None, err
    defs = {}
    for line in out.splitlines():
        m = re.match(r"#define (\w+)(\([^)]*\))?(?: (.*))?$", line)
        if m:
            params = tokens(m.group(2)) if m.group(2) is not None else None
            defs[m.group(1)] = (params, tokens(m.group(3) or ""))
    return defs, None


# Ids and positions, and isTagOwned: which declaration of a group introduced a tag, which changes
# when `typedef struct X *A, *B;` is written as two typedefs. Doc comments go too.
DROP = {"id", "loc", "range", "previousDecl", "parentDeclContextId", "isReferenced", "isUsed",
        "isTagOwned"}

# An unnamed record is spelled with where it was declared: `(anonymous at <file>:<line>:<col>)`.
WHERE = re.compile(r" at [^()]*:\d+:\d+\)")


def unplace(text):
    return WHERE.sub(")", text)


def strip(node):
    if isinstance(node, dict):
        out = {k: strip(v) for k, v in node.items() if k not in DROP and not k.endswith("Id")}
        if out.get("inner") == []:
            del out["inner"]
        return out
    if isinstance(node, list):
        # Doc comments clang attaches to a declaration are not part of it.
        return [strip(v) for v in node if not (isinstance(v, dict) and str(v.get("kind", "")).endswith("Comment"))]
    if isinstance(node, str):
        return unplace(node)
    return node


def decls(flags, overlay, source):
    rc, out, err = run(["-fsyntax-only", "-Xclang", "-ast-dump=json"] + flags + roots(overlay), source)
    if rc != 0:
        return None, err
    return unlink([strip(n) for n in json.loads(out).get("inner", [])]), None


# `extern "C"` gives functions and variables their linkage and does nothing to a type, so a block
# of it is spread into its declarations, each function and variable marked with the linkage.
def unlink(nodes):
    out = []
    for n in nodes:
        if n.get("kind") == "LinkageSpecDecl":
            for d in unlink(n.get("inner", [])):
                if d.get("kind") in ("FunctionDecl", "VarDecl"):
                    d = dict(d, linkage=n.get("language"))
                out.append(d)
        else:
            out.append(n)
    return out


def layouts(flags, overlay, source):
    rc, out, err = run(["-fsyntax-only", "-Xclang", "-fdump-record-layouts-complete"] + flags + roots(overlay),
                       source)
    if rc != 0:
        return None, err
    return unplace(out), None


def included(flags, overlay, source):
    """The overlay files a unit reads."""
    rc, out, err = run(["-fsyntax-only", "-H"] + flags + roots(overlay), source)
    found = set()
    for line in err.splitlines():
        m = re.match(r"^\.+ (.*)$", line)
        if m and m.group(1).startswith(overlay + "/"):
            found.add(os.path.relpath(m.group(1), overlay))
    return found


def describe_macros(a, b):
    lines = []
    for name in sorted(set(a) | set(b)):
        if a.get(name) != b.get(name):
            lines.append("    %s: %s -> %s" % (name, a.get(name, "(undefined)"), b.get(name, "(undefined)")))
            if len(lines) > 12:
                break
    return lines


def describe_decls(a, b):
    lines = []
    sm = difflib.SequenceMatcher(a=[json.dumps(x, sort_keys=True) for x in a],
                                 b=[json.dumps(x, sort_keys=True) for x in b], autojunk=False)
    name = lambda d: "%s %s" % (d.get("kind"), d.get("name", ""))
    for op, i1, i2, j1, j2 in sm.get_opcodes():
        if op == "equal":
            continue
        for x, y in zip(a[i1:i2] + [None] * max(0, (j2 - j1) - (i2 - i1)),
                        b[j1:j2] + [None] * max(0, (i2 - i1) - (j2 - j1))):
            lines.append("    %s -> %s" % (name(x) if x else "(none)", name(y) if y else "(none)"))
            if x and y and name(x) == name(y):
                at = json.dumps(x, indent=1, sort_keys=True).splitlines()
                bt = json.dumps(y, indent=1, sort_keys=True).splitlines()
                for d in list(difflib.unified_diff(at, bt, lineterm="", n=1))[2:10]:
                    lines.append("      " + d)
            if len(lines) > 16:
                return lines
    return lines


INSTRUMENTS = [
    ("macros", macros, describe_macros),
    ("decls", decls, describe_decls),
    ("layouts", layouts, None),
]


def compare_unit(flags, overlay, headers):
    """None when the original does not compile and the overlay agrees; else the problems."""
    problems = []
    original_fails = False
    for what, fn, describe in INSTRUMENTS:
        a, ea = fn(flags, None, unit(headers))
        b, eb = fn(flags, overlay, unit(headers, overlay))
        if what == "macros" and b is not None:
            b = {k: v for k, v in b.items() if k not in NEW_GUARDS}
        if a is None or b is None:
            if (a is None) != (b is None):
                err = (ea or eb).strip().splitlines()
                problems.append("  %s: %s does not compile: %s" % (what, "the original" if a is None else "the overlay",
                                                                   " / ".join(err[:3])))
            else:
                original_fails = True
            continue
        if a != b:
            problems.append("  %s differ" % what)
            if describe:
                problems.extend(describe(a, b))
    if original_fails and not problems:
        return None
    return problems


def alone(flags, header):
    rc, _, _ = run(["-fsyntax-only"] + flags + roots(None), unit([header]))
    return rc == 0


def check_config(config, headers, overlay):
    name, flags = config
    # Headers that compile only after others (an include they lack) go last, after what they need.
    first = [h for h in headers if alone(flags, h)]
    headers = first + [h for h in headers if h not in first]
    problems = compare_unit(flags, overlay, headers)
    covered = included(flags, overlay, unit(headers, overlay)) if problems == [] else set()
    if problems == []:
        return name, [], covered, []
    # Drill down: which header differs, which does not compile at all.
    per = []
    failing = []
    for k, h in enumerate(headers):
        context = [] if h in first else first
        p = compare_unit(flags, overlay, context + [h])
        if p is None:
            failing.append(h)
            continue
        covered |= included(flags, overlay, unit(context + [h], overlay))
        if p:
            per.append((h, p))
    return name, per, covered, failing


def run_set(set_name, overlay, generated, jobs, verbose, quiet=False):
    prefix, matrix = SETS[set_name]
    members = sorted(h for h in generated if h.startswith(prefix))
    fragments = {h for h in members if is_fragment(overlay, h)}
    # A header the hand-written tree did not have is proved through the headers that include it.
    originless = {h for h in members if not os.path.exists(os.path.join(ORIGINAL, source_of(h)))}
    headers = [h for h in members if h not in fragments and h not in originless]
    configs = matrix()
    failed = 0
    covered = set()
    never = {h: 0 for h in headers}
    say = (lambda *a: None) if quiet else print
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as pool:
        for name, per, cov, failing in pool.map(lambda c: check_config(c, headers, overlay), configs):
            covered |= cov
            for h in headers:
                if h not in failing:
                    never[h] += 1
            if per:
                failed += len(per)
                for h, problems in per:
                    say("[FAIL] %s [%s]" % (h, name))
                    for line in problems:
                        say(line)
            elif verbose:
                say("[ OK ] %s [%s]%s" % (set_name, name, (" (%d headers fail to compile in both)" % len(failing)) if failing else ""))
    # A header that compiles nowhere is compiled after the base headers of a configuration its path names.
    rest = sorted((set(members) - covered) | {h for h, n in never.items() if n == 0})
    text_only = []
    new = []
    for h in rest:
        # A header the hand-written tree did not have is proved only through its users.
        if not os.path.exists(os.path.join(ORIGINAL, source_of(h))):
            new.append(h)
            continue
        config = config_for(h, configs)
        pre = ["sprt/c/cross/__sprt_config.h"] if h.startswith(GEN + "c/") else []
        problems = compare_unit(config[1], overlay, pre + [h])
        if problems is None:
            text_only.append(h)
            continue
        if problems:
            failed += 1
            say("[FAIL] %s [%s, on its own]" % (h, config[0]))
            for line in problems:
                say(line)
        covered.add(h)
    say("[%s] %s: %d headers, %d fragments, %d new, %d configurations; %d compiled only on their own, %d nowhere%s%s" % (
        " OK " if not failed else "FAIL", set_name, len(headers), len(fragments), len(originless - fragments), len(configs),
        len(rest) - len(text_only) - len(new), len(text_only), (": " + ", ".join(text_only)) if text_only else "",
        ("; %d without an original and no user: %s" % (len(new), ", ".join(new))) if new else ""))
    return failed


def config_for(header, configs):
    m = re.match(r"^sprt/gen/c/cross/(\w+)_sprt/(?:(\w+)_sprt/)?", header)
    if m:
        platform, arch = m.group(1), m.group(2) or ""
        names = {"aarch64": "aarch64", "x86_64": "x86_64", "arm": "arm", "x86": "x86", "riscv64": "riscv64",
                 "loongarch64": "loongarch64", "e2k": "e2k", "e2k32": "e2k32", "e2k128": "e2k",
                 "wasm32": "wasm32", "wasm64": "wasm64"}
        want = platform + ("-" + names.get(arch, arch) if arch else "")
        if platform == "wasm":
            want = arch or "wasm32"
        if platform == "macos" and arch == "aarch64":
            want = "macos-arm64"
        if platform == "ios":
            want = "ios-arm64"
        for c in configs:
            if c[0].startswith(want + " ") or c[0].startswith(want):
                return c
    return configs[0]


def is_fragment(overlay, header):
    with open(os.path.join(overlay, header)) as f:
        text = f.read()
    return not re.search(r"^#ifndef \w+\n#define \w+\n", text, re.M)


def generate(xlkdl, out, inline=False, map_file=MAP):
    cmd = [xlkdl, "abi", "--map", map_file, "--out", out] + (["--inline"] if inline else [])
    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.returncode != 0:
        sys.exit("xlkdl failed:\n" + proc.stderr)
    found = []
    for root, _, files in os.walk(out):
        for f in files:
            found.append(os.path.relpath(os.path.join(root, f), out))
    return sorted(found)


# Each break changes one thing a reader of the header would rely on, and has to be caught.
BREAKS = [
    ("windows", "a field type", "sprt/gen/wrappers/windows/abi/structures.h", "\tDWORD dwLowDateTime;", "\tWORD dwLowDateTime;"),
    ("windows", "a guard", "sprt/gen/wrappers/windows/abi/basic_types.h", "#if !defined(_M_ARM64EC)\n#define MemoryBarrier",
     "#if defined(_M_ARM64EC)\n#define MemoryBarrier"),
    ("windows", "a macro body", "sprt/gen/wrappers/windows/abi/basic_types.h", "(((DWORD_PTR)(l)) & 0xffff))",
     "(((DWORD_PTR)(l)) & 0xfff))"),
    ("windows", "an arch fragment", "sprt/gen/wrappers/windows/abi/aarch64_sprt/context.h", "\tDWORD Fpcr;",
     "\tWORD Fpcr;"),
    ("libc", "a block", "sprt/gen/c/bits/stat_data.h", "st_nlink;", "st_nlink2;"),
    ("libc", "an override", "sprt/gen/c/cross/embox_sprt/signal.h", "((void (*)(int))0x3)", "((void (*)(int))0x7)"),
    ("libc", "a platform fragment", "sprt/gen/c/cross/linux_sprt/socket.h", "sa_family", "sa_family_"),
]


def self_test(overlay, generated, jobs):
    caught = 0
    for set_name, what, header, old, new in BREAKS:
        broken = tempfile.mkdtemp(prefix="kdl-abi-break-")
        try:
            shutil.copytree(overlay, broken, dirs_exist_ok=True)
            path = os.path.join(broken, header)
            with open(path) as f:
                text = f.read()
            if old not in text:
                print("[FAIL] self-test: no '%s' to break in %s" % (old, header))
                continue
            with open(path, "w") as f:
                f.write(text.replace(old, new, 1))
            failed = run_set(set_name, broken, generated, jobs, False, quiet=True)
            print("[%s] self-test: %s is %s" % (" OK " if failed else "FAIL", what, "caught" if failed else "NOT caught"))
            caught += 1 if failed else 0
        finally:
            shutil.rmtree(broken)
    return len(BREAKS) - caught


# A broken description has to be refused with the place of the break.
REFUSALS = [
    ("an unknown property", "windows/structures.kdl", "struct _FILETIME typedef=FILETIME also=",
     "struct _FILETIME typedef=FILETIME colour=red also=", "windows/structures.kdl:%d:35: unknown property 'colour'"),
    ("an unclosed bracket in a type", "windows/structures.kdl", 'field Data4 "BYTE [8]"', 'field Data4 "BYTE [8"',
     "windows/structures.kdl:%d:17: unbalanced brackets in the type 'BYTE [8'"),
    ("a name from a header not included", "windows/basic_types.kdl", "typedef ATOM WORD", "typedef ATOM LARGE_INTEGER",
     "windows/basic_types.kdl:%d:14: 'LARGE_INTEGER' comes from sprt/gen/wrappers/windows/abi/structures.h"),
    ("an unknown named condition", "bits/__sprt_int32_t.kdl", 'INT32_WIDTH 32 if="@assisted"',
     'INT32_WIDTH 32 if="@assistd"', "bits/__sprt_int32_t.kdl:%d:25: no condition '@assistd'"),
    ("a public macro without a rule", "cross/shared/linux+wasm/errno.kdl", "macro EPERM 1 public=#true",
     "macro `EPERM` 1 public=#true", "cross/shared/linux+wasm/errno.kdl:%d:24: a public macro stands at the top and has a name rule"),
    ("a number not spelled as in C", "bits/__sprt_int32_t.kdl", "macro INT32_WIDTH 32", "macro INT32_WIDTH 3_2",
     "bits/__sprt_int32_t.kdl:%d:19: the number 3_2 is not spelled as in C; write it as a string"),
    ("a target number used twice", "bits/__sprt_targets.kdl", "target x86 3 ", "target x86 4 ",
     "bits/__sprt_targets.kdl:%d:5: target x86 4 repeats x86_64 4"),
    ("an arch of the map not in the list", "bits/__sprt_targets.kdl", "target loongarch64 10",
     "target loongarch 10", "the map has the arch loongarch64_sprt of linux_sprt, which the list ARCH does not"),
]


def refusals(xlkdl):
    failed = 0
    source = os.path.dirname(MAP)
    for what, file, old, new, expect in REFUSALS:
        copy = tempfile.mkdtemp(prefix="kdl-abi-desc-")
        try:
            shutil.copytree(source, copy, dirs_exist_ok=True)
            path = os.path.join(copy, file)
            with open(path) as f:
                text = f.read()
            line = text[: text.index(old)].count("\n") + 1
            with open(path, "w") as f:
                f.write(text.replace(old, new, 1))
            out = tempfile.mkdtemp(prefix="kdl-abi-out-")
            proc = subprocess.run([xlkdl, "abi", "--map", os.path.join(copy, "sprt.kdl"), "--out", out],
                                  capture_output=True, text=True)
            shutil.rmtree(out)
            message = expect % line if "%d" in expect else expect
            ok = proc.returncode != 0 and message in proc.stderr.replace(copy + "/", "")
            print("[%s] refused: %s" % (" OK " if ok else "FAIL", what))
            if not ok:
                print("  expected: %s\n  got: %s" % (message, proc.stderr.strip()[:2000]))
                failed += 1
        finally:
            shutil.rmtree(copy)
    return failed


# The generator is deterministic, and --check accepts what it has just written.
def stable(xlkdl, overlay, inline):
    again = tempfile.mkdtemp(prefix="kdl-abi-again-")
    try:
        generate(xlkdl, again, inline)
        same = subprocess.run(["diff", "-r", overlay, again], capture_output=True).returncode == 0
        check = subprocess.run([xlkdl, "abi", "--map", MAP, "--out", overlay, "--check"] + (["--inline"] if inline else []),
                               capture_output=True).returncode == 0
    finally:
        shutil.rmtree(again)
    print("[%s] generator: the same output twice" % (" OK " if same else "FAIL"))
    print("[%s] generator: --check accepts its output" % (" OK " if check else "FAIL"))
    return (0 if same else 1) + (0 if check else 1)


# What runtime/include/sprt/gen holds is what the descriptions generate: no file stale, none extra.
def committed(xlkdl, generated):
    check = subprocess.run([xlkdl, "abi", "--map", MAP, "--out", INCLUDE, "--check"], capture_output=True, text=True)
    have = []
    for root, _, files in os.walk(os.path.join(INCLUDE, GEN)):
        have.extend(os.path.relpath(os.path.join(root, f), INCLUDE) for f in files)
    # The API map writes into sprt/gen too; compare_api.py proves those files.
    with tempfile.TemporaryDirectory(prefix="compare-api-files-") as api:
        subprocess.run([xlkdl, "api", "--map", API_MAP, "--root", api], capture_output=True, check=True)
        top = os.path.join(api, "include")
        for root, _, files in os.walk(top):
            generated = list(generated) + [os.path.relpath(os.path.join(root, f), top) for f in files]
            # The hand-written headers include the API ones now: their guards are new macros.
            for f in files:
                m = re.search(r"^#ifndef (\w+)\n#define \1$", open(os.path.join(root, f)).read(), re.M)
                if m:
                    NEW_GUARDS.add(m.group(1))
    extra = sorted(set(have) - set(generated))
    ok = check.returncode == 0 and not extra
    print("[%s] runtime/include/sprt/gen: %d headers as generated" % (" OK " if ok else "FAIL", len(generated)))
    if check.returncode != 0:
        print("  stale (regenerate: xlkdl abi --map runtime/kdl/abi/sprt.kdl --out runtime/include):")
        print("\n".join("    " + l for l in (check.stdout + check.stderr).strip().splitlines()[:20]))
    for f in extra:
        print("  not generated: %s" % f)
    return 0 if ok else 1


def extract_originals(rev, out):
    archive = subprocess.run(["git", "-C", REPO, "archive", rev, "runtime/include"], stdout=subprocess.PIPE, check=True)
    subprocess.run(["tar", "-x", "-C", out], input=archive.stdout, check=True)
    return os.path.join(out, "runtime", "include")


def find_xlkdl():
    found = sorted(glob.glob(os.path.join(REPO, "utils", "xlkdl", "stappler-build", "*", "debug", "*", "xlkdl")))
    return found[0] if found else None


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--xlkdl", default=find_xlkdl())
    parser.add_argument("--originals", help="the include root with the hand-written headers")
    parser.add_argument("--rev", default=ORIGINALS, help="take the hand-written headers from this git revision")
    parser.add_argument("--set", choices=sorted(SETS), action="append")
    parser.add_argument("--inline", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("-j", type=int, default=os.cpu_count() or 4)
    parser.add_argument("-v", action="store_true")
    args = parser.parse_args()
    if not args.xlkdl or not os.path.exists(args.xlkdl):
        sys.exit("xlkdl is not built: make -C utils/xlkdl, or pass --xlkdl")
    if shutil.which(CLANG) is None:
        sys.exit("%s not found (set CLANG=...)" % CLANG)

    global ORIGINAL
    overlay = tempfile.mkdtemp(prefix="kdl-abi-")
    originals = None
    try:
        if args.originals:
            ORIGINAL = os.path.abspath(args.originals)
        else:
            originals = tempfile.mkdtemp(prefix="kdl-abi-originals-")
            ORIGINAL = extract_originals(args.rev, originals)
        generated = generate(args.xlkdl, overlay, args.inline)
        for h in generated:
            if not os.path.exists(os.path.join(ORIGINAL, source_of(h))):
                with open(os.path.join(overlay, h)) as f:
                    m = re.search(r"^#ifndef (\w+)\n#define \1\n", f.read(), re.M)
                if m:
                    NEW_GUARDS.add(m.group(1))
        failed = stable(args.xlkdl, overlay, args.inline)
        if not args.inline:
            failed += committed(args.xlkdl, generated)
        for set_name in args.set or sorted(SETS, reverse=True):
            failed += run_set(set_name, overlay, generated, args.j, args.v)
        if args.self_test:
            failed += self_test(overlay, generated, args.j)
            failed += refusals(args.xlkdl)
    finally:
        shutil.rmtree(overlay)
        if originals:
            shutil.rmtree(originals)
    print("total failures: %d" % failed)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
