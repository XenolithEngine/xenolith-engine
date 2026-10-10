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

"""apple2kdl: the ABI of the hand-written headers of the Apple `+open` overlay as KDL descriptions.

    utils/xlkdl/apple2kdl.py [--src DIR | --rev REV] [--out runtime/kdl/abi] [--overlay DIR] [--check]

Reads the hand-written headers of runtime/toolchains/target-apple/open/sysroot - from `--src`, or
from the git revision `--rev` (by default the one before the split) - and splits each into its ABI
(macros, typedefs, enums, records) and its API (Objective-C, functions, external variables, the
bracketing macros). The ABI becomes a description under `apple-open/` and an entry of the map
`apple-open.kdl`; xlkdl writes it to `include_libc/__sprt_abi/<spelling>` of the overlay. The API
stays the hand-written header, which includes its ABI after its own imports. The headers the overlay
takes from other generators (the MIG `mach/*.h`, `sys/syscall.h`) and everything that comes from
apple-oss are not touched. `--check` writes nothing and fails when a file is not what it would write.
"""

import argparse
import os
import re
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import c2kdl as C  # noqa: E402

REPO = C.REPO
OVERLAY = os.path.join(REPO, "runtime", "toolchains", "target-apple", "open", "sysroot")
OVERLAY_REL = os.path.relpath(OVERLAY, REPO)

# The revision with the overlay before the split.
ORIGINALS = "ffe9fede"

# Written by other generators: gen-mig-headers.sh, gen-syscall-header.sh.
FOREIGN = ("include_libc/mach/", "include_libc/sys/syscall.h")

ABI_DIR = "__sprt_abi/"
FRAMEWORK = re.compile(r"^System/Library/Frameworks/(\w+)\.framework/Headers/(.+)$")

# Macros alone on a line that open or close a region: the API keeps them.
REGION = re.compile(r"^[A-Za-z_]\w*$")
NONNULL_BEGIN = ("NS_ASSUME_NONNULL_BEGIN", "CF_ASSUME_NONNULL_BEGIN")
NONNULL_END = ("NS_ASSUME_NONNULL_END", "CF_ASSUME_NONNULL_END")


def spelling(rel):
    """How a header of the overlay is included: <Metal/MTLTexture.h>, <math.h>."""
    m = FRAMEWORK.match(rel)
    if m:
        return "%s/%s" % (m.group(1), m.group(2))
    if rel.startswith("include_libc/"):
        return rel[len("include_libc/"):]
    raise C.ConvertError("%s: not a header of the sysroot" % rel)


def abi_spelling(rel):
    return ABI_DIR + spelling(rel)


class Statement:
    def __init__(self, kind, first, last, text, word=None):
        self.kind = kind      # define, cond, include, pp, region, objc, c
        self.first = first    # 0-based line range in the header, comments before it included
        self.last = last
        self.text = text      # the code, comments blanked
        self.word = word
        self.side = None      # abi, api, both


def statements(path, text):
    """The top-level statements of a header, each a range of whole lines."""
    code, _ = C.strip_comments(text)
    lines = code.split("\n")
    out = []
    i = 0
    n = len(lines)
    while i < n:
        s = lines[i].strip()
        if not s:
            i += 1
            continue
        start = i
        if s.startswith("#"):
            j = i
            while lines[j].rstrip().endswith("\\") and j + 1 < n:
                j += 1
            word = re.match(r"^#\s*(\w*)", s).group(1)
            if word in ("define", "undef"):
                kind = "define"
            elif word in ("if", "ifdef", "ifndef", "elif", "else", "endif"):
                kind = "cond"
            elif word in ("include", "import"):
                kind = "include"
            else:
                kind = "pp"
            out.append(Statement(kind, start, j, "\n".join(lines[start:j + 1]), word))
            i = j + 1
            continue
        if REGION.match(s) or re.match(r'^extern\s+"C"\s*\{$', s) or s == "}":
            out.append(Statement("region", start, i, s, s))
            i += 1
            continue
        if s.startswith("@"):
            j = i
            if not (s.startswith("@class") or re.match(r"^@protocol\s+[\w\s,]+;", s)):
                while j < n and lines[j].strip() != "@end" and not lines[j].strip().startswith("@end"):
                    j += 1
                if j >= n:
                    C.fail(path, start + 1, "an Objective-C block without @end")
            else:
                while j < n and ";" not in lines[j]:
                    j += 1
            out.append(Statement("objc", start, j, "\n".join(lines[start:j + 1])))
            i = j + 1
            continue
        # A C statement: to the ';' at depth 0, or the '}' that closes a function's body.
        depth = 0
        j = i
        done = False
        buf = []
        while j < n and not done:
            ln = lines[j]
            if ln.strip().startswith("#") and depth == 0 and j > i:
                C.fail(path, j + 1, "a directive inside a statement at the top")
            for k, c in enumerate(ln):
                if c in "({[":
                    depth += 1
                elif c in ")}]":
                    depth -= 1
                    if c == "}" and depth == 0:
                        head = "".join(buf) + ln[:k]
                        head = head[:head.find("{")] if "{" in head else head
                        body_ends = re.match(r"^\s*namespace\b", head) or (
                            "(" in head and not re.match(r"^\s*(typedef|struct|union|enum)\b", head) and "=" not in head)
                        if body_ends:
                            rest = ln[k + 1:].strip()
                            if rest and not rest.startswith("//"):
                                C.fail(path, j + 1, "code after a function body on its line")
                            done = True
                            break
                elif c == ";" and depth == 0:
                    rest = ln[k + 1:].strip()
                    if rest:
                        C.fail(path, j + 1, "two statements on one line")
                    done = True
                    break
            buf.append(ln + "\n")
            j += 1
        if not done:
            C.fail(path, start + 1, "an unterminated statement")
        out.append(Statement("c", start, j - 1, "".join(buf)))
        i = j
    # Comments and blank lines belong to the statement after them.
    prev = -1
    for st in out:
        st.first = prev + 1
        prev = st.last
    return out, len(lines)


def c_name(text):
    """The name a C declaration declares: a function's, or the last of a variable's declarator."""
    t = re.sub(r"\s+", " ", text).strip()
    m = re.search(r"(\w+)\s*\(", t)
    if m and not t.startswith("typedef"):
        # `CG_EXTERN CGPoint CGPointMake(...)`: the identifier before the first '(' that is not a macro call.
        for m in re.finditer(r"(\w+)\s*\(", t):
            if not re.match(r"^[A-Z][A-Z0-9_]+$", m.group(1)):
                return m.group(1)
    names = re.findall(r"\b([A-Za-z_]\w*)\b\s*(?:\[[^\]]*\])?\s*(?:=[^;]*)?;\s*$", t)
    return names[-1] if names else None


def typedef_names(text):
    t = re.sub(r"\s+", " ", text)
    m = re.match(r"^\s*typedef\s+(NS_ENUM|NS_OPTIONS|CF_ENUM|CF_OPTIONS)\s*\(\s*[^,]+,\s*(\w+)\s*\)", t)
    if m:
        return [m.group(2)]
    t = re.sub(r"\{.*\}", " ", t)
    m = re.findall(r"(\w+)\s*(?:\)\s*\([^)]*\))?\s*(?:\[[^\]]*\])?\s*[,;]", t)
    return m


def classify(path, sts):
    """abi, api or both for each statement, by the rules of Р37."""
    guard = None
    conds = [s for s in sts if s.kind in ("cond", "define")]
    if len(conds) >= 2 and conds[0].word == "ifndef" and conds[1].word == "define":
        g = conds[0].text.split()[-1]
        if conds[1].text.split()[1] == g:
            guard = g
    nonnull = False
    api_names = set()
    candidates = []
    for st in sts:
        st.nonnull = nonnull
        if st.kind == "region":
            st.side = "api"
            if st.word in NONNULL_BEGIN:
                nonnull = True
            elif st.word in NONNULL_END:
                nonnull = False
        elif st.kind in ("cond", "include"):
            st.side = "both"
        elif st.kind == "define":
            st.side = "abi"
            if guard and st.word == "define" and st.text.split()[1] == guard:
                st.side = "guard"
        elif st.kind == "pp":
            st.side = "abi" if st.word in ("error", "warning") else "api"
        elif st.kind == "objc":
            st.side = "api"
            for m in re.finditer(r"@(?:interface|protocol)\s+(\w+)", st.text):
                api_names.add(m.group(1))
            m = re.match(r"^\s*@class\s+([^;]+);", st.text)
            if m:
                api_names.update(x.strip() for x in m.group(1).split(","))
        else:
            t = st.text.strip()
            if re.match(r"^(typedef|struct|union|enum)\b", t):
                candidates.append(st)
                st.side = "abi"
                if "^" in t or (st.nonnull and "*" in t) or re.search(r"\bid\s*<|@", t):
                    st.side = "api"
                # A record with C++ member functions: not something a description holds.
                inner = t[t.find("{") + 1:] if "{" in t else ""
                if re.search(r"\)\s*(const\s*)?\{|\)\s*:\s*\w+\s*\(|\boperator\b", inner):
                    st.side = "api"
            else:
                st.side = "api"
                name = c_name(t)
                if name:
                    api_names.add(name)
    # A macro the header takes back with #undef is scaffolding of its API (the parameters of an
    # included list of prototypes), not ABI: it stays there with its #undef.
    undefined = {st.text.split()[1] for st in sts if st.kind == "define" and st.word == "undef" and len(st.text.split()) > 1}
    for st in sts:
        if st.kind == "define" and len(st.text.split()) > 1:
            name = re.match(r"^\s*#\s*\w+\s+(\w+)", st.text).group(1)
            if name in undefined:
                st.side = "api"
    # An include between the definition of such a macro and its #undef is what it parametrises.
    open_scaffold = set()
    for st in sts:
        if st.kind == "define" and len(st.text.split()) > 1:
            name = re.match(r"^\s*#\s*\w+\s+(\w+)", st.text).group(1)
            if name in undefined:
                if st.word == "define":
                    open_scaffold.add(name)
                else:
                    open_scaffold.discard(name)
        elif st.kind == "include" and open_scaffold:
            st.side = "api"
    if guard:
        for st in sts:
            if st.kind == "cond" and st.word == "ifndef" and st.text.split()[-1] == guard:
                st.side = "guard"
                break
        for st in reversed(sts):
            if st.kind == "cond" and st.word == "endif":
                st.side = "guard"
                break
    # What the API declares - and what a typedef left in it names - keeps an ABI candidate there.
    changed = True
    while changed:
        changed = False
        for st in candidates:
            if st.side == "api":
                names = typedef_names(st.text)
                if not set(names) <= api_names:
                    api_names.update(names)
                    changed = True
                continue
            words = set(re.findall(r"\b[A-Za-z_]\w*\b", st.text))
            if words & api_names:
                st.side = "api"
                api_names.update(typedef_names(st.text))
                changed = True
    return guard


def drop_empty_conditionals(lines):
    """Remove #if ... #endif groups left with nothing but comments and blank lines in them."""
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
                stack.append([i, True])
            elif d in ("elif", "else") and stack:
                pass
            elif d == "endif" and stack:
                start, _ = stack.pop()
                inner = lines[start + 1:i]
                code, _ = C.strip_comments("\n".join(inner))
                if all(not x.strip() or directive(x) in ("elif", "else") for x in code.split("\n")):
                    del lines[start:i + 1]
                    changed = True
                    break
    return lines


def squeeze(lines):
    out = []
    for ln in lines:
        if not ln.strip() and out and not out[-1].strip():
            continue
        out.append(ln)
    while out and not out[-1].strip():
        out.pop()
    return out


def split(path, text):
    """(the API header, the synthetic ABI header, whether there is any ABI) of one overlay header."""
    sts, n = statements(path, text)
    guard = classify(path, sts)
    src = text.split("\n")
    side = [None] * n
    for st in sts:
        for k in range(st.first, st.last + 1):
            side[k] = st.side
    abi_any = any(st.side == "abi" for st in sts)
    api, abi = [], []
    for k, ln in enumerate(src):
        s = side[k]
        if s is None:
            api.append(ln)
            continue
        if s in ("api", "guard"):
            api.append(ln)
        elif s == "abi":
            abi.append(ln)
        else:
            api.append(ln)
            abi.append(ln)
    if not abi_any:
        return text, None, False
    # A quoted include is found next to the header: the ABI, which is elsewhere, names it whole.
    base = os.path.dirname(spelling(path))
    abi = [re.sub(r'^(\s*#\s*(?:include|import)\s*)"([^"]+)"',
                  lambda m: "%s<%s>" % (m.group(1), os.path.normpath(os.path.join(base, m.group(2)))), ln) for ln in abi]
    # The ABI goes in after the API's first block of includes, outside every region.
    inc = "#include <%s>" % abi_spelling(path)
    pos = None
    seen_code = False
    for k, ln in enumerate(api):
        d = re.match(r"^\s*#\s*(include|import)\b", ln)
        if d and not seen_code:
            pos = k + 1
        elif ln.strip() and not ln.strip().startswith(("#", "/", "*")):
            seen_code = True
    if pos is None:
        gpos = next((k for k, ln in enumerate(api) if guard and re.match(r"^\s*#\s*define\s+%s\b" % re.escape(guard), ln)), None)
        pos = gpos + 1 if gpos is not None else 0
    api.insert(pos, inc)
    api = squeeze(drop_empty_conditionals(api))
    # The licence comment and the include guard are the generator's in the ABI.
    abi = squeeze(drop_empty_conditionals(abi))
    return "\n".join(api) + "\n", "\n".join(abi) + "\n", True


def guard_last(items):
    """In a run of macros under `!defined(X)`, the definition of X itself goes last: the generator
    tests each node's condition anew, and X defined first would turn the rest of the run off."""
    out = list(items)
    i = 0
    while i < len(out):
        it = out[i]
        m = None
        if it.kind == "macro" and it.cond:
            name = re.match(r"^\w+", it.name).group(0)
            if re.search(r"(^|&& )!defined\(%s\)( &&|$)" % re.escape(name), it.cond):
                m = name
        if m:
            key = "!defined(%s)" % m
            j = i + 1
            while j < len(out) and out[j].cond and key in out[j].cond and not (
                    out[j].kind == "macro" and re.match(r"^\w+", out[j].name).group(0) == m):
                j += 1
            if j > i + 1:
                out.insert(j - 1, out.pop(i))
                continue
        i += 1
    return out


class AppleConverter(C.Converter):
    def __init__(self, synthetic, api_texts):
        super().__init__()
        self.synthetic = synthetic    # abi spelling -> text
        self.api_texts = api_texts    # api spelling -> text

    def load(self):
        for p in sorted(self.synthetic):
            self.parse(p)
            h = self.headers[p]
            h.guard = self.guard_of(p)
        pending = True
        while pending:
            pending = False
            for h in list(self.headers.values()):
                for inc in h.includes:
                    target = inc["path"]
                    if target not in self.headers:
                        t = self.header(target)
                        t.external = True
                        if target in self.api_texts:
                            C.scan_external(t, self.api_texts[target])
                        else:
                            t.system = True
                        pending = True

    def parse(self, path):
        h = self.header(path)
        text = self.synthetic[path]
        h.text = text
        h.fragment = False
        events, comments, guard, levels = C.events_of(path, text)
        h.comments = [c for c in comments if "Copyright" not in c[2]]
        h.levels = levels
        parser = C.Parser(path, events)
        h.items = guard_last(parser.parse())
        self.place_includes(h, parser)
        self.add_symbols(h)

    def kdl_path(self, path):
        return "apple-open/" + path[len(ABI_DIR):-2] + ".kdl"

    def mirror(self, path):
        return self.kdl_path(path) if path.startswith(ABI_DIR) else None

    def implied_fragment(self, path):
        return False

    def guard_of(self, path):
        return "__SPRT_OPEN_ABI_" + re.sub(r"[^A-Z0-9]", "_", path[len(ABI_DIR):].upper()) + "_"

    def part_of(self, path):
        rest = path[len(ABI_DIR):] if path.startswith(ABI_DIR) else path
        top = rest.split("/")[0]
        if "/" in rest and os.path.isdir(os.path.join(OVERLAY, "System/Library/Frameworks/%s.framework" % top)):
            return top
        return "include_libc"

    def map_files(self):
        parts = {}
        for path in sorted(self.headers, key=lambda p: (not self.headers[p].external, p)):
            parts.setdefault(self.part_of(path), []).append(path)
        root = [
            "// The header map of the ABI of the hand-written headers of the Apple +open overlay, written by",
            "// utils/xlkdl/apple2kdl.py. The generated headers go to include_libc/__sprt_abi of",
            "// runtime/toolchains/target-apple/open/sysroot; the external ones are the hand-written API",
            "// headers of the overlay and the headers of the sysroot.",
            "",
            "profile apple-open {",
            "    license LICENSE.txt",
            "    guard-scheme root=%s prefix=__SPRT_OPEN_ABI_" % C.kdl_quoted(ABI_DIR),
            "    abi-root header=%s dir=\"apple-open/\"" % C.kdl_quoted(ABI_DIR),
            "}",
            "",
        ]
        files = {}
        for part in sorted(parts):
            root.append("import %s" % C.kdl_quoted("map/apple-open/%s.kdl" % part))
            out = ["// %s: the ABI of its hand-written headers, and what they include." % part]
            for path in parts[part]:
                out.append("")
                out.extend(self.header_lines(path))
            files["map/apple-open/%s.kdl" % part] = "\n".join(out) + "\n"
        files["apple-open.kdl"] = "\n".join(root) + "\n"
        return files


def overlay_headers(root):
    out = []
    for d, _, fs in os.walk(root):
        for f in fs:
            if not f.endswith(".h"):
                continue
            rel = os.path.relpath(os.path.join(d, f), root)
            if rel.startswith(FOREIGN) or rel.startswith("include_libc/" + ABI_DIR):
                continue
            out.append(rel)
    return sorted(out)


def extract(rev, out):
    archive = subprocess.run(["git", "-C", REPO, "archive", rev, OVERLAY_REL], stdout=subprocess.PIPE, check=True)
    subprocess.run(["tar", "-x", "-C", out], input=archive.stdout, check=True)
    return os.path.join(out, OVERLAY_REL)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--src", help="the overlay with the headers before the split")
    ap.add_argument("--rev", default=ORIGINALS)
    ap.add_argument("--out", default=C.OUT)
    ap.add_argument("--overlay", default=OVERLAY, help="where the API headers are written")
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()

    tmp = None
    if args.src:
        src = os.path.abspath(args.src)
    else:
        tmp = tempfile.TemporaryDirectory(prefix="apple2kdl-")
        src = extract(args.rev, tmp.name)

    # The converter's knobs, for this map: no name rules, no platforms, paths as they are included.
    C.DEFAULTS = []
    C.ABI_ROOTS = [(ABI_DIR, "apple-open/")]
    C.PLATFORM_ROOT = "__none__"
    C.gen_path = lambda p: p
    C.split_name = lambda raw: (None, raw)
    C.NAME_MACROS.clear()

    synthetic, api_texts, api_files = {}, {}, {}
    try:
        for rel in overlay_headers(src):
            with open(os.path.join(src, rel)) as f:
                text = f.read()
            api, abi, any_abi = split(rel, text)
            api_texts[spelling(rel)] = api
            api_files[rel] = api
            if any_abi:
                synthetic[abi_spelling(rel)] = abi
        conv = AppleConverter(synthetic, api_texts)
        conv.load()
        conv.link()
        files = conv.write(args.out)
    except C.ConvertError as e:
        print("apple2kdl: %s" % e, file=sys.stderr)
        return 2

    stale = 0
    outputs = [(os.path.join(args.out, rel), text) for rel, text in files.items()]
    outputs += [(os.path.join(args.overlay, rel), text) for rel, text in api_files.items()]
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
    split_count = len(synthetic)
    print("headers: %d, with an ABI part: %d, descriptions: %d" % (len(api_files), split_count,
                                                                  sum(1 for r in files if r.startswith("apple-open/"))))
    return 1 if stale else 0


if __name__ == "__main__":
    sys.exit(main())
