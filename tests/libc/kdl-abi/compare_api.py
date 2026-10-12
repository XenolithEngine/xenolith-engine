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

"""Proves that what xlkdl generates from runtime/kdl/api is the hand-written API it replaced.

The hand-written runtime is the git revision ORIGINALS (runtime/include, include_libc, libc_impl);
the generated one is the worktree, or under --self-test a copy regenerated from broken descriptions.

  headers   each unit is compiled against both trees for every configuration of its matrix:
            functions (`-ast-dump=json`: name, type with its calling convention, attributes, storage,
            inline, linkage, and for a body the function it calls with the parameters in order;
            parameter names are not compared), every other declaration, and the macros (`-E -dM`,
            less the guards of the generated headers)
  umbrella  `#include <dirent.h>` with SPRT_UMBRELLA_REQUIRED 0 (prototypes) and 1 (inline bodies)
  .def      LIBRARY and the set of EXPORTS, and `llvm-nm` of the import libraries llvm-lib builds
            from the old and the new file for x64 and arm64
  table     the DLL file, the class, the set of exports, the preloads
  wrappers  the old and the new dllstub2_<dll>.cc compiled with __SPRT_WIN_USE_IMPORT_LIB=0 for
            x86_64 and aarch64 Windows, both against the new loader tables: the disassembly of each
            function

    tests/libc/kdl-abi/compare_api.py [--xlkdl <binary>] [--rev REV] [--self-test] [-j N] [-v]

Before all that the fixture fixtures/api (every form of a function, an umbrella and a DLL) is
generated and compared with fixtures/api/expected, and nine broken copies of it have to be refused
with the place of the break; `--update-fixtures` writes the expected files instead.

`--self-test` also breaks the descriptions (a parameter type, a calling convention, the DLL of a
wrapper, an export, the guard of the umbrella bodies, an export of a DLL) and passes only when each
break is caught. CLANG names the compiler of the header units (default: clang); the wrappers are
built with the host toolchain of the target sysroots (TOOLCHAINS, default the xenolith checkout).
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

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import compare as A  # noqa: E402

REPO = A.REPO
RUNTIME = os.path.join(REPO, "runtime")
ORIGINALS = "814d1c4f"
API_MAP = os.path.join("kdl", "api", "sprt-api.kdl")
TOOLCHAINS = os.environ.get("TOOLCHAINS", "/home/sbkarr/stappler/xenolith/xenolith-engine/runtime/toolchains")
HOST = os.path.join(TOOLCHAINS, "hosts", "x86_64-unknown-linux-gnu")

DLL_DIR = "libc_impl/src/windows/dll"
WIN_DIR = "include/sprt/wrappers/windows"

def windows_matrix():
    """The Windows API is declared for Windows only: the matrix of the ABI less its Linux row."""
    return [c for c in A.windows_matrix() if "windows" in c[0]]


def umbrella_matrix():
    """The umbrellas are the libc of a target without one: each target, both bodies."""
    out = []
    for target, flags in A.LIBC_TARGETS:
        for lang, lflags in A.LANGS:
            for required in ("0", "1"):
                out.append(("%s %s UMBRELLA=%s" % (target, lang, required),
                            flags + lflags + ["-DSPRT_UMBRELLA_REQUIRED=" + required]))
    return out


def units(new):
    """Each hand-written header of the wrapper on its own (two of them conflict when included
    together), and the libc pair."""
    out = [("windows %s" % h, "#include <sprt/wrappers/windows/%s>\n" % h, windows_matrix)
           for h in sorted(os.listdir(os.path.join(new, WIN_DIR))) if h.endswith(".h")]
    out.append(("sprt dirent", "#include <sprt/c/__sprt_dirent.h>\n", A.libc_matrix))
    out.append(("umbrella dirent", "#include <dirent.h>\n", umbrella_matrix))
    return out


def roots(tree):
    return ["-I", A.STUBS, "-I", os.path.join(tree, "include_libc"), "-I", os.path.join(tree, "include")]


def run(args, source):
    return A.run(args, source)


def call_of(node, params):
    """The call a body makes: the function, and each argument as the index of the parameter it is."""
    calls = []

    def walk(n):
        if isinstance(n, dict):
            if n.get("kind") == "CallExpr" and not calls:
                calls.append(n)
                return
            for v in n.get("inner", []):
                walk(v)

    walk(node)
    if not calls:
        return ("no call",)

    def ref(n):
        if isinstance(n, dict):
            if n.get("kind") == "DeclRefExpr":
                return n.get("referencedDecl", {}).get("name")
            for v in n.get("inner", []):
                r = ref(v)
                if r:
                    return r
        return None

    inner = calls[0].get("inner", [])
    callee = ref(inner[0]) if inner else None
    args = []
    for a in inner[1:]:
        name = ref(a)
        args.append(params.index(name) if name in params else name)
    returns = '"kind": "ReturnStmt"' in json.dumps(node)
    return (callee, tuple(args), returns)


def function_of(n, linkage):
    inner = n.get("inner", [])
    params = [p.get("name") for p in inner if p.get("kind") == "ParmVarDecl"]
    attrs = tuple(sorted(i.get("kind") for i in inner if str(i.get("kind", "")).endswith("Attr")))
    body = None
    for i in inner:
        if i.get("kind") == "CompoundStmt":
            body = call_of(i, params)
    return (n.get("name"), n.get("type", {}).get("qualType"), n.get("storageClass"), bool(n.get("inline")),
            linkage, attrs, body)


def ast(flags, tree, source):
    """The functions of a unit, and its other declarations, both sorted."""
    rc, out, err = run(["-fsyntax-only", "-Xclang", "-ast-dump=json"] + flags + roots(tree), source)
    if rc != 0:
        return None, None, err
    funcs = []
    others = []

    def walk(nodes, linkage):
        for n in nodes:
            kind = n.get("kind")
            if kind == "LinkageSpecDecl":
                walk(n.get("inner", []), n.get("language"))
            elif kind == "FunctionDecl":
                funcs.append(function_of(n, linkage))
            else:
                # A pointer of the compiler (referencedMemberDecl) is no part of a declaration.
                others.append(re.sub(r'"0x[0-9a-f]{6,}"', '"0x"', json.dumps(A.strip(n), sort_keys=True)))

    walk(json.loads(out).get("inner", []), None)
    # A function has the language linkage of its first declaration: in C++ a redeclaration outside
    # extern "C" is still a C function.
    first = {}
    for f in funcs:
        first.setdefault(f[0], f[4])
    funcs = [f[:4] + (first[f[0]],) + f[5:] for f in funcs]
    return sorted(funcs, key=repr), sorted(others), None


# A function is declared by what precedes its name: the linkage attribute, the calling convention,
# the return type. On x64 and arm64 clang drops a __stdcall from the type, so the AST alone would not
# see it go; the preprocessed text does.
def prefixes(flags, tree, source, names):
    rc, out, err = run(["-E", "-P"] + flags + roots(tree), source)
    if rc != 0:
        return None
    toks = A.TOKEN.findall(out)
    found = []
    start = 0
    depth = 0
    for i, t in enumerate(toks):
        if t in ("(", "["):
            depth += 1
        elif t in (")", "]"):
            depth -= 1
        elif depth == 0 and t in (";", "{", "}"):
            start = i + 1
        elif depth == 0 and t in names and i + 1 < len(toks) and toks[i + 1] == "(":
            found.append((t, " ".join(toks[start:i])))
    return sorted(found)


def macros(flags, tree, source, guards):
    rc, out, err = run(["-E", "-dM"] + flags + roots(tree), source)
    if rc != 0:
        return None, err
    defs = {}
    for line in out.splitlines():
        m = re.match(r"#define (\w+)(\([^)]*\))?(?: (.*))?$", line)
        if m and m.group(1) not in guards:
            defs[m.group(1)] = (A.tokens(m.group(2)) if m.group(2) is not None else None,
                                A.tokens(m.group(3) or ""))
    return defs, None


def describe(a, b, what):
    sa, sb = set(a), set(b)
    out = []
    for x in sorted(sa - sb, key=repr)[:3]:
        out.append("    %s only in the original: %s" % (what, str(x)[:300]))
    for x in sorted(sb - sa, key=repr)[:3]:
        out.append("    %s only in the generated: %s" % (what, str(x)[:300]))
    if not out:
        out.append("    %s differ in multiplicity" % what)
    return out


def compare_config(name, flags, source, old, new, guards):
    fa, oa, err = ast(flags, old, source)
    fb, ob, err2 = ast(flags, new, source)
    # Where the original does not compile, the generated has to fail too.
    if fa is None:
        if fb is not None:
            return name, False, ["    the generated compiles, the original does not: " + err.strip().splitlines()[-1]]
        return name, None, ["    the original does not compile: " + err.strip().splitlines()[-1]]
    err = err2
    if fb is None:
        return name, False, ["    the generated does not compile: " + "\n      ".join(err.strip().splitlines()[:4])]
    problems = []
    if fa != fb:
        problems += describe(fa, fb, "function")
    if oa != ob:
        problems += describe(oa, ob, "declaration")
    names = set(f[0] for f in fa) | set(f[0] for f in fb)
    pa, pb = prefixes(flags, old, source, names), prefixes(flags, new, source, names)
    if pa != pb:
        problems += describe(pa or [], pb or [], "declared with")
    ma, _ = macros(flags, old, source, ())
    mb, _ = macros(flags, new, source, guards)
    if ma != mb:
        problems += describe(["%s %s" % (k, v) for k, v in ma.items()], ["%s %s" % (k, v) for k, v in mb.items()],
                             "macro")
    return name, not problems, problems


def guards_of(new):
    """The include guards of the generated API headers: the original had no such headers."""
    out = set()
    for d, _, files in os.walk(os.path.join(new, "include", "sprt", "gen")):
        for f in files:
            if f.endswith(".h") and ("/api" in d or "/umbrella" in d):
                text = open(os.path.join(d, f)).read()
                m = re.search(r"^#ifndef (\w+)\n#define \1$", text, re.M)
                if m:
                    out.add(m.group(1))
    return out


def run_headers(old, new, jobs, verbose, quiet=False, only=None):
    guards = guards_of(new)
    failures = 0
    work = [(unit, source, c) for unit, source, matrix in units(new) if only is None or unit in only
            for c in matrix()]
    with concurrent.futures.ThreadPoolExecutor(jobs) as pool:
        results = list(pool.map(lambda w: (w[0],) + compare_config(w[2][0], w[2][1], w[1], old, new, guards), work))
    summary = {}
    for unit, name, r, problems in results:
        summary.setdefault(unit, []).append((name, r, problems))
    windows = [u for u in summary if u.startswith("windows ")]
    for unit, rows in summary.items():
        bad = [x for x in rows if x[1] is False]
        failures += len(bad)
        if quiet:
            continue
        ok = sum(1 for _, r, _ in rows if r)
        skipped = sum(1 for _, r, _ in rows if r is None)
        if unit.startswith("windows ") and not bad and not verbose:
            continue
        print("%s: %d configurations identical%s%s" % (
            unit, ok, ", %d without an original" % skipped if skipped else "",
            ", %d DIFFER" % len(bad) if bad else ""))
        for name, r, problems in rows:
            if r is False or (r is None and verbose):
                print("  %s" % name)
                print("\n".join(problems))
    if not quiet and windows:
        rows = [x for u in windows for x in summary[u]]
        print("windows: %d headers, %d configurations identical, %d without an original (C++-only headers in C)%s"
              % (len(windows), sum(1 for x in rows if x[1]), sum(1 for x in rows if x[1] is None),
                 ", %d DIFFER" % sum(1 for x in rows if x[1] is False) if any(x[1] is False for x in rows) else ""))
    return failures


def read_def(path):
    lib, exports, section = None, set(), False
    for line in open(path):
        s = line.split(";")[0].strip()
        if s.startswith("LIBRARY"):
            lib = s.split()[1]
        elif s == "EXPORTS":
            section = True
        elif section and s:
            exports.add(s.split()[0])
    return lib, exports


def import_symbols(defpath, machine, tmp):
    out = os.path.join(tmp, "%s-%s.lib" % (os.path.basename(defpath), machine))
    subprocess.run([os.path.join(HOST, "bin", "llvm-lib"), "/def:" + defpath, "/out:" + out, "/machine:" + machine],
                   check=True, capture_output=True)
    nm = subprocess.run([os.path.join(HOST, "bin", "llvm-nm"), out], check=True, capture_output=True, text=True).stdout
    return sorted(l for l in nm.splitlines() if l and not l.endswith(":"))


# The table lists the functions of a DLL first and then its other exports: the order is no part of it.
TABLE = re.compile(r"struct (\w+) : DllTable \{\s*\w+\(\) : DllTable\(L\"([^\"]+)\", &__\w+_begin, &__\w+_end(, __preloads)?\)")


def read_table(path):
    text = open(path).read()
    m = TABLE.search(text)
    preloads = []
    listed = re.search(r"#define (\w+)\(X\)((?:[^\n]*\\\n)+[^\n]*)", text)
    if listed:
        preloads = re.findall(r"X\((\w+)\)", listed.group(2))
    elif m and m.group(3):
        preloads = re.findall(r"&(\w+),", re.search(r"__preloads\[\d+\] = \{(.*?)\};", text, re.S).group(1))
    return (m.group(1), m.group(2)) if m else None, sorted(re.findall(r"SPWIN_DEFINE_PROTO\((\w+)\)", text)), preloads


STUB_FLAGS = ["-x", "c++", "-std=gnu++2a", "-fms-compatibility-version=19.40", "-fexceptions", "-frtti", "-D_MT",
              "-D_WIN32_WINNT=0x0A00", "-DSPRT_BUILD_RUNTIME", "-nostdinc", "-ffreestanding", "-fbuiltin", "-O2",
              "-g0", "-D__SPRT_WIN_USE_IMPORT_LIB=0", "-Wno-unused-command-line-argument"]


def stub_object(tree, paths, name, target, tmp):
    """A wrapper file compiled the way libc_impl compiles it: after dllloader.cc, in its unit, against
    the loader and the tables of `tree`. The old wrappers are compiled against the new tables: where a
    record stands in a table is the loader's own business, and the tables are compared by themselves."""
    sysroot = os.path.join(TOOLCHAINS, "targets", target)
    loader = os.path.join(tree, DLL_DIR, "dllloader.h")
    text = "".join(re.sub(r'#include "(?:\.\./)?dllloader\.h"', '#include "%s"' % loader, open(p).read())
                   for p in paths)
    source = '#include "%s"\n%s' % (os.path.join(tree, DLL_DIR, "dllloader.cc"), text)
    out = os.path.join(tmp, "%s-%s.o" % (name, target))
    args = [os.path.join(HOST, "bin", "clang"), "--target=" + target, "--sysroot=" + sysroot,
            "-resource-dir", os.path.join(sysroot, "lib", "clang"),
            "-idirafter", os.path.join(HOST, "lib", "clang", "22", "include")] + STUB_FLAGS + [
            "-I", os.path.join(tree, "include"), "-I", os.path.join(tree, "include_libc"), "-c", "-o", out, "-"]
    proc = subprocess.run(args, input=source, capture_output=True, text=True)
    if proc.returncode != 0:
        return None, proc.stderr
    return out, None


# A wrapper in the form xlkdl writes.
STUB = re.compile(r"""^(?:WINAPI\s+)?[^{};#]*?\b(?P<name>\w+)\s*\([^{};]*\)\s*\{\s*
    auto\s+loader\s*=\s*sprt::DllLoader::get\(\);\s*
    (?:return\s+)?DLL_LOAD_AND_CALL\([^;]*\);\s*\}\n?""", re.M | re.X | re.S)


def subset(path, names, tmp):
    """The includes of a wrapper file and those of its wrappers that are named, as a file of its own."""
    text = open(path).read()
    head = "".join(l + "\n" for l in text.split("\n") if l.startswith("#include"))
    body = "".join(m.group(0) + "\n" for m in STUB.finditer(text) if m.group("name") in names)
    out = os.path.join(tmp, "subset-" + os.path.basename(path))
    open(out, "w").write(head + 'extern "C" {\n' + body + "}\n")
    return out


def functions_of_object(obj, names=None):
    """The disassembly of each named function, addresses and relocation targets kept as names."""
    dump = subprocess.run([os.path.join(HOST, "bin", "llvm-objdump"), "-d", "-r", "--no-show-raw-insn",
                           "--no-addresses", obj], check=True, capture_output=True, text=True).stdout
    out = {}
    current = None
    for line in dump.splitlines():
        m = re.match(r"^<(.+)>:$", line.strip())
        if m:
            current = m.group(1)
            out[current] = []
            continue
        if current is not None and line.strip():
            # Where a function stands in its section is no part of it: a target is kept relative to
            # its symbol, an address comment goes.
            text = re.sub(r"\s*# 0x[0-9a-f]+ <[^>]*>$", "", line.strip())
            text = re.sub(r"\b0x[0-9a-f]+ (<[^>]*>)", r"\1", text)
            # An arm64 page address is named after whatever the page holds; its relocation says what.
            text = re.sub(r"^(adrp\s+\w+,\s*)<[^>]*>", r"\1<page>", text)
            out[current].append(re.sub(r"\s+", " ", text))
    return {k: v for k, v in out.items() if names is None or k in names}


def run_dlls(old, new, verbose, quiet=False, only=None):
    failures = 0
    counts = {}
    broken = set()
    tmp = tempfile.mkdtemp(prefix="compare-api-")
    olddefs = os.path.join(old, "include", "sprt", "wrappers", "windows", "def")
    try:
        for dll in sorted(f[:-4] for f in os.listdir(olddefs) if f.endswith(".def") and (only is None or f[:-4] in only)):
            problems = []
            olddef = os.path.join(olddefs, dll + ".def")
            newdef = os.path.join(new, "include", "sprt", "gen", "wrappers", "windows", "def", dll + ".def")
            if not os.path.exists(newdef):
                problems.append("    .def: not generated")
            else:
                a, b = read_def(olddef), read_def(newdef)
                if a != b:
                    problems.append("    .def: %s %s, generated %s %s" % (a[0], sorted(a[1] - b[1])[:8], b[0],
                                                                         sorted(b[1] - a[1])[:8]))
                if a[1]:
                    for machine in ("x64", "arm64"):
                        if import_symbols(olddef, machine, tmp) != import_symbols(newdef, machine, tmp):
                            problems.append("    import library (%s): the symbols differ" % machine)
            oldtable = os.path.join(old, DLL_DIR, "dll_%s.h" % dll)
            newtable = os.path.join(new, DLL_DIR, "gen", "dll_%s.h" % dll)
            if os.path.exists(oldtable) != os.path.exists(newtable):
                problems.append("    table: %s" % ("not generated" if os.path.exists(oldtable) else "generated, and none before"))
            elif os.path.exists(oldtable):
                ta, tb = read_table(oldtable), read_table(newtable)
                if ta != tb:
                    problems.append("    table: %s, generated %s" % ((ta[0], len(ta[1]), ta[2]), (tb[0], len(tb[1]), tb[2])))
                    missing = sorted(set(ta[1]) ^ set(tb[1]))[:8]
                    if missing:
                        problems.append("      exports not in both: %s" % missing)
            oldstubs = os.path.join(old, DLL_DIR, "dllstub2_%s.cc" % dll)
            newstubs = [p for p in (os.path.join(new, DLL_DIR, "gen", "dllstub2_%s.cc" % dll),
                                    os.path.join(new, DLL_DIR, "dllstub_%s.cc" % dll)) if os.path.exists(p)]
            if os.path.exists(oldstubs):
                for target in ("x86_64-pc-windows-msvc", "aarch64-pc-windows-msvc"):
                    oa, err = stub_object(new, [oldstubs], dll + "-old", target, tmp)
                    olds, news = [oldstubs], newstubs
                    if oa is None:
                        # A file that never compiled has to fail as it is now too; its generated
                        # wrappers are compared with the ones they were written from.
                        if stub_object(new, newstubs, dll + "-new", target, tmp)[0] is not None:
                            problems.append("    wrappers (%s): compile now, and did not before" % target)
                            continue
                        broken.add(dll)
                        gen = newstubs[0]
                        names = set(m.group("name") for m in STUB.finditer(open(gen).read()))
                        olds, news = [subset(oldstubs, names, tmp)], [gen]
                        oa, err = stub_object(new, olds, dll + "-old", target, tmp)
                        if oa is None:
                            problems.append("    wrappers (%s): the original wrappers do not compile: %s" % (
                                target, err.strip()[:400]))
                            continue
                    ob, err = stub_object(new, news, dll + "-new", target, tmp)
                    if ob is None:
                        problems.append("    wrappers (%s): the generated do not compile: %s" % (target, err.strip()[:600]))
                        continue
                    fa, fb = functions_of_object(oa), functions_of_object(ob)
                    counts[dll] = len(fa)
                    if fa != fb:
                        diff = sorted(k for k in set(fa) | set(fb) if fa.get(k) != fb.get(k))
                        problems.append("    wrappers (%s): %d differ: %s" % (target, len(diff), ", ".join(diff[:6])))
            elif newstubs:
                problems.append("    wrappers: generated, and none before")
            failures += 1 if problems else 0
            if not quiet and (problems or verbose):
                print("%s.dll: %s" % (dll, "identical" if not problems else "DIFFERS"))
                print("\n".join(problems)) if problems else None
        if not quiet:
            print("DLLs: %d, %s; wrappers compiled and identical for %d DLLs, %d functions" % (
                len(os.listdir(olddefs)), "all identical: .def, import libraries, tables" if not failures
                else "%d DIFFER" % failures, len(counts), sum(counts.values())))
            for dll in sorted(broken):
                print("  %s: the hand-written wrappers do not compile, before and after; the generated ones"
                      " are compared with the ones they were written from" % dll)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    return failures


def extract(rev, out):
    archive = subprocess.run(["git", "-C", REPO, "archive", rev, "runtime/include", "runtime/include_libc",
                              "runtime/libc_impl"], stdout=subprocess.PIPE, check=True)
    subprocess.run(["tar", "-x", "-C", out], input=archive.stdout, check=True)
    return os.path.join(out, "runtime")


def generate(xlkdl, kdl_dir, root):
    proc = subprocess.run([xlkdl, "api", "--map", os.path.join(kdl_dir, "api", "sprt-api.kdl"), "--root", root],
                          capture_output=True, text=True)
    return proc.returncode, proc.stdout + proc.stderr


def copy_tree(dst):
    for part in ("include", "include_libc", "libc_impl"):
        shutil.copytree(os.path.join(RUNTIME, part), os.path.join(dst, part), symlinks=True)
    shutil.copytree(os.path.join(RUNTIME, "kdl"), os.path.join(dst, "kdl"))
    return dst


def edit(path, old, new):
    text = open(path).read()
    assert old in text, (path, old)
    open(path, "w").write(text.replace(old, new, 1))


# Each break edits the copied descriptions; what it breaks has to show as a difference.
BREAKS = [
    ("a parameter type", [("api/windows/bcrypt.kdl", "    cbBuffer ULONG\n", "    cbBuffer USHORT\n")]),
    ("a calling convention", [("api/windows/bcrypt.kdl", "defaults dll=bcrypt link=import cc=WINAPI",
                               "defaults dll=bcrypt link=import")]),
    ("the DLL of a wrapper", [("api/windows/bcrypt.kdl", "func BCryptDestroyKey NTSTATUS {",
                               "func BCryptDestroyKey NTSTATUS dll=dwmapi {")]),
    ("an export", [("api/windows/bcrypt.kdl", "func BCryptDecrypt NTSTATUS {", "func BCryptDecrypt NTSTATUS def=#false {")]),
    ("the guard of the umbrella bodies", [("api/sprt-api.kdl", " body-if=SPRT_UMBRELLA_REQUIRED", "")]),
    ("an export of a DLL", [("api/windows/bcrypt.kdl", "    export GetCipherInterface\n", "")]),
]


# The units a break of the descriptions above can show in.
BREAK_DLLS = {"bcrypt", "dwmapi"}
BREAK_UNITS = {"windows bcrypt.h", "windows user_api.h", "windows windows.h", "sprt dirent", "umbrella dirent"}


def self_test(xlkdl, old, jobs):
    caught = 0
    for name, edits in BREAKS:
        tmp = tempfile.mkdtemp(prefix="compare-api-break-")
        try:
            tree = copy_tree(tmp)
            for rel, a, b in edits:
                edit(os.path.join(tree, "kdl", rel), a, b)
            rc, out = generate(xlkdl, os.path.join(tree, "kdl"), tree)
            if rc != 0:
                print("  break '%s': xlkdl refused it: %s" % (name, out.strip().splitlines()[0]))
                caught += 1
                continue
            n = run_headers(old, tree, jobs, False, quiet=True, only=BREAK_UNITS) + run_dlls(old, tree, False, quiet=True, only=BREAK_DLLS)
            print("  break '%s': %s" % (name, "caught (%d)" % n if n else "NOT CAUGHT"))
            caught += 1 if n else 0
        finally:
            shutil.rmtree(tmp, ignore_errors=True)
    return len(BREAKS) - caught


FIXTURE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fixtures", "api")

# Each refusal edits a copy of the fixture; xlkdl has to fail with the message, at a file:line.
REFUSALS = [
    ("a function of no DLL", "api/win.kdl", "defaults dll=fx", "defaults dll=fy", "no DLL 'fy'"),
    ("an umbrella without a public name", "api.kdl", "    type DIR\n", "", "'DIR' has no public name"),
    ("a variadic wrapper", "api/win.kdl", "func Print int stub=manual", "func Print int",
     "a variadic function has no generated wrapper"),
    ("a variadic umbrella", "api/lib.kdl", "func logf int umbrella=#false", "func logf int",
     "a variadic function has no umbrella"),
    ("an unknown linkage", "api/lib.kdl", "defaults link=sprt", "defaults link=spr", "no linkage 'spr'"),
    ("unicode= out of charset=", "api/win.kdl", "charset=W unicode=W", "charset=W unicode=A",
     "unicode= names A, which charset= does not"),
    ("a function that is an export too", "api/win.kdl", "    export Hidden\n", "    export Hidden\n    export Close\n",
     "Close is a function of api/win.kdl"),
    ("a preload that is no export", "api/win.kdl", "preload Open GetTick", "preload Open Missing",
     "the preload Missing is neither a function nor an export"),
    ("a DLL described twice", "api/lib.kdl", "defaults link=sprt", "dll fx file=\"fx.dll\" table=FxTable\ndefaults link=sprt",
     "DLL fx is already described in api/win.kdl"),
]


def fixtures(xlkdl, update):
    failures = 0
    tmp = tempfile.mkdtemp(prefix="compare-api-fixture-")
    try:
        out = os.path.join(tmp, "out")
        proc = subprocess.run([xlkdl, "api", "--map", os.path.join(FIXTURE, "api.kdl"), "--root", out],
                              capture_output=True, text=True)
        if proc.returncode != 0:
            print("fixture: xlkdl failed:\n    " + (proc.stdout + proc.stderr).strip())
            return 1
        expected = os.path.join(FIXTURE, "expected")
        if update:
            shutil.rmtree(expected, ignore_errors=True)
            shutil.copytree(out, expected)
        got = {os.path.relpath(os.path.join(d, f), out) for d, _, fs in os.walk(out) for f in fs}
        want = {os.path.relpath(os.path.join(d, f), expected) for d, _, fs in os.walk(expected) for f in fs}
        bad = sorted(got ^ want) + sorted(f for f in got & want if open(os.path.join(out, f)).read()
                                          != open(os.path.join(expected, f)).read())
        failures += len(bad)
        print("fixture: %d files %s" % (len(got), "as expected" if not bad else "DIFFER: " + ", ".join(bad)))
        refused = 0
        for name, rel, old, new, message in REFUSALS:
            copy = os.path.join(tmp, "refusal")
            shutil.rmtree(copy, ignore_errors=True)
            shutil.copytree(FIXTURE, copy, ignore=shutil.ignore_patterns("expected"))
            edit(os.path.join(copy, rel), old, new)
            proc = subprocess.run([xlkdl, "api", "--map", os.path.join(copy, "api.kdl"), "--root",
                                   os.path.join(tmp, "refused")], capture_output=True, text=True)
            text = proc.stdout + proc.stderr
            placed = re.search(r"\.kdl:\d+:\d+: .*" + re.escape(message), text)
            if proc.returncode != 0 and placed:
                refused += 1
            else:
                print("  refusal '%s': %s" % (name, "accepted" if proc.returncode == 0 else
                                               "refused otherwise: " + text.strip().splitlines()[0]))
                failures += 1
        print("fixture: %d of %d broken copies refused at their place" % (refused, len(REFUSALS)))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    return failures


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--xlkdl", default=A.find_xlkdl())
    parser.add_argument("--rev", default=ORIGINALS)
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--update-fixtures", action="store_true")
    parser.add_argument("-j", type=int, default=os.cpu_count() or 4)
    parser.add_argument("-v", action="store_true")
    args = parser.parse_args()
    if not args.xlkdl or not os.path.exists(args.xlkdl):
        sys.exit("xlkdl is not built: make -C utils/xlkdl, or pass --xlkdl")

    failures = fixtures(args.xlkdl, args.update_fixtures)
    check = subprocess.run([args.xlkdl, "api", "--map", os.path.join(RUNTIME, API_MAP), "--root", RUNTIME, "--check"],
                           capture_output=True, text=True)
    if check.returncode != 0:
        print("the generated files are not what runtime/kdl/api generates:")
        print("\n".join("    " + l for l in (check.stdout + check.stderr).strip().splitlines()[:20]))
        failures += 1
    else:
        print("committed: the generated files are what runtime/kdl/api generates")

    tmp = tempfile.mkdtemp(prefix="compare-api-orig-")
    try:
        old = extract(args.rev, tmp)
        failures += run_headers(old, RUNTIME, args.j, args.v)
        failures += run_dlls(old, RUNTIME, args.v)
        if args.self_test:
            print("self-test:")
            failures += self_test(args.xlkdl, old, args.j)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    print("total failures: %d" % failures)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
