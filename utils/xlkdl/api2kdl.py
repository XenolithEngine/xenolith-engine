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

"""Function descriptions of runtime/kdl/api from the hand-written runtime of a git revision.

Windows: every hand-written header of sprt/wrappers/windows with prototypes is a set. Its
functions, and the function-pointer typedefs among them, go to runtime/kdl/api/windows/<name>.kdl
and from there to sprt/gen/wrappers/windows/api/<name>.h. The DLL of a function is the one whose
.def lists it; a name in two .def files belongs to the DLL with its wrapper, else with it in its
table, and is a bare export of the other. Each DLL of the .def files is a `dll` node of the first
description with one of its functions: its file, table, preloads, and the exports of its table
and .def no prototype declares. A wrapper of the generated form is generated; the rest of its
dllstub2_<dll>.cc stays by hand as dllstub_<dll>.cc.

libc: the pairs of SETS, an API header and the umbrella of include_libc.

It writes the descriptions, the map parts, the hand-written headers with their functions replaced
by an include of the generated header, the loader and the startup unit with the generated tables
and wrappers, and removes the .def, table and wrapper files `xlkdl api` writes now. Names are
written back as the ABI map spells them (`__SPRT_ID(DIR)` is DIR), from `xlkdl abi --index`. What
it does not recognize stops it with file:line.
"""

import argparse
import json
import os
import re
import subprocess
import sys
import textwrap

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import c2kdl as C  # noqa: E402

REPO = C.REPO
RUNTIME = os.path.join(REPO, "runtime")
ORIGINALS = "814d1c4f"
OUT = os.path.join(RUNTIME, "kdl", "api")
ABI_MAP = os.path.join(RUNTIME, "kdl", "abi", "sprt.kdl")
XLKDL = os.path.join(REPO, "utils", "xlkdl", "stappler-build", "x86_64-unknown-linux-gnu", "debug", "cc", "xlkdl")

DLL_DIR = "libc_impl/src/windows/dll"
DEF_DIR = "include/sprt/wrappers/windows/def"
WIN_DIR = "include/sprt/wrappers/windows"
STARTUP = "libc_impl/src/builtin_startup.cpp"
LOADER = "libc_impl/src/windows/dll/dllloader.h"
PRELOAD_HELPERS = "libc_impl/src/windows/dll/dll_ntdll_preload.h"

# The hand-written headers of the wrapper whose functions stay as written: compiler intrinsics,
# inline implementations on the runtime's own threads, and the configuration that declares the
# ntdll string imports (included by every header, so it cannot include a generated one).
WIN_SKIP = {"__sprt_intrin.h", "__sprt_threads.h", "__sprt_config.h"}

# The libc pairs: an API header, the includes of its generated header, and its umbrella.
SETS = [
    dict(api="libc/dirent.kdl", header="sprt/gen/c/api/__sprt_dirent.h",
         source="include/sprt/c/__sprt_dirent.h",
         includes=["sprt/c/bits/__sprt_def.h", "sprt/gen/c/bits/__sprt_ssize_t.h",
                   "sprt/gen/c/cross/__sprt_fstypes.h", "sprt/gen/c/cross/__sprt_dir_ptr.h"],
         part="libc",
         umbrella=dict(header="sprt/gen/c/umbrella/dirent.h", source="include_libc/dirent.h",
                       path="dirent.h", includer="sprt/c/__sprt_dirent.h")),
]

# Implied by the profile: where a description is found from its header.
API_ROOTS = [("sprt/gen/wrappers/windows/api/", "windows/"), ("sprt/gen/c/api/__sprt_", "libc/")]

LINKAGE = {"__SPRT_WIN_IMPORT": "import", "SPRT_API": "sprt"}
CALLING = {"WINAPI": "WINAPI", "__SPRT_WINAPI": "WINAPI", "CALLBACK": "CALLBACK", "NTAPI": "NTAPI",
           "APIENTRY": "APIENTRY", "WSAAPI": "WSAAPI", "__stdcall": "__stdcall", "__cdecl": "__cdecl"}
ATTRIBUTES = {"__declspec", "__attribute__", "__attribute"}
ATTRIBUTE_MACROS = {"__SPRT_NORETURN", "SPRT_NORETURN", "DECLSPEC_NORETURN", "__SPRT_NOINLINE"}
FUNC_RULE = {"libc": "id"}

LICENSE = open(os.path.join(RUNTIME, "kdl", "abi", "LICENSE.txt")).read()

class ConvertError(Exception):
    pass


def fail(path, line, message):
    raise ConvertError("%s:%d: %s" % (path, line, message))


def git_show(rev, path):
    try:
        return subprocess.run(["git", "-C", REPO, "show", "%s:runtime/%s" % (rev, path)],
                              check=True, capture_output=True, text=True).stdout
    except subprocess.CalledProcessError:
        return None


class Spelling:
    """C spellings of the ABI map's names: `__SPRT_ID(DIR)` and `__sprt_DIR` are DIR with rule id."""

    def __init__(self):
        out = subprocess.run([XLKDL, "abi", "--map", ABI_MAP, "--index"], check=True,
                             capture_output=True, text=True).stdout
        self.names = {}
        for d in json.loads(out):
            rule = d.get("rule")
            if not rule:
                continue
            for c in d.get("c", []):
                self.names.setdefault(c, (rule, d["name"]))

    def type_text(self, path, line, toks):
        """A type as the description writes it: names with a rule written bare."""
        out = []
        i = 0
        while i < len(toks):
            t = toks[i]
            if (t.text == "__SPRT_ID" and i + 3 < len(toks) and toks[i + 1].text == "("
                    and toks[i + 3].text == ")"):
                spelled = "__SPRT_ID(%s)" % toks[i + 2].text
                if spelled not in self.names:
                    fail(path, line, "%s is no name of the ABI map" % spelled)
                out.append(C.Tok(t.space, self.names[spelled][1], "id", t.line))
                i += 4
                continue
            if t.kind == "id" and t.text.startswith("__SPRT_") and t.text in self.names:
                out.append(C.Tok(t.space, self.names[t.text][1], "id", t.line))
            elif t.kind == "id" and t.text.startswith("__SPRT_"):
                fail(path, line, "%s is no name of the ABI map" % t.text)
            else:
                out.append(t)
            i += 1
        return normalize(out)


def normalize(toks):
    """Tokens as one line: a space between words, none inside brackets or before a comma."""
    out = ""
    prev = None
    for t in toks:
        if prev is not None:
            glue = not (t.text in (",", ")", "]") or prev.text in ("(", "[")
                        or (t.text == "(" and prev.text in ("*", ")")) or (t.text == "[" and prev.kind == "id")
                        or (prev.text == "*" and t.text in ("*", ")")))
            if glue and (t.space or prev.kind == "id" and t.kind == "id" or prev.text == ","
                         or t.text == "*" or prev.text == "*" and t.kind == "id"):
                out += " "
        out += t.text
        prev = t
    return out


def split_top(toks, sep=","):
    parts, cur, depth = [], [], 0
    for t in toks:
        if t.text in ("(", "["):
            depth += 1
        elif t.text in (")", "]"):
            depth -= 1
        if t.text == sep and depth == 0:
            parts.append(cur)
            cur = []
            continue
        cur.append(t)
    if cur or parts:
        parts.append(cur)
    return parts


class Func:
    def __init__(self, line):
        self.kind = "func"
        self.line = line
        self.end = line
        self.cond = ""
        self.name = None
        self.ret = None
        self.params = []        # (name or None, type) or ("...", None)
        self.empty = None
        self.link = None
        self.cc = None
        self.attr = None
        self.rule = None
        self.comments = []
        self.dll = None
        self.stub = None
        self.def_ = True
        self.table = True
        self.redeclare = False
        self.umbrella = True
        self.nothrow = True


class Typedef:
    """A typedef among the prototypes: it moves with them, the functions after it name it."""

    def __init__(self, line, end, cond, name, type_):
        self.kind = "typedef"
        self.line = line
        self.end = end
        self.cond = cond
        self.name = name
        self.type = type_
        self.comments = []


class Statement:
    def __init__(self, first, last, text, cond, outer):
        self.first = first
        self.last = last
        self.text = text
        self.cond = cond
        # The line of the outermost #if the statement stands in (0 when none).
        self.outer = outer


def include_guard(lines):
    directives = [(i + 1, l.strip()) for i, l in enumerate(lines) if l.strip().startswith("#")]
    if len(directives) >= 2:
        m1 = re.match(r"^#\s*ifndef\s+(\w+)\s*$", directives[0][1])
        m2 = re.match(r"^#\s*define\s+(\w+)\s*$", directives[1][1])
        if m1 and m2 and m1.group(1) == m2.group(1):
            return m1.group(1)
    return None


def statements(code):
    """Top-level statements outside preprocessor lines, each with the condition it stands under.
    Region lines - a macro alone on its line (__SPRT_BEGIN_DECL), `extern "C" {` and its `}` - are
    no statements."""
    lines = code.split("\n")
    guard = include_guard(lines)
    guard_pending = guard is not None
    stack = []
    out = []
    cur = []
    first = None
    depth = 0
    cond = outer = None

    def condition():
        return C.conjunction([lv.branch() for lv in stack if not lv.guard])

    for no, line in enumerate(lines, 1):
        s = line.strip()
        if s.startswith("#"):
            d = s[1:].strip()
            word = re.match(r"^(\w*)", d).group(1)
            rest = d[len(word):].strip()
            if word in ("if", "ifdef", "ifndef"):
                if guard_pending and word == "ifndef" and rest == guard:
                    lv = C.Level(None, guard=True)
                    guard_pending = False
                else:
                    expr = ("defined(%s)" % rest if word == "ifdef" else "!defined(%s)" % rest
                            if word == "ifndef" else C.normalize_defined(rest))
                    lv = C.Level(expr)
                lv.line = no
                stack.append(lv)
            elif word == "elif":
                stack[-1].previous.append(stack[-1].current)
                stack[-1].current = C.normalize_defined(rest)
            elif word == "else":
                stack[-1].previous.append(stack[-1].current)
                stack[-1].current = None
            elif word == "endif":
                stack.pop()
            continue
        if not cur and (not s or re.fullmatch(r"\w+", s) or s in ('extern "C" {', "}", '} // extern "C"')):
            continue
        if first is None:
            first = no
            cond = condition()
            opened = [lv.line for lv in stack if not lv.guard]
            outer = opened[0] if opened else 0
        cur.append(line)
        depth += line.count("{") - line.count("}")
        if (s.endswith(";") or s.endswith("}")) and depth == 0:
            out.append(Statement(first, no, "\n".join(cur), cond, outer))
            cur, first = [], None
    return out


def take_attributes(toks):
    """An attribute group at the front of the tokens (`__declspec(noreturn)`), and the rest."""
    if len(toks) < 2 or toks[0].text not in ATTRIBUTES or toks[1].text != "(":
        return None, toks
    depth = 0
    for i in range(1, len(toks)):
        if toks[i].text == "(":
            depth += 1
        elif toks[i].text == ")":
            depth -= 1
            if depth == 0:
                return C.join(toks[:i + 1]).replace(" (", "(").replace("( ", "(").replace(" )", ")"), toks[i + 1:]
    return None, toks


def unconditional_includes(text):
    """The `#include <…>` of a header outside any #if but its guard, each once."""
    lines = C.strip_comments(text)[0].split("\n")
    guard = include_guard(lines)
    depth = 0
    out = []
    for l in lines:
        s = l.strip()
        m = re.match(r"^#\s*(\w+)\s*(.*)$", s)
        if not m:
            continue
        word, rest = m.group(1), m.group(2).strip()
        if word in ("if", "ifdef", "ifndef"):
            if not (word == "ifndef" and rest == guard and depth == 0 and guard):
                depth += 1
            else:
                guard = None
        elif word == "endif":
            depth = max(0, depth - 1)
        elif word == "include" and depth == 0:
            inc = re.match(r"^<([^>]+)>", rest)
            if inc and inc.group(1) not in out:
                out.append(inc.group(1))
    return out


def parse_prototype(spell, path, first, last, text):
    """A prototype `[LINKAGE] [ATTR] [CC] RET NAME(PARAMS);`, or None when the statement is other."""
    toks = C.tokenize(text.strip().rstrip(";"), first)
    if not toks or toks[-1].text != ")" or toks[0].text in ("typedef", "extern", "static", "template",
                                                              "namespace", "using"):
        return None
    f = Func(first)
    f.end = last
    attrs = []
    while toks:
        if toks[0].text in LINKAGE:
            f.link = LINKAGE[toks.pop(0).text]
        elif toks[0].text in CALLING and f.cc is None:
            f.cc = CALLING[toks.pop(0).text]
        elif toks[0].text in ATTRIBUTE_MACROS:
            attrs.append(toks.pop(0).text)
        else:
            attr, rest = take_attributes(toks)
            if attr is None:
                break
            attrs.append(attr)
            toks = rest
    f.attr = " ".join(attrs) or None
    # The parameter list is the last parenthesis group; the name stands right before it.
    depth = 0
    open_at = None
    for i in range(len(toks) - 1, -1, -1):
        if toks[i].text == ")":
            depth += 1
        elif toks[i].text == "(":
            depth -= 1
            if depth == 0:
                open_at = i
                break
    if open_at is None or open_at == 0:
        return None
    head = toks[:open_at]
    params = toks[open_at + 1:-1]
    if (len(head) >= 4 and head[-1].text == ")" and head[-4].text == "__SPRT_ID"
            and head[-3].text == "("):
        f.name = head[-2].text
        f.rule = "id"
        head = head[:-4]
    elif head and head[-1].kind == "id" and len(head) > 1:
        f.name = head[-1].text
        head = head[:-1]
    else:
        # A call of a macro (`__SPRT_DEF_LDAR(…)`), not a declaration.
        return None
    # A calling convention after the return type stays there, as part of it.
    f.ret = spell.type_text(path, first, head)
    if "(" in f.ret:
        fail(path, first, "a declarator this converter does not split: '%s'" % text.strip())
    parts = split_top(params)
    if len(parts) == 1 and len(parts[0]) == 1 and parts[0][0].text in ("void", "VOID"):
        f.empty = parts[0][0].text
        parts = []
    for p in parts:
        if not p:
            fail(path, first, "an empty parameter in '%s'" % text.strip())
        if len(p) == 1 and p[0].text == "...":
            f.params.append(("...", None))
            continue
        name = None
        # `T (CC *name)(...)`: the name is inside the first group.
        for i in range(len(p) - 2):
            if p[i].text == "(":
                j = i + 1
                while j < len(p) and p[j].text in CALLING:
                    j += 1
                if j + 2 < len(p) and p[j].text == "*" and p[j + 1].kind == "id" and p[j + 2].text == ")":
                    name = p[j + 1].text
                    p = p[:j + 1] + p[j + 2:]
                    break
        else:
            # `T name[N]`: the name is before the brackets.
            bracket = next((k for k, t in enumerate(p) if t.text == "["), len(p))
            if 1 < bracket < len(p) and p[bracket - 1].kind == "id":
                name = p[bracket - 1].text
                p = p[:bracket - 1] + p[bracket:]
            elif len(p) > 1 and p[-1].kind == "id" and p[-2].text not in ("struct", "union", "enum") \
                    and p[-1].text not in ("const", "volatile"):
                name = p[-1].text
                p = p[:-1]
        f.params.append((name, spell.type_text(path, first, p)))
    return f


def parse_typedef(spell, path, st):
    """`typedef RET (CC *NAME)(PARAMS);` or `typedef TYPE NAME;`, as a name and a type-id."""
    toks = C.tokenize(st.text.strip().rstrip(";"), st.first)
    if not toks or toks[0].text != "typedef" or any(t.text in ("{", ",") and False for t in toks):
        return None
    toks = toks[1:]
    if any(t.text == "{" for t in toks):
        return None
    for i in range(len(toks) - 2):
        if toks[i].text == "(":
            j = i + 1
            while j < len(toks) and toks[j].text in CALLING:
                j += 1
            if j + 2 < len(toks) and toks[j].text == "*" and toks[j + 1].kind == "id" and toks[j + 2].text == ")":
                return Typedef(st.first, st.last, st.cond, toks[j + 1].text,
                               spell.type_text(path, st.first, toks[:j + 1] + toks[j + 2:]))
    if len(toks) >= 2 and toks[-1].kind == "id" and not any(t.text in ("(", ",", "[") for t in toks):
        return Typedef(st.first, st.last, st.cond, toks[-1].text, spell.type_text(path, st.first, toks[:-1]))
    fail(path, st.first, "a typedef this converter does not split: '%s'" % st.text.strip())


def comments_before(comments, line):
    """The comments that end right above a line, with no blank line between: their text lines, and
    the line the first of them starts on (the line itself when there are none)."""
    taken = []
    for start, end, body in reversed(comments):
        if end == line - 1:
            taken.insert(0, body)
            line = start
        elif end < line - 1:
            break
    text = []
    for body in taken:
        text += [x.strip().lstrip("*").strip() if x.strip().startswith("*") else x.strip()
                 for x in body.strip("\n").split("\n")]
    return text, line


def header_items(spell, path, text, report, move_typedefs=True):
    """The prototypes of a header, and the typedefs among them, in order. A statement that looks
    like a prototype and is none goes to the report."""
    code, comments = C.strip_comments(text)
    sts = statements(code)
    items = []
    for st in sts:
        f = parse_prototype(spell, path, st.first, st.last, st.text)
        if f is None:
            flat = " ".join(st.text.split())
            if re.search(r"\)\s*(\w+\s*)*;$", flat) and not flat.startswith(("typedef", "extern ", "#")) \
                    and "(" in flat and "{" not in flat:
                report.append("%s:%d: left as written: %s" % (path, st.first, flat[:120]))
            continue
        f.cond = st.cond
        f.outer = st.outer
        f.comments, f.top = comments_before(comments, st.first)
        items.append(f)
    if move_typedefs and items:
        lo, hi = items[0].line, items[-1].line
        for st in sts:
            if lo < st.first < hi and st.text.strip().startswith("typedef"):
                t = parse_typedef(spell, path, st)
                if t is not None:
                    t.comments, t.top = comments_before(comments, st.first)
                    t.outer = st.outer
                    items.append(t)
        items.sort(key=lambda x: x.line)
    return items


'''+'''
UMBRELLA = re.compile(r"""^SPRT_UMBRELLA_FUNC\s*\n(?P<decl>.*?)\s*(?P<end>SPRT_UMBRELLA_END(?:_EXCEPT)?)\s*\n
    \#if\s+SPRT_UMBRELLA_REQUIRED\s*\n\{\s*(?P<body>.*?)\s*\}\s*\n\#endif""", re.S | re.M | re.X)


def umbrella_functions(spell, path, text):
    code, _ = C.strip_comments(text)
    out = {}
    for m in UMBRELLA.finditer(code):
        line = code.count("\n", 0, m.start()) + 1
        f = parse_prototype(spell, path, line, line, m.group("decl") + ";")
        if f is None:
            fail(path, line, "an umbrella that is not a function")
        body = re.match(r"^(?:return\s+)?(__sprt_\w+|__SPRT_ID\(\s*\w+\s*\))\s*\((.*)\)\s*;$", m.group("body").strip(), re.S)
        if not body:
            fail(path, line, "an umbrella body that is not a call of the sprt function")
        callee = re.sub(r"^__SPRT_ID\(\s*(\w+)\s*\)$", r"__sprt_\1", body.group(1))
        args = [a.strip() for a in body.group(2).split(",") if a.strip()]
        f.callee = callee
        f.args = args
        f.nothrow = m.group("end") == "SPRT_UMBRELLA_END"
        f.span = (m.start(), m.end())
        out[f.name] = f
    return out


def git_ls(rev, path):
    proc = subprocess.run(["git", "-C", REPO, "ls-tree", "--name-only", "%s:runtime/%s" % (rev, path)],
                          capture_output=True, text=True)
    return proc.stdout.split() if proc.returncode == 0 else []


def read_defs(rev):
    """Each .def: (its comment lines, its LIBRARY, its exports in order)."""
    out = {}
    for name in git_ls(rev, DEF_DIR):
        if not name.endswith(".def"):
            continue
        text = git_show(rev, "%s/%s" % (DEF_DIR, name))
        comments, names, lib, exports = [], [], None, False
        for line in text.split("\n"):
            if line.strip().startswith(";"):
                comments.append(line.strip()[1:].strip())
            s = line.split(";")[0].strip()
            if s.startswith("LIBRARY"):
                lib = s.split()[1]
            elif s == "EXPORTS":
                exports = True
            elif exports and s:
                names.append(s.split()[0])
        out[name[:-4]] = dict(comments=comments, file=lib, exports=names)
    return out


TABLE = re.compile(r"struct (\w+) : DllTable \{\s*\w+\(\) : DllTable\(L\"([^\"]+)\", &__\w+_begin, &__\w+_end(, __preloads)?\)")


def read_table(rev, dll):
    path = "%s/dll_%s.h" % (DLL_DIR, dll)
    text = git_show(rev, path)
    if text is None:
        return None
    m = TABLE.search(text)
    if not m:
        fail(path, 1, "not a table this converter reads")
    exports = re.findall(r"SPWIN_DEFINE_PROTO\((\w+)\)", text)
    preloads, preload_list = [], None
    lm = re.search(r"#define (\w+)\(X\)((?:[^\n]*\\\n)+[^\n]*)", text)
    if lm:
        preload_list = lm.group(1)
        preloads = re.findall(r"X\((\w+)\)", lm.group(2))
    elif m.group(3):
        block = re.search(r"__preloads\[\d+\] = \{(.*?)\};", text, re.S)
        preloads = re.findall(r"&(\w+),", block.group(1))
    return dict(table=m.group(1), file=m.group(2), exports=exports, preloads=preloads,
                preload_list=preload_list, text=text)


STUB = re.compile(r"""^(?:WINAPI\s+)?(?P<decl>[^{};#]*?\b(?P<name>\w+)\s*\([^{};]*\))\s*\{\s*
    auto\s+loader\s*=\s*sprt::DllLoader::get\(\);\s*
    (?:return\s+)?DLL_LOAD_AND_CALL\(\s*loader\s*,\s*(?P<dll>\w+)\s*,\s*(?P<fn>\w+)(?P<args>[^;]*)\);\s*\}\n?""",
                  re.M | re.X | re.S)


def read_stubs(text, dll):
    """The wrappers of a file in the form xlkdl writes: name -> (arguments, span)."""
    out = {}
    for m in STUB.finditer(text):
        if m.group("dll") != dll or m.group("fn") != m.group("name"):
            continue
        out[m.group("name")] = ([a.strip() for a in m.group("args").split(",") if a.strip()], m.span())
    return out


DEFINITION = re.compile(r"^[^\n#;{}]*?\b(\w+)\s*\([^;{}]*\)\s*(?:\w+\s*)*\{", re.M)


def kdl_func(f, defaults, out):
    for c in f.comments:
        out.append(("// " + c).rstrip())
    props = []
    if f.cond:
        props.append("if=%s" % C.kdl_quoted(f.cond))
    for key, value in (("link", f.link or "none"), ("cc", f.cc), ("dll", f.dll)):
        # A redeclaration has no DLL of its own: it takes whatever the defaults say.
        if key == "dll" and f.redeclare:
            continue
        if value != defaults.get(key):
            props.append("%s=%s" % (key, C.kdl_string(value) if value else '""'))
    if f.attr:
        props.append("attr=%s" % C.kdl_quoted(f.attr))
    if f.redeclare:
        props.append("redeclare=#true")
    elif f.dll:
        if f.stub == "manual":
            props.append('stub="manual"')
        elif f.stub is False and f.table_dll:
            props.append("stub=#false")
        if not f.table and f.table_dll:
            props.append("table=#false")
    if not f.umbrella:
        props.append("umbrella=#false")
    if not f.nothrow:
        props.append("noexcept=#false")
    if f.empty:
        props.append("empty=%s" % C.kdl_string(f.empty))
    head = "func %s %s%s" % (C.kdl_string(f.name), C.kdl_string(f.ret), "".join(" " + p for p in props))
    params = []
    for pname, ptype in f.params:
        if pname == "...":
            params.append('"..."')
        else:
            params.append("%s %s" % (C.kdl_string(pname) if pname else "-", C.kdl_string(ptype)))
    if not params:
        out.append(head)
        return
    one = "%s { %s }" % (head, "; ".join(params))
    if len(one) <= 100:
        out.append(one)
        return
    out.append(head + " {")
    out.extend("    " + p for p in params)
    out.append("}")


def kdl_typedef(t, out):
    for c in t.comments:
        out.append(("// " + c).rstrip())
    out.append("typedef %s %s%s" % (C.kdl_string(t.name), C.kdl_quoted(t.type),
                                    " if=%s" % C.kdl_quoted(t.cond) if t.cond else ""))


def mode(values):
    """The most frequent value, the first of them on a tie."""
    counts = {}
    for v in values:
        counts[v] = counts.get(v, 0) + 1
    return max(counts, key=lambda v: (counts[v], -values.index(v))) if counts else None


def drop_empty_conditionals(lines):
    """Remove #if ... #endif groups left with nothing but blank lines in them."""
    def directive(ln):
        m = re.match(r"^\s*#\s*(\w+)", ln)
        return m.group(1) if m else None

    changed = True
    while changed:
        changed = False
        stack = []
        for i, ln in enumerate(lines):
            d = directive(ln)
            if d in ("if", "ifdef", "ifndef"):
                stack.append(i)
            elif d == "endif" and stack:
                start = stack.pop()
                if all(not x.strip() or directive(x) in ("elif", "else") for x in lines[start + 1:i]):
                    del lines[start:i + 1]
                    changed = True
                    break
    return lines


def drop_empty_regions(lines, include):
    """A __SPRT_BEGIN_DECL block, or a raw extern "C" block, that holds nothing (or only the
    include) goes, the include standing in its place."""
    text = "\n".join(lines)
    blank = r"(?:[ \t]*\n)*"
    inc = re.escape(include)
    for begin, end in ((r"__SPRT_BEGIN_DECL\n", r"__SPRT_END_DECL\n"),
                       (r"#ifdef __cplusplus\nextern \"C\" \{\n#endif\n", r"#ifdef __cplusplus\n\}(?: // extern \"C\")?\n#endif\n")):
        text = re.sub(r"(?m)^" + begin + blank + "(" + inc + r"\n)?" + blank + end,
                      lambda m: m.group(1) or "", text + "\n")[:-1]
    return text.split("\n")


def squeeze(lines):
    out = []
    for ln in lines:
        if not ln.strip() and len(out) >= 2 and not out[-1].strip() and not out[-2].strip():
            continue
        out.append(ln)
    return out


class Converter:
    def __init__(self, rev):
        self.rev = rev
        self.spell = Spelling()
        self.files = {}         # description, map part, under runtime/kdl/api
        self.sources = {}       # rewritten hand-written files, under runtime/
        self.removed = []       # files xlkdl api writes now, under runtime/
        self.report = []
        self.defs = read_defs(rev)
        self.abi_headers = self.abi_map_headers()

    def read(self, path):
        text = git_show(self.rev, path)
        if text is None:
            raise ConvertError("%s: not in %s" % (path, self.rev))
        return text

    @staticmethod
    def abi_map_headers():
        out = set()
        root = os.path.join(RUNTIME, "kdl", "abi")
        for f in [os.path.join(root, "sprt.kdl")] + [os.path.join(root, "map", x) for x in os.listdir(os.path.join(root, "map"))
                                                       if x.endswith(".kdl")]:
            out.update(re.findall(r'^header "([^"]+)"', open(f).read(), re.M))
        return out

    # --- the sets ---------------------------------------------------------------------------------

    def windows_sets(self):
        out = []
        for name in sorted(git_ls(self.rev, WIN_DIR)):
            if not name.endswith(".h") or name in WIN_SKIP:
                continue
            source = "%s/%s" % (WIN_DIR, name)
            text = self.read(source)
            items = header_items(self.spell, source, text, self.report)
            if not any(i.kind == "func" for i in items):
                continue
            base = name[:-2]
            includes = unconditional_includes(text)
            out.append(dict(api="windows/%s.kdl" % base, header="sprt/gen/wrappers/windows/api/%s.h" % base,
                            source=source, includes=includes, part="windows", items=items, text=text))
        return out

    def convert(self):
        sets = self.windows_sets()
        for s in SETS:
            s = dict(s)
            s["text"] = self.read(s["source"])
            s["items"] = header_items(self.spell, s["source"], s["text"], self.report, move_typedefs=False)
            sets.append(s)
        self.sets = sets
        seen = {}
        for s in sets:
            for f in s["items"]:
                if f.kind != "func":
                    continue
                if f.rule and FUNC_RULE.get(s["part"]) != f.rule:
                    fail(s["source"], f.line, "%s has the rule %s, the defaults another" % (f.name, f.rule))
                if f.name in seen:
                    f.redeclare = True
                    self.report.append("%s:%d: %s declared again (first in %s)" % (s["source"], f.line, f.name,
                                                                                    seen[f.name]))
                else:
                    seen[f.name] = s["source"]
            if "umbrella" in s:
                self.match_umbrella(s, [f for f in s["items"] if f.kind == "func"])
        self.assign_dlls(sets)
        self.convert_dlls(sets)
        for s in sets:
            self.files[s["api"]] = self.description(s)
            self.sources[s["source"]] = self.rewrite(s)
        for part in sorted(set(s["part"] for s in sets)):
            self.files["map/%s.kdl" % part] = self.map_part(part, [s for s in sets if s["part"] == part])
        self.wire_build()

    def match_umbrella(self, s, funcs):
        u = s["umbrella"]
        text = self.read(u["source"])
        umbrellas = umbrella_functions(self.spell, u["source"], text)
        by_name = {f.name: f for f in funcs}
        for name, uf in umbrellas.items():
            f = by_name.get(name)
            line = text.count("\n", 0, uf.span[0]) + 1
            if f is None:
                fail(u["source"], line, "the umbrella %s has no function in %s" % (name, s["source"]))
            if uf.callee != "__sprt_" + name:
                fail(u["source"], line, "the umbrella %s calls %s" % (name, uf.callee))
            names = [p[0] for p in uf.params]
            if uf.args != names:
                fail(u["source"], line, "the umbrella %s passes %s, not its parameters" % (name, uf.args))
            if uf.ret != f.ret or [p[1] for p in uf.params] != [p[1] for p in f.params]:
                fail(u["source"], line, "the umbrella %s has other types than %s: %s(%s) and %s(%s)" % (
                    name, s["source"], uf.ret, ", ".join(p[1] for p in uf.params), f.ret,
                    ", ".join(p[1] for p in f.params)))
            f.nothrow = uf.nothrow
        for f in funcs:
            if f.name not in umbrellas:
                f.umbrella = False
        u["text"] = text
        u["spans"] = [uf.span for uf in umbrellas.values()]

    # --- the DLLs ---------------------------------------------------------------------------------

    def assign_dlls(self, sets):
        """The DLL of each function: the one whose .def lists it, and of two the one with its wrapper,
        else with it in its table."""
        self.tables = {d: read_table(self.rev, d) for d in self.defs}
        self.stub_texts = {}
        defined = set()
        for name in git_ls(self.rev, DLL_DIR):
            if name.endswith(".cc"):
                text = C.strip_comments(git_show(self.rev, "%s/%s" % (DLL_DIR, name)))[0]
                defined.update(DEFINITION.findall(text))
                m = re.match(r"dllstub2_(\w+)\.cc$", name)
                if m:
                    self.stub_texts[m.group(1)] = git_show(self.rev, "%s/%s" % (DLL_DIR, name))
        self.stubs = {d: read_stubs(C.strip_comments(t)[0], d) for d, t in self.stub_texts.items()}
        by_def = {}
        for d, info in self.defs.items():
            for n in info["exports"]:
                by_def.setdefault(n, []).append(d)
        for s in sets:
            if s["part"] != "windows":
                continue
            for f in s["items"]:
                if f.kind != "func" or f.redeclare:
                    continue
                dlls = by_def.get(f.name, [])
                if len(dlls) > 1:
                    with_stub = [d for d in dlls if f.name in self.stubs.get(d, {})]
                    with_table = [d for d in dlls if self.tables[d] and f.name in self.tables[d]["exports"]]
                    choice = (with_stub or with_table or dlls)[0]
                    self.report.append("%s: in the .def of %s; the function of %s" % (f.name, ", ".join(dlls), choice))
                    dlls = [choice]
                if not dlls:
                    if f.link == "import":
                        self.report.append("%s:%d: %s is imported and in no .def" % (s["source"], f.line, f.name))
                    continue
                f.dll = dlls[0]
                table = self.tables[f.dll]
                f.table_dll = table is not None
                f.table = table is not None and f.name in table["exports"]
                stub = self.stubs.get(f.dll, {}).get(f.name)
                if stub and stub[0] == [p[0] for p in f.params] and f.table:
                    f.stub = "gen"
                elif f.name in defined:
                    f.stub = "manual"
                else:
                    f.stub = False
        for s in sets:
            for f in s["items"]:
                if f.kind == "func" and not hasattr(f, "table_dll"):
                    f.table_dll = False

    def convert_dlls(self, sets):
        """The dll node of each DLL, the hand-written rest of its wrappers, and what goes away."""
        funcs = {}
        for s in sets:
            for f in s["items"]:
                if f.kind == "func" and f.dll and not f.redeclare:
                    funcs.setdefault(f.dll, []).append(f)
        self.dll_nodes = {}
        self.manual_stubs = {}
        for dll in sorted(self.defs):
            info, table = self.defs[dll], self.tables[dll]
            names = set(f.name for f in funcs.get(dll, []))
            out = ["// %s" % c for c in info["comments"]]
            props = "dll %s file=%s" % (C.kdl_string(dll), C.kdl_quoted(info["file"]))
            if table:
                if table["file"].lower() != info["file"].lower():
                    fail("%s/dll_%s.h" % (DLL_DIR, dll), 1, "the table loads %s, the .def is of %s" % (
                        table["file"], info["file"]))
                props += " table=%s" % C.kdl_string(table["table"])
                if table["preload_list"]:
                    props += " preload-list=%s" % table["preload_list"]
            exports = []
            listed = set(info["exports"])
            if table:
                for e in table["exports"]:
                    if e not in names:
                        exports.append("export %s%s" % (C.kdl_string(e), " def=#true" if e in listed else ""))
            in_table = set(table["exports"]) if table else set()
            for e in info["exports"]:
                if e not in names and e not in in_table:
                    exports.append("export %s def=#true%s" % (C.kdl_string(e), " table=#false" if table else ""))
            body = []
            if table and table["preloads"]:
                body.append("preload %s" % " ".join(C.kdl_string(p) for p in table["preloads"]))
            body += exports
            if body:
                out.append(props + " {")
                out += ["    " + b for b in body]
                out.append("}")
            else:
                out.append(props)
            self.dll_nodes[dll] = out
            self.removed.append("%s/%s.def" % (DEF_DIR, dll))
            if table:
                self.removed.append("%s/dll_%s.h" % (DLL_DIR, dll))
            # The order of the .def is that of the descriptions.
            order = [f.name for f in funcs.get(dll, [])] + [e for e in info["exports"] if e not in names]
            if order != info["exports"]:
                self.report.append("%s.def: in the order of the descriptions" % dll)
            if dll in self.stub_texts:
                self.removed.append("%s/dllstub2_%s.cc" % (DLL_DIR, dll))
                self.split_stubs(dll, [f for f in funcs.get(dll, []) if f.stub == "gen"])
        if "ntdll" in self.tables and self.tables["ntdll"] and self.tables["ntdll"]["preload_list"]:
            self.preload_helpers(self.tables["ntdll"])

    def split_stubs(self, dll, generated):
        """The wrappers of a DLL the generator does not write stay in dllstub_<dll>.cc."""
        text = self.stub_texts[dll]
        spans = sorted(read_stubs(text, dll)[f.name][1] for f in generated)
        rest = []
        pos = 0
        for a, b in spans:
            rest.append(text[pos:a])
            pos = b
        rest.append(text[pos:])
        rest = "".join(rest)
        code = C.strip_comments(rest)[0]
        if not DEFINITION.search(code) and not re.search(r"^\s*[^#\s/{}][^\n]*;", code.split('extern "C" {', 1)[-1], re.M):
            return
        rest = re.sub(r'(extern "C" \{\n)\n+', r"\1", rest)
        rest = re.sub(r"\n{3,}", "\n\n", rest)
        self.manual_stubs[dll] = "%s/dllstub_%s.cc" % (DLL_DIR, dll)
        self.sources[self.manual_stubs[dll]] = rest

    def preload_helpers(self, table):
        """What the ntdll table held besides its records: the code that checks a raw call of a
        preloaded function, on the list the generated table now defines."""
        text = table["text"]
        start = text.index("namespace sprt {") + len("namespace sprt {\n")
        end = text.index("struct NtdllTable")
        body = text[start:end]
        body = re.sub(r"#define %s\(X\)(?:[^\n]*\\\n)+[^\n]*\n" % table["preload_list"], "", body)
        contract = (
            "// The ntdll functions DllTable::init() resolves eagerly are the `preload` of the ntdll DLL in\n"
            "// runtime/kdl/api; the generated table preloads them and defines them as %s.\n"
            "// The preload is mandatory: if any of these cannot be resolved, init() returns false,\n"
            "// DllLoader::load() returns LOADER_ERROR_FAIL_TO_PRELOAD, and mainCRTStartup aborts the process\n"
            "// before any code runs. Consequently these - and only these - functions may be invoked through\n"
            "// a raw `.fn` pointer without a runtime null check, which SPWIN_NTDLL_PRELOADED_FN() verifies at\n"
            "// compile time.\n" % table["preload_list"])
        body = re.sub(r"// ── ntdll preload contract.*?drift apart\.\n", lambda m: contract, body, flags=re.S)
        guard = "CORE_RUNTIME_LIBC_IMPL_SRC_WINDOWS_DLL_DLL_NTDLL_PRELOAD_H_"
        self.sources[PRELOAD_HELPERS] = (LICENSE.rstrip("\n") + "\n\n#ifndef %s\n#define %s\n\n#include \"gen/dll_ntdll.h\"\n\n"
                                         "namespace sprt {\n%s} // namespace sprt\n\n#endif // %s\n") % (
            guard, guard, body.rstrip("\n") + "\n\n", guard)

    # --- the files --------------------------------------------------------------------------------

    def description(self, s):
        funcs = [f for f in s["items"] if f.kind == "func"]
        text = "The functions of %s, written to %s%s." % (
            s["source"], s["header"],
            " and, with their public names, to %s" % s["umbrella"]["header"] if "umbrella" in s else "")
        out = ["// " + line for line in textwrap.wrap(text, 97)]
        out.append("")
        for dll in sorted(self.dll_nodes):
            if self.dll_owner(dll) == s["api"]:
                out.extend(self.dll_nodes[dll])
                out.append("")
        defaults = {}
        firsts = [f for f in funcs if not f.redeclare]
        for key, attr in (("dll", "dll"), ("link", "link"), ("cc", "cc")):
            v = mode([getattr(f, attr) or ("none" if key == "link" else None) for f in firsts or funcs])
            if v:
                defaults[key] = v
        if defaults:
            out.append("defaults %s" % " ".join(
                "%s=%s" % (k, C.kdl_string(defaults[k])) for k in ("dll", "link", "cc") if k in defaults))
            out.append("")
        for it in s["items"]:
            if it.kind == "func":
                kdl_func(it, defaults, out)
            else:
                kdl_typedef(it, out)
        return "\n".join(out) + "\n"

    def dll_owner(self, dll):
        """The description a DLL is written in: the one with the most of its functions (the first of
        them on a tie), or windows.h's for a DLL without functions."""
        counts = [(sum(1 for f in s["items"] if f.kind == "func" and f.dll == dll and not f.redeclare), -i, s["api"])
                  for i, s in enumerate(self.sets)]
        best = max(counts)
        if best[0]:
            return best[2]
        for s in self.sets:
            if s["source"].endswith("/windows.h"):
                return s["api"]
        return self.sets[0]["api"]

    def rewrite(self, s):
        """The hand-written header without its functions and typedefs that moved; the generated
        header is included where the first of them was, outside the #if it stood in."""
        lines = s["text"].split("\n")
        drop = set()
        for it in s["items"]:
            start = it.top
            for i in range(start, it.end + 1):
                drop.add(i)
            if it.end < len(lines) and not lines[it.end].strip() and (start < 2 or not lines[start - 2].strip()):
                drop.add(it.end + 1)
        lead = min(s["items"], key=lambda x: x.line)
        at = lead.top
        if lead.outer:
            at = min(at, lead.outer)
        include = "#include <%s>" % s["header"]
        out = []
        for no, line in enumerate(lines, 1):
            if no == at:
                out.append(include)
                if no in drop:
                    continue
            if no not in drop:
                out.append(line)
        out = drop_empty_conditionals(out)
        out = drop_empty_regions(out, include)
        if "umbrella" in s:
            u = s["umbrella"]
            utext = u["text"]
            ulines = utext.split("\n")
            udrop = set()
            for a, b in u["spans"]:
                f = utext.count("\n", 0, a) + 1
                last = utext.count("\n", 0, b) + 1
                for i in range(f, last + 1):
                    udrop.add(i)
                if last < len(ulines) and not ulines[last].strip():
                    udrop.add(last + 1)
            uinclude = "#include <%s>" % u["header"]
            uat = min(udrop)
            uout = []
            for no, line in enumerate(ulines, 1):
                if no == uat:
                    uout.append(uinclude)
                if no not in udrop:
                    uout.append(line)
            uout = drop_empty_regions(uout, uinclude)
            self.sources[u["source"]] = "\n".join(squeeze(uout))
        return "\n".join(squeeze(out))

    def externals(self, includes, out, seen):
        """Hand-written headers the generated ones include, with their includes, as far as the ABI map
        does not have them already."""
        for inc in includes:
            if inc in seen or inc in self.abi_headers or inc.startswith("sprt/gen/"):
                continue
            seen.add(inc)
            text = self.find_header(inc)
            if text is None:
                continue
            sub = [x for x in unconditional_includes(text)
                   if x in self.abi_headers or x.startswith("sprt/gen/") or self.find_header(x) is not None]
            out.append((inc, sub))
            self.externals(sub, out, seen)

    def find_header(self, inc):
        """A hand-written header by the way it is included: under include/, or include_libc/."""
        for root in ("include/", "include_libc/"):
            text = git_show(self.rev, root + inc)
            if text is not None:
                return text
        return None

    def map_part(self, part, sets):
        if part == "libc":
            out = ["// The libc functions of the API map: the generated headers, what they include, and the",
                   "// hand-written headers that include them.", ""]
        else:
            out = ["// The Windows functions of the API map: the generated headers, what they include, and the",
                   "// hand-written headers they reach that the ABI map does not have.", ""]
        own = set(s["header"] for s in sets)
        for s in sets:
            implied = None
            for root, d in API_ROOTS:
                if s["header"].startswith(root) and s["header"].endswith(".h"):
                    implied = d + s["header"][len(root):-2] + ".kdl"
                    break
            api = "" if implied == s["api"] else " api=%s" % C.kdl_quoted(s["api"])
            out.append("header %s%s {" % (C.kdl_quoted(s["header"]), api))
            out.extend("    include %s" % C.kdl_quoted(i) for i in s["includes"])
            out.append("}")
            out.append("")
            if "umbrella" in s:
                u = s["umbrella"]
                out.append("header %s external=#true {" % C.kdl_quoted(u["includer"]))
                out.append("    include %s" % C.kdl_quoted(s["header"]))
                out.append("}")
                out.append("")
                public = self.public_names(s)
                out.append("header %s external=#true {" % C.kdl_quoted(u["path"]))
                out.append("    include %s" % C.kdl_quoted(u["includer"]))
                out.append("    include %s" % C.kdl_quoted(u["header"]))
                out.append("    type %s" % " ".join(public))
                out.append("}")
                out.append("")
                out.append("header %s umbrella=%s fragment=#true {" % (C.kdl_quoted(u["header"]), C.kdl_quoted(s["header"])))
                out.append("    include %s" % C.kdl_quoted(s["header"]))
                out.append("}")
                out.append("")
        if part == "windows":
            ext = []
            self.externals([i for s in sets for i in s["includes"]], ext, set(own))
            for inc, sub in sorted(ext):
                if sub:
                    out.append("header %s external=#true {" % C.kdl_quoted(inc))
                    out.extend("    include %s" % C.kdl_quoted(x) for x in sub)
                    out.append("}")
                else:
                    out.append("header %s external=#true" % C.kdl_quoted(inc))
                out.append("")
        while out and not out[-1]:
            out.pop()
        return "\n".join(out) + "\n"

    def public_names(self, s):
        """The names with a rule the umbrella's functions use: the include_libc header declares them."""
        ruled = set(v[1] for v in self.spell.names.values())
        names = []
        for f in s["items"]:
            if f.kind != "func" or not f.umbrella:
                continue
            for text in [f.ret] + [p[1] for p in f.params if p[1]]:
                for t in C.tokenize(text):
                    if t.kind == "id" and t.text in ruled and t.text not in names:
                        names.append(t.text)
        return names

    def wire_build(self):
        """The loader includes the generated tables (and the preload checks), the startup unit the
        generated wrappers and the hand-written rest."""
        text = self.read(LOADER)
        for dll, table in self.tables.items():
            if table:
                text = text.replace('#include "dll_%s.h"\n' % dll, '#include "gen/dll_%s.h"\n' % dll)
        if PRELOAD_HELPERS in self.sources:
            text = text.replace('#include "gen/dll_ntdll.h"\n', '#include "gen/dll_ntdll.h"\n#include "dll_ntdll_preload.h"\n')
        self.sources[LOADER] = text
        text = self.read(STARTUP)
        for dll in self.stub_texts:
            line = '#include "windows/dll/dllstub2_%s.cc"\n' % dll
            new = '#include "windows/dll/gen/dllstub2_%s.cc"\n' % dll
            if dll in self.manual_stubs:
                new += '#include "windows/dll/dllstub_%s.cc"\n' % dll
            text = text.replace(line, new)
        self.sources[STARTUP] = text


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--rev", default=ORIGINALS, help="take the hand-written runtime of this revision")
    ap.add_argument("--check", action="store_true", help="write nothing; fail when a file differs")
    args = ap.parse_args()

    conv = Converter(args.rev)
    try:
        conv.convert()
    except ConvertError as e:
        print("api2kdl: %s" % e, file=sys.stderr)
        return 2

    stale = 0
    outputs = [(os.path.join(OUT, rel), text) for rel, text in conv.files.items()]
    outputs += [(os.path.join(RUNTIME, rel), text) for rel, text in conv.sources.items()]
    for path, text in sorted(outputs):
        have = open(path).read() if os.path.exists(path) else None
        if have == text:
            continue
        if args.check:
            print("stale: %s" % os.path.relpath(path, REPO))
            stale += 1
            continue
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w") as f:
            f.write(text)
    for rel in conv.removed:
        path = os.path.join(RUNTIME, rel)
        if os.path.exists(path):
            if args.check:
                print("not removed: %s" % os.path.relpath(path, REPO))
                stale += 1
            else:
                os.remove(path)
    for line in conv.report:
        print(line)
    return 1 if stale else 0


if __name__ == "__main__":
    sys.exit(main())
