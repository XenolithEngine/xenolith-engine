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

"""Shared helpers for the porting scripts in utils/porting.

The scripts read the engine tree, toolchains and sysroots and report what they find. None of them
writes into the engine: temporary files go to a private temporary directory that is removed on exit.
"""

import json
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile

ENGINE_ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))

# Status of one report item. FAIL is a required piece that is missing or wrong; WARN is a likely
# problem or a missing optional piece; INFO is a fact worth knowing; SKIP is a check that could not run.
OK, FAIL, WARN, INFO, SKIP = "OK", "FAIL", "WARN", "INFO", "SKIP"

_COLORS = {OK: "32", FAIL: "31", WARN: "33", INFO: "36", SKIP: "90"}


class Report:
    """Collects checks grouped by plan step and prints them as text or JSON."""

    def __init__(self, title, use_json=False, color=None, quiet=False):
        self.title = title
        self.use_json = use_json
        self.quiet = quiet
        self.color = sys.stdout.isatty() if color is None else color
        self.sections = []
        self._current = None

    def section(self, step, title):
        self._current = {"step": step, "title": title, "items": []}
        self.sections.append(self._current)
        return self

    def add(self, status, message, hint=None, details=None):
        if self._current is None:
            self.section("", "")
        item = {"status": status, "message": message}
        if hint:
            item["hint"] = hint
        if details:
            item["details"] = list(details)
        self._current["items"].append(item)
        return status

    def ok(self, message, **kw):
        return self.add(OK, message, **kw)

    def fail(self, message, **kw):
        return self.add(FAIL, message, **kw)

    def warn(self, message, **kw):
        return self.add(WARN, message, **kw)

    def info(self, message, **kw):
        return self.add(INFO, message, **kw)

    def skip(self, message, **kw):
        return self.add(SKIP, message, **kw)

    def check(self, condition, message, hint=None, missing=FAIL, details=None):
        """OK when the condition holds, otherwise `missing` (FAIL by default) with the hint."""
        if condition:
            return self.ok(message, details=details)
        return self.add(missing, message, hint=hint, details=details)

    def counts(self):
        result = {OK: 0, FAIL: 0, WARN: 0, INFO: 0, SKIP: 0}
        for s in self.sections:
            for i in s["items"]:
                result[i["status"]] += 1
        return result

    def _paint(self, status):
        text = "%-4s" % status
        if self.color:
            return "\033[%sm%s\033[0m" % (_COLORS[status], text)
        return text

    def finish(self):
        """Prints the report and returns the process exit code (1 when anything FAILed)."""
        counts = self.counts()
        if self.use_json:
            json.dump({"title": self.title, "sections": self.sections, "counts": counts}, sys.stdout,
                      indent=2, ensure_ascii=False)
            sys.stdout.write("\n")
        else:
            print("# " + self.title)
            for s in self.sections:
                items = s["items"]
                if self.quiet:
                    items = [i for i in items if i["status"] in (FAIL, WARN)]
                    if not items:
                        continue
                header = ("%s. %s" % (s["step"], s["title"])) if s["step"] else s["title"]
                print("\n== " + header)
                for i in items:
                    print("  [%s] %s" % (self._paint(i["status"]), i["message"]))
                    for d in i.get("details", [])[:40]:
                        print("         " + d)
                    if len(i.get("details", [])) > 40:
                        print("         ... %d more" % (len(i["details"]) - 40))
                    if "hint" in i and i["status"] in (FAIL, WARN):
                        print("         hint: " + i["hint"])
            print("\nsummary: %d ok, %d fail, %d warn, %d info, %d skip"
                  % (counts[OK], counts[FAIL], counts[WARN], counts[INFO], counts[SKIP]))
        return 1 if counts[FAIL] else 0


def add_common_args(parser):
    parser.add_argument("--engine", default=ENGINE_ROOT, help="engine root (default: this checkout)")
    parser.add_argument("--json", action="store_true", help="print the report as JSON")
    parser.add_argument("--quiet", "-q", action="store_true", help="print only FAIL and WARN items")
    parser.add_argument("--no-color", action="store_true", help="never color the output")


def make_report(args, title):
    return Report(title, use_json=args.json, color=False if args.no_color else None, quiet=args.quiet)


class Engine:
    """Read-only access to an engine checkout."""

    def __init__(self, root):
        self.root = os.path.abspath(root)

    def path(self, rel):
        return os.path.join(self.root, rel)

    def exists(self, rel):
        return os.path.exists(self.path(rel))

    def isdir(self, rel):
        return os.path.isdir(self.path(rel))

    def read(self, rel):
        try:
            with open(self.path(rel), "r", encoding="utf-8", errors="replace") as f:
                return f.read()
        except OSError:
            return None

    def listdir(self, rel):
        try:
            return sorted(os.listdir(self.path(rel)))
        except OSError:
            return []

    def contains(self, rel, pattern, flags=0):
        """True when the file matches the regex; None when the file does not exist."""
        text = self.read(rel)
        if text is None:
            return None
        return re.search(pattern, text, flags) is not None

    def git_tag(self):
        try:
            out = subprocess.run(["git", "-C", self.root, "describe", "--tags"], capture_output=True,
                                 text=True, timeout=30)
            return out.stdout.strip() if out.returncode == 0 else None
        except (OSError, subprocess.SubprocessError):
            return None


def word(token):
    """A regex that finds `token` not glued to other letters or digits (underscores may touch it)."""
    return r"(?<![A-Za-z0-9])" + re.escape(token) + r"(?![A-Za-z0-9])"


def platform_aliases(engine, sprt):
    """The platform macro and every group macro __sprt_def.h derives from it (SPRT_HOSTED_RTOS, ...)."""
    text = engine.read("runtime/include/sprt/c/bits/__sprt_def.h") or ""
    aliases = [sprt]
    for cond, group in re.findall(r"#if ([^\n]*)\n#define (SPRT_[A-Z_]+) 1\n", text):
        if re.search(word(sprt), cond):
            aliases.append(group)
    if sprt in ("SPRT_MACOS", "SPRT_IOS", "SPRT_DARWIN_UNKNOWN"):
        aliases.append("SPRT_APPLE")  # defined inside the __APPLE__ branch for every Apple platform
    return aliases


def make_list(text, name):
    """The words of a simple `NAME := a b c` (or `=`) make assignment, following `\\` continuations."""
    if text is None:
        return None
    m = re.search(r"^\s*" + re.escape(name) + r"\s*[:?+]?=\s*((?:.*\\\n)*.*)$", text, re.M)
    if not m:
        return None
    return m.group(1).replace("\\\n", " ").split()


def make_rule_exists(text, target):
    return text is not None and re.search(r"^" + re.escape(target) + r"\s*:(?!=)", text, re.M) is not None


# Triples ----------------------------------------------------------------------------------------

ARCH_ALIASES = {"arm64": "aarch64", "amd64": "x86_64", "x64": "x86_64", "i386": "i686", "x86": "i686"}

# Engine architecture name -> (LLVM backend for LLVM_TARGETS_TO_BUILD, kernel ARCH=, bits).
KNOWN_ARCHES = {
    "x86_64": ("X86", "x86", 64),
    "i686": ("X86", "x86", 32),
    "aarch64": ("AArch64", "arm64", 64),
    "armv7a": ("ARM", "arm", 32),
    "arm": ("ARM", "arm", 32),
    "riscv64": ("RISCV", "riscv", 64),
    "riscv32": ("RISCV", "riscv", 32),
    "loongarch64": ("LoongArch", "loongarch", 64),
    "loongarch32": ("LoongArch", "loongarch", 32),
    "ppc64le": ("PowerPC", "powerpc", 64),
    "ppc64": ("PowerPC", "powerpc", 64),
    "s390x": ("SystemZ", "s390", 64),
    "mips64": ("Mips", "mips", 64),
    "wasm32": ("WebAssembly", None, 32),
    "wasm64": ("WebAssembly", None, 64),
    "e2k": (None, "e2k", 64),
}


def normalize_arch(arch):
    return ARCH_ALIASES.get(arch, arch)


def parse_triple(triple):
    """Splits `<arch>-<vendor>-<os>[-<env>][+<variant>]` into a dict."""
    base, _, variant = triple.partition("+")
    parts = base.split("-")
    result = {"triple": triple, "base": base, "variant": variant, "arch": normalize_arch(parts[0]),
              "vendor": parts[1] if len(parts) > 1 else "", "os": parts[2] if len(parts) > 2 else "",
              "env": "-".join(parts[3:]) if len(parts) > 3 else ""}
    return result


def host_triple():
    """This machine's triple, spelled the way make/utils/init-sh.mk spells it."""
    u = os.uname() if hasattr(os, "uname") else None
    if u is None:
        machine = os.environ.get("PROCESSOR_ARCHITECTURE", "AMD64").lower()
        return ("aarch64" if machine == "arm64" else "x86_64") + "-pc-windows-msvc"
    arch = normalize_arch(u.machine)
    if u.sysname == "Darwin":
        return arch + "-apple-macosx"
    if u.sysname == "Linux":
        libc = "gnu"
        try:
            with open("/bin/sh", "rb") as f:
                if b"ld-musl-" in f.read(4096):
                    libc = "musl"
        except OSError:
            pass
        return arch + "-unknown-linux-" + libc
    return arch + "-unknown-" + u.sysname.lower()


def toolchain_dirs(engine, kind, triple):
    """Candidate directories for hosts/<triple> or targets/<triple>, in build-system lookup order."""
    return [engine.path(os.path.join("toolchains", kind, triple)),
            engine.path(os.path.join("runtime", "toolchains", kind, triple))]


def find_toolchain_dir(engine, kind, triple):
    for d in toolchain_dirs(engine, kind, triple):
        if os.path.isdir(d):
            return d
    return None


def find_host_clang(engine, triple=None):
    """The clang of a host toolchain that runs here, or None."""
    candidates = [triple] if triple else [host_triple()]
    if not triple and candidates[0].endswith("-musl"):
        candidates.append(candidates[0][:-4] + "gnu")
    for t in candidates:
        d = find_toolchain_dir(engine, "hosts", t)
        if d:
            for name in ("clang", "clang.exe"):
                p = os.path.join(d, "bin", name)
                if os.path.exists(p):
                    return p
    return shutil.which("clang")


def clang_resource_include(clang):
    """lib/clang/<N>/include next to the given clang, as host.mk passes it with -idirafter."""
    root = os.path.dirname(os.path.dirname(os.path.realpath(clang)))
    base = os.path.join(root, "lib", "clang")
    for v in sorted(os.listdir(base), reverse=True) if os.path.isdir(base) else []:
        inc = os.path.join(base, v, "include")
        if os.path.isfile(os.path.join(inc, "stddef.h")):
            return inc
    return None


# Make --------------------------------------------------------------------------------------------

def make_program():
    for name in ("gmake", "make"):
        p = shutil.which(name)
        if p:
            try:
                out = subprocess.run([p, "--version"], capture_output=True, text=True, timeout=10)
                if "GNU Make" in out.stdout:
                    return p
            except (OSError, subprocess.SubprocessError):
                pass
    return None


def eval_make_vars(makefiles, names, extra=None, cwd=None, program=None):
    """Includes `makefiles` into a scratch makefile and returns the expanded values of `names`.

    Returns None when GNU make is not available or the evaluation fails.
    """
    program = program or make_program()
    if not program:
        return None
    lines = list(extra or [])
    lines += ["include %s" % m for m in makefiles]
    lines += ["$(info @@%s=$(%s))" % (n, n) for n in names]
    lines += [".PHONY: __xlport_all", "__xlport_all: ; @:"]
    with tempfile.TemporaryDirectory(prefix="xlport-") as tmp:
        mk = os.path.join(tmp, "eval.mk")
        with open(mk, "w") as f:
            f.write("\n".join(lines) + "\n")
        try:
            out = subprocess.run([program, "-s", "--no-print-directory", "-f", mk, "__xlport_all"],
                                 capture_output=True, text=True, timeout=60, cwd=cwd or tmp)
        except (OSError, subprocess.SubprocessError):
            return None
    if out.returncode != 0:
        return None
    result = {}
    for line in out.stdout.splitlines():
        if line.startswith("@@") and "=" in line:
            k, _, v = line[2:].partition("=")
            result[k] = v.strip()
    return result


TARGET_MK_VARS = ["TARGET_SYSROOT", "TARGET_SYSTEM", "TARGET_NAME", "TARGET_ARCH",
                  "TARGET_GENERAL_CFLAGS", "TARGET_GENERAL_CXXFLAGS", "TARGET_GENERAL_LDFLAGS",
                  "TARGET_GENERAL_SFLAGS", "TARGET_INCLUDE_DIR", "TARGET_INCLUDE_DIR_LIBC",
                  "TARGET_LIB_DIR", "TARGET_LIB_DIR_LIBC"]

HOST_MK_VARS = ["HOST_ROOT", "HOST_BINDIR", "HOST_CC", "HOST_CXX", "HOST_AR", "HOST_GLSLANG",
                "HOST_SPIRV_LINK", "HOST_GENERAL_CFLAGS", "HOST_GENERAL_CXXFLAGS", "HOST_GENERAL_LDFLAGS"]


def read_toolchain_mk(path, names):
    """Values of a host.mk/target.mk: through GNU make when possible, else a plain textual reading."""
    values = eval_make_vars([os.path.abspath(path)], names)
    if values is not None:
        return values, "make"
    text = open(path, encoding="utf-8", errors="replace").read()
    root = os.path.dirname(os.path.abspath(path))
    values = {}
    for m in re.finditer(r"^([A-Z_]+)\s*[:?]?=\s*(.*)$", text, re.M):
        values[m.group(1)] = m.group(2).strip()
    for k in list(values):
        v = values[k]
        v = v.replace("$(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))", root)
        for _ in range(4):
            v = re.sub(r"\$\(([A-Z_]+)\)", lambda m: values.get(m.group(1), ""), v)
        values[k] = v
    return {n: values.get(n, "") for n in names}, "text"


# Binaries ----------------------------------------------------------------------------------------

ELF_MACHINES = {3: "i686", 8: "mips", 20: "ppc", 21: "ppc64", 22: "s390x", 40: "arm", 43: "sparc64",
                62: "x86_64", 183: "aarch64", 243: "riscv", 258: "loongarch"}
PE_MACHINES = {0x14C: "i686", 0x8664: "x86_64", 0xAA64: "aarch64", 0x1C4: "arm", 0xA641: "arm64ec"}
MACHO_CPUS = {0x01000007: "x86_64", 0x0100000C: "aarch64", 7: "i686", 12: "arm"}


def _elf_arch(head):
    if len(head) < 20:
        return None
    is64 = head[4] == 2
    endian = "<" if head[5] == 1 else ">"
    machine = struct.unpack(endian + "H", head[18:20])[0]
    name = ELF_MACHINES.get(machine, "elf-machine-%d" % machine)
    if name in ("riscv", "loongarch"):
        name += "64" if is64 else "32"
    if name == "mips" and is64:
        name = "mips64"
    if name == "ppc64" and endian == "<":
        name = "ppc64le"
    return name


def _object_arch(head):
    """(format, arch) of an object file header, or None when the format is unknown."""
    if head[:4] == b"\x7fELF":
        return "elf", _elf_arch(head)
    if head[:4] in (b"\xcf\xfa\xed\xfe", b"\xce\xfa\xed\xfe"):
        return "mach-o", MACHO_CPUS.get(struct.unpack("<I", head[4:8])[0], "unknown")
    if head[:4] == b"\xca\xfe\xba\xbe" and len(head) >= 8:
        count = struct.unpack(">I", head[4:8])[0]
        cpus = []
        for i in range(min(count, 16)):
            off = 8 + i * 20
            if len(head) >= off + 4:
                cpus.append(MACHO_CPUS.get(struct.unpack(">I", head[off:off + 4])[0], "unknown"))
        return "mach-o-fat", "universal:" + ",".join(cpus)
    if head[:4] == b"\x00asm":
        return "wasm", "wasm"
    if head[:2] == b"BC" or head[:4] == b"\xde\xc0\x17\x0b":
        return "llvm-bitcode", None
    if head[:2] == b"MZ" and len(head) >= 0x40:
        off = struct.unpack("<I", head[0x3C:0x40])[0]
        if len(head) >= off + 6 and head[off:off + 4] == b"PE\0\0":
            return "pe", PE_MACHINES.get(struct.unpack("<H", head[off + 4:off + 6])[0], "unknown")
        return "pe", None
    if head[:4] == b"\x00\x00\xff\xff" and len(head) >= 8:  # COFF short import record
        return "coff-import", PE_MACHINES.get(struct.unpack("<H", head[6:8])[0], "unknown")
    if len(head) >= 2 and struct.unpack("<H", head[:2])[0] in PE_MACHINES:
        return "coff", PE_MACHINES[struct.unpack("<H", head[:2])[0]]
    return None


def binary_info(path, max_members=64):
    """(format, arch) of an executable, object or static archive; arch None when unknown."""
    try:
        with open(path, "rb") as f:
            head = f.read(4096)
            if head[:8] != b"!<arch>\n":
                if head[:2] == b"MZ":
                    f.seek(0)
                    head = f.read(1 << 16)
                return _object_arch(head) or ("unknown", None)
            pos = 8
            for _ in range(max_members):
                f.seek(pos)
                hdr = f.read(60)
                if len(hdr) < 60:
                    break
                name = hdr[:16].decode("latin-1").strip()
                try:
                    size = int(hdr[48:58].decode("latin-1").strip())
                except ValueError:
                    break
                if name not in ("/", "//", "/SYM64/", "__.SYMDEF", "__.SYMDEF SORTED") \
                        and not name.startswith("#1/__.SYMDEF"):
                    data_off = pos + 60
                    if name.startswith("#1/"):  # BSD long name stored before the data
                        data_off += int(name[3:])
                    f.seek(data_off)
                    obj = _object_arch(f.read(1 << 12))
                    if obj:
                        return ("archive/" + obj[0], obj[1])
                pos += 60 + size + (size & 1)
            return ("archive", None)
    except OSError:
        return ("unreadable", None)


def arch_matches(expected, found):
    if found is None or expected is None:
        return None
    expected = normalize_arch(expected)
    if found.startswith("universal:"):
        return expected in found[len("universal:"):].split(",")
    if expected in ("armv7a", "armv7", "arm"):
        return found == "arm"
    if expected.startswith("wasm"):
        return found == "wasm"
    return found == expected


# Preprocessor ------------------------------------------------------------------------------------

def clang_macros(clang, args, source, lang="c"):
    """Object-like macros defined after preprocessing `source` with `clang -dM -E`.

    Returns (macros, error_text). Function-like macros are dropped.
    """
    with tempfile.TemporaryDirectory(prefix="xlport-") as tmp:
        src = os.path.join(tmp, "probe." + ("cpp" if lang == "c++" else "c"))
        with open(src, "w") as f:
            f.write(source)
        try:
            out = subprocess.run([clang] + list(args) + ["-dM", "-E", src], capture_output=True,
                                 text=True, timeout=120)
        except (OSError, subprocess.SubprocessError) as e:
            return {}, str(e)
    macros = {}
    for line in out.stdout.splitlines():
        m = re.match(r"#define ([A-Za-z_][A-Za-z0-9_]*)(\(.*?\))?\s?(.*)$", line)
        if m and not m.group(2):
            macros[m.group(1)] = m.group(3)
    return macros, (out.stderr if out.returncode != 0 else "")


_INT_SUFFIX = re.compile(r"\b(0[xX][0-9A-Fa-f]+|\d+)([uUlL]+)\b")
_CAST = re.compile(r"\(\s*(?:const\s+)?(?:unsigned|signed)?\s*(?:char|short|int|long|long long|"
                   r"__u?int\d+_t|u?int\d+_t|size_t|mode_t|__sprt_[a-z_]+)\s*\)")


def eval_macro(name, macros, depth=0, seen=None):
    """The integer value of an object-like macro, or None when it is not a plain integer expression."""
    if name not in macros or depth > 16:
        return None
    seen = set(seen or ()) | {name}
    expr = macros[name].strip()
    if not expr:
        return None
    expr = _CAST.sub("", expr)
    expr = _INT_SUFFIX.sub(lambda m: m.group(1), expr)
    expr = re.sub(r"\b0([0-7]+)\b", lambda m: "0o" + m.group(1), expr)

    def ident(m):
        n = m.group(0)
        if n.startswith("0x") or n.startswith("0o") or n in ("and", "or", "not"):
            return n
        if n in seen:
            raise ValueError(n)
        v = eval_macro(n, macros, depth + 1, seen)
        if v is None:
            raise ValueError(n)
        return "(%d)" % v

    try:
        expr = re.sub(r"\b[A-Za-z_][A-Za-z0-9_]*\b", ident, expr)
    except ValueError:
        return None
    expr = expr.replace("&&", " and ").replace("||", " or ").replace("/", "//")
    expr = re.sub(r"!(?!=)", " not ", expr)
    if not re.fullmatch(r"[\s0-9a-fA-FxXo()+\-*/%<>&|^~notandr]*", expr):
        return None
    try:
        v = eval(expr, {"__builtins__": {}}, {})
    except Exception:
        return None
    if isinstance(v, bool):
        v = int(v)
    return v if isinstance(v, int) else None


# Source scanning ---------------------------------------------------------------------------------

SCAN_DIRS = ["make", "runtime", "stappler", "xenolith", "utils", "tests", ".github"]
SCAN_FILES = ["install.sh", "install.ps1", "README.md"]
SCAN_EXT = {".h", ".hpp", ".c", ".cc", ".cpp", ".m", ".mm", ".mk", ".py", ".sh", ".ps1", ".yml",
            ".yaml", ".s", ".S", ".cmake", ".inc", ".def"}
DOC_EXT = {".adoc", ".md"}
SKIP_DIRS = {".git", "stappler-build", "build", "intermediate", "targets", "hosts", "licenses", "tmp",
             "__pycache__", "node_modules", "musl-libc", "thirdparty", "src", "sysroot-out",
             "sysroot-gcc", "sysroot-clang1", "sysroot-clang2", "sysroot-clang-out", "sysroot-stage0",
             "sysroot-target", "replacements", "keys", "porting"}
# `src` is a vendored download only inside runtime/toolchains; everywhere else it is our code.
SKIP_ONLY_UNDER = {"src": os.path.join("runtime", "toolchains")}


def iter_source_files(engine, docs=False, dirs=None):
    exts = SCAN_EXT | (DOC_EXT if docs else set())
    for top in dirs or (SCAN_DIRS + (["docs"] if docs else [])):
        base = engine.path(top)
        if os.path.isfile(base):
            yield top
            continue
        for dirpath, dirnames, filenames in os.walk(base):
            rel_dir = os.path.relpath(dirpath, engine.root)
            keep = []
            for d in dirnames:
                if d in SKIP_DIRS:
                    under = SKIP_ONLY_UNDER.get(d)
                    if under is None or rel_dir.startswith(under):
                        continue
                if d.startswith("sysroot") or d.startswith("."):
                    continue
                keep.append(d)
            dirnames[:] = sorted(keep)
            for name in sorted(filenames):
                ext = os.path.splitext(name)[1]
                if ext in exts or name in ("Makefile", "GNUmakefile"):
                    path = os.path.join(dirpath, name)
                    try:
                        if os.path.getsize(path) > 4 << 20:
                            continue
                    except OSError:
                        continue
                    yield os.path.relpath(path, engine.root)
    if not dirs:
        for f in SCAN_FILES:
            if engine.exists(f):
                yield f


def scan_refs(engine, like_tokens, new_tokens, window=12, docs=False, dirs=None):
    """Places that mention a reference token with none of the new tokens within `window` lines.

    Returns (missing, covered): lists of (path, line_number, line_text). A list of architectures or
    platforms is usually a few lines long, so a new entry that is missing near the reference is the
    signal a port forgot that list.
    """
    like_re = re.compile("|".join(word(t) for t in like_tokens))
    new_re = re.compile("|".join(word(t) for t in new_tokens)) if new_tokens else None
    missing, covered = [], []
    for rel in iter_source_files(engine, docs=docs, dirs=dirs):
        text = engine.read(rel)
        if text is None or not like_re.search(text):
            continue
        lines = text.splitlines()
        new_lines = [i for i, l in enumerate(lines) if new_re and new_re.search(l)]
        for i, l in enumerate(lines):
            if like_re.search(l):
                near = any(abs(i - j) <= window for j in new_lines)
                (covered if near else missing).append((rel, i + 1, l.strip()[:160]))
    return missing, covered


def group_by_file(entries):
    files = {}
    for path, line, text in entries:
        files.setdefault(path, []).append((line, text))
    return files
