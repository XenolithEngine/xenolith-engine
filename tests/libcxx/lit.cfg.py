# Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
#
# Lit configuration that runs the upstream libc++ conformance suite
# (llvm-project/libcxx/test/std/...) against the sprt freestanding STL instead
# of libc++ itself. This is the "foreign standard library" testing mode the
# libc++ suite explicitly supports (cf. configs/stdlib-native.cfg.in): the test
# sources exercise *standard* behaviour, so pointing the include path at the
# sprt headers and linking the sprt runtime measures conformance.
#
# The test format and all toolchain wiring live in sprt_format.py (kept
# importable so lit's parallel workers can unpickle it). run.sh injects the
# toolchain contract through environment variables.

import os
import sys

_HERE = os.path.dirname(__file__)
if _HERE not in sys.path:
    sys.path.insert(0, _HERE)

import sprt_format  # noqa: E402

config.name = "sprt-stl-conformance"
config.test_format = sprt_format.SprtStlTest()
config.suffixes = [".pass.cpp", ".compile.pass.cpp"]
# Point the suite at the chosen slice of the upstream libc++ test tree.
config.test_source_root = os.environ["SPRT_TEST_ROOT"]
config.test_exec_root = os.environ["SPRT_BUILD_DIR"]
