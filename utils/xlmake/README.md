# xlmake

It's like Ninja, but it's **make**.

`xlmake` is a GNU-make-compatible makefile engine and build driver built on the Stappler
`stappler_makefile` library and the `runtime` dispatch reactor. It reads GNU-make-style
makefiles, and can either **inspect** them (print variables, recipes, prerequisites, a
`--print-data-base` dump) or **build** them, running recipes as child processes multiplexed
through a single-threaded event loop.

It is the build driver for the Stappler/Xenolith make system (`make/universal.mk`), and it is
drop-in enough that GNU-make-oriented tooling — notably the VSCode *Makefile Tools* extension —
can use it unmodified.

---

## Table of contents

- [Building](#building)
- [Two modes](#two-modes)
- [Command-line reference](#command-line-reference)
- [How it works](#how-it-works)
- [Similarities with GNU make](#similarities-with-gnu-make)
- [Differences from GNU make](#differences-from-gnu-make)
- [Environment variables](#environment-variables)
- [xlmake-specific extensions](#xlmake-specific-extensions)
- [Paths with spaces](#paths-with-spaces)
- [Supported make language](#supported-make-language)
- [Tooling compatibility](#tooling-compatibility)
- [Examples](#examples)
- [Exit codes](#exit-codes)

---

## Building

`xlmake` is itself a Stappler executable. From the engine root:

```sh
make -C utils/xlmake -j8
# binary: utils/xlmake/stappler-build/<target>/<build-type>/<cc>/xlmake
```

It depends on three modules only: `stappler_makefile`, `stappler_filesystem`, `runtime`.

---

## Two modes

The **first** command-line token selects the mode; every later flag applies to that mode.

| First token | Mode | Purpose |
|---|---|---|
| *(none / a normal arg)* | **Build** *(default)* | resolve the dependency graph and run recipes |
| `-b`, `--build` | Build | same as default, stated explicitly |
| `-i`, `--inspect` | Inspect | print variables / recipes / prerequisites, never run a recipe |
| `-h`, `--help` | Help | usage text |

Like `make`, a bare `xlmake` builds the default goal of the makefile in the current directory
(searching `GNUmakefile`, `makefile`, `Makefile` in that order). The default goal follows GNU's
`.DEFAULT_GOAL` variable: it is mirrored from the first explicitly declared ordinary (non-`.`-special,
non-`%`-pattern) target and can be overridden by assigning `.DEFAULT_GOAL` a target name (or cleared to
`.DEFAULT_GOAL :=` for no default goal). Reading `$(.DEFAULT_GOAL)` yields the current default goal.

---

## Command-line reference

### Shared options (both modes)

| Option | Meaning |
|---|---|
| `-f`, `--file FILE` | read `FILE` as a makefile (repeatable) |
| `-C`, `--directory DIR` | change into `DIR` first (like GNU `-C`); affects relative paths and recursive `$(MAKE) -C` |
| `-W`, `--pedantic` | report every engine warning, not just the default set |
| `VAR=VALUE` | set a variable from the command line (also `:=`, `::=`, `:::=`, `+=`, `?=`); beats makefile assignments, as in GNU make |
| `P:VAR=VALUE` | as above, but encode spaces in `VALUE` as a path so a space-containing path stays one word; the `P:` marker is stripped, the variable is `VAR` — see [Paths with spaces](#paths-with-spaces) |
| *positional* | target goals (default goal when none given) |

### Build mode (default)

| Option | Meaning |
|---|---|
| `-j`, `--jobs [N]` | up to `N` concurrent recipes (default: all cores; `-j1` serializes; bare `-j` is unlimited) |
| `-k`, `--keep-going` | keep building independent targets after a failure |
| `-n`, `--dry-run` | print recipe commands without running them |
| `-s`, `--silent` | do not echo recipe command lines |
| `-B`, `--always-make` | treat every target as out of date |
| `-p`, `--print-data-base` | dump the makefile database and exit |
| `-q`, `--question` | run nothing; exit 1 if any target is out of date |
| `-w`, `--print-directory` | print `Entering/Leaving directory` around the build |
| `--no-print-directory` | suppress those lines (overrides `-w` and the sub-make auto-enable) |
| `--no-space-escape` | do not shell-escape spaces in recipe paths; you quote them yourself, as in GNU make — see [Paths with spaces](#paths-with-spaces) |
| `-r`, `--no-builtin-rules` / `-R`, `--no-builtin-variables` | accepted, no effect (xlmake ships no built-in implicit-rule database) |

### Inspect mode (`-i`)

All inspect actions are combinable and act on the named targets, or the default goal when none
are given.

| Option | Meaning |
|---|---|
| `-p`, `--print-vars` | print every variable: `name [origin, flavor] = value` |
| `-V`, `--var NAME` | print one variable's expanded value (repeatable) |
| `--recipe` | print each target's fully expanded recipe |
| `--prerequisites` | print each target's prerequisite list |
| `-r`, `--recursive` | with `--prerequisites`: transitive closure in dependency order; with `-q`: the out-of-date set |
| `-q`, `--out-of-date` | restrict `--recipe`/`--prerequisites` to out-of-date items |
| `-P`, `--phony-prereqs` | with `-r -q`, judge a phony target by its prerequisites |

> **Note on overloaded short flags:** `-p`, `-q`, and `-r` mean different things in the two
> modes (`-p` = *print-vars* in inspect vs *print-data-base* in build; `-q` = *out-of-date* vs
> *question*; `-r` = *recursive* vs the *no-builtin-rules* no-op). The mode is fixed by the first
> token, so there is no ambiguity at parse time.

---

## How it works

The executor is a **single-threaded, non-blocking build reactor**. The main thread does *all*
makefile work — building the plan, resolving and expanding recipes, stat-ing files, printing
output. The only parallel units are **child processes**:

- Each recipe command line is launched through the runtime dispatch reactor
  (`dispatch::Looper::spawnProcess`) as a `/bin/sh -c` child, with **stdout and stderr merged**
  onto one pipe.
- Inter-recipe parallelism (up to the `-j` limit) is achieved by keeping several children in
  flight at once, all multiplexed by one event loop. There are **no worker threads, no
  `ThreadPool`, no blocking `popen`, and no hand-rolled `fork`/`waitpid`/`poll`** in the build
  path.
- A child's exit fires a completion that advances its recipe, frees the job slot, and dispatches
  any freshly-unblocked nodes.

**Build plan & scheduling.** Targets become a topologically ordered plan; each node tracks its
unmet-prerequisite count and is dispatched when that count reaches zero. Normal prerequisites
propagate a rebuild (cascade); order-only prerequisites gate ordering only.

**Output buffering.** By default each target's output is **buffered and flushed atomically** when
the target finishes, so a target's block stays contiguous even under `-j`. Two cases stream
**line-buffered** live instead: a recursive `$(MAKE)` (so its sub-build progress is visible), and
any target that opts in with [`.TARGET_BUFFER=line`](#target-buffer). Streaming writes only whole
lines, so concurrent targets never tear each other's output.

**Progress counter.** Real builds print a ninja-style `[N/M] name (time)` line per built target
(`M` is computed up front with the same staleness gate the dispatcher uses, so `N` reaches `M`
exactly). `time` is that target's own wall-clock recipe time (`742ms`, `12.3s`, `1m 05s`); it is
omitted for a live-streamed target (recursive `$(MAKE)`, or [`.TARGET_BUFFER=line`](#target-buffer)),
whose counter is printed before the recipe finishes. The whole counter is off in `--dry-run`,
`--silent`, and `--print-data-base`. See [`.TARGET_NAME`](#target-name).

**Recursive make.** `$(MAKE)` expands to xlmake's own path. Sub-makes track depth via `MAKELEVEL`
(exported one level deeper to each child), print `xlmake[N]: Entering/Leaving directory` like
`make[N]:`, and tag the progress counter with their depth.

---

## Similarities with GNU make

xlmake aims for GNU make 4.x source compatibility for the common feature set:

- **Variables:** recursive (`=`), simple (`:=`, `::=`, `:::=`), append (`+=`), conditional
  (`?=`); `override`; `export`/`unexport` (per-variable and the bare export-all form);
  `define`/`endef` multi-line variables; variable *origins* and *flavors*.
- **Rules:** explicit rules, multiple targets, pattern rules (`%`), order-only prerequisites
  (`|`), automatic variables (`$@`, `$<`, `$^`, `$?`, `$*`, …), and **target-specific variables**
  (`target: VAR = value`). Targets and prerequisites may contain `\ `-escaped spaces (see
  [Paths with spaces](#paths-with-spaces)).
- **Recipes:** `@` (silent), `-` (ignore errors), `+` (run even under `-n`) line prefixes; one
  shell (`/bin/sh -c`) per line; a recipe line that **expands to multiple lines** (via a variable
  or a canned `define`) runs each as its own command with its own prefix.
- **Conditionals & directives:** `ifdef`/`ifndef`/`ifeq`/`ifneq`/`else`/`endif`, `include` /
  `-include` / `sinclude`, `undefine`.
- **The standard predefined variables:** `CC`, `CXX`, `AR`, `RM`, the `COMPILE.*`/`LINK.*` recipe
  templates, `MAKE`, `MAKE_COMMAND`, `CURDIR`, `MAKECMDGOALS`, `SHELL`, `.SHELLFLAGS`, `SUFFIXES`,
  `.LIBPATTERNS`, … (origin *default*, so makefile/command-line assignments override them).
- **`.DEFAULT_GOAL`:** the default-goal variable — auto-set to the first ordinary target, readable as
  `$(.DEFAULT_GOAL)`, and overridable (assign a target name, or clear it for no default goal).
- **Compatibility flags** used by tooling: `-p`, `-q`, `-B`, `-w`, `-n`, `-s`, `-k`, `-j`,
  `-C`, `-f`, command-line `VAR=VALUE`.

---

## Differences from GNU make

**xlmake** designed for speed, many heavy GNU make features id omitted.

These are the behaviors to be aware of when porting a makefile:

- **The environment is loaded lazily, not bulk-imported.** GNU make turns every environment variable
  into a make variable up front; xlmake instead resolves an environment variable **on demand** — the
  first time `$(FOO)` (or `$(origin FOO)`, or `ifdef FOO`) needs a value that nothing else defines, it
  is read from the environment, given origin *environment*, and cached. So `FOO=bar xlmake` **does**
  make `$(FOO)` visible, exactly as in GNU make, but the table is never pre-filled with the whole
  environment. One precedence nuance: xlmake's **built-in defaults are not overridden by the
  environment** (a fallback is consulted only for a name nothing else defines, and the defaults *are*
  definitions). So `CC=clang xlmake` leaves `$(CC)` as the default `cc`; assign `CC` in the makefile
  or on the command line (`xlmake CC=clang`) to change it. See
  [Environment variables](#environment-variables).
- **Target-specific variables are own-recipe scope only.** A `target: VAR = value` assignment
  applies while expanding *that target's* recipe; it does **not** propagate down to the
  prerequisites' recipes the way GNU make's do.
- **Target-specific `+=` is eager.** It is combined into a simple value at apply time (it loses
  recursive laziness), so a later change to the base variable is not picked up inside that target.
- **No built-in implicit-rule database.** There are no built-in suffix/pattern rules (`%.o: %.c`,
  etc.); define the pattern rules you need. `-r`/`-R` are accepted as no-ops.
- **Some special targets are recognized but inert.** `.PHONY`, `.PRECIOUS`, `.SECONDARY`,
  `.INTERMEDIATE`, `.SUFFIXES`, `.DEFAULT` are acted on; `.NOTPARALLEL`, `.ONESHELL`,
  `.DELETE_ON_ERROR`, `.SILENT`, `.IGNORE` are parsed but not yet enforced.
- **stdout/stderr are merged** for every recipe (one pipe), and non-recursive recipe output is
  buffered until the target completes (see [How it works](#how-it-works)).
- **Diagnostic markers in `--print-data-base` are fixed English byte-strings** (GNU localizes
  them); this is deliberate, so parsers stay stable across locales.
- **One mode per invocation**, selected by the first token (no mixing inspect and build actions).

---

## Environment variables

xlmake bridges the process environment and the make variable table in both directions, the GNU make
way, with one efficiency twist: nothing is bulk-copied.

### Reading: lazy import

An environment variable is pulled in **on first reference**. The first time an undefined name is
needed — `$(FOO)`, `$(origin FOO)`, `$(flavor FOO)`, `ifdef FOO`, `$(FOO:.c=.o)`, … — xlmake reads it
from the environment, exposes it with origin *environment*, and caches it for the rest of the run:

```sh
DEBUG=1 xlmake            # $(DEBUG) is "1", $(origin DEBUG) is "environment"
```

Precedence follows GNU make's order, **except that the environment does not override xlmake's built-in
defaults**: the environment is consulted only for a name that nothing else defines. So a makefile
assignment, a command-line assignment, and a built-in default (`CC`, `CXX`, the `COMPILE.*`/`LINK.*`
templates, …) all win over the environment. To change a defaulted variable, assign it in the makefile
or on the command line (`xlmake CC=clang`), not via the environment. A name with no built-in default
(your own `$(STAPPLER_BUILD_ROOT)`, `$(DEBUG)`, …) is taken straight from the environment.

An environment value is used **verbatim**, so an embedded space is an ordinary word separator. When an
environment variable holds a path that may contain spaces, wrap it with
[`$(xl_make_path …)`](#extension-functions) at the point of use:

```makefile
ROOT := $(xl_make_path $(STAPPLER_BUILD_ROOT))   # keep "/home/My App/make" one word
```

### Writing: `export` / `unexport`

`export` marks a variable to be placed in the environment of every recipe child process — and of any
recursive `$(MAKE)` — so the change propagates down the whole build tree (a child inherits xlmake's
environment, so an exported variable a sub-make reads back simply arrives as origin *environment*).
Exported values are visible to `$(shell …)` too. All the GNU forms are supported:

```makefile
export PATH                       # export an already-defined variable
export CFLAGS := -O2 -g           # define and export in one line (also =, +=, ?=)
export VERBOSE QUIET              # export several names at once
unexport SECRET                   # keep a variable out of the child environment

export                            # export-all: export every user-set variable from here on
unexport                          # turn export-all back off
```

A per-variable `export`/`unexport` always beats the bare export-all toggle. Under export-all only
*user-set* variables are exported — the built-in defaults and the automatic variables (`$@`, `$<`, …)
are excluded — and only names that are valid environment identifiers (`[A-Za-z_][A-Za-z0-9_]*`), so the
engine's dotted specials (`.DEFAULT_GOAL`, `COMPILE.c`, …) never leak. A variable that came from the
environment is already there, so it propagates to children without an explicit `export`. The exported
value is the variable's fully expanded text, computed once after the makefiles are read.

> **Limitation:** `export define NAME … endef` exports nothing — the multi-line variable is still
> defined, but not marked for export; use a separate `export NAME` line.

---

## xlmake-specific extensions

### Special target-specific variables

#### `.TARGET_NAME`
Overrides the label shown by the progress counter for a target. Resolved in the target's own
scope; falls back to the target's name as written in the makefile.

```makefile
app: .TARGET_NAME := my-app      # progress line shows "[N/M] my-app"
```

#### `.TARGET_BUFFER`
Forces how a target's recipe output is buffered:

- `line` — stream output **live, line-buffered**, the same way a recursive `$(MAKE)` is streamed
  (useful for long-running recipes whose incremental progress should appear as it is produced).
- `full` *(default, or any other value / unset)* — buffer the whole block and flush it atomically
  when the target completes.

```makefile
codegen: .TARGET_BUFFER := line
codegen:
	@./long-running-generator.sh
```

### Coloured compiler diagnostics

Because recipe output goes through a pipe, `clang`/`gcc` would auto-disable colour. xlmake sets
**`XLMAKE_COLOR=1`** (and `CLICOLOR_FORCE=1`) when its own stdout is a terminal — and exports the
decision to recursive sub-makes. Makefiles can key off `$(XLMAKE_COLOR)` to add
`-fdiagnostics-color=always` to compiler flags (the Stappler make system does this). When stdout
is a pipe/redirect (e.g. under tooling), colour is left off so logs stay clean.

### Provided variables

- `XLMAKE_VERSION` — the xlmake version string.
- `XLMAKE_COLOR` — `1`/`0`, the colour decision described above.
- `XL_UNAME_SYSNAME`, `XL_UNAME_NODENAME`, `XL_UNAME_RELEASE`, `XL_UNAME_VERSION`,
  `XL_UNAME_MACHINE`, `XL_UNAME_DOMAINNAME` — from `uname(2)`.
- `XL_GLIBC_VERSION` — host glibc version, when detectable.

### In-process recipe directives

These predefined variables (origin *default*, so a makefile can still override them) expand to a
marker the executor performs **in-process** — no shell, no child process, no `fork`/`exec`. They are
cheaper than spawning `mkdir`/`cp`/`echo`, and behave identically on every platform, **including
Windows**, where there is no POSIX shell.

| Directive | Acts like | Notes |
|---|---|---|
| `$(MKDIR) <dir>…` | `mkdir -p` | one or more directories; an existing directory is success |
| `$(REMOVE) <path>…` | `rm -rf` | recursive; a missing path is success. (`$(RM)` is left as the GNU default `rm -f`, which *does* spawn a process) |
| `$(CP) <src> <dst>` | `cp -f` | overwrites an existing file destination; a directory destination receives `<src>` inside it |
| `$(WRITE) <file> <text>` | `echo text > file` | truncate/create; one surrounding quote layer is stripped and a trailing newline ensured |
| `$(APPEND) <file> <text>` | `echo text >> file` | append; same quoting/newline rule |
| `$(ECHO) <text>` | `echo` | print one line to xlmake's output (shown even in non-verbose mode) |

```makefile
$(OUTDIR):
	$(MKDIR) $(OUTDIR)

build/config.h: ; $(WRITE) $@ "#define VERSION \"$(VERSION)\""

clean:
	$(REMOVE) $(OUTDIR)
```

Their path operands accept spaces (write an authored space as `\ `) — see
[Paths with spaces](#paths-with-spaces).

### Extension functions

Direct file I/O and path helpers, modeled on GNU make's `$(file …)`. Like `$(shell)`, the I/O ones
touch the filesystem at expansion time and are only safe under the trusted-makefile model.

| Function | Result |
|---|---|
| `$(xl_cat <file>)` | the file's contents, with a single trailing newline removed (empty if missing) |
| `$(xl_write <file>,<text>)` | create/overwrite `<file>` with `<text>`; expands to nothing |
| `$(xl_append <file>,<text>)` | append `<text>` to `<file>` (created if needed); expands to nothing |
| `$(xl_mkdir <dir>)` | `mkdir -p <dir>`; expands to nothing |
| `$(xl_make_path <text>)` | force-encode every space in `<text>` as a path placeholder, so a space-containing path stays one word — see [Paths with spaces](#paths-with-spaces) |
| `$(xl_make_plain <text>)` | inverse of `xl_make_path`: decode path placeholders back to real spaces, yielding plain text — see [Paths with spaces](#paths-with-spaces) |

---

## Paths with spaces

GNU make uses whitespace as its universal word separator, so a path that contains a space is
normally torn into separate words and the build breaks. xlmake handles space-containing paths
**transparently**: internally a space *inside a path* is held as a reserved placeholder byte (not
whitespace), so the path stays a single word through every list, function and pattern; it is turned
back into a real space only at the edges — when a recipe is launched, when a file is touched, and
when a path is printed. Nothing special is needed for the common case of a build tree under a
directory with a space (`/home/me/My Project`, `C:/Users/John Doe/…`).

**Authored paths.** Write `\ ` (backslash-space) for a literal space in a target, prerequisite,
variable value or function argument, exactly as in GNU make:

```makefile
SRCDIR := My\ Sources
build/my\ widget.o: $(SRCDIR)/my\ widget.c
	$(CC) -c $< -o $@
```

Inside a **recipe body** a `\ ` is left untouched and passed to the shell verbatim (matching GNU
make) — only the path-bearing parts of a rule treat `\ ` as an escaped space.

**Discovered paths.** Spaces in paths produced by `$(wildcard)`, `$(realpath)`, `$(abspath)` and in
the `$(CURDIR)`, `$(MAKEFILE_LIST)`, `$(MAKE_COMMAND)`/`$(MAKE)` variables are preserved
automatically, so `$(lastword $(MAKEFILE_LIST))`, `$(dir …)`, `$(notdir …)`, `$(patsubst …)` etc.
each operate on the whole path.

**Recipes.** When a recipe runs, a path's space is escaped for the shell so even an *unquoted* recipe
works: POSIX emits `\ ` (backslash-space); Windows emits a quoted space (`My" "Src`, which the
command-line parser fuses into one argument). The escaping is **quote-aware** — a path you already
wrapped in quotes (`"$<"`) becomes a plain space and is never double-escaped. Pass
[`--no-space-escape`](#build-mode-default) to disable escaping entirely: the placeholder then decodes
to a plain space and you quote recipe paths yourself, exactly like GNU make.

**`$(shell …)`.** A space-containing path used inside `$(shell …)` is escaped for the shell with the
same quote-aware rule as recipes — so `$(shell cat $(SRCDIR)/my file.c)` works unquoted, and
`$(shell cat "$(SRCDIR)/my file.c")` works quoted (the placeholder inside quotes becomes a plain
space). No `xl_make_path`/`xl_make_plain` dance is needed just to pass a discovered path to a command.

**Recursive make.** `$(MAKE)` works even when xlmake's own path contains a space
(`/opt/My Tools/xlmake`) — the program path reaches the recipe shell as a single argument.

**In-process directives.** `$(MKDIR)`, `$(REMOVE)`, `$(CP)` and the `$(WRITE)`/`$(APPEND)` file path
take space-containing operands with no quoting needed (xlmake parses their operands itself); write an
authored space as `\ `:

```makefile
$(CP) $(SRCDIR)/my\ file.c build/my\ file.c
```

**Force-encoding a path.** `$(xl_make_path <text>)` encodes every space in its argument, to make a
path that arrived **without** `\ ` escaping safe — e.g. from `$(shell …)`, an environment variable or
a plain literal. It is a pure text transform (no filesystem access) and idempotent:

```makefile
GEN := $(xl_make_path $(shell printf '%s' "$(HOME)/My Tools/gen"))
```

**Decoding a path.** `$(xl_make_plain <text>)` is the inverse: it turns the placeholders back into real
spaces, giving plain text (which re-splits into words if expanded unquoted). Use it when a value must
be handed to something that expects literal spaces in make text — e.g. embedding a path in a message.

**Command-line variables.** A value passed on the command line is taken verbatim (like GNU make), so
a space in it is *not* treated as a path. Prefix the assignment with **`P:`** to encode it:

```sh
xlmake 'P:STAPPLER_BUILD_ROOT=/home/My App/make' all
# STAPPLER_BUILD_ROOT (marker stripped) keeps the space as part of the path
```

**Limits.** A recipe that pipes or redirects a space path through the shell (`> "$@"`, `cmd | …`)
still needs your own quoting; ordinary compile/link/copy lines and the in-process directives do not.
The reserved placeholder byte (`0x1F`) is assumed not to occur in real paths.

---

## Supported make language

**Functions** (GNU `$(fn …)` syntax):

```
subst patsubst strip findstring filter filter-out sort
word wordlist words firstword lastword
dir notdir suffix basename addsuffix addprefix join
wildcard realpath abspath
if or and foreach call let
origin flavor eval shell file
error warning info
```

**xlmake extension functions:** `xl_cat` `xl_write` `xl_append` `xl_mkdir` `xl_make_path` `xl_make_plain` — see
[Extension functions](#extension-functions).

**In-process recipe directives:** `$(WRITE)` `$(APPEND)` `$(MKDIR)` `$(REMOVE)` `$(CP)` `$(ECHO)` —
see [In-process recipe directives](#in-process-recipe-directives).

**Directives:** `ifdef` `ifndef` `ifeq` `ifneq` `else` `endif` `define` `endef`
`include` `-include` `sinclude` `override` `undefine` `export` `unexport`.

**Special targets acted on:** `.PHONY` `.PRECIOUS` `.SECONDARY` `.INTERMEDIATE` `.SUFFIXES`
`.DEFAULT`.

---

## Tooling compatibility

xlmake implements the GNU-make subset the **VSCode Makefile Tools** extension relies on:

- `-p` / `--print-data-base` emits a `# Variables` section and a `# Files` target list (with
  `#  Phony target …` annotations) framed by `# Finished Make data base`, using fixed English
  markers a parser can match.
- `-q` / `--question` returns the standard 0/1 status without running anything.
- `-n`, `-B`, `-w`, `-C`, `-f`, and command-line `VAR=VALUE` behave as the extension expects.

These query paths return before any build, so their output stays free of the progress counter and
colour escapes.

---

## Examples

The [`samples/`](samples) directory contains small, focused makefiles, each with header comments
showing the `xlmake` command(s) to try and the GNU `make` equivalent:

| Sample | Demonstrates |
|---|---|
| `build-parallel.mk` | parallel build (`-j`), `-n`, `clean`, up-to-date no-ops |
| `target-vars.mk` | target-specific variables (`=`, `+=`, multiple targets, `private`) |
| `c-project.mk` | a small C-project layout with pattern rules |
| `depgraph.mk` / `find-recursive.mk` | dependency graphs and recursive prerequisite walks |
| `patsubst.mk` / `functions.mk` / `filepath.mk` | text/function/path expansion |
| `eval.mk` / `cmdline.mk` / `include-main.mk` | `eval`, command-line assignments, `include` |
| `warnings.mk` / `stappler-cases.mk` | engine warnings and Stappler-specific patterns |
| `special-chars.mk` | `+`/`?`/`,`/`(`/`)` literal in target and variable names (e.g. `libc++`) |
| `aggregator.mk` | recipe-less aggregator targets (`all: a b` with no recipe) |

For example:

```sh
xlmake -f samples/build-parallel.mk -j4 all      # parallel build
xlmake -i -f samples/target-vars.mk --recipe prog  # inspect an expanded recipe
xlmake -i -f samples/depgraph.mk --prerequisites -r all   # transitive prereqs
xlmake -f samples/special-chars.mk all           # '+'/'?'/',' literal in target names
xlmake -f samples/aggregator.mk world            # recipe-less aggregator targets
```

---

## Exit codes

| Code | Meaning |
|---|---|
| `0` | success / everything up to date (build); query satisfied (`-q`: nothing to do) |
| `1` | `-q`/`--question`: a target is out of date; or a usage error |
| `2` | build failure, no rule to make a target, or a dependency cycle |
