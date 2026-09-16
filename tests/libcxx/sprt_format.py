# Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
#
# Lit TestFormat that compiles (+links+runs) a single upstream libc++ test
# source against the sprt freestanding STL. Kept in its own importable module so
# that lit's parallel workers can unpickle the format (a class defined inside
# lit.cfg.py is not importable and breaks multiprocessing).
#
# All toolchain specifics arrive through environment variables (set by run.sh)
# and are read at import time, so the module behaves identically in the parent
# and in spawned/forked workers.

import contextlib
import fcntl
import os
import shlex
import subprocess

import lit.formats
import lit.Test
from lit.BooleanExpression import BooleanExpression

# Custom failure codes so the summary distinguishes *why* a test failed instead
# of lumping every failure under "Failed". They auto-register with lit (the
# ResultCode ctor appends to ResultCode._all_codes), so print_summary emits one
# line per label, and they pickle across -j workers because this module is
# importable. isFailure=True keeps them counted as failures (nonzero exit).
#   COMPILE_FAIL — the source did not compile against sprt (missing symbol,
#                  concept mismatch, hard error): a coverage gap in the STL.
#   LINK_FAIL    — compiled but did not link (undefined reference in runtime).
#   RUN_FAIL     — compiled, linked, but the test asserted/aborted/timed out at
#                  runtime: sprt built the wrong behavior, not a missing feature.
COMPILE_FAIL = lit.Test.ResultCode("COMPILE_FAIL", "Compile Failed", True)
LINK_FAIL = lit.Test.ResultCode("LINK_FAIL", "Link Failed", True)
RUN_FAIL = lit.Test.ResultCode("RUN_FAIL", "Runtime Failed", True)


def _env(name, default=None):
    v = os.environ.get(name, default)
    if v is None:
        raise RuntimeError("sprt_format: required env var %s is not set" % name)
    return v


CXX = _env("SPRT_CXX")
CC = _env("SPRT_CC")
COMPILE_FLAGS = shlex.split(_env("SPRT_COMPILE_FLAGS"))
LINK_FLAGS = shlex.split(os.environ.get("SPRT_LINK_FLAGS", ""))
EXEC = shlex.split(os.environ.get("SPRT_EXEC", ""))
STD_VER = int(os.environ.get("SPRT_STD_VER", "20"))
BUILD_DIR = _env("SPRT_BUILD_DIR")

# Compile-only baseline mode: stop after a successful `-c` compile, never link or
# run (SPRT_COMPILE_ONLY=1). Measures header/library COMPILATION conformance in
# isolation from the runtime link, which is a separate porting stage. In this mode
# the runtime object list and link flags are irrelevant and may be absent.
COMPILE_ONLY = os.environ.get("SPRT_COMPILE_ONLY", "") not in ("", "0")

_rt_objs_file = os.environ.get("SPRT_RT_OBJS_FILE")
if _rt_objs_file and os.path.exists(_rt_objs_file):
    with open(_rt_objs_file) as _f:
        RT_OBJS = [ln.strip() for ln in _f if ln.strip()]
else:
    RT_OBJS = []

# --- feature set (see lit.cfg.py / README for the rationale) -----------------
_STD_LADDER = [3, 11, 14, 17, 20, 23, 26]
FEATURES = {"c++%d" % STD_VER}
for _n in _STD_LADDER:
    if _n <= STD_VER:
        FEATURES.add("std-at-least-c++%d" % _n)
FEATURES.update({
    "no-exceptions",
    "no-random-device",
    "libcpp-has-no-experimental-tzdb",
    # Absent capabilities in this layer. libc++ gates the runtime-dependent tests of
    # each behind these tags; header-only tests it leaves ungated still compile-fail
    # when we lack the header entirely (e.g. <filesystem>, <memory_resource>).
    "no-filesystem",
    "availability-filesystem-missing",     # no <filesystem>
    "availability-pmr-missing",            # no <memory_resource> (pmr)
    "availability-tzdb-missing",           # no <chrono> time-zone database
    "no-tzdb",
    "availability-synchronization_library-missing",  # no <barrier>/<latch>/<semaphore>
    # PSTL (<execution> parallel algorithms) and <syncstream> are BOTH disabled inside
    # libc++ itself (incomplete/experimental); it tags their tests UNSUPPORTED with the
    # lit feature names below. These must match libc++'s spelling exactly — the earlier
    # "no-incomplete-pstl" was a non-matching alias, so the ~32 pstl.*/is_execution_policy
    # tests were mis-scored as compile-fails instead of UNSUPPORTED.
    "libcpp-has-no-incomplete-pstl",       # no <execution> / parallel algorithms
    "libcpp-has-no-experimental-syncstream",  # <syncstream> gated off as experimental
    # NB: no-localization is intentionally NOT set — <locale> is implemented now.
})
# Target-derived features. The MSVC C++ ABI differs from Itanium in ways libc++'s
# own tests already account for (zero-length-array size, [[no_unique_address]]
# packing of non-empty members, ...): those tests are tagged UNSUPPORTED/XFAIL: msvc.
# A clang *-windows-msvc target uses that same ABI, so advertise `msvc` for it and the
# tags classify correctly instead of counting as spurious failures.
if any(a.startswith("--target=") and "windows-msvc" in a for a in COMPILE_FLAGS):
    FEATURES.add("msvc")

FEATURES.update(shlex.split(os.environ.get("SPRT_EXTRA_FEATURES", "")))

# The whole std/experimental/ TS subtree (simd, ...) is gated by
# libcxx/test/std/experimental/lit.local.cfg on the `c++experimental` feature:
#   if "c++experimental" not in config.available_features: config.unsupported = True
# Our custom TestFormat yields sources directly (getTestsInDirectory) and never
# evaluates lit.local.cfg, so without this the experimental sources are compiled
# and counted as compile-fails. sprt does not enable _LIBCPP_ENABLE_EXPERIMENTAL,
# so mirror that one directory gate exactly (NOT a general lit.local.cfg processor:
# the no-filesystem/no-localization gates elsewhere would over-reclassify — our
# fstream file I/O works and passes despite the missing <filesystem> header).
_EXPERIMENTAL_GATED = "c++experimental" not in FEATURES

# windows-msvc config-inherent XFAILs. These tests fail for a documented reason that is
# NOT a sprt bug and that upstream libc++ already treats as expected-fail for the SAME
# root cause on other configs — but its XFAIL directives are target-string gated and do
# not enumerate x86_64-pc-windows-msvc. Our config hits the identical issue, so mark them
# expected-fail (a future fix flips them to XPASS, which flags the change). Scoped to the
# msvc feature, so the Linux runs — where these PASS — are unaffected.
_IS_MSVC = "msvc" in FEATURES
_MSVC_XFAIL = (
    # ctype<char>::classic_table() blank bit: we define _LIBCPP_PROVIDES_DEFAULT_RUNE_TABLE
    # → libc++'s ISO builtin table sets blank for '\t' (isblank('\t') is true, correct).
    # The test's `#elif defined(_WIN32)` branch assumes the MS-CRT table (blank only for
    # ' '); there is no lit feature to select it. Matching it would be an ISO regression
    # and a patch to vendored libc++. Our table is the more correct one.
    "facet.ctype.char.statics/classic_table.pass.cpp",
    # setfill_wchar_max: _LIBCPP_ABI_VERSION==1 stores basic_ios' fill as int_type with an
    # eof() "unset" sentinel; on 16-bit wchar_t WEOF==WCHAR_MAX==0xFFFF, so fill(WCHAR_MAX)
    # reads back as "unset" → widen(' '). Upstream XFAILs this exact collision for
    # aarch64/armv7-linux abi-v1; x86_64-pc-windows-msvc (also 16-bit wchar, abi-v1) is the
    # same case, just not in their target list.
    "std.manip/setfill_wchar_max.pass.cpp",
)


def _eval(expr, features):
    try:
        return BooleanExpression.evaluate(expr, features)
    except ValueError:
        return False


def _match_any(exprs, features):
    return any(_eval(e.strip(), features) for e in exprs if e.strip())


def _match_all(exprs, features):
    return all(_eval(e.strip(), features) for e in exprs if e.strip())


COMPILE_TIMEOUT = int(os.environ.get("SPRT_COMPILE_TIMEOUT", "120"))
RUN_TIMEOUT = int(os.environ.get("SPRT_RUN_TIMEOUT", "20"))

# Serialize the EXEC step when running under wine. lit's -j workers are separate
# PROCESSES, so running one wine .exe per worker in parallel oversubscribes the CPU
# (each wine process is heavy) and the slow ones hit RUN_TIMEOUT (exit=124) — false
# run-fails that pass fine serially. wine also shares one wineserver, so concurrent
# processes contend there too. Compile/link are native and parallel-safe, so we lock
# ONLY the wine run: an exclusive flock on a shared file (all workers of a lit run
# share SPRT_BUILD_DIR) admits one wine process at a time while compiles overlap.
# On native targets SPRT_EXEC is empty → no lock, full parallelism.
_IS_WINE = any("wine" in e for e in EXEC)
_WINE_LOCK_PATH = os.path.join(BUILD_DIR, ".wine-exec.lock")


@contextlib.contextmanager
def _wine_exec_gate():
    if not _IS_WINE:
        yield
        return
    os.makedirs(BUILD_DIR, exist_ok=True)
    with open(_WINE_LOCK_PATH, "w") as f:
        fcntl.flock(f, fcntl.LOCK_EX)
        try:
            yield
        finally:
            fcntl.flock(f, fcntl.LOCK_UN)


_RUN_ENV = dict(os.environ)
# The upstream suite expects C-locale diagnostics ("Not a directory", ...);
# without this a localized host locale leaks into strerror-based messages.
_RUN_ENV["LC_ALL"] = "C"


def _run(cmd, timeout=None, cwd=None):
    # New session so a hung/looping test (or its children) can be killed as a group.
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                         start_new_session=True, env=_RUN_ENV, cwd=cwd)
    try:
        out, err = p.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        import signal
        try:
            os.killpg(os.getpgid(p.pid), signal.SIGKILL)
        except OSError:
            p.kill()
        p.communicate()
        return 124, "", "TIMEOUT after %ss" % timeout
    return p.returncode, out.decode(errors="replace"), err.decode(errors="replace")


def _rm(path):
    try:
        os.remove(path)
    except OSError:
        pass


def _verdict(expect_fail, code, msg):
    if expect_fail:
        if code == lit.Test.PASS:
            return lit.Test.Result(lit.Test.XPASS, "unexpectedly passed")
        return lit.Test.Result(lit.Test.XFAIL, msg)
    return lit.Test.Result(code, msg)


class SprtStlTest(lit.formats.TestFormat):
    """Compile a libc++ conformance source against the sprt STL; link+run .pass.cpp."""

    def getTestsInDirectory(self, testSuite, path_in_suite, litConfig, localConfig):
        # Yield only files directly in this directory; lit descends into
        # subdirectories itself and calls this again per level. Recursing here
        # (os.walk) would double-count every nested test once per ancestor.
        source_path = testSuite.getSourcePath(path_in_suite)
        for fn in sorted(os.listdir(source_path)):
            if not (fn.endswith(".pass.cpp") or fn.endswith(".compile.pass.cpp")):
                continue
            if os.path.isfile(os.path.join(source_path, fn)):
                yield lit.Test.Test(testSuite, path_in_suite + (fn,), localConfig)

    def _parse_directives(self, src):
        unsupported, requires, xfail, addflags, filedeps = [], [], [], [], []
        try:
            with open(src, "r", errors="replace") as f:
                for line in f:
                    s = line.strip()
                    if not s.startswith("//"):
                        continue
                    body = s[2:].strip()
                    for tag, bucket in (("UNSUPPORTED:", unsupported),
                                        ("REQUIRES:", requires), ("XFAIL:", xfail)):
                        if body.startswith(tag):
                            bucket.extend(body[len(tag):].split(","))
                    if body.startswith("FILE_DEPENDENCIES:"):
                        filedeps.extend(
                            d.strip() for d in body[len("FILE_DEPENDENCIES:"):].split(","))
                    if body.startswith("ADDITIONAL_COMPILE_FLAGS"):
                        rest = body[len("ADDITIONAL_COMPILE_FLAGS"):]
                        if rest.startswith("("):
                            feat, _, flags = rest[1:].partition("):")
                            if (not feat) or _eval(feat, FEATURES):
                                addflags.extend(shlex.split(flags))
                        elif rest.startswith(":"):
                            addflags.extend(shlex.split(rest[1:]))
        except OSError:
            pass
        return unsupported, requires, xfail, addflags, filedeps

    def execute(self, test, litConfig):
        src = test.getSourcePath()

        # Mirror std/experimental/lit.local.cfg: the experimental TS subtree is
        # UNSUPPORTED unless c++experimental is available (which sprt never enables).
        if _EXPERIMENTAL_GATED and "/std/experimental/" in src.replace(os.sep, "/"):
            return lit.Test.Result(lit.Test.UNSUPPORTED,
                                   "std/experimental gated off (c++experimental not available)")

        unsupported, requires, xfail, addflags, filedeps = self._parse_directives(src)

        if _match_any(unsupported, FEATURES):
            return lit.Test.Result(lit.Test.UNSUPPORTED, "matched UNSUPPORTED directive")
        if requires and not _match_all(requires, FEATURES):
            return lit.Test.Result(lit.Test.UNSUPPORTED, "unmet REQUIRES directive")
        expect_fail = _match_any(xfail, FEATURES)
        if _IS_MSVC and not expect_fail:
            _src_posix = src.replace(os.sep, "/")
            if any(frag in _src_posix for frag in _MSVC_XFAIL):
                expect_fail = True

        is_compile_only = COMPILE_ONLY or src.endswith(".compile.pass.cpp")
        name = "_".join(test.path_in_suite)
        os.makedirs(BUILD_DIR, exist_ok=True)
        obj = os.path.join(BUILD_DIR, name + ".o")

        cc = [CXX] + COMPILE_FLAGS + addflags + ["-c", "-o", obj, src]
        rc, out, err = _run(cc, timeout=COMPILE_TIMEOUT)
        if rc != 0:
            return _verdict(expect_fail, COMPILE_FAIL,
                            "compile failed:\n%s\n%s" % (" ".join(cc), err))
        if is_compile_only:
            _rm(obj)
            return _verdict(expect_fail, lit.Test.PASS, "")

        exe = os.path.join(BUILD_DIR, name + ".exe")
        ln = [CC, obj] + RT_OBJS + LINK_FLAGS + ["-o", exe]
        rc, out, err = _run(ln, timeout=COMPILE_TIMEOUT)
        if rc != 0:
            _rm(obj)
            return _verdict(expect_fail, LINK_FAIL, "link failed:\n%s" % err)

        # Each test runs in its own scratch directory: tests that create files do
        # not collide, and declared FILE_DEPENDENCIES are copied in from the test's
        # source directory (mirrors upstream lit's %{exec} behaviour).
        import shutil
        rundir = os.path.join(BUILD_DIR, name + ".dir")
        shutil.rmtree(rundir, ignore_errors=True)
        os.makedirs(rundir, exist_ok=True)
        srcdir = os.path.dirname(src)
        for dep in filedeps:
            dep_src = os.path.join(srcdir, dep)
            if os.path.exists(dep_src):
                dst = os.path.join(rundir, os.path.basename(dep))
                if os.path.isdir(dep_src):
                    shutil.copytree(dep_src, dst, dirs_exist_ok=True)
                else:
                    shutil.copy(dep_src, dst)

        with _wine_exec_gate():
            rc, out, err = _run(EXEC + [exe], timeout=RUN_TIMEOUT, cwd=rundir)
        _rm(obj)
        _rm(exe)
        shutil.rmtree(rundir, ignore_errors=True)
        if rc != 0:
            return _verdict(expect_fail, RUN_FAIL,
                            "run exit=%d\n%s\n%s" % (rc, out, err))
        return _verdict(expect_fail, lit.Test.PASS, "")
