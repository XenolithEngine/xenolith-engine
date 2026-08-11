---
name: code-style
description: >-
  Write C/C++ in the Xenolith/Stappler monorepo the way the codebase is written.
  Use BEFORE creating or editing sources in runtime/, stappler/, xenolith/,
  tests/ or utils/ — new file, new header, new class, new platform branch, new
  allocation. Covers .cpp SCU vs .cc subunits, license/include-guard/namespace
  layout, SP_PUBLIC and SPRT_API, naming (_member, handleXxx, Rc<T>::create),
  #if SPRT_WINDOWS platform guards, pool vs malloc ownership rules (AllocPool,
  sprt::__delete, new (pool) T), independent pools that move between threads
  (PoolRef, SharedRef<T>, PoolObject), threads and dispatch (Looper, Task,
  Handle), the scene graph (Node geometry, and using System/Component instead of
  subclassing Node), strings (StringView lifetime, mem_std vs
  mem_pool, CallbackStream, reader API), error handling with no exceptions
  (Status, Result<T>, slog(), asserts), weakly-typed data (data::Value: the
  mem_std/mem_pool interfaces, setValue(value, key) order, the read-only
  Value::Null sentinel, JSON/CBOR round-tripping), windows and OS integration
  (Context::createWindow + WindowSceneInfo, ui::SubWindow popups/tooltips,
  WindowCapabilities, DialogRequest and the reveal/trash shell actions), forms
  (ui::FormSystem as the focus group, addFormField adapters, FormFieldSlots,
  deferred focus), and what .clang-format already enforces.
---

# Xenolith/Stappler code style

Full articles live in [docs/usage/codestyle/](../../../docs/usage/codestyle/),
grouped by what you are touching — `sources/`, `platform/`, `core/`, `scene/`,
`ui/`, `window/`, plus the standalone `deferred-computation.adoc`. This card holds the
rules that are cheap to keep in context, and routes to the article when you need
the detail. Read the article before writing a new file or touching an unfamiliar
layer; the rules below are enough for an ordinary edit.

## The design principle

**Computation that can be deferred, must be deferred.** If a result is not needed
now, record that it is stale and compute it once where it is actually read — that
collapses N mutations into one recomputation, elides work nobody reads, and runs
with complete inputs. It is why the engine has dirty flags + phases instead of
work in setters, lazy caches (`_transformCacheDirty`), sort-before-draw,
per-field style accessors, `Callback<void(StringView)>` instead of returned
strings, and demand-driven dispatch (subtree listener counters, frame stack).
Don't defer when the bookkeeping costs more than the work, when it muddies
lifetime, or for error detection. Details and examples:
[deferred-computation.adoc](../../../docs/usage/codestyle/deferred-computation.adoc).

## Golden rules

1. **`.cpp`/`.c` are compile units (SCU); `.cc` files are `#include`-only
   subunits.** A new `.cc` is not built until some `.cpp` includes it. Never
   compile a `.cc` standalone to "check" it — build its parent `.cpp`.
2. **Every file starts with the MIT license block** (`/** … **/`), copied from a
   neighbouring file (the copyright holder differs per subtree).
3. **Include guard = repo-relative path, uppercased, trailing `_`** —
   `XENOLITH_RENDERER_UI_ATOMS_XLUIBUTTON_H_`. No `#pragma once`. Repeat the
   guard in the `#endif` comment.
4. **Namespaces:** `namespace STAPPLER_VERSIONIZED stappler::…` for
   stappler/xenolith, plain `namespace sprt` for runtime. Never write
   `namespace stappler::…` without the macro.
5. **Exported entities** get `SP_PUBLIC` (stappler/xenolith) or `SPRT_API` /
   `SPRT_GLOBAL` (runtime).
6. **Includes are never auto-sorted** (`SortIncludes: Never`) and order matters
   in an SCU. Append; don't reorganize. Use `// IWYU pragma: keep` for umbrella
   headers that look unused.
7. **Naming:** types and enumerators `PascalCase`; functions/locals `camelCase`;
   members `_camelCase`; file statics `s_camelCase`; macros `SCREAMING_SNAKE`
   with an `SP_`/`SPRT_`/`XL_` prefix (`__SPRT_*` is internal — read only).
   Files: `SP*` in stappler, `XL*` in xenolith, lowercase in `runtime/include/sprt`,
   aggregators `*.scu.cpp`.
8. **Virtual notification hooks are `handleXxx()`**, never `onXxx()`
   (`handleEnter`, `handleContentSizeDirty`, `handleLayoutChildren`).
9. **Ref-counted objects are created with `Rc<T>::create(...)`**, which calls
   `virtual bool init(...)`. No bare `new T` — `RefAlloc` deletes `operator new`.
   Own with `Rc<T>`; use raw pointers only for non-owning back-references.
10. **Pool memory is not heap memory.** A type allocated from a `pool_t` must
    derive from `AllocPool` (that base is what makes `sprt::__delete` skip
    `free()`). `new (pool) T` on a non-`AllocPool` type is a compile error —
    `sprt/cxx/new` deletes the global `operator new(size_t, pool_t *)` so it can
    no longer decay into placement new *at the pool's address*.
11. **To move data between threads, move a pool.** A pool with its own internal
    allocator (or one over an external `allocator_t` carried alongside) owns its
    data outright: `Rc<PoolRef>` for a refcounted pool, `Rc<SharedRef<T>>` for an
    object living in and owning its pool (`SharedRefMode::Allocator`, payload
    usually `: memory::PoolObject`). Allocators are thread-safe, pools are not —
    transfer ownership, never share concurrently.
12. **Aggregate `Type{value}` initializes the base class**, not the first field,
    when the type has a base (e.g. `: memory::AllocPool`). Pass `{}` first or use
    designated initializers.
13. **Platform guards are `#if SPRT_WINDOWS` / `SPRT_LINUX` / `SPRT_APPLE` /
    `SPRT_ANDROID` / `SPRT_WASM` — `#if`, not `#ifdef`**, and never raw `_WIN32`
    / `__APPLE__`. `#ifdef __SPRT_WASM` is wrong (that's an override input).
    Arch: `#if __SPRT_ARCH_ID == __SPRT_ARCH_ID_AARCH64`; pointer size via
    `__SIZEOF_POINTER__`.
14. **Code behind a foreign-platform guard is not compiled by a host build.**
    Verify it by building that target — see [AGENTS.md](../../../AGENTS.md) §6.
15. **Do not hand-align or hand-wrap.** [.clang-format](../../../.clang-format)
    decides: tabs (width 4), continuation indent 8, column limit 100,
    `Node *node` / `const Mat4 &m`, attached braces, auto-inserted braces.
16. **`StringView` is a non-owning view — pass it by value**, and remember
    `data()` may not be NUL-terminated (`terminated()`). Owned strings are chosen
    by interface: `mem_std::String` (malloc) / `mem_pool::String` (pool), generic
    code takes `template <typename Interface>` — `memory::StandardInterface` or
    `memory::PoolInterface` (`StandartInterface` is a kept-for-compatibility alias
    of the former; don't write it in new code). View → owned:
    `.str<Interface>()`; view → pool copy:
    `.pdup(pool)`. Emit text through `const Callback<void(StringView)> &`
    (`CallbackStream`) instead of returning `String`: it streams with `<<` via
    `sprt::io_traits<T>`, and a `StringStream` *is* such a callback. Specialize
    `sprt::io_traits<T>` for a new type — that teaches `<<`, `toString` and
    `slog()` at once. A `Callback` does not own its functor: parameter only,
    never stored.
17. **Nothing throws** (`-fno-exceptions` on Linux/macOS/Android/wasm; Windows keeps
    `-fexceptions` — SEH unwinding and the destructor funclets `longjmp` needs
    depend on it, so never add `-fno-cxx-exceptions` there). Failures
    are values: `Status` for operations, `Result<T>` for value-or-nothing, `bool`
    for `init()`. Test with `status::isSuccessful(st)`, **never** `st ==
    Status::Ok` (`Done`/`Suspended` are also successes). Convert platform errors
    with `errnoToStatus()` / `lastErrorToStatus()`.
18. **Log with `slog().error("Tag", …)`** (source location is captured; args are
    concatenated, no format string). `assert`/`sprt_passert` are debug-only —
    never put required side effects inside one.
19. **Scene graph: compose, don't subclass.** Default to a `System` (behaviour,
    opting into phases via `SystemFlags`) or a `Component` (data keyed by
    `static ComponentId Id`, one per id per node, read by systems); for a single
    callback use `node->set*Callback(...)`, which makes a `CallbackSystem` for
    you. Subclass `Node` only when the node *draws* differently, or for a
    structural root like `Scene`. To pick **which phase / `SystemFlags` / hook**
    a behaviour belongs in, read
    [design/node-system-event-pipeline.adoc](../../../docs/design/node-system-event-pipeline.adoc)
    — don't re-derive the phase order from the sources. Geometry: Y up, `anchorPoint` normalized,
    `contentSize` untransformed, rotation in radians; convert points with
    `convertToNodeSpace`/`convertToWorldSpace`. Node identity feeds CSS
    (`NodeIdentity` component): `setType()` → tag selector, **`setName()` → `#id`
    selector** (a node's name *is* its CSS id, and names are not unique),
    `addStyleClass()` → `.class`; numeric `setTag()` is invisible to CSS.
20. **The runtime is a POSIX libc on every platform** — write POSIX, and POSIX
    paths (`C:\Dir` is `/c/Dir`; the runtime converts, you don't). What a platform
    cannot do is gated by `__SPRT_CONFIG_HAVE_*` and fails with `ENOSYS` (no
    `fork` on Windows/wasm; no `exec`, epoll, futex, timerfd on wasm). The
    replacement is a higher-level runtime API, not an `#if`: `Looper::spawnProcess`
    for `exec`, `watchFile` for inotify, `connectSocket` for BSD sockets. Build
    paths with `filepath::merge` / `filesystem::findPath`, never by concatenation.
21. **Use `sprt` primitives, not `std::`, in `runtime/`, `stappler/`, `xenolith/`**
    (`utils/` and `tests/` are exempt): `sprt::mutex`/`unique_lock`, `sprt::thread`,
    `sprt::atomic`, `Rc<T>` instead of `shared_ptr`, `mem_std::`/`mem_pool::`
    containers, `StringView`, `Result<T>`, `<sprt/cxx/...>` headers. The runtime
    ships libc++, but its port to a given target can lag while sprt primitives are
    guaranteed everywhere the engine builds.
22. **Threads: post, don't spawn.** One `Looper` per thread
    (`Looper::acquire()`); `performOnThread(fn, ref)` targets a thread,
    `performAsync(fn, ref)` the worker pool — the `Ref *` keeps the callback's
    owner alive. Looper callbacks always run on that looper's thread; the scene
    graph is app-thread-only and therefore lock-free. Async operations return an
    `Rc<Handle>` you must **keep** (that is how you cancel; `nullptr` means the
    backend doesn't support it). Never block a looper thread, and never pass data
    from a thread's own pool to another thread (rule 11 is the way to move a
    dataset).
23. **`data::Value` is the boundary type** (config, files, IPC, command line,
    inspector) — a `struct` is what you use inside a subsystem. It is templated
    on the memory interface: `mem_std::Value` (malloc) vs `mem_pool::Value`
    (dies with the pool); in `xenolith::` an unqualified `Value` is the mem_std
    one, and only those two interfaces link. Setters take **`setValue(value,
    key)` — value first**. A failed lookup or rejected write returns a
    reference to the shared `Value::Null`: reading it is safe by design,
    **writing through it is a bug** (debug asserts, release drops it, a write
    past the guards faults — the sentinel is in read-only memory), so insert by
    naming the key and bind sub-values as `const Value &`. The non-const
    `getString()`/`getArray()`/`getDict()`/`as*()` assert in debug when the type
    misses — read containers through a `const Value &`, mutate only after an
    `is*()` check. Type conversion only
    happens from `EMPTY` (a key write makes a dict, an append makes an array);
    an indexed write takes the next free slot when the index is past the end, so
    it grows an array by one at most. Encode
    with `data::write` / `data::toString` / `data::save`, decode with
    `data::read<Interface>` (format auto-detected, failure = `EMPTY` value, so
    check the shape). Bytes survive CBOR/Serenity, **not** JSON.
24. **A window carries its own identity; an OS dialog is a request you keep.**
    Ask for a window with `Context::createWindow(Rc<WindowInfo> &&)` and put what
    it *is* — scene builder, prebuilt queue, close callback — in
    `WindowInfo::appData` as an `Rc<WindowSceneInfo>`; **never look a window up by
    `id`** (the runtime re-uniques a colliding one). That handle is app-thread
    only, its `getWindow()` is null until the window appears, and its close
    callback fires exactly once however the window went away — including "never
    opened". A popup/dialog/tooltip belonging to a window is **`ui::SubWindow`**,
    which becomes a native subwindow or an in-scene overlay depending on
    `WindowCapabilities::Subwindows` — check capabilities before offering
    fullscreen, decorations or mode switching too. OS dialogs are an
    `Rc<sprt::window::DialogRequest>` handed to `AppWindow::openDialog` (parented,
    cancelled with the window) or `Context::openDialog` (windowless); **keep the
    `Rc` — it is the cancellation token**, `callback` is required and runs exactly
    once on the target looper, and `Status::Declined` means the user cancelled,
    not that something failed. `RevealInFileManager`/`MoveToTrash` are shell
    actions on the same seam, with `paths` as input.
25. **A form is one `ui::FormSystem` on the node it is rooted at, and that system
    *is* the focus group.** Fields are attached to the widgets, not declared in
    the form — `ui::addFormField(input)` / `addFormField(checkbox)` /
    `addFormButton(button, role)`, or `addFormField(node, FormFieldSlots{…})` for
    a widget of your own; each joins the **nearest** form above it, so nesting is
    by construction. The form never touches a widget: the whole seam is
    `FormFieldSlots` (an empty slot is a no-op, no `collect` means never
    collected), and **the field name is the node's name**, which is also its CSS
    id. **A focus change is deferred to the next commit** — `focusField()` /
    `focusNext()` only record a request, `getFocusedField()` is what is
    committed and `getPendingField()` what was asked for, so never read focus
    back on the same hop. The tab ring is document order (= z-order: give
    siblings **distinct** `ZOrder`, `sortAllChildren` is unstable) and is rebuilt
    per frame, so hidden and disabled fields fall out of it for free. A rejected
    field is marked with a **style class** (`invalid`), because the CSS subset has
    no `:invalid`.

26. **A dock is one `ui::DockSystem` on the node it is rooted at, and every frame
    and divider is a FLAT child of that node** — the split tree is pure data
    inside the system, nothing in the scene graph nests. So the dock root must
    not carry a `LayoutSystem` and must never be `display: flex` (a
    `SystemManagedLayout` marker enforces the second and tells the style resolver
    the dock owns its children's `ContentSize`), and a frame is identified by its
    `DockNodeHandle`, **never** by child index — `sortAllChildren` is unstable.
    A panel is registered once (`id`, title, icon, minimum, **lazy builder**) and
    its node is built at most once and then kept across every move. Constraints
    only ever get stronger: a frame's minimum is its declared floor raised to the
    largest panel in it plus the tab strip, and a split's is its children's sum
    along its axis plus the divider. `ratio` divides the space left **after** the
    minimums, not the whole extent. A mutation writes the tree immediately but
    the **rects follow on the next layout pass**.

26. **Drag and drop is one `DragSystem` per scene, and a drop target registers
    itself by being DRAWN.** Add a `DropTarget` to a node and it publishes its
    world rect from inside its own visit, so the topmost target receives and an
    invisible one does not exist. `accept` is a **pure predicate** — it runs during
    hit testing, for targets that never become current; feedback belongs in
    `enter`/`over`/`leave`, which bracket exactly. A source captures the pointer
    (`setExclusive()`) or the drag dies at its own edge, and `updateDrag` takes a
    world **position**, never an accumulated delta. The payload is the clipboard's
    model — a live object for the in-process path plus a lazy MIME encoder — so a
    drop target and a paste target are one handler, and an encode callback must be
    thread-agnostic. Actions are a negotiation: the source offers a mask, the
    modifier states a *preference*, the target has the last word.

## Where to read more

| Task | Article |
|---|---|
| Designing a subsystem, an API or a data structure; deciding what to compute when | [deferred-computation.adoc](../../../docs/usage/codestyle/deferred-computation.adoc) |
| Adding a source file; `.cpp` vs `.cc`; compile-checking one file | [units-and-files.adoc](../../../docs/usage/codestyle/sources/units-and-files.adoc) |
| New header: license, guard, namespace, visibility macro, include order | [file-layout.adoc](../../../docs/usage/codestyle/sources/file-layout.adoc) |
| Naming a file, type, member, handler; the `init()`/`create()` pattern | [naming.adoc](../../../docs/usage/codestyle/sources/naming.adoc) |
| Platform/arch `#if`, per-platform values, how to verify guarded code | [platform-guards.adoc](../../../docs/usage/codestyle/platform/platform-guards.adoc) |
| `Rc`/`Ref`, pools vs malloc, `AllocPool`, `__delete`, pool traps; independent pools (`PoolRef`, `SharedRef<T>`) that travel between threads | [memory-and-ownership.adoc](../../../docs/usage/codestyle/core/memory-and-ownership.adoc) |
| What the formatter enforces and what it deliberately leaves alone | [formatting.adoc](../../../docs/usage/codestyle/sources/formatting.adoc) |
| Passing/building/parsing text, `StringView` lifetime, pool vs malloc strings, unicode | [strings.adoc](../../../docs/usage/codestyle/core/strings.adoc) |
| A function that can fail; `Status`/`Result<T>`, logging, assertions | [errors-and-status.adoc](../../../docs/usage/codestyle/core/errors-and-status.adoc) |
| Posting work to a thread, timers, async I/O, `Looper`/`Task`/`Handle`, cross-thread lifetime | [threads-and-dispatch.adoc](../../../docs/usage/codestyle/core/threads-and-dispatch.adoc) |
| Node geometry, anchor/contentSize/transforms, coordinate conversion, which `Node` subclass to use | [node-geometry.adoc](../../../docs/usage/codestyle/scene/node-geometry.adoc) |
| Adding behaviour or data to a node — `System`, `Component`, and why not to subclass `Node` | [node-system-component.adoc](../../../docs/usage/codestyle/scene/node-system-component.adoc) |
| Making a node draggable or droppable; the payload a drag shares with the clipboard | [drag-and-drop.adoc](../../../docs/usage/codestyle/scene/drag-and-drop.adoc) |
| **Which node phase / `SystemFlags` / `handle*` hook to use** — phase order, dirty flags, frame-stack child events, dispatch priority | [design/node-system-event-pipeline.adoc](../../../docs/design/node-system-event-pipeline.adoc) |
| Calling libc, POSIX paths and Windows conversion, what's missing per platform, `sprt` vs `std::` | [runtime-libc.adoc](../../../docs/usage/codestyle/platform/runtime-libc.adoc) |
| Reading/writing a `data::Value`, JSON/CBOR/Serenity, config and IPC payloads | [data-value.adoc](../../../docs/usage/codestyle/core/data-value.adoc) |
| Shipping a directory of resources inside the binary; `FileCategory::Embedded`, `LOCAL_EMBED_DIRS`, the BundleFS format | [embedded-files.adoc](../../../docs/usage/codestyle/core/embedded-files.adoc) |
| Building a form; adding a field, a validator or a widget of your own to one; Tab order, focus, submit/reset | [ui/forms.adoc](../../../docs/usage/codestyle/ui/forms.adoc) |
| An IDE-style layout of parked panels: frames, tabs, dividers, drag & drop between frames, saving the arrangement | [ui/docking.adoc](../../../docs/usage/codestyle/ui/docking.adoc) |
| Opening a second window, a popup/menu/tooltip; fullscreen, monitors, window state | [windows.adoc](../../../docs/usage/codestyle/window/windows.adoc) |
| Asking the OS for a file/folder/colour/font; reveal-in-file-manager, move-to-trash | [dialogs.adoc](../../../docs/usage/codestyle/window/dialogs.adoc) |
| **`data::Value` in depth** — accessors, container access, custom encoders, interface conversion, the `Value::Null` trap, pitfalls table | [data/value.adoc](../../../docs/usage/data/value.adoc) |
| Everything, plus topics not yet written up | [index.adoc](../../../docs/usage/codestyle/index.adoc) |

Adjacent skills: `xenolith-build` (how to build/verify), `css-engine` (CSS
subset rules for `.css`/pug), `gui-debug`, `wine-debug`, `wasm-test`.
