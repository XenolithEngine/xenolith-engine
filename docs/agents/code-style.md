# Code style

*How code in this repository is written, and the rules that bite most often.*

*Part of the [build & test guide](../../AGENTS.md).*

Full reference: **[docs/usage/codestyle/index.adoc](../usage/codestyle/index.adoc)**
— one article per topic. Read the relevant article before creating a file, a
header, a platform branch, or an allocation. The essentials:

- `.cpp`/`.c` = compile units (SCU); `.cc` = `#include`-only subunits
  ([units](../usage/codestyle/sources/units-and-files.adoc)).
- MIT license block, path-derived include guard (`XENOLITH_..._H_`, no `#pragma
  once`), `namespace STAPPLER_VERSIONIZED stappler::…` (runtime: `namespace
  sprt`), `SP_PUBLIC` / `SPRT_API` on exported entities, includes never sorted
  ([file layout](../usage/codestyle/sources/file-layout.adoc)).
- Types `PascalCase`, functions `camelCase`, members `_camelCase`, file statics
  `s_name`; virtual hooks are `handleXxx()`, not `onXxx()`; files are `SP*` /
  `XL*` / lowercase-in-`sprt`, aggregators `*.scu.cpp`
  ([naming](../usage/codestyle/sources/naming.adoc)).
- Platform tests are `#if SPRT_WINDOWS` / `SPRT_APPLE` / … — `#if`, not `#ifdef`,
  never raw `_WIN32`; arch via `__SPRT_ARCH_ID == __SPRT_ARCH_ID_*`
  ([platform guards](../usage/codestyle/platform/platform-guards.adoc)).
- Ref-counted objects: `Rc<T>::create()` + `virtual bool init(...)`, never bare
  `new`. Pool-allocated types must derive from `AllocPool`; `new (pool) T` on
  anything else is a silent corruption, and aggregate `Type{value}` initializes
  the base class ([memory](../usage/codestyle/core/memory-and-ownership.adoc)).
- Layout is [.clang-format](../../.clang-format)'s job: tabs (4), continuation 8,
  column limit 100, `Node *node`, attached braces
  ([formatting](../usage/codestyle/sources/formatting.adoc)).
- `data::Value` is the boundary type (config, files, IPC, command line):
  `setValue(value, key)` takes the **value first**, a failed lookup returns the
  shared `Value::Null` — read-only memory, so assigning through it asserts in
  debug and faults if it gets past the guards — an indexed write past the end of
  an array takes the next free slot, `Value{5}` is the array `[5]`, and bytes
  survive CBOR but not JSON
  ([data::Value](../usage/codestyle/core/data-value.adoc), full guide:
  [docs/usage/data/value.adoc](../usage/data/value.adoc)).
- A key combination is a **named global hotkey**: register it once
  (`HotkeyRegistry::add("org.example.app.save", HotkeyCombo::parse("Ctrl+S"))`)
  and subscribe with `listener->addHotkey(id, cb, flags)`. Delivery is in
  `InputDispatcher`, **ahead of the ordinary key route**, so a subscriber needs no
  key mask and is **not hit-tested against the pointer**; return `true` to consume,
  `false` to pass it on. The keyboard owner is offered it first, then the normal
  walk order. `FocusedOnly` means "entitled to keys in this focus group", not
  `isFocused()`. A hotkey is the press only — something shown *while a key is held*
  still wants a recognizer. A combination may demand one side of a modifier
  (`"CtrlL+K"`), which every backend but wasm reports; and
  `HotkeyOptions::ReserveFromTextInput` makes the text-input processor decline it,
  so an `Alt`/`Super` chord survives a focused field — opt-in, because `Escape` and
  `Backspace` belong to the IME ([hotkeys](../usage/codestyle/scene/hotkeys.adoc)).
- A directory named in `LOCAL_EMBED_DIRS` / `MODULE_<X>_EMBED_DIRS` is compiled
  into the binary (BundleFS) and read back through **`FileCategory::Embedded`**,
  where the bundle's mount name is the directory's own name. `Embedded` is
  independent of `Bundled` (the on-disk app bundle) and is strictly read-only;
  compression is per-bundle, needs `stappler_data`, and only `xlmake` performs it
  ([embedded files](../usage/codestyle/core/embedded-files.adoc)).
- Windows are asked for with `Context::createWindow`, and what a window **is**
  travels with the request as `WindowInfo::appData` — never look one up by `id`,
  which the runtime may re-unique. Popups/dialogs/tooltips are `ui::SubWindow`
  (native subwindow or in-scene overlay); check `WindowCapabilities` before
  offering fullscreen or decoration controls. A scene reads where its window is
  with `getRenderServer()->getWindowGeometry()` (a content rect in the **logical**
  units `WindowInfo::rect` takes, so it can be handed straight back) and is told
  about changes by `Scene::handleWindowGeometryChanged`; always check
  `hasPosition` before saving an origin, and set
  `WindowCreationFlags::UsePosition` to ask for one back
  ([windows](../usage/codestyle/window/windows.adoc)).
- OS dialogs are an `Rc<sprt::window::DialogRequest>` you **keep** — it is the
  cancellation token; the callback is required, runs exactly once, and
  `Status::Declined` is the user cancelling, not a failure
  ([dialogs](../usage/codestyle/window/dialogs.adoc)).
- The clipboard is `Rc<ClipboardSession>` over the app thread, never the three
  `AppThread` calls directly. A payload is a `ClipboardOffer` — MIME types **in
  order of preference** plus eager or lazy bytes, the same object a drag carries;
  a read states a **preference list** matched by PREFIX and is answered **exactly
  once**, which the platforms do not do on their own (wayland drops an unoffered
  type in silence, the base controller answers twice). **`cancel()` when the
  reason for the read goes away** — a widget losing focus does it in two places,
  because the platform usually revokes input rather than going through `blur()`.
  An empty offer is refused (on Android that means *clear the clipboard*), a
  write is never a receipt, and policy — a masked field refusing to copy — stays
  with the widget ([clipboard](../usage/codestyle/window/clipboard.adoc)).
- Undo for text is `ui::TextHistory`, over `hist::CommandBus` from
  `SPCommandHistory.h`. It records at the ONE point where text changes — the IME
  **echo**, not a widget command, because the runtime's processor owns printable
  keys and a typed character never reaches `insertText`. It is **on** for
  `ui::TextView`/`ui::CodeEditor` and **off** for a plain `ui::TextInput`: a field
  commits into somebody's document, and `Ctrl+Z` there must take back the document
  edit, not the typing. A run of keystrokes is one entry until its idle window
  passes (`breakRun()` ends one on demand); a paste, a cut and a newline are each
  their own. A handler with nothing to undo answers **false**, so the chord reaches
  whoever is below — that is how an application arbitrates two histories.
- A form is one `ui::FormSystem` on the node it is rooted at, and that system
  **is** the focus group; fields are attached to the widgets (`ui::addFormField`,
  `addFormButton`, or `FormFieldSlots` for a widget of your own) and join the
  nearest form above them. The field name is the node's name (= its CSS id), a
  focus change is committed only on the next frame (`getPendingField()` is what
  was asked for), and the tab ring is document order — so give siblings distinct
  `ZOrder` ([forms](../usage/codestyle/ui/forms.adoc)).
- Input atoms: `ui::TextInput` (text), `ui::NumberField` (a number — the range is
  **declared**, and typing past it is refused while dragging past it is clamped),
  `ui::Select` (a drop-down: a closed `Panel` plus a real menu surface, so the
  list's keyboard is `MenuSystem`'s and the closed control's arrows are its own),
  `ui::VectorField` (a row of `NumberField`s that is ONE form field — Tab walks
  its components and leaves only at the ends), `ui::ColorField` (a swatch and a
  hex line; the system colour dialog behind `isDialogSupported`, and a picker of
  its own where there is none), `ui::Chip` / `ui::ChipRow` (a row of chips that is
  ONE form field collecting an ARRAY of ids — the limit and the uniqueness are
  **declared**, so at the maximum the "+" is dead rather than refusing after the
  press, duplicates are allowed by default because an element chain repeats, and
  the wrapped height is reported through the measurement protocol),
  `ui::Slider` (a track, a fill and a handle — it carries a step INDEX and never a
  fraction, so a drag and an arrow key that land on the same notch produce the
  SAME number rather than two that agree to six places, and a declared maximum
  that is not a whole number of steps away is reported rather than trimmed; keys
  answer along the widget's own axis only), `ui::Checkbox`, `ui::Button`. All of
  them take their whole look from CSS through a per-type applier, and a refusal is
  the style class `invalid` — there is no `:invalid` pseudo-class in the subset.
- **An image is measured in BLOCKS, never in pixels.** `getFormatBlockSize` returns
  BYTES PER BLOCK — 8 for BC1, 16 for BC7 and every ASTC — so
  `getFormatBlockSize(fmt) * width * height` is right only where the block happens
  to be one pixel and over-counts every compressed format by the size of its tile.
  Use `core::getFormatImageSize(fmt, extent, layers)`; for a graphics API's
  `bytesPerRow` / `rowsPerImage` use `getFormatRowSize(fmt, width)` and
  `getFormatRowCount(fmt, height)`, both of which count blocks and round up.
  `getFormatBlockExtent` is what says how many pixels a block covers, and
  multi-planar formats are explicitly out of its scope. The uncompressed path is
  byte for byte what it always was.
- **A control's states are independent classes, and two of them carry a reason.** `invalid` is "what is written here is wrong"; `disabled` is the
  mechanical off, and tracks `:disabled`; `read-only` is readable and copyable but
  not editable; `locked` is "you may not write here at all, and here is why" —
  `ui::setEditLock(node, reason)` paints it, clears the `Enabled` bit, hangs the
  reason off a `ui::TooltipComponent` (only if the node has no hint of its own) and
  takes the field out of the form's tab ring. A locked control is also disabled,
  and the two compose: unlocking restores what the application last asked for, not
  "on". `ui::applyControlEnabled` is the **single** writer of the `Enabled` bit and
  of the `disabled` class — do not flip either by hand. `unavailable` is the fifth
  and narrowest: an ACTION the control offers cannot be performed (no system colour
  dialog on this platform, a dialog that failed). It is not `invalid` — the value is
  fine, the way in is missing — and a capability refusal must never borrow the
  validation channel.
- **A number can name its unit**, and a unit is a LABEL: `ui::NumberField::setUnit`
  and `ui::VectorField::setUnit` (one per row, not one per component) draw a word
  beside the number and inset the text viewport to make room. Nothing converts or
  validates against it, and it never enters `getText()` — `parse(format(v)) == v`
  has to keep holding.
- **A list of names is not a `MenuSource`.** `ui::Select` and `ui::ChipRow` take
  data (`SelectOption`); `ui::makeSelectOptions` builds that list from plain
  strings for the id==title case, in both the `StringView` and the `String`
  spelling because a `SpanView<StringView>` cannot be made from a `Vector<String>`.
- Two widgets are surfaces rather than atoms, and both exist because a list is
  the wrong shape for what they do. `ui::SearchPicker` is a query line over a
  virtualized result list (`ui::SearchSystem` + a `SearchSource`): a list of
  hundreds is not a menu, so the query line keeps focus and the arrows move a
  selection somewhere else. What it opened is reached with `getContent()` and
  never by casting `getPopup()->getLayout()`: a popup surface's layout is a
  WRAPPER and the panel `PopupSurfaceConfig::makePanel` built is a child of it,
  so that cast answered null for every caller that ever tried - and null is also
  what a closed picker looks like, which is why it read as a timing problem.
  `SubWindow::getPanel()` is the general form of the same answer. Grouped
  (`grouped`, category from `SearchItemsData["category"]` or `group`), the tree
  opens with every category CLOSED, and `highlight` - the value the list opens ON
  - reveals the one holding it and no others (`revealHit`); a list whose
  categories were all shut showed no selection, gave the arrows no row to step
  off and gave Enter nothing to activate. `isGrouping()` answers the mode, which
  cannot be derived from outside: with everything collapsed there are FEWER rows
  than hits, not more. `ui::InlineEditor` edits over a **rectangle** rather
  than inside a node (`beginInlineEdit` / `beginInlineTextEdit`, or a
  `ui::InlineEditTarget` on the node itself): a virtualized row is destroyed by
  scrolling and by `invalidateSource()`, and a `ui::TextInput` holds the IME, so
  an editor parented into the cell would lose the typed text to a rebuild nobody
  asked for. It ends on Enter, on a press outside, on a scroll and on the anchor
  leaving the scene - all of them COMMITTING - with Escape and the opening of the
  NEXT edit cancelling, and the commit is delivered at most once however many of
  those arrive together. ONE session is open application-wide
  (`InlineEditSession::getActive()`), and opening another cancels it BEFORE the
  new one reads any geometry, because that cancel is free to move the rows it
  would be placed over; a caller that wants the outgoing value keeps it by
  committing first, and a caller whose commit was REFUSED has to decide there and
  then rather than leave the ending to the next open.
  A caller that supplies its OWN editor through `setFactory` must also supply
  `setCollectCallback`: this side is handed a node it cannot interpret, so
  without it the commit carries a Nil - and it did, silently, until the studio's
  control binder became the first caller to take that path. `collect` stays
  optional because a display has nothing to report, which is exactly what made
  the hole invisible.
- **A layout result is read when the pass that produced it says so.** A node
  attached from inside a frame catches up on the visit's phases as it is attached
  (`Node::runPendingPhases`), so its geometry is readable on the next line;
  attached from outside one - an input handler, a menu callback - there is no pass
  to catch up on, and the same read gives the fallback. There is deliberately no
  synchronous off-frame settle: every phase body needs the pass's `FrameInfo` and
  its `systemStack`, and a synthetic one would be a second implementation of the
  pipeline. So the caller ASKS TO BE TOLD - `Node::settleForMeasure()` for a
  child's size, `ui::TreeView`/`ui::TableView::requestRebuildNodes(cb)` for a row
  the view builds at its own pace, whose callback runs at the END of the rebuild,
  inside the visit, when every row it built has already caught up. Polling from a
  visit-end callback with a frame counter is the shape to refuse: it asks after the
  answer was complete, and the counter merges "the pass has not run" with "this
  will never happen" - and the second is not a matter of time. For a virtualized
  row it means "outside the scroll window", which is decidable at once and fixed by
  scrolling to it. `examples/window/dndtree` names a new element the moment it is
  created and is written that way end to end.
- `ui::TableView` publishes its ROW GEOMETRY - `getRowRect`, `getCellRect`,
  `getRowIndexAt`, `getRowBoundaryAt`, shared with `ui::TreeView` through
  `ui::RowGeometrySource` (`TreeView` adds `getRowContentRect`, the row's box cut
  back to where its NAME starts - an inline rename opened over the whole row puts
  its text several columns left of the text it replaces) - and it answers for a
  row that has no node, because
  only the nodes are virtualized: `rebuildRows()` commits one controller item per
  row with the height it resolved beforehand. Reordering builds on that:
  `setReorderCallback` asks `bool(from, to)` where `to` is the row's FINAL index,
  a false REFUSES and moves neither the order nor the selection, and on
  acceptance the selection follows the ROW rather than the index. The grip is a
  column the CALLER declares under `TableView::ReorderColumnKey` - the view fills
  that cell with an icon and a `DragSource` but never inserts the column itself,
  which would renumber every other column behind the caller's back. Keyboard
  equivalent is `EngineHotkeys::moveItemUp` / `moveItemDown` (Alt+Up / Alt+Down,
  registered `ReserveFromTextInput` because an Alt chord carries a keychar), gated
  on there being a selected row so that a table nobody picked in declines rather
  than swallows.
- A menu is `ui::MenuSource` (the model) plus one `ui::MenuSystem` on the node it
  is built into; a popup is that same pair inside a `ui::SubWindow`
  (`ui::openMenuForNode`). **One measurement decides everything** —
  `MenuSystem::measure` resolves the shared columns and every wrapped row height,
  and the same call sizes the popup surface before any node exists. The system
  owns its children's geometry, so the menu node must not carry a `LayoutSystem`;
  an item's accelerator is a named `HotkeyId` and `bindMenuHotkeys` subscribes a
  listener to all of them. The keyboard is a **mode**
  (`setKeyboardEnabled`), off by default and turned on by `openMenu` for the menu
  it builds: it installs an **`Exclusive` `FocusGroup`**, because key events are
  not hit-tested and without one the arrows of an open menu also reach everything
  else in the window ([menus](../usage/codestyle/ui/menus.adoc)).
