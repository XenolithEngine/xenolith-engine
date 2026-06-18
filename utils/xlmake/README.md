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
- [xlmake-specific extensions](#xlmake-specific-extensions)
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
(searching `GNUmakefile`, `makefile`, `Makefile` in that order). It's rule for the implicit default goal is different from GNU make, so, better define it with `.DEFAULT`

---

## Command-line reference

### Shared options (both modes)

| Option | Meaning |
|---|---|
| `-f`, `--file FILE` | read `FILE` as a makefile (repeatable) |
| `-C`, `--directory DIR` | change into `DIR` first (like GNU `-C`); affects relative paths and recursive `$(MAKE) -C` |
| `-W`, `--pedantic` | report every engine warning, not just the default set |
| `VAR=VALUE` | set a variable from the command line (also `:=`, `::=`, `:::=`, `+=`, `?=`); beats makefile assignments, as in GNU make |
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

**Progress counter.** Real builds print a ninja-style `[N/M] name` line per built target (`M` is
computed up front with the same staleness gate the dispatcher uses, so `N` reaches `M` exactly).
It is off in `--dry-run`, `--silent`, and `--print-data-base`. See [`.TARGET_NAME`](#target-name).

**Recursive make.** `$(MAKE)` expands to xlmake's own path. Sub-makes track depth via `MAKELEVEL`
(exported one level deeper to each child), print `xlmake[N]: Entering/Leaving directory` like
`make[N]:`, and tag the progress counter with their depth.

---

## Similarities with GNU make

xlmake aims for GNU make 4.x source compatibility for the common feature set:

- **Variables:** recursive (`=`), simple (`:=`, `::=`, `:::=`), append (`+=`), conditional
  (`?=`); `override`; `define`/`endef` multi-line variables; variable *origins* and *flavors*.
- **Rules:** explicit rules, multiple targets, pattern rules (`%`), order-only prerequisites
  (`|`), automatic variables (`$@`, `$<`, `$^`, `$?`, `$*`, …), and **target-specific variables**
  (`target: VAR = value`).
- **Recipes:** `@` (silent), `-` (ignore errors), `+` (run even under `-n`) line prefixes; one
  shell (`/bin/sh -c`) per line; a recipe line that **expands to multiple lines** (via a variable
  or a canned `define`) runs each as its own command with its own prefix.
- **Conditionals & directives:** `ifdef`/`ifndef`/`ifeq`/`ifneq`/`else`/`endif`, `include` /
  `-include` / `sinclude`, `undefine`.
- **The standard predefined variables:** `CC`, `CXX`, `AR`, `RM`, the `COMPILE.*`/`LINK.*` recipe
  templates, `MAKE`, `MAKE_COMMAND`, `CURDIR`, `MAKECMDGOALS`, `SHELL`, `.SHELLFLAGS`, `SUFFIXES`,
  `.LIBPATTERNS`, … (origin *default*, so makefile/command-line assignments override them).
- **Compatibility flags** used by tooling: `-p`, `-q`, `-B`, `-w`, `-n`, `-s`, `-k`, `-j`,
  `-C`, `-f`, command-line `VAR=VALUE`.

---

## Differences from GNU make

**xlmake** designed for speed, many heavy GNU make features id omitted.

These are the behaviors to be aware of when porting a makefile:

- **The environment is not bulk-imported.** GNU make turns every environment variable into a make
  variable; xlmake injects only a curated set — `MAKELEVEL`, `HOME`, the GNU standard defaults
  (`CC`, `CXX`, `MAKE`, …), and xlmake's own `XL_*`/`XLMAKE_*` variables. An arbitrary
  `FOO=bar xlmake` does **not** make `$(FOO)` visible.
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

**Directives:** `ifdef` `ifndef` `ifeq` `ifneq` `else` `endif` `define` `endef`
`include` `-include` `sinclude` `override` `undefine`.

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

For example:

```sh
xlmake -f samples/build-parallel.mk -j4 all      # parallel build
xlmake -i -f samples/target-vars.mk --recipe prog  # inspect an expanded recipe
xlmake -i -f samples/depgraph.mk --prerequisites -r all   # transitive prereqs
```

---

## Exit codes

| Code | Meaning |
|---|---|
| `0` | success / everything up to date (build); query satisfied (`-q`: nothing to do) |
| `1` | `-q`/`--question`: a target is out of date; or a usage error |
| `2` | build failure, no rule to make a target, or a dependency cycle |
