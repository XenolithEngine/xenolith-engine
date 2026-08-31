#!/usr/bin/env python3
"""Drive the CSS stands (XL_STATE_TEST first, then css/selector) over the inspector socket.

The CSS engine has eleven demo layouts and, until this one, no headless check at all: a selector
that stopped matching would be found by somebody looking at a screen. What is checked here is not
the parser - css/hover already does that, by assigning interactive bits directly - but the SEAM: a
widget's own state reaching a selector. The form rejects an empty required field, an edit lock takes
a control away, a text input is switched to read-only, a progress bar is given no total, the submit
button becomes the form's default, the tab ring is walked; every assertion below is about what a
rule then matched.

Three things worth knowing about the assertions:

  * they read the RESOLVED style, not the colour on screen. The claim is that a rule matched, and a
    widget that does not paint its own background would otherwise fail a check about the cascade;
  * one state is watched through TWO properties: `:invalid` paints `background-color` on the field
    and `color` through a second rule. A state that reached the cascade but not the property, or the
    other way round, fails here;
  * `:focus-visible` is asserted on the CHECKBOX. A text input is always focus-visible by design -
    it shows a caret the moment it has focus, however focus got there - so it is the one widget
    that cannot tell the two apart.

The "tap" side of that is requested programmatically, through state.focus, and that is not a
shortcut: a tap reaches the form as FormSystem::focusField, the same call, and it is the CALL the
rule is written in terms of. A synthetic pointer press does not move focus at all in a headless run
(text-input-check.py focuses programmatically for the same reason), so tapping here would be
checking the pointer plumbing rather than the pseudo-class.

    tests/window/style-check.py [path-to-testapp]

With no argument it expects the debug x86_64-linux binary in place. It starts its own app instance,
runs the checks and prints "N checks, M failures"; exit status is the result.
"""
import json, os, socket, struct, subprocess, sys, time

ADDR = os.environ.get("XENOLITH_INSPECTOR_SOCK", "/tmp/xl-style-check.sock")

# The stand's stylesheet, duplicated on purpose: a check that reads its expectations out of the
# thing it is checking cannot fail.
BASE = 0x616161
BASE_COLOR = 0x101010
VALID = 0x43A047
INVALID = 0xE53935
INVALID_CLASS_COLOR = 0xFF00FF
READ_WRITE = 0x1E88E5
READ_ONLY = 0x8E24AA
OPTIONAL = 0x6D4C41
REQUIRED = 0xFB8C00
INDETERMINATE = 0x00897B
DEFAULT = 0xC0CA33
FOCUS = 0x3949AB
FOCUS_VISIBLE = 0xD81B60
FOCUS_WITHIN = 0x00ACC1

# ...and the selector stand's, for the second half of the run
SEL_BASE = 0x616161
NOT_CLASS = 0x43A047
NOT_TAG = 0x1E88E5
NOT_ID = 0x8E24AA
NOT_STATE = 0xFB8C00
NOT_LIST = 0x7CB342
IS_MATCH = 0x00897B
WHERE_MATCH = 0x5E35B1
IS_SPEC_WIN = 0xFF0000
WHERE_LOSES = 0xC0CA33
BAD_FALLBACK = 0x00ACC1

# document::InteractiveFlags, likewise duplicated
F_ENABLED = 1 << 0
F_FOCUS = 1 << 1
F_INVALID = 1 << 5
F_READONLY = 1 << 6
F_INDETERMINATE = 1 << 7
F_REQUIRED = 1 << 8
F_DEFAULT = 1 << 9
F_FOCUS_VISIBLE = 1 << 10
F_FOCUS_WITHIN = 1 << 11

# Keys are addressed by NAME, and a Tab that carries no keychar skips the text-input processor -
# the false positive that once hid a whole class of key bugs (see form-check.py)


class Session:
    def __init__(self, path=ADDR, timeout=20.0):
        self.s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.s.settimeout(timeout)
        self.s.connect(path)
        self.s.sendall(b"xenolith/1 json\n")
        line = b""
        while not line.endswith(b"\n"):
            line += self.s.recv(1)
        assert line.startswith(b"# xenolith/1 ok"), line
        self.serial = 0
        self.buf = b""

    def call(self, cmd, **kw):
        self.serial += 1
        req = {"serial": self.serial, "cmd": cmd}
        req.update(kw)
        payload = json.dumps(req).encode()
        self.s.sendall(struct.pack("<I", len(payload)) + payload)
        while True:
            while len(self.buf) < 4:
                self.buf += self.s.recv(65536)
            size = struct.unpack("<I", self.buf[:4])[0]
            while len(self.buf) < 4 + size:
                self.buf += self.s.recv(65536)
            frame = self.buf[4:4 + size]
            self.buf = self.buf[4 + size:]
            resp = json.loads(frame)
            if resp.get("serial") == self.serial:
                return resp

    def ok(self, cmd, **kw):
        r = self.call(cmd, **kw)
        if r.get("status") != "ok":
            raise SystemExit(f"{cmd} failed: {r.get('error')}")
        return r.get("result")

    # The command name is positional-only: a command with an argument OF ITS OWN called `name`
    # (`layout`, `calc.set-var`) would otherwise collide with this parameter
    def invoke(self, command, /, **args):
        return self.ok("invoke", name=command, args=args)

    def close(self):
        self.s.close()


def key(code, char, mods=0):
    ev = {"event": "KeyPressed", "keycode": code, "keychar": char, "modifiers": mods}
    up = dict(ev)
    up["event"] = "KeyReleased"
    return [ev, up]


def start_app(binary):
    env = dict(os.environ)
    env["XL_STATE_TEST"] = "1"
    env["XENOLITH_INSPECTOR_ADDRESS"] = "unix:" + ADDR
    try:
        os.unlink(ADDR)
    except OSError:
        pass
    proc = subprocess.Popen([binary, "--headless", "--width", "1024", "--height", "768"],
            env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    for _ in range(600):
        if os.path.exists(ADDR):
            try:
                s = Session()
                s.close()
                return proc
            except OSError:
                pass
        time.sleep(0.05)
    proc.kill()
    raise SystemExit("app did not come up")


checks = 0
failures = 0


def check(name, ok, detail=""):
    global checks, failures
    checks += 1
    if ok:
        print(f"  ok   {name}")
    else:
        failures += 1
        print(f"  FAIL {name} {detail}")


binary = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(
        os.path.abspath(__file__)), "stappler-build/x86_64-unknown-linux-gnu/debug/cc/testapp")

proc = start_app(binary)
s = Session()


def step(n=2):
    # A state flip restyles on the next frame, and the headless window renders on demand
    s.ok("frame", count=n)
    time.sleep(0.15)


def state():
    return s.invoke("state.state", settle=0.0)


def bg(st, name):
    return st[name]["background"]


def flags(st, name):
    return st[name]["flags"]


def hexc(v):
    return f"#{v:06x}" if isinstance(v, int) else str(v)


def hundredths(x):
    # The arithmetic stand reports lengths in hundredths, so an expectation never depends on how a
    # float prints
    return int(round(x * 100.0))


def wait_for_style(seconds=15.0):
    # The first resolve happens once the stand has been laid out and styled
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        step(3)
        st = state()
        if bg(st, "rw") != 0:
            return st
    raise SystemExit("the stand never resolved a style")


try:
    st = wait_for_style()

    print("\n-- 1. the pairs, before anything is touched")
    check("a field with no complaint is :valid", bg(st, "req") == VALID,
            hexc(bg(st, "req")))
    check("...and carries no Invalid bit", (flags(st, "req") & F_INVALID) == 0,
            hex(flags(st, "req")))
    check("a plain field is :read-write", bg(st, "rw") == READ_WRITE, hexc(bg(st, "rw")))
    check("setReadOnly(true) is :read-only", bg(st, "ro") == READ_ONLY, hexc(bg(st, "ro")))
    check("...and carries the ReadOnly bit", (flags(st, "ro") & F_READONLY) != 0,
            hex(flags(st, "ro")))
    check("a field without the flag is :optional", bg(st, "opt") == OPTIONAL,
            hexc(bg(st, "opt")))
    check("a field with it is :required", bg(st, "required") == REQUIRED,
            hexc(bg(st, "required")))
    check("...and the two differ by the flag alone",
            (flags(st, "required") & F_REQUIRED) != 0 and (flags(st, "opt") & F_REQUIRED) == 0,
            f'{hex(flags(st, "required"))} vs {hex(flags(st, "opt"))}')
    check("a bar with a total is not :indeterminate", bg(st, "bar") == BASE, hexc(bg(st, "bar")))

    print("\n-- 2. the form rejects, and the rejection is a selector")
    r = s.invoke("state.submit", settle=0.0)
    check("submit refused", r["submitted"] is False, str(r))
    step()
    st = state()
    check("the empty required field is :invalid", bg(st, "req") == INVALID, hexc(bg(st, "req")))
    check("...carrying the Invalid bit", (flags(st, "req") & F_INVALID) != 0,
            hex(flags(st, "req")))
    check("...and the same state reached the other property too",
            st["req"]["color"] == INVALID_CLASS_COLOR, hexc(st["req"]["color"]))
    check("a field that was not rejected stays :valid", bg(st, "opt") == OPTIONAL,
            hexc(bg(st, "opt")))

    s.invoke("state.set-text", target="req", text="filled", settle=0.0)
    s.invoke("state.set-text", target="required", text="filled", settle=0.0)
    r = s.invoke("state.submit", settle=0.0)
    check("submit accepted once the fields are filled", r["submitted"] is True, str(r))
    step()
    st = state()
    check("the mark came off: :valid again", bg(st, "req") == VALID, hexc(bg(st, "req")))
    check("...and so did the class", st["req"]["color"] == BASE_COLOR, hexc(st["req"]["color"]))

    print("\n-- 3. two sources of :read-only, and one bit")
    s.invoke("state.set-lock", target="locked", value=True, reason="owned by a wire", settle=0.0)
    step()
    st = state()
    check("a locked control is :read-only", bg(st, "locked") == READ_ONLY,
            hexc(bg(st, "locked")))
    check("...and :disabled too - the lock raises both", (flags(st, "locked") & F_ENABLED) == 0,
            hex(flags(st, "locked")))

    # The widget asks for read-only WHILE the lock is on: the lock wins, and the answer is kept
    s.invoke("state.set-readonly", target="locked", value=True, settle=0.0)
    s.invoke("state.set-lock", target="locked", value=False, settle=0.0)
    step()
    st = state()
    check("unlocking gives back what the WIDGET asked for, not 'writable'",
            bg(st, "locked") == READ_ONLY, hexc(bg(st, "locked")))
    check("...and the control is enabled again", (flags(st, "locked") & F_ENABLED) != 0,
            hex(flags(st, "locked")))

    s.invoke("state.set-readonly", target="locked", value=False, settle=0.0)
    step()
    st = state()
    check("and clearing the widget's own mode finally makes it :read-write",
            bg(st, "locked") == READ_WRITE, hexc(bg(st, "locked")))

    print("\n-- 4. a bar with no total")
    s.invoke("state.set-progress", settle=0.0)
    step()
    st = state()
    check("no total is :indeterminate", bg(st, "bar") == INDETERMINATE, hexc(bg(st, "bar")))
    check("...carrying the bit", (flags(st, "bar") & F_INDETERMINATE) != 0,
            hex(flags(st, "bar")))
    s.invoke("state.set-progress", value=0.25, settle=0.0)
    step()
    st = state()
    check("a total takes it away again", bg(st, "bar") == BASE, hexc(bg(st, "bar")))

    print("\n-- 5. :default is the button Enter will actually press")
    check("the submit button is the form's default", st.get("defaultButton") == "submit",
            str(st.get("defaultButton")))
    check("...and it is painted", bg(st, "submit") == DEFAULT, hexc(bg(st, "submit")))
    check("...carrying the bit", (flags(st, "submit") & F_DEFAULT) != 0,
            hex(flags(st, "submit")))

    # A LOCK is what takes a control out of the ring - `isFocusable()` reads the lock, while a
    # button's own setEnabled leaves its form listener registered. So this is also the honest way to
    # ask the question the highlight is about: can Enter still reach this button?
    s.invoke("state.set-lock", target="submit", value=True, reason="not now", settle=0.0)
    step()
    st = state()
    check("a button that left the ring is not the default any more",
            st.get("defaultButton") is None and bg(st, "submit") != DEFAULT,
            f'{st.get("defaultButton")} {hexc(bg(st, "submit"))}')
    s.invoke("state.set-lock", target="submit", value=False, settle=0.0)
    step()
    st = state()
    check("and it takes the slot back", st.get("defaultButton") == "submit",
            str(st.get("defaultButton")))

    print("\n-- 6. focus that arrived by keyboard wants to be seen")
    # Counted from a known field rather than from the top of the ring: one step, so the check does
    # not quietly become a check about how many fields the stand happens to have
    s.invoke("state.focus", target="required", settle=0.0)
    step()
    s.ok("input", native=True, events=key("TAB", "\t"))
    step()
    st = state()
    check("Tab landed on the checkbox", st.get("focusedField") == "check",
            str(st.get("focusedField")))
    check("a keyboard walk is :focus-visible", bg(st, "check") == FOCUS_VISIBLE,
            hexc(bg(st, "check")))
    check("...carrying both bits",
            (flags(st, "check") & F_FOCUS) != 0 and (flags(st, "check") & F_FOCUS_VISIBLE) != 0,
            hex(flags(st, "check")))

    # A direct request is the path a tap takes: focus, and no outline
    s.invoke("state.focus", target="check", settle=0.0)
    step()
    st = state()
    check("a direct request is :focus WITHOUT :focus-visible", bg(st, "check") == FOCUS,
            hexc(bg(st, "check")))
    check("...the FocusVisible bit is gone, Focus is not",
            (flags(st, "check") & F_FOCUS) != 0 and (flags(st, "check") & F_FOCUS_VISIBLE) == 0,
            hex(flags(st, "check")))

    print("\n-- 7. :focus-within climbs, and comes back down")
    s.invoke("state.focus", target="nested", settle=0.0)
    step()
    st = state()
    check("the nested field has focus", st.get("focusedField") == "nested",
            str(st.get("focusedField")))
    check("its own panel is :focus-within", bg(st, "inner") == FOCUS_WITHIN,
            hexc(bg(st, "inner")))
    check("and so is the panel above it", bg(st, "outer") == FOCUS_WITHIN,
            hexc(bg(st, "outer")))
    check("both carry the bit",
            (flags(st, "inner") & F_FOCUS_WITHIN) != 0
            and (flags(st, "outer") & F_FOCUS_WITHIN) != 0,
            f'{hex(flags(st, "inner"))} {hex(flags(st, "outer"))}')
    check("a text field is focus-visible even by a direct request",
            (flags(st, "nested") & F_FOCUS_VISIBLE) != 0, hex(flags(st, "nested")))

    s.invoke("state.focus", target="check", settle=0.0)
    step()
    st = state()
    check("focus leaving the panels takes the state with it",
            bg(st, "inner") == BASE and bg(st, "outer") == BASE,
            f'{hexc(bg(st, "inner"))} {hexc(bg(st, "outer"))}')
    check("...and no marker is left behind",
            (flags(st, "inner") & F_FOCUS_WITHIN) == 0
            and (flags(st, "outer") & F_FOCUS_WITHIN) == 0,
            f'{hex(flags(st, "inner"))} {hex(flags(st, "outer"))}')

    # --------------------------------------------------------------------------------------------
    # Section two lives in another stand. The inspector's `layout` command swaps it in; the previous
    # stand's commands disappear with it, so the order of these sections is fixed, not incidental.
    print("\n-- 8. switching to the selector stand")
    s.invoke("layout", name="selector", settle=0.3)
    step(3)
    sel = s.invoke("selector.state", settle=0.0)
    check("the selector stand answers", "not-class-off" in sel, str(list(sel)[:3]))

    def sbg(name):
        return sel[name]["background"]

    def scolor(name):
        return sel[name]["color"]

    print("\n-- 9. :not() is a test, not a decoration")
    check(":not(.x) matches a node without the class", sbg("not-class-off") == NOT_CLASS,
            hexc(sbg("not-class-off")))
    check("...and not one with it", sbg("not-class-on") == SEL_BASE,
            hexc(sbg("not-class-on")))
    check(":not(tag) reads the node's TYPE", sbg("not-tag-off") == NOT_TAG,
            hexc(sbg("not-tag-off")))
    check("...and excludes the named type", sbg("not-tag-on") == SEL_BASE,
            hexc(sbg("not-tag-on")))
    check(":not(#id) reads the node's NAME", sbg("not-id-off") == NOT_ID,
            hexc(sbg("not-id-off")))
    check("...and excludes the named id", sbg("hit") == SEL_BASE, hexc(sbg("hit")))
    check(":not(:hover) excludes a STATE the same way", sbg("not-state-off") == NOT_STATE,
            hexc(sbg("not-state-off")))
    check("...and a hovered node is excluded", sbg("not-state-on") == SEL_BASE,
            hexc(sbg("not-state-on")))
    check(":not(a, b) means NEITHER", sbg("not-list-none") == NOT_LIST,
            hexc(sbg("not-list-none")))
    check("...one of them is enough to exclude", sbg("not-list-one") == SEL_BASE,
            hexc(sbg("not-list-one")))

    # The predicate has to be re-read, not baked in at parse time
    s.invoke("selector.set-class", target="not-class-off", **{"class": "x"}, value=True,
            settle=0.0)
    step()
    sel = s.invoke("selector.state", settle=0.0)
    check("adding the class at runtime stops the match", sbg("not-class-off") == SEL_BASE,
            hexc(sbg("not-class-off")))
    s.invoke("selector.set-class", target="not-class-off", **{"class": "x"}, value=False,
            settle=0.0)
    step()
    sel = s.invoke("selector.state", settle=0.0)
    check("...and removing it brings the match back", sbg("not-class-off") == NOT_CLASS,
            hexc(sbg("not-class-off")))

    print("\n-- 10. :is() and :where() match alike")
    check(":is(.a, .b) matches the first option", sbg("is-a") == IS_MATCH, hexc(sbg("is-a")))
    check("...and the second", sbg("is-b") == IS_MATCH, hexc(sbg("is-b")))
    check("...and nothing else", sbg("is-c") == SEL_BASE, hexc(sbg("is-c")))
    check(":where(.a, .b) matches exactly the same way", sbg("where-a") == WHERE_MATCH,
            hexc(sbg("where-a")))
    check("...and misses the same way", sbg("where-c") == SEL_BASE, hexc(sbg("where-c")))

    print("\n-- 11. ...and count differently, which is the whole point")
    # `.sp:is(#spec, .zzz)` is a class + an id; `.sp.sp2` is two classes and is written LATER.
    # Position cannot save it, so the winner is decided by specificity alone.
    check(":is() carries the LARGEST specificity of its options",
            scolor("spec") == IS_SPEC_WIN, hexc(scolor("spec")))
    # `.wa:where(.wb)` is one class + nothing; `.wa` is one class and comes later. They tie, and a
    # tie goes to the later rule - which can only happen if :where() really counted for zero.
    check(":where() carries none, so the later plain rule wins",
            sbg("where-both") == WHERE_LOSES, hexc(sbg("where-both")))
    check("...and it still had to MATCH to be considered", sbg("where-one") == WHERE_LOSES,
            hexc(sbg("where-one")))

    print("\n-- 11a. box decoration stays where it was written")
    # `outline-*` and `border-radius` are not inherited on the web, and were here: every descendant
    # of a box with a 1px outline drew one of its own, which is what put a border around every row
    # of a menu whose surface declared one. The colour beside them is the control - it IS inherited,
    # so a child reporting nothing at all would read as "no style reached this node" instead.
    check("the box itself has the outline it declared",
            sel["inh-parent"]["hasOutlineWidth"] and sel["inh-parent"]["hasOutlineColor"]
            and sel["inh-parent"]["hasOutlineStyle"], sel["inh-parent"])
    check("...and the radius", sel["inh-parent"]["hasRadius"], sel["inh-parent"])
    check("a child inherits neither the outline width nor its colour",
            not sel["inh-child"]["hasOutlineWidth"] and not sel["inh-child"]["hasOutlineColor"],
            sel["inh-child"])
    check("...nor the outline style", not sel["inh-child"]["hasOutlineStyle"], sel["inh-child"])
    check("...nor the corner radius", not sel["inh-child"]["hasRadius"], sel["inh-child"])
    # The control, in two halves: the child DOES take the parent's inherited property, and a sample
    # outside the box does not - so "the child reports nothing" cannot pass for "nothing declares
    # it anywhere".
    check("but it does inherit what IS inherited, which is what makes the four above meaningful",
            sel["inh-child"]["hasFontWeight"], sel["inh-child"])
    check("...and a sample outside the box has none of it",
            not sel["bad5"]["hasFontWeight"], sel["bad5"])

    print("\n-- 12. a refused argument takes its own rule down, and nothing else")
    for name, what in (("bad1", "a combinator inside :is()"),
            ("bad2", "a nested functional pseudo-class"), ("bad3", "an empty :is()"),
            ("bad4", "a structural pseudo-class inside :is()"),
            ("bad5", "an unbalanced argument")):
        check(f"{what} is refused, and the next rule survives", sbg(name) == BAD_FALLBACK,
                hexc(sbg(name)))

    # --------------------------------------------------------------------------------------------
    print("\n-- 13. switching to the arithmetic stand")
    s.invoke("layout", name="calc", settle=0.3)
    step(3)

    def calc_state():
        return s.invoke("calc.state", settle=0.0)

    cs = calc_state()
    check("the arithmetic stand answers", "sum" in cs, str(list(cs)[:3]))

    def width(name):
        return cs[name]["width"] if cs[name]["has"] else None

    def applied(name):
        return cs[name]["applied"]

    print("\n-- 14. calc(): four operations, parentheses, var()")
    for name, expect in (("sum", 24.0), ("diff", 60.0), ("mul-right", 32.0), ("mul-left", 32.0),
            ("div", 16.0), ("parens", 30.0), ("nested", 40.0), ("with-var", 12.0)):
        check(f"calc {name} = {expect:g}px", width(name) == hundredths(expect),
                f"{width(name)} != {hundredths(expect)}")
    # a percent is stored as a FRACTION, so 50% + 10% is 0.6 rather than 60
    check("calc percent + percent stays a fraction", width("percent") == hundredths(0.6),
            str(width("percent")))

    print("\n-- 15. min(), max(), clamp()")
    for name, expect in (("min-basic", 12.0), ("max-basic", 30.0), ("clamp-low", 20.0),
            ("clamp-mid", 35.0), ("clamp-high", 60.0), ("min-in-calc", 30.0),
            ("min-of-calc", 45.0), ("min-var", 4.0)):
        check(f"{name} = {expect:g}px", width(name) == hundredths(expect), f"{width(name)} != {hundredths(expect)}")

    print("\n-- 16. an expression that cannot reduce is DROPPED, not approximated")
    # every one of these must fall back to `.box { width: 40px }` - a half-folded length would be
    # the one failure mode worth fearing here
    for name, why in (("mixed-units", "100% - 20px"), ("unit-squared", "2px * 3px"),
            ("div-by-zero", "10px / 0"), ("unit-plus-number", "10px + 5"),
            ("unbalanced", "an unclosed paren"), ("min-one", "min() of one value"),
            ("min-mixed", "min(10px, 50%)"), ("clamp-short", "clamp() of two"),
            ("clamp-mixed", "clamp with a foreign unit")):
        check(f"{why} is refused", width(name) == hundredths(40.0), f"{width(name)} != {hundredths(40.0)}")

    print("\n-- 17. a custom property declared on ONE node")
    check("the node sees its own property", cs["local"]["k"] == "3", str(cs["local"]["k"]))
    check("...and calc() uses it", width("local") == hundredths(30.0), str(width("local")))
    check("a child inherits it", cs["local-child"]["k"] == "3", str(cs["local-child"]["k"]))
    check("...and computes with it", width("local-child") == hundredths(6.0), str(width("local-child")))
    check("a node-local declaration beats a rule that matched the same node",
            cs["local-wins"]["k"] == "2" and width("local-wins") == hundredths(200.0),
            f'{cs["local-wins"]["k"]} {width("local-wins")}')
    check("the name is normalised: 'K' is '--k'",
            cs["normalized"]["k"] == "25" and width("normalized") == hundredths(25.0),
            f'{cs["normalized"]["k"]} {width("normalized")}')
    check("nothing leaks to a sibling", cs["sum"]["k"] == "", repr(cs["sum"]["k"]))

    print("\n-- 18. changing it repaints, and only invalidation can")
    check("the initial value was APPLIED, not merely resolved", applied("local") == hundredths(30.0),
            str(applied("local")))
    check("...and applied to the inheriting child", applied("local-child") == hundredths(6.0),
            str(applied("local-child")))

    # Nothing moves, no class flips, no rule starts or stops matching: the custom-property path is
    # the only thing that can carry this into the applied size.
    s.invoke("calc.set-var", target="local", name="--k", value="5", settle=0.0)
    s.invoke("calc.remove-var", target="removed", name="--k", settle=0.0)
    step(3)
    cs = calc_state()

    check("the changed value resolved", width("local") == hundredths(50.0), str(width("local")))
    check("...and was applied", applied("local") == hundredths(50.0), str(applied("local")))
    check("the child followed", width("local-child") == hundredths(10.0) and applied("local-child") == hundredths(10.0),
            f'{width("local-child")} {applied("local-child")}')
    check("removing the property falls back to the var() default",
            cs["removed"]["k"] == "" and applied("removed") == hundredths(7.0),
            f'{cs["removed"]["k"]} {applied("removed")}')

finally:
    try:
        s.ok("quit")
    except Exception:
        pass
    try:
        proc.wait(timeout=10)
    except Exception:
        proc.kill()

print(f"\n{checks} checks, {failures} failures")
sys.exit(1 if failures else 0)
