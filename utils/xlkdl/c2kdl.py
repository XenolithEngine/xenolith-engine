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

"""c2kdl: the ABI headers of the runtime as KDL descriptions and the header map xlkdl reads.

    utils/xlkdl/c2kdl.py [--src DIR | --rev REV] [--out runtime/kdl/abi] [--check] [--report FILE]

Reads the hand-written headers (the sets in GENERATED) from an include root - `--src`, or
runtime/include of the git revision `--rev`, by default the last one that had them - writes one
description per header and the map `sprt.kdl`, and prints a report: the external headers, the
names left as written because the map does not show them visible, and what was dropped. What it
cannot represent it does not guess: it stops with `file:line`. `--check` writes nothing and fails
when the files on disk are not what it would write.

The map places every generated header under sprt/gen (`sprt/c/bits/x.h` is `sprt/gen/c/bits/x.h`)
with the guard of its source; the external ones keep their paths.

Every decision a header needed beyond the rules of the language is a table below (EXTERNAL,
IN_PLACE, OVERRIDES, MANUAL), so the conversion can be rerun.
"""

import argparse
import glob
import hashlib
import os
import re
import subprocess
import sys
import tempfile

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
INCLUDE = os.path.join(REPO, "runtime", "include")

# The last revision with the hand-written headers in runtime/include.
ORIGINALS = "ffe9fede"
OUT = os.path.join(REPO, "runtime", "kdl", "abi")

GENERATED = [
    "sprt/wrappers/windows/abi/*.h",
    "sprt/c/bits/*.h",
    "sprt/c/cross/*.h",
    "sprt/c/cross/*_sprt/*.h",
    "sprt/c/cross/*_sprt/*_sprt/*.h",
]

# Not ABI, or glue to the OS: written by hand, listed in the map with the names they provide.
EXTERNAL = {
    "sprt/c/bits/__sprt_def.h",
    "sprt/c/bits/__sprt_config.h",
    "sprt/c/bits/__sprt_cpuset_t.h",
    "sprt/c/cross/__sprt_config.h",
    "sprt/c/cross/embox_user_sprt/aarch64_sprt/svc.h",
    "sprt/c/cross/embox_user_sprt/aarch64_sprt/memmap.h",
    "sprt/c/cross/nuttx_sprt/setjmp_os.h",
}

# Descriptions written by hand: the converter maps them and leaves them alone.
# They have no hand-written original: their names are read from what xlkdl generated.
MANUAL = {
    "sprt/c/bits/__sprt_targets.h": "bits/__sprt_targets.kdl",
}

# The hand-written headers that stay: read from the working tree, as they are now.
WORKTREE = os.path.join(REPO, "runtime", "include")

# Dispatchers whose platform fragments use names the dispatcher defines first: the fragment stays
# where it is, as a block. Every other include goes to the top of the header through the map.
IN_PLACE = {
    "sprt/c/cross/__sprt_socket.h",
    "sprt/c/cross/__sprt_dir_ptr.h",
    "sprt/c/cross/__sprt_polltypes.h",
}

# Fragments that take another fragment and redefine part of it: the include becomes a block, an
# #undef with the #define after it an override, and what stood before the include is dropped (it
# only kept the redefinition quiet).
OVERRIDES = {
    "sprt/c/cross/embox_sprt/signal.h",
    "sprt/c/cross/nuttx_sprt/errno.h",
}

# Headers whose block under an architecture's condition becomes a fragment of that architecture,
# put in place by `use ... if=`: {header: (fragment file, {condition: arch directory})}.
ARCH_SPLITS = {
    "sprt/wrappers/windows/abi/context_api.h": ("context.h", {
        "__SPRT_ARCH_ID == __SPRT_ARCH_ID_X86_64": "x86_64_sprt",
        "!(__SPRT_ARCH_ID == __SPRT_ARCH_ID_X86_64) && __SPRT_ARCH_ID == __SPRT_ARCH_ID_AARCH64": "aarch64_sprt",
    }),
}

# Fragments that are a list of record members, written inside a record by a block.
MEMBER_FRAGMENTS = {
    "sprt/c/bits/stat_data.h",
}

# The rule a name of a kind of node takes when it is written without one, by the source path.
DEFAULTS = [
    ("sprt/c/", {"macro": "p", "typedef": "id"}),
    ("sprt/wrappers/", {"macro": "p"}),
]

# Where a description lives when the map does not name it: the mirror of the generated path.
ABI_ROOTS = [("sprt/gen/wrappers/windows/abi/", "windows/"), ("sprt/gen/c/", "")]

# Macros that declare an enum with its underlying type: `typedef NS_ENUM(NSUInteger, Name) {...};`
ENUM_MACROS = ("NS_ENUM", "NS_OPTIONS", "CF_ENUM", "CF_OPTIONS")

# Names of struct-name macros, `__SPRT_SOCKADDR_NAME`, by the tag they spell (rule n); filled
# from the sources before parsing.
NAME_MACROS = {}

# Guards of the generated headers, by their place under sprt/gen: the guards of the sources.
GUARD_SCHEMES = [("sprt/gen/c/", "CORE_RUNTIME_INCLUDE_C_"), ("sprt/gen/", "SPRT_")]
PLATFORM_ROOT = "sprt/c/cross"
COMPUTED = re.compile(r"^SPRT_CROSS_CONFIG_NAME\(\s*sprt/c/cross/__SPRT_PLATFORM_NAME/(__SPRT_ARCH_NAME/)?([\w.]+)\s*\)$")

ATTRIBUTE_MACROS = {"SPRT_ALIGNAS", "__attribute__", "__declspec", "DECLSPEC_ALIGN", "__SPRT_DECLSPEC_ALIGN", "alignas", "_Alignas"}
CALLING = {"__stdcall", "__cdecl", "__fastcall", "__vectorcall", "__thiscall", "__clrcall"}


class ConvertError(Exception):
    pass


def gen_path(path):
    """Where a generated header goes: its source path under sprt/gen."""
    assert path.startswith("sprt/")
    return "sprt/gen/" + path[len("sprt/"):]


def fail(path, line, message):
    raise ConvertError("%s:%d: %s" % (path, line, message))


# --- comments and lines ---------------------------------------------------------------------------

def strip_comments(text):
    """The text with comments blanked (newlines kept), and the comments: (line, end line, text)."""
    out = []
    comments = []
    i = 0
    line = 1
    n = len(text)
    while i < n:
        c = text[i]
        if c == "/" and i + 1 < n and text[i + 1] == "/":
            j = text.find("\n", i)
            j = n if j < 0 else j
            comments.append((line, line, text[i + 2:j].rstrip()))
            out.append(" " * (j - i))
            i = j
            continue
        if c == "/" and i + 1 < n and text[i + 1] == "*":
            j = text.find("*/", i + 2)
            if j < 0:
                raise ConvertError("unclosed comment at line %d" % line)
            body = text[i + 2:j]
            end_line = line + body.count("\n")
            comments.append((line, end_line, body))
            out.append("".join("\n" if ch == "\n" else " " for ch in text[i:j + 2]))
            line = end_line
            i = j + 2
            continue
        if c in "\"'":
            j = i + 1
            while j < n and text[j] != c:
                if text[j] == "\\":
                    j += 1
                if j < n and text[j] == "\n":
                    break
                j += 1
            out.append(text[i:j + 1])
            i = j + 1
            continue
        if c == "\n":
            line += 1
        out.append(c)
        i += 1
    return "".join(out), comments


def logical_lines(code):
    """Physical lines joined across backslashes: (first line number, text with the joins as \\n)."""
    lines = code.split("\n")
    out = []
    i = 0
    while i < len(lines):
        start = i
        text = lines[i]
        while text.endswith("\\") and i + 1 < len(lines):
            i += 1
            text = text[:-1].rstrip() + "\n" + lines[i]
        out.append((start + 1, text))
        i += 1
    return out


# --- tokens -----------------------------------------------------------------------------------------

TOKEN = re.compile(r"""
    (?P<space>[ \t\r\n]*)
    (?P<tok>
        [A-Za-z_$][\w$]*
      | \.?\d(?:[\w.]|'(?=\w)|(?<=[eEpP])[+-])*
      | "(?:\\.|[^"\\\n])*"
      | '(?:\\.|[^'\\\n])*'
      | ->|\.\.\.|::|\#\#|<<|>>|<=|>=|==|!=|&&|\|\|
      | \S
    )""", re.X)


class Tok:
    __slots__ = ("space", "text", "kind", "line")

    def __init__(self, space, text, kind, line):
        self.space = space
        self.text = text
        self.kind = kind
        self.line = line

    def __repr__(self):
        return "Tok(%r)" % self.text


def kind_of(text):
    c = text[0]
    if c.isalpha() or c in "_$":
        return "id"
    if c.isdigit() or (c == "." and len(text) > 1):
        return "num"
    if c in "\"'":
        return "lit"
    return "punct"


def tokenize(text, line=0):
    out = []
    pos = 0
    while pos < len(text):
        m = TOKEN.match(text, pos)
        if not m or not m.group("tok"):
            break
        tok = m.group("tok")
        out.append(Tok(m.group("space"), tok, kind_of(tok), line + text.count("\n", 0, m.start("tok"))))
        pos = m.end()
    return out


def join(tokens, first_space=False):
    out = []
    for i, t in enumerate(tokens):
        if i > 0 or first_space:
            out.append(" " if t.space and "\n" not in t.space else t.space)
        out.append(t.text)
    return "".join(out).strip() if not first_space else "".join(out)


# --- names ------------------------------------------------------------------------------------------

def split_name(raw):
    """A defined name as the map writes it: (rule, name)."""
    m = re.match(r"^__SPRT_ID\(\s*(\w+)\s*\)$", raw)
    if m:
        return ("id", m.group(1))
    if raw in NAME_MACROS:
        return ("n", NAME_MACROS[raw])
    if raw.startswith("__SPRT_") and raw != "__SPRT_ID":
        return ("p", raw[len("__SPRT_"):])
    return (None, raw)


def kdl_name(rule, name):
    return ("(%s)" % rule if rule else "") + kdl_string(name)


def defaults_of(path):
    for root, d in DEFAULTS:
        if path.startswith(root):
            return d
    return {}


def kdl_name_of(path, kind, rule, name):
    """A name for a node of `kind` in a header under `path`: its default rule goes unwritten, and
    no rule where there is a default is the name in backticks."""
    default = defaults_of(path).get(kind)
    if default is None:
        return kdl_name(rule, name)
    if rule == default:
        return kdl_string(name)
    if rule is None:
        return kdl_string("`%s`" % name)
    return kdl_name(rule, name)


C_NUMBER = re.compile(r"^-?(?:0|[1-9][0-9]*|0x[0-9a-fA-F]+)$")


def kdl_body(text):
    """A macro body or an enumerator value: a number KDL reads as it is in C goes unquoted."""
    return text if C_NUMBER.match(text) else kdl_string(text)


def split_and(expr):
    parts, depth, cur, i = [], 0, "", 0
    while i < len(expr):
        c = expr[i]
        if c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
        if depth == 0 and expr.startswith(" && ", i):
            parts.append(cur)
            cur = ""
            i += 4
            continue
        cur += c
        i += 1
    parts.append(cur)
    return parts


def cond_name(expr):
    """A name for a condition, from what it tests (expr is written as in the description)."""
    if re.match(r"^CONFIG_COMPILER_ASSISTED_INTS && defined\(\w+\)$", expr):
        return "assisted"
    m = re.match(r"^ARCH_ID == ARCH_ID_(\w+)$", expr)
    if m:
        return m.group(1).lower()
    if expr == "defined(__e2k__) && defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ > 8":
        return "e2k128"
    if expr == "defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1":
        return "hosted_build"
    if expr == "defined(__SPRT_BUILD)":
        return "build"
    names = re.findall(r"defined\((\w+)\)", expr)
    if names and re.sub(r"defined\(\w+\)|\|\||\s", "", expr) == "":
        n = set(names)
        if {"__LLP64__", "__ILP32__", "__LP64__"} <= n:
            return "data_model"
        if {"__LLP64__", "__ILP32__"} <= n:
            return "llp64_or_ilp32"
        if "__LLP64__" in n:
            return "llp64"
        if "__ILP32__" in n:
            return "ilp32"
    words = [w.strip("_").lower() for w in re.findall(r"[A-Za-z_]\w*", expr) if w != "defined"]
    name = "_".join(w for w in words[:3] if w) or "cond"
    return name if not name[0].isdigit() else "c_" + name


# --- the preprocessor structure ---------------------------------------------------------------------

def name_conditions(conds, exprs, namer):
    """Names for the #if expressions a description repeats: {expr: name} in order of appearance."""
    counts = {}
    for c in conds:
        for e in exprs:
            if e not in counts:
                counts[e] = 0
            if substitute(c, [(e, "x")]) != c:
                counts[e] += 1
    names = {}
    taken = set()
    for e in exprs:
        if e in names or counts.get(e, 0) < 3 or len(e) < 20:
            continue
        base = namer(e)
        name, k = base, 2
        while name in taken:
            name, k = "%s_%d" % (base, k), k + 1
        taken.add(name)
        names[e] = name
    return names


def is_atomic(expr):
    """As xlkdl decides it: a name or number, or a name with one call or group closing the end."""
    e = expr.strip()
    m = re.match(r"^\w*", e)
    if m.end() == len(e):
        return bool(e)
    if e[m.end()] != "(":
        return False
    depth = 0
    for i in range(m.end(), len(e)):
        if e[i] == "(":
            depth += 1
        elif e[i] == ")":
            depth -= 1
            if depth == 0:
                return i == len(e) - 1
    return False


def substitute(cond, named):
    """A condition with each named expression written `@name` where it stands as an operand of the
    `&&` chain: bare, in parentheses, or negated."""
    parts = split_and(cond)
    for expr, name in sorted(named, key=lambda x: -len(x[0])):
        sub = split_and(expr)
        out, i = [], 0
        while i < len(parts):
            if len(sub) > 1 and parts[i:i + len(sub)] == sub:
                out.append("@" + name)
                i += len(sub)
                continue
            p = parts[i]
            if p == expr:
                out.append("@" + name)
            elif p == "!(%s)" % expr:
                # xlkdl puts a negated name in parentheses unless it is atomic.
                out.append(("!(@%s)" if is_atomic(expr) else "!@%s") % name)
            elif p == "(%s)" % expr:
                # Between `&&`s it adds them for an expression with `||` or `?` of its own.
                out.append(("@%s" if needs_parens(expr) else "(@%s)") % name)
            elif p == "!" + expr and re.match(r"^defined\(\w+\)$", expr):
                out.append("!@" + name)
            else:
                out.append(p)
            i += 1
        parts = out
    return " && ".join(parts)


def strip_cond(cond, outer):
    """A condition under `outer`, without it."""
    if not cond or cond == outer:
        return ""
    if cond.startswith(outer + " && "):
        return cond[len(outer) + 4:]
    return cond


def negate(expr):
    e = expr.strip()
    m = re.match(r"^defined\s*\(\s*\w+\s*\)$", e)
    if m:
        return "!" + e
    m = re.match(r"^!\s*(defined\s*\(\s*\w+\s*\))$", e)
    if m:
        return m.group(1)
    return "!(%s)" % e


def needs_parens(expr):
    depth = 0
    i = 0
    while i < len(expr):
        c = expr[i]
        if c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
        elif depth == 0 and (expr.startswith("||", i) or c == "?"):
            return True
        i += 1
    return False


def conjunction(parts):
    parts = [p.strip() for p in parts if p and p.strip()]
    if not parts:
        return ""
    if len(parts) == 1:
        return parts[0]
    return " && ".join("(%s)" % p if needs_parens(p) else p for p in parts)


def normalize_defined(expr):
    expr = re.sub(r"\s+", " ", expr.strip())
    return re.sub(r"\bdefined\s+(\w+)", r"defined(\1)", expr)


class Level:
    def __init__(self, cond, guard=False):
        self.previous = []
        self.current = cond
        self.guard = guard

    def branch(self):
        return conjunction([negate(p) for p in self.previous] + ([self.current] if self.current is not None else []))


class Event:
    """A token of code, or a directive other than a conditional, with the condition around it."""

    def __init__(self, kind, line, cond, **kw):
        self.kind = kind
        self.line = line
        self.cond = cond
        self.__dict__.update(kw)


def events_of(path, text):
    code, comments = strip_comments(text)
    lines = logical_lines(code)
    stack = []
    events = []
    guard = None
    levels = []

    # The include guard: the first directive is #ifndef G and the next #define G.
    directives = [(ln, t.strip()) for ln, t in lines if t.strip().startswith("#")]
    if len(directives) >= 2:
        m1 = re.match(r"^#\s*ifndef\s+(\w+)\s*$", directives[0][1])
        m2 = re.match(r"^#\s*define\s+(\w+)\s*$", directives[1][1])
        if m1 and m2 and m1.group(1) == m2.group(1):
            first_code = [ln for ln, t in lines if t.strip() and not t.strip().startswith("#")]
            if not first_code or first_code[0] > directives[1][0]:
                guard = m1.group(1)

    def cond():
        return conjunction([lv.branch() for lv in stack if not lv.guard])

    guard_pending = guard is not None
    skip_guard_define = guard is not None
    for ln, t in lines:
        s = t.strip()
        if not s:
            continue
        if s.startswith("#"):
            d = s[1:].strip()
            word = re.match(r"^(\w*)", d).group(1)
            rest = d[len(word):].strip()
            if word in ("if", "ifdef", "ifndef"):
                if guard_pending and word == "ifndef" and rest == guard:
                    stack.append(Level(None, guard=True))
                    guard_pending = False
                    continue
                if word == "ifdef":
                    expr = "defined(%s)" % rest
                elif word == "ifndef":
                    expr = "!defined(%s)" % rest
                else:
                    expr = normalize_defined(rest)
                stack.append(Level(expr))
                levels.append(expr)
            elif word == "elif":
                lv = stack[-1]
                lv.previous.append(lv.current)
                lv.current = normalize_defined(rest)
                levels.append(lv.current)
            elif word == "else":
                lv = stack[-1]
                lv.previous.append(lv.current)
                lv.current = None
            elif word == "endif":
                if not stack:
                    fail(path, ln, "#endif without #if")
                stack.pop()
            elif word == "define" and skip_guard_define and rest == guard:
                skip_guard_define = False
            else:
                events.append(Event("dir", ln, cond(), word=word, rest=rest, raw=s))
            continue
        for tok in tokenize(t, ln):
            events.append(Event("tok", tok.line, cond(), tok=tok))
    if stack:
        fail(path, lines[-1][0], "unbalanced #if")
    return events, comments, guard, levels


# --- declarations -----------------------------------------------------------------------------------

class Item:
    def __init__(self, kind, line, cond, **kw):
        self.kind = kind
        self.line = line
        self.cond = cond
        self.children = []
        self.comment = []
        self.trailing = None
        self.__dict__.update(kw)


class Parser:
    """Events of one header into items; names stay as written until the visible map is known."""

    def __init__(self, path, events, members=False):
        self.members = members
        self.path = path
        self.events = events
        self.pos = 0
        self.items = []
        self.includes = []  # (line, cond, kind, path, in_place_index or None)
        self.pending_pack = None
        self.pending_warning = None
        self.extern_c = None

    def error(self, line, message):
        fail(self.path, line, message)

    def peek(self):
        return self.events[self.pos] if self.pos < len(self.events) else None

    # Directives -----------------------------------------------------------------------------------

    def directive(self, ev, into, in_record):
        word, rest = ev.word, ev.rest
        if word == "define":
            into.append(self.define(ev))
        elif word == "undef":
            into.append(Item("undef", ev.line, ev.cond, name=rest.strip()))
        elif word == "error":
            into.append(Item("error", ev.line, ev.cond, text=rest))
        elif word in ("include", "import"):
            self.include(ev, into, in_record)
        elif word == "pragma":
            self.pragma(ev)
        else:
            self.error(ev.line, "unsupported directive #%s" % word)

    def define(self, ev):
        m = re.match(r"^(\w+)(\([^)]*\))?(.*)$", ev.rest, re.S)
        if not m:
            self.error(ev.line, "unreadable #define")
        name, params, body = m.group(1), m.group(2), m.group(3)
        if params is not None:
            params = [p.strip() for p in params[1:-1].split(",") if p.strip()]
        body = body.strip()
        lines = body.split("\n")
        body = "\n".join([lines[0].strip()] + [l.rstrip() for l in lines[1:]])
        return Item("macro", ev.line, ev.cond, name=name, params=params, body=body)

    def include(self, ev, into, in_record):
        target = ev.rest.strip()
        m = re.match(r'^<([^>]+)>$', target) or re.match(r'^"([^"]+)"$', target)
        if m:
            path = m.group(1)
            if target.startswith('"'):
                path = os.path.normpath(os.path.join(os.path.dirname(self.path), path))
            kind = "path"
        else:
            c = COMPUTED.match(target)
            if not c:
                self.error(ev.line, "unsupported include %s" % target)
            kind = "arch" if c.group(1) else "platform"
            path = c.group(2)
        self.includes.append(dict(line=ev.line, cond=ev.cond, kind=kind, path=path, into=into,
                                  index=len(into), in_record=in_record))

    def pragma(self, ev):
        rest = ev.rest
        m = re.match(r"^pack\s*\(\s*push\s*,\s*(\d+)\s*\)$", rest)
        if m:
            self.pending_pack = (m.group(1), ev.cond, ev.line)
            return
        if re.match(r"^pack\s*\(\s*pop\s*\)$", rest):
            return
        m = re.match(r'^clang\s+diagnostic\s+ignored\s+"([^"]+)"$', rest)
        if m:
            self.pending_warning = m.group(1)
            return
        if re.match(r"^clang\s+diagnostic\s+(push|pop)$", rest):
            return
        self.error(ev.line, "unsupported #pragma %s" % rest)

    # Code ---------------------------------------------------------------------------------------

    def parse(self):
        while self.pos < len(self.events):
            ev = self.events[self.pos]
            if ev.kind == "dir":
                self.pos += 1
                self.directive(ev, self.items, False)
                continue
            self.declaration(self.items, record=self.members)
        return self.items

    def collect(self, into, in_record, stop):
        """Tokens up to `stop` at depth 0, the directives between them handled on the way."""
        toks = []
        depth = 0
        while True:
            ev = self.peek()
            if ev is None:
                return toks, None
            if ev.kind == "dir":
                if depth == 0:
                    self.pos += 1
                    self.directive(ev, into, in_record)
                    continue
                return toks, None
            t = ev.tok
            if depth == 0 and t.text in stop:
                self.pos += 1
                return toks, t.text
            if t.text in "([":
                depth += 1
            elif t.text in ")]":
                depth -= 1
            elif t.text == "{":
                return toks, "{"
            elif t.text == "}":
                return toks, "}"
            toks.append(ev)
            self.pos += 1

    def declaration(self, into, record):
        first = self.peek()
        cond = first.cond
        line = first.line
        head = []
        # Leading tokens up to a body, a ';' or a ',' (the end of a member list in an enum is the parser's).
        while True:
            ev = self.peek()
            if ev is None:
                self.error(line, "unterminated declaration")
            if ev.kind == "dir":
                if not head:
                    self.pos += 1
                    self.directive(ev, into, record)
                    if self.peek() is None or (self.peek().kind == "tok" and self.peek().tok.text == "}"):
                        return
                    first = self.peek()
                    cond, line = first.cond, first.line
                    continue
                self.error(ev.line, "a directive inside a declaration")
            t = ev.tok
            if t.text == "}" and not head and record:
                return
            if not head and not record:
                if self.extern_block(t, ev):
                    return
            if t.text in (";", "{", "}"):
                break
            head.append(ev)
            self.pos += 1
            if head and head[-1].tok.text == "(":
                # A parenthesised group belongs to the head whole.
                depth = 1
                while depth > 0:
                    ev2 = self.peek()
                    if ev2 is None or ev2.kind == "dir":
                        self.error(line, "a directive or the end inside parentheses")
                    head.append(ev2)
                    self.pos += 1
                    if ev2.tok.text == "(":
                        depth += 1
                    elif ev2.tok.text == ")":
                        depth -= 1
        ev = self.peek()
        t = ev.tok
        if t.text == ";":
            self.pos += 1
            if not head:
                return
            self.plain(head, cond, line, into, record)
            return
        if t.text == "{":
            self.pos += 1
            self.body(head, cond, line, into, record)
            return
        self.error(t.line, "unexpected '%s'" % t.text)

    def extern_block(self, t, ev):
        if t.text in ("__SPRT_BEGIN_DECL", "__SPRT_END_DECL"):
            self.pos += 1
            self.extern_c = "macro" if t.text == "__SPRT_BEGIN_DECL" else None
            return True
        if t.text == "extern" and self.pos + 2 < len(self.events):
            a, b = self.events[self.pos + 1], self.events[self.pos + 2]
            if a.kind == "tok" and a.tok.text == '"C"' and b.kind == "tok" and b.tok.text == "{":
                self.pos += 3
                self.extern_c = "raw"
                return True
        if t.text == "}" and self.extern_c == "raw":
            self.pos += 1
            self.extern_c = None
            return True
        return False

    def plain(self, head, cond, line, into, record):
        toks = [e.tok for e in head]
        if toks[0].text == "typedef":
            spec, decls = split_declarators(self, toks[1:], line)
            for d in decls:
                name, typ = name_and_type(self, spec, d, line)
                into.append(Item("typedef", line, cond, name=name, type=typ))
            return
        if toks[0].text in ("struct", "union", "enum") and len(toks) == 2:
            into.append(Item("record", line, cond, tag=toks[1].text, keyword=toks[0].text,
                             members=None, typedef=None, also=None, attr=None, attr_after=None,
                             base=None, member=None, pack=None, pack_cond=None, warning=None))
            return
        if record:
            spec, decls = split_declarators(self, toks, line)
            for d in decls:
                bits = None
                ds = [x for x in d]
                for i, x in enumerate(ds):
                    if x.text == ":":
                        bits = join(ds[i + 1:])
                        ds = ds[:i]
                        break
                attr = None
                spec2 = []
                for x in spec:
                    if x.kind == "attr":
                        attr = x.text
                    else:
                        spec2.append(x)
                ds2 = []
                for x in ds:
                    if x.kind == "attr":
                        attr = x.text
                    else:
                        ds2.append(x)
                name, typ = name_and_type(self, spec2, ds2, line)
                into.append(Item("field", line, cond, name=name, type=typ, bits=bits, attr=attr))
            return
        m = toks[0].text
        if m == "__SPRT_DECLARE_HANDLE" and len(toks) == 4:
            into.append(Item("typedef", line, cond, name=toks[2].text, type="HANDLE"))
            return
        if any(t.text == "(" for t in toks):
            into.append(Item("decl", line, cond, text=join(toks), extern_c=self.extern_c))
            return
        self.error(line, "unsupported declaration '%s'" % join(toks))

    def body(self, head, cond, line, into, record):
        toks = [e.tok for e in head]
        is_typedef = bool(toks) and toks[0].text == "typedef"
        if is_typedef:
            toks = toks[1:]
        if is_typedef and len(toks) >= 4 and toks[0].text in ENUM_MACROS and toks[1].text == "(" and toks[-1].text == ")":
            # `typedef NS_ENUM(base, Name) { ... };`
            parts = split_list(toks[2:-1])
            if len(parts) != 2 or len(parts[1]) != 1:
                self.error(line, "%s that is not (base, name)" % toks[0].text)
            item = Item("record", line, cond, keyword="enum", tag=parts[1][0].text, attr=None, base=render_tokens(parts[0]),
                        members=[], typedef=None, also=None, attr_after=None, member=None, pack=None, pack_cond=None,
                        warning=None, macro=toks[0].text)
            self.enum_body(item)
            ev = self.peek()
            if ev is None or ev.kind != "tok" or ev.tok.text != ";":
                self.error(line, "a declarator after %s" % toks[0].text)
            self.pos += 1
            into.append(item)
            return
        if not toks or toks[0].text not in ("struct", "union", "enum"):
            self.error(line, "a body that is not a record or an enum")
        keyword = toks[0].text
        attr = None
        tag = None
        base = None
        rest = collapse(toks[1:])
        i = 0
        while i < len(rest):
            x = rest[i]
            if x.kind == "attr":
                attr = x.text
            elif x.text == ":":
                base = join(rest[i + 1:])
                break
            elif x.kind in ("id", "wrap"):
                tag = x.text if x.kind == "id" else "__SPRT_ID(%s)" % x.text
            i += 1
        item = Item("record", line, cond, keyword=keyword, tag=tag, attr=attr, base=base, members=[],
                    typedef=None, also=None, attr_after=None, member=None, pack=None, pack_cond=None,
                    warning=None)
        if self.pending_pack and not record:
            item.pack, item.pack_cond = self.pending_pack[0], self.pending_pack[1]
            self.pending_pack = None
        if self.pending_warning and not record:
            item.warning = self.pending_warning
            self.pending_warning = None
        if keyword == "enum":
            self.enum_body(item)
        else:
            while True:
                ev = self.peek()
                if ev is None:
                    self.error(line, "unclosed record")
                if ev.kind == "tok" and ev.tok.text == "}":
                    self.pos += 1
                    break
                if ev.kind == "dir":
                    self.pos += 1
                    self.directive(ev, item.members, True)
                    continue
                self.declaration(item.members, record=True)
        # After the brace: attributes, declarators, ';'.
        tail = []
        while True:
            ev = self.peek()
            if ev is None or ev.kind == "dir":
                self.error(line, "unterminated record")
            self.pos += 1
            if ev.tok.text == ";":
                break
            tail.append(ev.tok)
        tail = collapse(tail)
        attrs = [x.text for x in tail if x.kind == "attr"]
        tail = [x for x in tail if x.kind != "attr"]
        if attrs:
            item.attr_after = " ".join(attrs)
        decls = split_list(tail)
        if is_typedef:
            if not decls:
                self.error(line, "a typedef without a name")
            first = decls[0]
            if len(first) != 1 or first[0].kind not in ("id", "wrap"):
                # `typedef struct T {...} N[1];`: the record, then typedefs of its tag.
                if not tag:
                    self.error(line, "the first name of a typedef'd anonymous record is not plain")
                into.append(item)
                spec = [Tok("", keyword, "id", line)] + collapse(tokenize(" " + tag))
                for d in decls:
                    name, typ = name_and_type(self, spec, d, line)
                    into.append(Item("typedef", line, cond, name=name, type=typ))
                return
            item.typedef = first[0].text if first[0].kind == "id" else "__SPRT_ID(%s)" % first[0].text
            if len(decls) > 1:
                item.also = ", ".join(render_tokens(d) for d in decls[1:])
        elif decls:
            if not record:
                self.error(line, "a record declaring variables")
            if len(decls) != 1 or len(decls[0]) != 1:
                self.error(line, "a nested record with a complex member declarator")
            item.member = decls[0][0].text
        into.append(item)

    def enum_body(self, item):
        toks = []
        while True:
            ev = self.peek()
            if ev is None:
                self.error(item.line, "unclosed enum")
            if ev.kind == "dir":
                self.error(ev.line, "a directive inside an enum")
            self.pos += 1
            if ev.tok.text == "}":
                break
            toks.append(ev)
        for part in split_list([e.tok for e in toks]):
            if not part:
                continue
            name = part[0].text
            value = None
            if len(part) > 1:
                if part[1].text != "=":
                    self.error(part[0].line, "an enumerator that is not NAME = VALUE")
                value = render_tokens(part[2:])
            item.members.append(Item("value", part[0].line, "", name=name, value=value))


def collapse(toks):
    """`__SPRT_ID ( x )` into one wrapped name, attribute macros with their arguments into one."""
    out = []
    i = 0
    while i < len(toks):
        t = toks[i]
        if t.text == "__SPRT_ID" and i + 3 < len(toks) and toks[i + 1].text == "(" and toks[i + 3].text == ")":
            out.append(Tok(t.space, toks[i + 2].text, "wrap", t.line))
            i += 4
            continue
        if t.text in ATTRIBUTE_MACROS and i + 1 < len(toks) and toks[i + 1].text == "(":
            depth = 0
            j = i + 1
            while j < len(toks):
                if toks[j].text == "(":
                    depth += 1
                elif toks[j].text == ")":
                    depth -= 1
                    if depth == 0:
                        break
                j += 1
            out.append(Tok(t.space, render_tokens(toks[i:j + 1]), "attr", t.line))
            i = j + 1
            continue
        out.append(t)
        i += 1
    return out


def render_tokens(toks):
    out = []
    for i, t in enumerate(toks):
        if i > 0 and t.space:
            out.append(" ")
        if t.kind == "wrap":
            out.append("__SPRT_ID(%s)" % t.text)
        else:
            out.append(t.text)
    return "".join(out)


def split_list(toks):
    parts = [[]]
    depth = 0
    for t in toks:
        if t.text in "([{":
            depth += 1
        elif t.text in ")]}":
            depth -= 1
        if t.text == "," and depth == 0:
            parts.append([])
            continue
        parts[-1].append(t)
    return [p for p in parts if p] if any(parts) else []


def split_declarators(parser, toks, line):
    """Specifiers and the declarators after them: `unsigned long DWORD, *PDWORD`."""
    toks = collapse(toks)
    parts = split_list(toks)
    if not parts:
        parser.error(line, "an empty declaration")
    first = parts[0]
    i = 0
    while i < len(first) and first[i].kind in ("id", "wrap", "attr"):
        i += 1
    if i < len(first) and first[i].text == "(":
        # `int (*fn)(void)` groups; `T name(args)` is a function type with its name before `(`.
        j = i + 1
        while j < len(first) and (first[j].kind == "id" and (first[j].text in CALLING or first[j].text.startswith("__SPRT_") or first[j].text in ("WINAPI", "CALLBACK", "NTAPI", "APIENTRY"))):
            j += 1
        grouping = j < len(first) and first[j].text in ("*", "&", "^")
        if not grouping:
            i -= 1
    elif i == len(first) or first[i].text in ("[", ":"):
        i -= 1
        while i > 0 and first[i].kind == "attr":
            i -= 1
    spec = first[:i]
    decls = [first[i:]] + parts[1:]
    return spec, decls


def name_and_type(parser, spec, decl, line):
    """The declared name, and the type-id: the declaration with the name left out."""
    idx = None
    depth_paren = 0
    depth_bracket = 0
    params = []
    prev = None
    for k, t in enumerate(decl):
        if t.text == "(":
            is_params = prev is not None and (prev.kind in ("id", "wrap") or prev.text == ")")
            params.append(is_params)
            depth_paren += 1
        elif t.text == ")":
            params.pop()
            depth_paren -= 1
        elif t.text == "[":
            depth_bracket += 1
        elif t.text == "]":
            depth_bracket -= 1
        elif t.kind in ("id", "wrap") and depth_bracket == 0 and not any(params):
            if t.text not in CALLING and t.text not in ("const", "volatile", "restrict", "__restrict") and not (t.kind == "id" and t.text.startswith("__SPRT_") and k + 1 < len(decl) and decl[k + 1].text in ("*", "&")):
                idx = k
        prev = t
    if idx is None:
        parser.error(line, "no name in the declarator '%s'" % render_tokens(decl))
    t = decl[idx]
    name = t.text if t.kind == "id" else "__SPRT_ID(%s)" % t.text
    rest = decl[:idx] + decl[idx + 1:]
    typ = render_tokens(spec)
    tail = render_tokens(rest)
    if tail:
        typ = (typ + " " + tail) if typ else tail
    typ = re.sub(r"\s+", " ", typ).strip()
    typ = typ.replace("( ", "(").replace(" )", ")")
    return name, typ


# --- kdl output -------------------------------------------------------------------------------------

KDL_FORBIDDEN = set('\\/(){};[]"#=')


def is_kdl_identifier(s):
    if not s or any(c in KDL_FORBIDDEN or c.isspace() for c in s):
        return False
    if s in ("true", "false", "null", "inf", "-inf", "nan"):
        return False
    if s[0].isdigit():
        return False
    if s[0] in "+-" and len(s) > 1 and (s[1].isdigit() or (s[1] == "." and len(s) > 2 and s[2].isdigit())):
        return False
    if s[0] == "." and len(s) > 1 and s[1].isdigit():
        return False
    return True


def kdl_string(s):
    if is_kdl_identifier(s):
        return s
    out = ['"']
    for c in s:
        if c == '"':
            out.append('\\"')
        elif c == "\\":
            out.append("\\\\")
        elif c == "\n":
            out.append("\\n")
        elif c == "\t":
            out.append("\\t")
        elif c == "\r":
            out.append("\\r")
        elif ord(c) < 0x20 or ord(c) == 0x7F:
            out.append("\\u{%x}" % ord(c))
        else:
            out.append(c)
    out.append('"')
    return "".join(out)


def kdl_quoted(s):
    k = kdl_string(s)
    return k if k.startswith('"') else '"%s"' % k


# --- the conversion ---------------------------------------------------------------------------------

class Header:
    def __init__(self, path):
        self.path = path
        self.external = path in EXTERNAL
        self.system = False
        self.fragment = False
        self.guard = None
        self.items = []
        self.includes = []
        self.symbols = set()  # (rule, name)
        self.blocks = []  # (kind, path) of `use`
        self.comments = []
        self.users = set()
        self.abi = None
        self.text = None
        self.levels = []
        self.named = {}


def scan_external(h, text):
    """Names a hand-written header defines, by its directives and declarations, leniently."""
    code, _ = strip_comments(text)
    for ln, t in logical_lines(code):
        s = t.strip()
        m = re.match(r"^#\s*define\s+(\w+)", s)
        if m:
            h.symbols.add(split_name(m.group(1)))
        m = re.match(r'^#\s*include\s*[<"]([^>"]+)[>"]', s)
        if m:
            p = m.group(1)
            if s.find('"') >= 0:
                p = os.path.normpath(os.path.join(os.path.dirname(h.path), p))
            if p.startswith("sprt/gen/"):
                p = "sprt/" + p[len("sprt/gen/"):]
            h.includes.append(dict(kind="path", path=p, cond="", line=ln))
    for m in re.finditer(r"\btypedef\b[^;{]*?(__SPRT_ID\s*\(\s*\w+\s*\)|\w+)\s*(?:\[[^\]]*\])?\s*;", code):
        h.symbols.add(split_name(re.sub(r"\s", "", m.group(1))))
    for m in re.finditer(r"\}\s*(__SPRT_ID\s*\(\s*\w+\s*\)|\w+)\s*;", code):
        h.symbols.add(split_name(re.sub(r"\s", "", m.group(1))))
    for m in re.finditer(r"\b(?:struct|union|enum)\s+(__SPRT_ID\s*\(\s*\w+\s*\)|\w+)", code):
        h.symbols.add(split_name(re.sub(r"\s", "", m.group(1))))


def all_items(items):
    for it in items:
        yield it
        if it.kind == "record":
            yield from all_items(it.members or [])
        yield from all_items(it.children)


class Converter:
    def __init__(self):
        self.headers = {}
        self.platforms = {}
        self.report = {"literal": [], "dropped": [], "external": [], "system": []}

    def header(self, path):
        h = self.headers.get(path)
        if not h:
            h = Header(path)
            self.headers[path] = h
        return h

    def load(self):
        paths = []
        for pattern in GENERATED:
            paths.extend(os.path.relpath(p, INCLUDE) for p in glob.glob(os.path.join(INCLUDE, pattern)))
        paths = sorted(set(paths))
        # Struct-name macros: `#define __SPRT_X_NAME __SPRT_ID(tag)` makes __SPRT_X_NAME the name
        # rule n writes for tag.
        for p in paths:
            with open(os.path.join(INCLUDE, p)) as f:
                for m in re.finditer(r"#\s*define\s+(__SPRT_(\w+)_NAME)\s+__SPRT_ID\(\s*(\w+)\s*\)", f.read()):
                    tag = m.group(3)
                    stem = tag[:-2] if tag.endswith("_t") and len(tag) > 2 else tag
                    if stem.upper() == m.group(2):
                        NAME_MACROS[m.group(1)] = tag
        for d in sorted(glob.glob(os.path.join(INCLUDE, PLATFORM_ROOT, "*_sprt"))):
            name = os.path.basename(d)
            self.platforms[name] = sorted(os.path.basename(a) for a in glob.glob(os.path.join(d, "*_sprt")) if os.path.isdir(a))
        for p in paths:
            self.parse(p)
        for p, abi in MANUAL.items():
            h = self.header(p)
            h.guard = self.guard_of(p)
            full = os.path.join(WORKTREE, gen_path(p))
            if os.path.exists(full):
                with open(full) as f:
                    scan_external(h, f.read())
                h.includes = [inc for inc in h.includes if inc["path"] != p]
        # Every include target is in the map: external when it is not generated.
        pending = True
        while pending:
            pending = False
            for h in list(self.headers.values()):
                for inc in h.includes:
                    for target in self.variants(inc["kind"], inc["path"]):
                        if target not in self.headers:
                            t = self.header(target)
                            t.external = True
                            full = os.path.join(WORKTREE, target)
                            if os.path.exists(full):
                                with open(full) as f:
                                    scan_external(t, f.read())
                            else:
                                t.system = True
                            pending = True

    def variants(self, kind, path):
        if kind == "path":
            return [path]
        if kind == "platform":
            return ["%s/%s/%s" % (PLATFORM_ROOT, p, path) for p in sorted(self.platforms)]
        return ["%s/%s/%s/%s" % (PLATFORM_ROOT, p, a, path) for p in sorted(self.platforms) for a in self.platforms[p]]

    def parse(self, path):
        h = self.header(path)
        full = os.path.join(WORKTREE if h.external else INCLUDE, path)
        with open(full) as f:
            text = f.read()
        h.text = text
        h.fragment = re.match(r"^sprt/c/cross/\w+_sprt/", path) is not None or path == "sprt/c/bits/stat_data.h"
        if h.external:
            scan_external(h, text)
            return
        events, comments, guard, levels = events_of(path, text)
        h.guard = guard
        h.comments = comments
        h.levels = levels
        parser = Parser(path, events, members=path in MEMBER_FRAGMENTS)
        h.items = parser.parse()
        if parser.pending_pack or parser.pending_warning:
            fail(path, 0, "a #pragma that no record follows")
        self.place_includes(h, parser)
        self.add_symbols(h)
        if path in ARCH_SPLITS:
            self.split_arch(h)

    def add_symbols(self, h):
        for it in all_items(h.items):
            if it.kind in ("macro", "typedef"):
                h.symbols.add(split_name(it.name))
            elif it.kind == "record":
                if it.tag:
                    h.symbols.add(split_name(it.tag))
                if it.typedef:
                    h.symbols.add(split_name(it.typedef))
                if it.also:
                    for part in it.also.split(","):
                        ids = re.findall(r"__SPRT_ID\(\w+\)|\w+", part)
                        if ids:
                            h.symbols.add(split_name(ids[-1]))
            elif it.kind == "value":
                h.symbols.add(split_name(it.name))

    def split_arch(self, h):
        """The run of top-level items under an architecture's condition goes to a fragment of that
        architecture, without the condition; a `use` under it takes its place."""
        name, arches = ARCH_SPLITS[h.path]
        for cond, arch in arches.items():
            idx = [i for i, it in enumerate(h.items) if it.cond == cond]
            if not idx or idx != list(range(idx[0], idx[-1] + 1)):
                fail(h.path, 0, "no contiguous block under %s to split" % cond)
            moved = h.items[idx[0]:idx[-1] + 1]
            for it in moved:
                for x in all_items([it]):
                    x.cond = strip_cond(x.cond, cond)
                if it.kind == "record" and it.pack_cond:
                    it.pack_cond = strip_cond(it.pack_cond, cond)
            # Comments after the item before the block belong to the fragment.
            start = self.last_line(h.items[idx[0] - 1]) + 1 if idx[0] > 0 else 1
            frag = "%s/%s/%s" % (os.path.dirname(h.path), arch, name)
            f = self.header(frag)
            f.start = start
            f.fragment = True
            f.items = moved
            f.comments = h.comments
            f.levels = h.levels
            self.add_symbols(f)
            use = Item("use", start, cond, kind_="path", path=frag)
            use.last = max(self.last_line(it) for it in moved)
            h.items[idx[0]:idx[-1] + 1] = [use]
            h.blocks.append(("path", frag))

    def place_includes(self, h, parser):
        override = h.path in OVERRIDES
        seen = set()
        uses = []
        for inc in parser.includes:
            key = (inc["kind"], inc["path"], inc["cond"])
            if inc["in_record"] or (h.path in IN_PLACE and inc["kind"] != "path") or override:
                uses.append(inc)
                continue
            if key in seen:
                continue
            seen.add(key)
            h.includes.append(inc)
        # Blocks go where they stood, last first so the indices hold.
        for inc in reversed(uses):
            item = Item("use", inc["line"], inc["cond"], kind_=inc["kind"], path=inc["path"])
            inc["into"].insert(inc["index"], item)
            h.blocks.append((inc["kind"], inc["path"]))
        if override:
            self.make_overrides(h)

    def make_overrides(self, h):
        items = h.items
        k = next((i for i, it in enumerate(items) if it.kind == "use"), None)
        if k is None:
            fail(h.path, 0, "an override header without an include")
        use = items[k]
        for it in items[:k]:
            self.report["dropped"].append("%s:%d: %s %s" % (h.path, it.line, it.kind, getattr(it, "name", "")))
        rest = items[k + 1:]
        out = []
        i = 0
        while i < len(rest):
            it = rest[i]
            if it.kind == "undef" and i + 1 < len(rest) and rest[i + 1].kind == "macro" and rest[i + 1].name == it.name:
                use.children.append(rest[i + 1])
                i += 2
                continue
            out.append(it)
            i += 1
        h.items = [use] + out

    # Visibility, as xlkdl computes it.

    def link(self):
        for h in self.headers.values():
            h.include_headers = []
            for inc in h.includes:
                for t in self.variants(inc["kind"], inc["path"]):
                    h.include_headers.append(self.headers[t])
                    self.headers[t].users.add(h.path)
            h.block_headers = []
            for kind, path in h.blocks:
                for t in self.variants(kind, path):
                    if t not in self.headers:
                        fail(h.path, 0, "a block from %s, which is not converted" % t)
                    h.block_headers.append(self.headers[t])
                    self.headers[t].users.add(h.path)
        self.global_names = {}
        for h in self.headers.values():
            for rule, name in h.symbols:
                self.global_names.setdefault(name, set()).add((rule, h.path))

    def visible(self, h):
        if hasattr(h, "_visible"):
            return h._visible
        seen = set()
        out = {}

        def walk(x):
            if x.path in seen:
                return
            seen.add(x.path)
            for rule, name in x.symbols:
                out.setdefault(name, set()).add(rule)
            for y in x.include_headers:
                walk(y)
            for y in x.block_headers:
                walk(y)
            if x.fragment:
                for u in sorted(x.users):
                    walk(self.headers[u])

        walk(h)
        h._visible = out
        return out

    def kdlize(self, h, text, params=()):
        """C text with names written the way xlkdl renders them back to this exact text."""
        vis = self.visible(h)
        toks = tokenize(text)
        out = []
        i = 0
        while i < len(toks):
            t = toks[i]
            out.append(t.space)
            prev = toks[i - 1].text if i > 0 else None
            if t.kind != "id" or t.text in params or prev in (".", "->", "@"):
                out.append(t.text)
                i += 1
                continue
            if t.text == "__SPRT_ID" and i + 3 < len(toks) and toks[i + 1].text == "(" and toks[i + 2].kind == "id" and toks[i + 3].text == ")":
                name = toks[i + 2].text
                if "id" in vis.get(name, ()) and name not in params:
                    out.append(name)
                else:
                    out.append("__SPRT_ID(%s)" % name)
                    self.report["literal"].append("%s: __SPRT_ID(%s)" % (h.path, name))
                i += 4
                continue
            rule, name = split_name(t.text)
            if rule in ("p", "n"):
                if rule in vis.get(name, ()) and name not in params:
                    out.append(name)
                else:
                    out.append(t.text)
                    if name in self.global_names:
                        self.report["literal"].append("%s: %s" % (h.path, t.text))
                i += 1
                continue
            rules = vis.get(t.text)
            if rules and (rules - {None}):
                out.append("`%s`" % t.text)
            elif not rules and t.text in self.global_names and any(r is None for r, _ in self.global_names[t.text]):
                out.append("`%s`" % t.text)
                self.report["literal"].append("%s: %s (not included)" % (h.path, t.text))
            else:
                out.append(t.text)
            i += 1
        return "".join(out)

    # Writing.

    def comment_lines(self, h, line_from, line_to, indent):
        out = []
        for c in h.comments:
            if "Permission is hereby granted" in c[2] or re.match(r"^\s*clang-format (on|off)\s*$", c[2]):
                continue
            if line_from <= c[0] < line_to:
                for l in c[2].strip("\n").split("\n"):
                    l = l.rstrip()
                    l = re.sub(r"^\s*\*(?!/)\s?", "", l) if c[0] != c[1] else l.strip()
                    out.append(indent + ("// " + l if l else "//"))
        return out

    def cond_text(self, h, cond):
        return self.kdlize(h, substitute(cond, list(h.named.items())) if h.named else cond)

    def alias_of(self, h, it):
        """The macro a `#define NAME __SPRT_NAME` points at, when it is an alias."""
        if it.kind != "macro":
            return None
        rule, name = split_name(it.name)
        if rule is not None:
            return None
        m = re.match(r"^__SPRT_(\w+)(\(([^)]*)\))?$", it.body or "")
        if m and m.group(1) != "ID":
            target = m.group(1)
            if "p" in self.visible(h).get(target, ()):
                args = [a.strip() for a in m.group(3).split(",")] if m.group(2) else None
                if (args is None and it.params is None) or (args is not None and args == (it.params or [])):
                    return target
        return None

    def item_lines(self, h, it, indent, record=False, outer=""):
        cond = []
        own = strip_cond(it.cond, outer) if outer else it.cond
        if own:
            cond = [" if=%s" % kdl_quoted(self.cond_text(h, own))]
        k = it.kind
        if k == "public":
            return [indent + "public" + "".join(cond)]
        if k == "macro":
            rule, name = split_name(it.name)
            alias = self.alias_of(h, it)
            if alias is not None:
                s = "alias %s" % kdl_string(name)
                if alias != name:
                    s += " to=%s" % kdl_string(alias)
                if it.params is not None:
                    s += " params=%s" % kdl_quoted(" ".join(it.params))
                return [indent + s + "".join(cond)]
            s = "macro %s" % kdl_name_of(h.path, "macro", rule, name)
            if it.body:
                s += " %s" % kdl_body(self.kdlize(h, it.body, it.params or ()))
            if it.params is not None:
                s += " params=%s" % kdl_quoted(" ".join(it.params))
            if getattr(it, "public", False):
                s += " public=#true"
            return [indent + s + "".join(cond)]
        if k == "undef":
            rule, name = split_name(it.name)
            return [indent + "undef %s" % kdl_name_of(h.path, "undef", rule, name) + "".join(cond)]
        if k == "error":
            text = it.text
            m = re.match(r'^"([^"\\]*)"$', text)
            if m:
                return [indent + "error %s" % kdl_quoted(m.group(1)) + "".join(cond)]
            if text.startswith('"'):
                return [indent + "error %s" % kdl_quoted(text) + "".join(cond)]
            return [indent + "error %s raw=#true" % kdl_quoted(text) + "".join(cond)]
        if k == "typedef":
            rule, name = split_name(it.name)
            return [indent + "typedef %s %s" % (kdl_name_of(h.path, "typedef", rule, name), kdl_string(self.kdlize(h, it.type))) + "".join(cond)]
        if k == "decl":
            s = "decl %s" % kdl_quoted(self.kdlize(h, it.text))
            if it.extern_c == "macro":
                s += " extern-c=#true"
            elif it.extern_c == "raw":
                s += ' extern-c="raw"'
            return [indent + s + "".join(cond)]
        if k == "field":
            s = "field %s %s" % (kdl_string(it.name), kdl_string(self.kdlize(h, it.type)))
            if it.bits:
                s += " bits=%s" % kdl_quoted(self.kdlize(h, it.bits))
            if it.attr:
                s += " attr=%s" % kdl_quoted(self.kdlize(h, it.attr))
            return [indent + s + "".join(cond)]
        if k == "value":
            s = "value %s" % kdl_name(*split_name(it.name))
            if it.value is not None:
                s += " %s" % kdl_body(self.kdlize(h, it.value))
            return [indent + s + "".join(cond)]
        if k == "use":
            if it.kind_ == "path":
                s = "use %s" % kdl_quoted(gen_path(it.path))
            else:
                s = "use %s=%s" % (it.kind_, kdl_string(it.path))
            if not it.children:
                return [indent + s + "".join(cond)]
            out = [indent + s + "".join(cond) + " {"]
            for c in it.children:
                out.extend(self.item_lines(h, c, indent + "    "))
            out.append(indent + "}")
            return out
        if k == "record" and getattr(it, "macro", None):
            s = "enum %s macro=%s base=%s" % (kdl_string(it.tag), kdl_string(it.macro), kdl_string(self.kdlize(h, it.base)))
            out = [indent + s + "".join(cond) + " {"]
            prev = it.line
            for m in it.members:
                out.extend(self.comment_lines(h, prev, m.line, indent + "    "))
                out.extend(self.item_lines(h, m, indent + "    ", outer=it.cond))
                prev = m.line + 1
            out.append(indent + "}")
            return out
        if k == "record":
            s = it.keyword
            if record:
                if it.member:
                    s += " %s" % kdl_string(it.member)
                if it.tag:
                    s += " tag=%s" % kdl_name(*split_name(it.tag))
            else:
                if it.tag:
                    s += " %s" % kdl_name(*split_name(it.tag))
                if it.typedef:
                    s += " typedef=%s" % kdl_name_of(h.path, "typedef", *split_name(it.typedef))
                if it.also:
                    s += " also=%s" % kdl_quoted(self.kdlize(h, it.also))
                if it.pack:
                    s += " pack=%s" % kdl_quoted(it.pack)
                    if it.pack_cond and it.pack_cond != it.cond:
                        s += " pack-if=%s" % kdl_quoted(self.cond_text(h, it.pack_cond))
                if it.warning:
                    s += " ignore-warning=%s" % kdl_quoted(it.warning)
                if it.base:
                    s += " base=%s" % kdl_quoted(self.kdlize(h, it.base))
            if it.attr:
                s += " attr=%s" % kdl_quoted(self.kdlize(h, it.attr))
            if it.attr_after:
                s += " attr-after=%s" % kdl_quoted(self.kdlize(h, it.attr_after))
            s += "".join(cond)
            members = it.members
            if members is None:
                return [indent + s]
            out = [indent + s + " {"]
            prev = it.line
            for m in members:
                out.extend(self.comment_lines(h, prev, m.line, indent + "    "))
                mc = m.cond
                if m.cond and it.cond and m.cond.startswith(it.cond):
                    mc = m.cond
                out.extend(self.item_lines(h, m, indent + "    ", record=(it.keyword != "enum"), outer=it.cond))
                prev = m.line + 1
            out.append(indent + "}")
            return out
        raise ConvertError("%s:%d: cannot write %s" % (h.path, it.line, k))

    def make_public(self, h):
        """The aliases a description ends with, in the order of their macros and under one
        condition, become `public=#true` on the macros and one `public` node in their place."""
        items = h.items
        run = 0
        while run < len(items):
            it = items[len(items) - 1 - run]
            if it.kind != "macro" or self.alias_of(h, it) != split_name(it.name)[1]:
                break
            run += 1
        if run < 2:
            return
        tail = items[len(items) - run:]
        if len(set(it.cond for it in tail)) != 1 or any(it.params is not None for it in tail):
            return
        macros = {}
        for i, it in enumerate(items[:len(items) - run]):
            rule, name = split_name(it.name) if it.kind == "macro" else (None, None)
            if rule == "p" and name not in macros:
                macros[name] = it
        order = [macros.get(split_name(it.name)[1]) for it in tail]
        if any(m is None or m.params is not None for m in order):
            return
        positions = [items.index(m) for m in order]
        if positions != sorted(positions):
            return
        # Every macro of that name has to be public, or the node writes too few.
        for m in order:
            m.public = True
        node = Item("public", tail[0].line, tail[0].cond)
        h.items = items[:len(items) - run] + [node]

    def description(self, h):
        lines = []
        prev = getattr(h, "start", 1)
        self.make_public(h)
        conds = [it.cond for it in all_items(h.items) if it.cond]
        conds += [it.pack_cond for it in all_items(h.items) if getattr(it, "pack_cond", None)]
        h.named = name_conditions(conds, h.levels, lambda e: cond_name(self.kdlize(h, e)))
        for e, name in h.named.items():
            lines.append("cond %s %s" % (name, kdl_quoted(self.kdlize(h, e))))
        for it in h.items:
            for c in self.comment_lines(h, prev, it.line, ""):
                lines.append(c)
            lines.extend(self.item_lines(h, it, "", record=h.path in MEMBER_FRAGMENTS))
            prev = it.line + 1
            last = self.last_line(it)
            if last:
                prev = max(prev, last + 1)
        # The license block and the guard carry nothing a description needs.
        return "\n".join(lines) + "\n"

    def last_line(self, it):
        best = max(it.line, getattr(it, "last", 0))
        for c in all_items([it]):
            best = max(best, c.line)
        return best

    def kdl_path(self, path):
        if path in MANUAL:
            return MANUAL[path]
        rel = path
        for prefix, repl in (("sprt/wrappers/windows/abi/", "windows/"), ("sprt/c/", "")):
            if rel.startswith(prefix):
                rel = repl + rel[len(prefix):]
                break
        return rel[:-2] + ".kdl"

    def guard_of(self, path):
        path = gen_path(path)
        rest = path
        prefix = ""
        for root, pre in GUARD_SCHEMES:
            if path.startswith(root):
                rest = path[len(root):]
                prefix = pre
                break
        return prefix + re.sub(r"[^A-Z0-9]", "_", rest.upper()) + "_"

    def shared_path(self, paths):
        """Where a description several headers share lives: by who shares it."""
        parts = [re.match(r"^sprt/c/cross/(\w+)_sprt/(?:(\w+)_sprt/)?([\w.]+)\.h$", p) for p in paths]
        if not all(parts):
            return self.kdl_path(paths[0])
        platforms = "+".join(sorted(set(m.group(1) for m in parts)))
        arches = sorted(set(m.group(2) for m in parts if m.group(2)))
        files = sorted(set(m.group(3) for m in parts))
        rel = "cross/shared/" + platforms
        if arches:
            rel += "/" + "+".join(arches)
            if len(arches) != len(set(m.group(2) or "" for m in parts)):
                rel += "+common"
        return "%s/%s.kdl" % (rel, files[0])

    def write(self, out_dir):
        files = {}
        groups = {}
        texts = {}
        for path in sorted(self.headers):
            h = self.headers[path]
            if h.external:
                continue
            if path in MANUAL:
                h.abi = MANUAL[path]
                continue
            if not h.items:
                continue
            text = self.description(h)
            digest = hashlib.md5(text.encode()).hexdigest()
            groups.setdefault(digest, []).append(path)
            texts[digest] = text
        taken = set()
        for digest, paths in groups.items():
            rel = self.kdl_path(paths[0]) if len(paths) == 1 else self.shared_path(paths)
            base, k = rel, 2
            while rel in taken:
                rel, k = base[:-4] + "-%d.kdl" % k, k + 1
            taken.add(rel)
            for p in paths:
                self.headers[p].abi = rel
            files[rel] = texts[digest]
        self.written = set(files)
        files.update(self.map_files())
        return files

    def mirror(self, path):
        """The description xlkdl finds for a generated header when the map names none."""
        g = gen_path(path)
        for root, d in ABI_ROOTS:
            if g.startswith(root) and g.endswith(".h"):
                return d + g[len(root):-2] + ".kdl"
        return None

    def implied_fragment(self, path):
        return re.match(r"^%s/\w+/[^/]" % re.escape(PLATFORM_ROOT), path) is not None

    def like_arches(self):
        """`arch A like=B`: the arches whose forwarders are exactly B's files A has none of."""
        out = {}
        for platform, arches in self.platforms.items():
            def files_of(arch):
                pre = "%s/%s/%s/" % (PLATFORM_ROOT, platform, arch)
                return {p[len(pre):]: self.headers[p] for p in self.headers if p.startswith(pre) and "/" not in p[len(pre):]}
            for a in arches:
                own = files_of(a)
                fwd = {}
                for f, h in own.items():
                    if not h.items and len(h.includes) == 1 and not h.includes[0]["cond"] and h.includes[0]["kind"] == "path":
                        m = re.match(r"^%s/%s/(\w+)/%s$" % (re.escape(PLATFORM_ROOT), platform, re.escape(f)), h.includes[0]["path"])
                        if m:
                            fwd[f] = m.group(1)
                targets = set(fwd.values())
                if len(targets) != 1:
                    continue
                b = targets.pop()
                if b == a or b in out.get(platform, {}):
                    continue
                theirs = files_of(b)
                if set(theirs) - set(own) or any(f not in theirs for f in fwd):
                    continue
                out.setdefault(platform, {})[a] = (b, set(fwd))
        # An arch others are like cannot itself be like another.
        for platform in out:
            for a in list(out[platform]):
                if a in [b for b, _ in out[platform].values()]:
                    del out[platform][a]
        return out

    def part_of(self, path):
        """The part of the map a header is declared in: windows, the OS of a cross fragment, libc."""
        if path.startswith("sprt/wrappers/windows/"):
            return "wrappers-windows"
        m = re.match(r"^sprt/c/cross/(\w+)_sprt/", path)
        if m:
            return m.group(1)
        h = self.headers[path]
        if h.system:
            users = sorted(self.part_of(u) for u in h.users)
            if users:
                return users[0]
        return "libc"

    def header_lines(self, path):
        h = self.headers[path]
        props = []
        if h.external:
            props.append("external=#true")
        mirror = None if h.external else self.mirror(path)
        if h.abi and h.abi != mirror:
            props.append("abi=%s" % kdl_quoted(h.abi))
        if not h.abi and mirror and mirror in self.written:
            raise ConvertError("%s: no description, and %s is at its mirror path" % (path, mirror))
        if not h.external and h.fragment != self.implied_fragment(path):
            props.append("fragment=%s" % ("#true" if h.fragment else "#false"))
        if not h.external:
            if h.guard and (h.fragment or h.guard != self.guard_of(path)):
                props.append("guard=%s" % kdl_string(h.guard))
            elif not h.guard and not h.fragment:
                raise ConvertError("%s: a header without a guard that is not a fragment" % path)
        body = []
        for inc in h.includes:
            s = "include "
            if inc["kind"] == "path":
                target = inc["path"]
                s += kdl_quoted(target if self.headers[target].external else gen_path(target))
            else:
                s += "%s=%s" % (inc["kind"], kdl_string(inc["path"]))
            if inc.get("cond") and not h.external:
                s += " if=%s" % kdl_quoted(self.kdlize(h, inc["cond"]))
            body.append("    " + s)
        if h.external:
            ruled = {}
            for rule, name in h.symbols:
                ruled.setdefault(rule, []).append(name)
            for rule in (None, "p", "id"):
                names = sorted(ruled.get(rule, []))
                for i in range(0, len(names), 6):
                    body.append("    type " + " ".join(kdl_name(rule, n) for n in names[i:i + 6]))
        line = ("header %s %s" % (kdl_quoted(path if h.external else gen_path(path)), " ".join(props))).rstrip()
        if body:
            return [line + " {"] + body + ["}"]
        return [line]

    PART_TITLES = {
        "wrappers-windows": "The Windows API wrappers: the ABI of sprt/gen/wrappers/windows/abi and what it includes.",
        "libc": "The libc ABI shared by every platform: sprt/gen/c/bits, the cross dispatchers, and the\n// hand-written headers they include.",
    }

    def map_files(self):
        parts = {}
        for path in sorted(self.headers, key=lambda p: (not self.headers[p].external, p)):
            parts.setdefault(self.part_of(path), []).append(path)
        order = ["libc", "wrappers-windows"] + sorted(p for p in parts if p not in ("libc", "wrappers-windows"))
        root = [
            "// The header map of the runtime's ABI headers, written by utils/xlkdl/c2kdl.py: the profile they",
            "// are written with, and the parts with the headers - generated from a description, or",
            "// external, written by hand and listing the names the descriptions take from them.",
            "",
            "profile sprt {",
            "    rule p prefix=__SPRT_",
            "    rule id wrap=__SPRT_ID spelled=__sprt_",
            "    rule n prefix=__SPRT_ suffix=_NAME case=upper trim=_t",
            "    license LICENSE.txt",
            "    platform-root %s" % kdl_quoted(gen_path(PLATFORM_ROOT)),
            '    platform-include "SPRT_CROSS_CONFIG_NAME(%s/__SPRT_PLATFORM_NAME/{})"' % gen_path(PLATFORM_ROOT),
            '    arch-include "SPRT_CROSS_CONFIG_NAME(%s/__SPRT_PLATFORM_NAME/__SPRT_ARCH_NAME/{})"' % gen_path(PLATFORM_ROOT),
            "    extern-c begin=__SPRT_BEGIN_DECL end=__SPRT_END_DECL",
        ]
        for r, prefix in GUARD_SCHEMES:
            root.append("    guard-scheme root=%s prefix=%s" % (kdl_quoted(r), kdl_string(prefix)))
        for r, d in DEFAULTS:
            root.append("    defaults root=%s %s" % (kdl_quoted(gen_path(r)), " ".join("%s=%s" % kv for kv in d.items())))
        for r, d in ABI_ROOTS:
            root.append("    abi-root header=%s dir=%s" % (kdl_quoted(r), kdl_quoted(d)))
        root.append("}")
        root.append("")
        files = {}
        likes = self.like_arches()
        for part in order:
            if part not in parts:
                continue
            root.append("import %s" % kdl_quoted("map/%s.kdl" % part))
            out = []
            if part in self.PART_TITLES:
                out.append("// " + self.PART_TITLES[part])
            else:
                out.append("// The %s platform: the cross fragments under %s/%s_sprt." % (part, gen_path(PLATFORM_ROOT), part))
                arches = self.platforms.get(part + "_sprt", [])
                like = likes.get(part + "_sprt", {})
                out.append("")
                if arches:
                    own = [a for a in arches if a not in like]
                    nodes = ["arch %s;" % " ".join(own)] if own else []
                    nodes += ["arch %s like=%s;" % (a, like[a][0]) for a in arches if a in like]
                    out.append("platforms { platform %s_sprt { %s }; }" % (part, " ".join(nodes)))
                else:
                    out.append("platforms { platform %s_sprt; }" % part)
            for path in parts[part]:
                m = re.match(r"^%s/(\w+)/(\w+)/([^/]+)$" % re.escape(PLATFORM_ROOT), path)
                if m and m.group(3) in likes.get(m.group(1), {}).get(m.group(2), (None, ()))[1]:
                    continue
                out.append("")
                out.extend(self.header_lines(path))
            files["map/%s.kdl" % part] = "\n".join(out) + "\n"
        files["sprt.kdl"] = "\n".join(root) + "\n"
        return files


def extract_originals(rev, out):
    """runtime/include of a git revision, unpacked under `out`; returns its path."""
    archive = subprocess.run(["git", "-C", REPO, "archive", rev, "runtime/include"], stdout=subprocess.PIPE, check=True)
    subprocess.run(["tar", "-x", "-C", out], input=archive.stdout, check=True)
    return os.path.join(out, "runtime", "include")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--src", help="the include root with the hand-written headers")
    ap.add_argument("--rev", default=ORIGINALS, help="take runtime/include of this git revision")
    ap.add_argument("--out", default=OUT)
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--report")
    args = ap.parse_args()

    global INCLUDE
    tmp = None
    if args.src:
        INCLUDE = os.path.abspath(args.src)
    else:
        tmp = tempfile.TemporaryDirectory(prefix="c2kdl-")
        INCLUDE = extract_originals(args.rev, tmp.name)

    conv = Converter()
    try:
        conv.load()
        conv.link()
        files = conv.write(args.out)
    except ConvertError as e:
        print("c2kdl: %s" % e, file=sys.stderr)
        return 2

    stale = 0
    for rel, text in sorted(files.items()):
        path = os.path.join(args.out, rel)
        have = open(path).read() if os.path.exists(path) else None
        if have == text:
            continue
        if args.check:
            print("stale: %s" % rel)
            stale += 1
            continue
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w") as f:
            f.write(text)

    report = []
    report.append("external headers: %d" % sum(1 for h in conv.headers.values() if h.external))
    report.append("  system: %s" % ", ".join(sorted(p for p, h in conv.headers.items() if h.system)))
    report.append("descriptions: %d files for %d headers" % (len(files) - 1, sum(1 for h in conv.headers.values() if h.abi)))
    report.append("names left as written (%d):" % len(conv.report["literal"]))
    report.extend("  " + x for x in sorted(set(conv.report["literal"])))
    report.append("dropped before an overridden block (%d):" % len(conv.report["dropped"]))
    report.extend("  " + x for x in conv.report["dropped"])
    text = "\n".join(report) + "\n"
    if args.report:
        with open(args.report, "w") as f:
            f.write(text)
    else:
        print(text, end="")
    return 1 if stale else 0


if __name__ == "__main__":
    sys.exit(main())
