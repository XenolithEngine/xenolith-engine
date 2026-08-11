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

#include <sprt/runtime/window/clipboard.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class Node;
class DragSession;

// What a drop would DO with the payload. The three values are not an invention: they map one to one
// onto XdndActionCopy/Move/Link, DROPEFFECT_COPY/MOVE/LINK, NSDragOperation{Copy,Move,Link} and
// wl_data_device_manager_dnd_action. Modelling them from the start is what keeps the target API
// from changing when the OS path lands.
//
// A mask says "any of these is acceptable"; the resolved action is always a SINGLE bit.
enum class DragActions : uint32_t {
	None = 0,

	Copy = 1 << 0,
	Move = 1 << 1,
	Link = 1 << 2,

	All = Copy | Move | Link,
};

SP_DEFINE_ENUM_AS_MASK(DragActions)

// Whether this drag may leave the process. v1 implements Never only; Always is rejected by
// beginDrag with a warning.
//
// It lives on the OFFER, not on the session, and that is deliberate. Wayland's start_drag needs the
// serial of a live button press and X11 needs a grab taken at press time, so "start internally and
// escalate when the pointer leaves the window" is not implementable on any of them. The decision
// has to be made at begin or not at all.
enum class DragExternalPolicy : uint8_t {
	Never,
	Always,
};

/** The payload of one drag, with two ways in.

`getLocal()` is the in-process fast path: a live object the source and the target agree on out of
band, keyed by `getLocalType()`. Nothing is serialized, nothing is copied.

The clipboard half is the OS-shaped one: a list of MIME types and a lazy encoder. It is what an
external drag can carry, and the ONLY thing it can carry - which is why it is also the API a target
should be written against whenever the data is expressible as bytes.

For a drag that came from another process `getLocal()` is null. A target that only understands
`getLocal()` still works, it just refuses those drags; a target written against `getTypes()` and
`encode()` accepts both without knowing the difference.

THREADING. `encode()` runs the offer's encode callback on the CALLER's thread. In v1 that is always
the app thread, but the callback is stored inside a `sprt::window::ClipboardData` whose contract
says it may run anywhere, and once the OS path exists it will. So an encode callback must capture
copies of what it needs and must never touch the scene graph. */
class SP_PUBLIC DragData : public Ref {
public:
	virtual ~DragData() = default;

	virtual bool init(Rc<sprt::window::ClipboardData> &&, Rc<Ref> && = nullptr,
			StringView localType = StringView());

	sprt::window::ClipboardData *getClipboardData() const { return _clipboard; }

	SpanView<sprt::window::String> getTypes() const;
	bool hasType(StringView) const;

	// The first type in `preference` this data can produce, or an empty view. Matching is by
	// prefix, so a preference of "text/plain" also selects "text/plain;charset=utf-8"
	StringView preferType(SpanView<StringView> preference) const;

	// Materialize the bytes for one type. Empty if the type is not on offer or the encoder
	// declined. See the threading note above
	sprt::window::Bytes encode(StringView type) const;

	// The live object, for a drag that started in this process; null otherwise
	Ref *getLocal() const { return _local; }
	StringView getLocalType() const { return _localType; }

	bool isLocal(StringView type) const { return _local && _localType == type; }

protected:
	Rc<sprt::window::ClipboardData> _clipboard;
	Rc<Ref> _local;
	String _localType;
};

/** What a source declares when it starts a drag.

Everything is optional except the actions. A drag with no payload at all is legal (the source and
the target may communicate purely through `local`), and so is a drag with no visual. */
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
	// Builds the node that follows the pointer. Called ONCE, inside beginDrag. The node must not
	// carry an InputListener - it would sit between the pointer and the source that owns the drag.
	// `decoratorOffset` is added to the pointer position, in the drag system owner's space
	Function<Rc<Node>()> decorator;
	Vec2 decoratorOffset;

	// Where to park it. Null means the drag system's owner, which is the right answer whenever the
	// decorator draws itself. It is NOT the right answer when the decorator takes its look from a
	// stylesheet: a StyleResolver only sees its own subtree, so a ghost parked above that subtree
	// comes out unstyled. Such a source names the node its own styling is resolved under
	Node *decoratorParent = nullptr;

	// Builds the OS-shaped half of this offer, MOVING `encode` out of it. This is the object the
	// clipboard takes and the one an OS drag will carry, so a source that can be dragged and one
	// that can be copied describe their payload exactly once, in one place
	Rc<sprt::window::ClipboardData> takeClipboardData(Ref *owner = nullptr);

	// --- completion ---------------------------------------------------------
	// Runs exactly once, after the drop has been applied (or not). DragActions::None means the
	// drag ended without a drop - cancelled, refused, or dropped nowhere. This is where a Move
	// source deletes its original and a Copy source does not
	Function<void(DragActions)> completion;
};

// One drag update, as a target sees it.
struct SP_PUBLIC DragEvent {
	DragSession *session = nullptr;
	DragData *data = nullptr;

	// world (screen) space, physical pixels - the space input events arrive in
	Vec2 worldLocation;

	// the same point in the receiving target owner's node space
	Vec2 location;

	// Everything the SOURCE is willing to do. A target answers with a subset of this
	DragActions allowed = DragActions::None;

	// The single action the modifiers ask for, already clamped to `allowed`. It is a PREFERENCE,
	// not a demand: a target that cannot do it may still accept something else, and then that
	// something else is what happens. Which is why the two are separate fields - collapsing them
	// would make a Copy-only target unable to take a drag the user happened to hold Shift over
	DragActions preferred = DragActions::None;

	InputModifier modifiers = InputModifier::None;
};

// A target's answer to "would you take this, and as what?".
struct SP_PUBLIC DragResponse {
	// Subset of DragEvent::allowed this target would accept - usually `event.allowed & whatICanDo`.
	// None means "not here", and the search continues with whatever is under this target.
	//
	// If the set contains DragEvent::preferred, that is what the drop will do; otherwise the
	// first of Copy/Move/Link in the set wins
	DragActions accepted = DragActions::None;

	// Optional: override the cursor the drag would otherwise derive from the resolved action
	WindowCursor cursor = WindowCursor::Undefined;
};

/** The whole seam between a drop target and the drag system.

`accept` is a PREDICATE and must be pure. It is called during hit testing, for candidates that may
never become the current target, and possibly several times in one frame. Do not move indicators,
do not touch the scene graph, do not remember anything in it.

`enter` / `over` / `leave` are notifications for the CURRENT target only, and are where visual
feedback belongs. `enter` and `leave` bracket exactly: every enter gets its leave, including when
the drag is cancelled or the target leaves the scene mid-drag.

An empty slot is a no-op. A target with no `accept` never accepts anything, which makes an
unconfigured DropTarget inert rather than surprising. */
struct SP_PUBLIC DropTargetSlots {
	Function<DragResponse(const DragEvent &)> accept;

	Function<void(const DragEvent &)> enter;
	Function<void(const DragEvent &)> over;
	Function<void(const DragEvent &)> leave;

	// Apply the drop. `action` is a single resolved bit. Returning false means nothing was
	// actually done, and the source's completion is told DragActions::None
	Function<bool(const DragEvent &, DragActions action)> drop;
};

// The single place the MIME preference rule lives: matching is by PREFIX, and the first entry of
// `preference` that matches anything wins. That is what makes a caller asking for "text/plain" also
// accept "text/plain;charset=utf-8", which is what half the world actually puts on a clipboard.
//
// It is a free function because both sides of the boundary need it and only one of them has a
// DragData: a paste's type selector is handed a bare list of strings, on an unknown thread
SP_PUBLIC StringView preferMimeType(SpanView<StringView> available, SpanView<StringView> preference);

// Which single action the modifiers ask for, clamped to what the source allows.
//
// Ctrl = Copy, Shift = Move, Ctrl+Shift = Link - the convention every desktop shares. With no
// modifier the source's default wins. The result is always a subset of `allowed`, and falls back to
// the first of Copy/Move/Link present in `allowed` when the requested one is not offered, so a
// caller never has to handle an empty answer for a non-empty `allowed`
SP_PUBLIC DragActions modifiersToActions(InputModifier mods, DragActions allowed,
		DragActions dflt);

// Reduce a mask to the single action a drop would perform, preferring Copy over Move over Link.
// None in, None out
SP_PUBLIC DragActions pickAction(DragActions mask);

// Cursor for a resolved (single-bit) action. DragActions::None maps to NoDrop
SP_PUBLIC WindowCursor actionToCursor(DragActions action);

} // namespace stappler::xenolith

#endif // XENOLITH_APPLICATION_DRAG_XLDRAGTYPES_H_
