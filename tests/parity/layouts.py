#!/usr/bin/env python3
"""
Backend parity over the tests/window layout registry.

compare.sh asks whether two backends agree on a handful of synthetic scenes in
tests/headless. This asks it of the real UI: every layout the test app can show -
buttons, panels, menus, tables, text inputs, pug templates, the CSS stands - drawn
headless by the reference backend and by the subject, and compared with the same
last-bit rule imgdiff.py applies. That is what the GLES milestone accepts on: the
labels and widgets of tests/window have to land where the Vulkan reference puts them.

Both sides run with XL_FLAT_QUEUE=1 and XL_HIDE_FPS=1, and neither is optional:

  * the GLES backend implements the flat queue only (XL2dScene.cc serves it even for a
    Default request), so a reference left on the default queue would be comparing two
    different frame graphs - that one has a depth buffer, and what occludes what
    changes with it. A badge drawn over a glyph is the visible form of that mistake;
  * the FPS counter changes every frame, so a scene carrying it never settles, and the
    capture rule - two identical frames in a row - has nothing to settle on.

Layouts are enumerated from the app itself (the `layouts` command), so the list is
whatever the registry currently holds; nothing here has to be updated when a stand is
added.

Usage:
  layouts.py [options] [layout ...]
    layout...           run only these, by path ("widgets/menu") or short name ("menu")
    --reference API     backend to use as reference (default vulkan)
    --subject API       backend to compare against it (default gles)
    --group NAME        only layouts of this registry group (css, widgets, render, ...)
    --size WxH          surface size (default 800x600)
    --settle SECONDS    seconds the app renders a layout before answering (default 1.0,
                        the app's own; a shorter one races the stands that wait on
                        something outside the frame - the CSS live-reload pair waits
                        on a filesystem watch, and captured too early it disagrees
                        with ITSELF, never mind with another backend)
    --keep DIR          keep every capture in DIR (created if missing)
    -l, --list          print the layout list and exit
    -v, --verbose       keep the captures and write a diff map for a diverging layout

A layout that fails is captured a second time on both sides before the verdict is
written. Several stands step on their own clock - a class applied a second in, a
selection walking the tree, a box that appears "while the test runs" - and two apps
do not reach the same moment at the same time, so one snapshot against the other can
differ with both backends correct. A side that disagrees with ITSELF across the two
captures is reported UNSTABLE and is not counted as a divergence; a mismatch that
reproduces with both sides holding still is.

Requirements: python3 with numpy and Pillow (imgdiff.py's), and a Vulkan ICD for the
reference runs. The binary is tests/window's; build it with GLES=1 first:

  make -C tests/window STAPPLER_TARGET=x86_64-unknown-linux-gnu GLES=1 -j8
"""

import argparse
import os
import shutil
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
TARGET = "x86_64-unknown-linux-gnu"
BIN = os.path.join(ROOT, "tests/window/stappler-build", TARGET, "debug/cc/testapp")

sys.path.insert(0, HERE)
import xlclient  # noqa: E402  (the inspector protocol, shared with the parity harness)


class App:
    """One headless test app, on its own inspector socket, shut down on the way out."""

    def __init__(self, api, width, height, log_path, sock_path):
        env = dict(os.environ)
        env["XENOLITH_INSPECTOR_ADDRESS"] = "unix:" + sock_path
        env["XL_FLAT_QUEUE"] = "1"
        env["XL_HIDE_FPS"] = "1"
        try:
            os.unlink(sock_path)
        except OSError:
            pass

        self.log_path = log_path
        self.log = open(log_path, "wb")
        self.proc = subprocess.Popen(
                [BIN, "--headless", "--gapi", api,
                        "--width", str(width), "--height", str(height)],
                env=env, stdout=self.log, stderr=subprocess.STDOUT)
        self.session = None
        deadline = time.monotonic() + 40.0
        last = None
        while time.monotonic() < deadline and self.proc.poll() is None:
            try:
                self.session = xlclient.Session("unix:" + sock_path, 60.0)
                break
            except OSError as e:
                last = e
                time.sleep(0.25)
        if self.session is None:
            self.close()
            raise OSError("%s app never accepted a connection: %s" % (api, last))

    def close(self):
        if self.session is not None:
            try:
                self.session.call("quit", graceful=True)
            except OSError:
                pass
            self.session.close()
            self.session = None
        if self.proc.poll() is None:
            try:
                self.proc.wait(timeout=15)
            except subprocess.TimeoutExpired:
                self.proc.terminate()
                try:
                    self.proc.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    self.proc.kill()
        self.log.close()

    def capture(self, layout, settle, frames=2, min_rounds=6, rounds=10):
        """Show a layout and read back the frame it settled on.

        The settle rule is xlclient's, for xlclient's reason: `screenshot` returns the
        last PRESENTED image, which trails the frames just submitted, and before the
        first frame carrying the switch arrives two captures agree on the *previous*
        layout. So burn min_rounds rounds, then trust equality."""
        result = self.session.call("invoke", name="layout",
                args={"name": layout, "settle": settle})
        if isinstance(result, dict) and result.get("ok") is False:
            raise OSError(result.get("error", "layout refused"))

        previous = None
        for index in range(rounds):
            self.session.call("frame", count=frames)
            info = self.session.call("screenshot")
            data = xlclient.decode_bytes(info["data"])
            if data == previous and index + 1 >= min_rounds:
                return data
            previous = data
        raise OSError("capture did not settle in %d rounds" % rounds)


def compare(reference, actual, diff_map=None):
    """imgdiff.py's verdict on two PNGs: (matched, its one-line report)."""
    cmd = [sys.executable, os.path.join(HERE, "imgdiff.py"), reference, actual]
    if diff_map:
        cmd += ["--out-diff", diff_map]
    proc = subprocess.run(cmd, capture_output=True, text=True)
    return proc.returncode == 0, (proc.stdout or proc.stderr).strip()


def enumerate_layouts(width, height, work):
    app = App("gles", width, height, os.path.join(work, "registry.log"),
            os.path.join(work, "registry.sock"))
    try:
        result = app.session.call("invoke", name="layouts", args={})
    finally:
        app.close()
    return result.get("layouts", [])


def main():
    parser = argparse.ArgumentParser(add_help=True, description=__doc__,
            formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("layouts", nargs="*")
    parser.add_argument("--reference", default="vulkan")
    parser.add_argument("--subject", default="gles")
    parser.add_argument("--group")
    parser.add_argument("--size", default="800x600")
    parser.add_argument("--settle", type=float, default=1.0)
    parser.add_argument("--keep")
    parser.add_argument("-l", "--list", action="store_true")
    parser.add_argument("-v", "--verbose", action="store_true")
    args = parser.parse_args()

    if not os.path.exists(BIN):
        print("missing binary: %s\nbuild it with: make -C tests/window "
                "STAPPLER_TARGET=%s GLES=1 -j8" % (BIN, TARGET), file=sys.stderr)
        return 2

    width, _, height = args.size.partition("x")
    width, height = int(width), int(height)

    work = tempfile.mkdtemp(prefix="xl-layouts-")
    if args.keep:
        os.makedirs(args.keep, exist_ok=True)

    try:
        registry = enumerate_layouts(width, height, work)
        if not registry:
            print("the app reported no layouts", file=sys.stderr)
            return 2

        selected = []
        for entry in registry:
            path, name = entry.get("path", ""), entry.get("name", "")
            if args.group and entry.get("group") != args.group:
                continue
            if args.layouts and path not in args.layouts and name not in args.layouts:
                continue
            selected.append(entry)

        if args.list:
            for entry in selected:
                print(entry.get("path"))
            return 0

        missing = [it for it in args.layouts
                if it not in {e.get("path") for e in registry}
                and it not in {e.get("name") for e in registry}]
        if missing:
            print("unknown layout(s): %s" % ", ".join(missing), file=sys.stderr)
            return 2
        if not selected:
            print("nothing selected", file=sys.stderr)
            return 2

        # One app per backend for the whole run: the registry is static and switching
        # layouts is what the `layout` command is for, so 66 layouts cost two processes
        # rather than 132. A layout that leaves state behind would be visible as a
        # divergence in the NEXT one, which is a property worth having.
        reference = App(args.reference, width, height,
                os.path.join(work, "reference.log"), os.path.join(work, "reference.sock"))
        subject = None
        try:
            subject = App(args.subject, width, height,
                    os.path.join(work, "subject.log"), os.path.join(work, "subject.sock"))

            passed = failed = unstable = 0
            failures = []
            unstables = []
            for entry in selected:
                path = entry.get("path")
                base = path.replace("/", "-")

                def shot(app, name):
                    data = app.capture(path, args.settle)
                    png = os.path.join(work, "%s-%s.png" % (name, base))
                    with open(png, "wb") as f:
                        f.write(data)
                    return png

                try:
                    ref_png = shot(reference, "ref")
                    act_png = shot(subject, "act")
                except OSError as e:
                    print("%-28s FAIL (%s)" % (path, e), flush=True)
                    failed += 1
                    failures.append(path)
                    continue

                diff_png = os.path.join(work, "diff-%s.png" % base) if args.verbose else None
                ok, report = compare(ref_png, act_png, diff_png)

                # A divergence has to survive being asked twice, and the second answer says which
                # question failed. Several stands step on their own clock - a class applied a
                # second in, a selection walking the tree, a box that appears "while the test runs"
                # - and two apps do not reach the same moment at the same time, so a snapshot of
                # one against a snapshot of the other differs without either backend being wrong.
                # Re-capturing both sides tells the two apart: a side that disagrees with ITSELF is
                # a stand that does not hold still, and a mismatch that reproduces with both sides
                # stable is the real thing.
                if not ok:
                    try:
                        ref2_png = shot(reference, "ref2")
                        act2_png = shot(subject, "act2")
                    except OSError as e:
                        print("%-28s FAIL %s (recheck: %s)" % (path, report, e), flush=True)
                        failed += 1
                        failures.append(path)
                        continue

                    ref_stable, ref_report = compare(ref_png, ref2_png)
                    act_stable, act_report = compare(act_png, act2_png)
                    if not ref_stable or not act_stable:
                        moved = args.reference if not ref_stable else args.subject
                        print("%-28s UNSTABLE (%s moved between captures: %s)"
                                % (path, moved, ref_report if not ref_stable else act_report),
                                flush=True)
                        unstable += 1
                        unstables.append(path)
                        continue
                    ok, report = compare(ref2_png, act2_png, diff_png)

                if ok:
                    print("%-28s OK   %s" % (path, report), flush=True)
                    passed += 1
                else:
                    print("%-28s FAIL %s" % (path, report), flush=True)
                    failed += 1
                    failures.append(path)
                    if args.verbose:
                        print("    diff map: %s" % diff_png)

                if args.keep:
                    shutil.copy(ref_png, os.path.join(args.keep, "ref-%s.png" % base))
                    shutil.copy(act_png, os.path.join(args.keep, "act-%s.png" % base))
        finally:
            if subject is not None:
                subject.close()
            reference.close()

        print("-" * 40)
        print("matching: %d   diverging: %d   unstable: %d   (of %d)"
                % (passed, failed, unstable, passed + failed + unstable))
        if unstables:
            print("unstable (not a verdict on the backend): %s" % " ".join(unstables))
        if failures:
            print("diverging: %s" % " ".join(failures))
            if args.verbose:
                print("captures and logs in %s (kept)" % work)
            return 1
        print("ALL LAYOUTS MATCH")
        return 0
    finally:
        if not args.verbose:
            shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        sys.exit(130)
