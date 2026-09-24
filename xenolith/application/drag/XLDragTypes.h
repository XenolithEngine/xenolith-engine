/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 **/

#ifndef XENOLITH_APPLICATION_DRAG_XLDRAGTYPES_H_
#define XENOLITH_APPLICATION_DRAG_XLDRAGTYPES_H_

#include "XLInput.h" // IWYU pragma: keep
#include "XLNodeInfo.h" // IWYU pragma: keep
#include "XLClipboard.h" // IWYU pragma: keep - preferMimeType and ClipboardOffer

#include <sprt/runtime/window/clipboard.h>
#include <sprt/runtime/window/drop.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class Node;
class DragSession;
class AppThread;

// What a drop does with the payload. A mask means "any of these"; a resolved action is one bit.
using DragActions = sprt::window::DragActions;

// Whether this drag may leave the process. Only Never is implemented; beginDrag rejects Always.
// Set on the offer: Wayland and X11 need a live press serial/grab, so it cannot change mid-drag.
enum class DragExternalPolicy : uint8_t {
	Never,
	Always,
};

/** The payload of one drag, with two ways in.

`getLocal()` is the in-process path: a live object keyed by `getLocalType()`, null for a drag from
another process. The clipboard half (MIME types plus a lazy encoder) is all an external drag can
carry, so targets should prefer `getTypes()` and `read()` when the data is expressible as bytes.

`encode()` runs the encode callback on the caller's thread, which may be any thread: the callback
must capture copies and never touch the scene graph. A drag from another application has no
encoder: its bytes come only through `read()`, and only until the drop is finished. */
class SP_PUBLIC DragData : public Ref {
public:
	// The bytes are borrowed for the call
	using ReadCallback = Function<void(Status, BytesView)>;

	virtual ~DragData() = default;

	virtual bool init(Rc<sprt::window::ClipboardData> &&, Rc<Ref> && = nullptr,
			StringView localType = StringView());

	// A drag from another application, answered by `offer` through the app thread
	virtual bool init(NotNull<sprt::window::DropOffer>, NotNull<AppThread>);

	sprt::window::ClipboardData *getClipboardData() const { return _clipboard; }

	SpanView<sprt::window::String> getTypes() const;
	bool hasType(StringView) const;

	// The first type in `preference` this data can produce, or an empty view. Matching is by
	// prefix, so a preference of "text/plain" also selects "text/plain;charset=utf-8"
	StringView preferType(SpanView<StringView> preference) const;

	// Materialize the bytes for one type. Empty if the type is not on offer, the encoder
	// declined, or the data is external. See the threading note above
	sprt::window::Bytes encode(StringView type) const;

	/* The bytes of one type, for any drag. The callback runs exactly once on the app thread:
	in-process data answers before this returns, external data later. `target` is retained until
	then. False, with no callback, when the type is not on offer or the drop is already finished.

	The OS hears the outcome of an external drop once the `drop` slot returned and every read begun
	by then has answered. */
	bool read(StringView type, ReadCallback &&, Ref *target = nullptr);

	// A drag from another application
	bool isExternal() const { return _external != nullptr; }

	// The live object, for a drag that started in this process; null otherwise
	Ref *getLocal() const { return _local; }
	StringView getLocalType() const { return _localType; }

	bool isLocal(StringView type) const { return _local && _localType == type; }

protected:
	friend class DragSession;

	struct External;

	External *getExternal() const;

	// The drop is decided; an external offer is finished as soon as no read is outstanding
	void settle(DragActions performed);

	Rc<sprt::window::ClipboardData> _clipboard;
	Rc<Ref> _local;
	String _localType;

	// External, held as Rc<Ref>: the type is private to the .cc
	Rc<Ref> _external;
};

/** What a source declares when it starts a drag.

Everything is optional except the actions: a drag may have no clipboard payload and no visual. */
struct SP_PUBLIC DragOffer {
	// --- payload, in-process half -------------------------------------------
	Rc<Ref> local;
	String localType;

	// --- payload, OS-shaped half --------------------------------------------
	// `label` is a user-facing description (it becomes ClipboardData::label). `types` are MIME
	// types in order of preference. `encode` must be thread-agnostic: capture copies, never
	// capture a scene node, never touch the scene graph
	String label;
	Vector<String> types;
	Function<sprt::window::Bytes(StringView)> encode;

	// --- negotiation --------------------------------------------------------
	DragActions allowedActions = DragActions::Move;
	DragActions defaultAction = DragActions::Move;
	DragExternalPolicy externalPolicy = DragExternalPolicy::Never;

	// --- visual -------------------------------------------------------------
	// Builds the node that follows the pointer. Called once, inside beginDrag. The node must not
	// carry an InputListener - it would sit between the pointer and the source.
	// `decoratorOffset` is added to the pointer position, in the drag system owner's space
	Function<Rc<Node>()> decorator;
	Vec2 decoratorOffset;

	/* Do not build the decorator at beginDrag; the source supplies it later through
	DragSession::setDecorator (e.g. a frame cutout that needs a rendered frame first). Until then
	nothing follows the pointer. */
	bool decoratorDeferred = false;

	// Where to park it; null means the drag system's owner. A stylesheet-styled decorator needs a
	// parent inside its StyleResolver's subtree, or it comes out unstyled
	Node *decoratorParent = nullptr;

	// Builds the clipboard half of this offer, moving `encode` out of it. The same object serves
	// the clipboard and a drag
	Rc<sprt::window::ClipboardData> takeClipboardData(Ref *owner = nullptr);

	// --- completion ---------------------------------------------------------
	// Runs exactly once, after the drop. DragActions::None means no drop (cancelled, refused, or
	// dropped nowhere). A Move source deletes its original here
	Function<void(DragActions)> completion;
};

// One drag update, as a target sees it.
struct SP_PUBLIC DragEvent {
	DragSession *session = nullptr;
	DragData *data = nullptr;

	// The node receiving this - the one carrying the DropTargetComponent. Null while the drag is
	// over nothing
	Node *target = nullptr;

	// world (screen) space, physical pixels - the space input events arrive in
	Vec2 worldLocation;

	// the same point in the receiving target owner's node space
	Vec2 location;

	// Everything the source allows. A target answers with a subset of this
	DragActions allowed = DragActions::None;

	// The single action the modifiers ask for, clamped to `allowed`. A preference: a target that
	// cannot do it may accept something else, and that is what happens
	DragActions preferred = DragActions::None;

	InputModifier modifiers = InputModifier::None;
};

// A target's answer: whether it takes the drag, and as what.
struct SP_PUBLIC DragResponse {
	// Subset of DragEvent::allowed this target accepts; None continues the search below it. The
	// drop does DragEvent::preferred if present, otherwise the first of Copy/Move/Link in the set
	DragActions accepted = DragActions::None;

	// Optional: override the cursor the drag would otherwise derive from the resolved action
	WindowCursor cursor = WindowCursor::Undefined;
};

/** The interface between a drop target and the drag system.

`accept` must be pure: it runs during hit testing, for candidates that may never become the current
target, possibly several times per frame. `enter`/`over`/`leave` fire only for the current target
and carry visual feedback; every enter gets its leave, including on cancel or the target leaving
the scene. An empty slot is a no-op; without `accept` the target accepts nothing. */
struct SP_PUBLIC DropTargetSlots {
	Function<DragResponse(const DragEvent &)> accept;

	Function<void(const DragEvent &)> enter;
	Function<void(const DragEvent &)> over;
	Function<void(const DragEvent &)> leave;

	// Apply the drop. `action` is a single resolved bit. Returning false means nothing was done,
	// and the source's completion gets DragActions::None
	Function<bool(const DragEvent &, DragActions action)> drop;
};

// Which single action the modifiers ask for: Ctrl = Copy, Shift = Move, Ctrl+Shift = Link, else
// `dflt`. Falls back to the first of Copy/Move/Link in `allowed`, so it is never None for a
// non-empty `allowed`
SP_PUBLIC DragActions modifiersToActions(InputModifier mods, DragActions allowed,
		DragActions dflt);

// Reduce a mask to the single action a drop would perform, preferring Copy over Move over Link.
// None in, None out
SP_PUBLIC DragActions pickAction(DragActions mask);

// Cursor for a resolved (single-bit) action. DragActions::None maps to NoDrop
SP_PUBLIC WindowCursor actionToCursor(DragActions action);

} // namespace stappler::xenolith

#endif // XENOLITH_APPLICATION_DRAG_XLDRAGTYPES_H_
