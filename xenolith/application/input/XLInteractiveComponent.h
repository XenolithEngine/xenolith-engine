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

#ifndef XENOLITH_APPLICATION_INPUT_XLINTERACTIVECOMPONENT_H_
#define XENOLITH_APPLICATION_INPUT_XLINTERACTIVECOMPONENT_H_

#include "XLCoreInput.h"
#include "XLNode.h"
#include "SPDiagnosticRegistry.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

using InteractiveState = sprt::window::InteractiveFlags;

/* What a control knows about itself BESIDE the state a stylesheet can see.

These are not InteractiveFlags and must not be: that enumeration is exactly the set of pseudo-
classes, and mixing in something CSS has no word for would make `matchesPseudo` answer questions
nobody asked it. These four are bookkeeping.

`OwnerEnabled` and `OwnerReadOnly` are what the WIDGET last asked for, kept while a lock overrides
it. Enablement and the lock are two independent sources of one effect - a control locked because a
wire owns its value AND disabled because its whole panel is inactive has to stay unusable until BOTH
are cleared - so unlocking restores what the application wanted rather than switching on something
it had switched off for reasons of its own. */
enum class ControlFlags : uint32_t {
	None = 0,

	// Something else owns this control's value: it may not be edited, and `lockReason` says why.
	Locked = 1 << 0,

	OwnerEnabled = 1 << 1,
	OwnerReadOnly = 1 << 2,

	// The lock installed the hint, so the lock may take it away again. A hint the application put
	// there itself is never touched.
	OwnsTooltip = 1 << 3,
};

SP_DEFINE_ENUM_AS_MASK(ControlFlags)

struct InteractiveComponent {
	static ComponentId Id;
	// <=0 - inactive, >0 - active;
	// implementet as counter so multiple sources can mark node as active
	// (e.g. mouse down on node + active IME)
	int activeCounter = 0;
	int focusCounter = 0;
	int hoverCounter = 0;
	InteractiveState state = InteractiveState::Enabled;

	// Bookkeeping, not style. Default: nobody has locked anything and the widget considers itself
	// enabled.
	ControlFlags flags = ControlFlags::OwnerEnabled;

	// Why this control is locked, as a code in the diagnostic registry; NoMessage when it is not.
	// A code rather than a String: the sentence is identical in every instance of one situation,
	// and a per-node copy of it would be an allocation per node for nothing.
	uint32_t lockReason = diagnostic::NoMessage;

	bool handleHover(int value) {
		hoverCounter = hoverCounter + value;
		if (hoverCounter > 0 && !sprt::hasFlag(state, InteractiveState::Hover)) {
			return updateState(state | InteractiveState::Hover);
		} else if (hoverCounter <= 0 && sprt::hasFlag(state, InteractiveState::Hover)) {
			return updateState(state & ~InteractiveState::Hover);
		}
		return false;
	}

	bool handleActive(int value) {
		activeCounter = activeCounter + value;
		if (activeCounter > 0 && !sprt::hasFlag(state, InteractiveState::Active)) {
			return updateState(state | InteractiveState::Active);
		} else if (activeCounter <= 0 && sprt::hasFlag(state, InteractiveState::Active)) {
			return updateState(state & ~InteractiveState::Active);
		}
		return false;
	}

	bool handleFocus(int value) {
		focusCounter = focusCounter + value;
		if (focusCounter > 0 && !sprt::hasFlag(state, InteractiveState::Focus)) {
			return updateState(state | InteractiveState::Focus);
		} else if (focusCounter <= 0 && sprt::hasFlag(state, InteractiveState::Focus)) {
			return updateState(state & ~InteractiveState::Focus);
		}
		return false;
	}

	bool updateState(InteractiveState newState) {
		if (newState != state) {
			state = newState;
			return true;
		}
		return false;
	}

	bool hasControlFlag(ControlFlags flag) const { return sprt::hasFlag(flags, flag); }

	// Bookkeeping changes nothing a selector can see, so it never reports "dirty": a style pass
	// triggered by remembering what setEnabled asked for would be a pass for nothing.
	void setControlFlag(ControlFlags flag, bool value) {
		flags = value ? (flags | flag) : (flags & ~flag);
	}
};

/* The writers. Every bit a stylesheet can ask about is written HERE and nowhere else.

Six widgets used to carry a copy of these lines inside updateInteractiveState() and two more inline
in setEnabled(); five of the nine added a style class and four did not, so half the kit could not be
painted by a stylesheet at all. Worse, a node with no InteractiveComponent reads as state 0, and
`:disabled` is "not :enabled" - so a widget that never wrote the component matched `:disabled` while
it was perfectly ENABLED. Calling applyControlEnabled once from init() is what closes that. */
SP_PUBLIC bool applyControlEnabled(NotNull<Node>, bool enabled);
SP_PUBLIC bool applyControlChecked(NotNull<Node>, bool checked);
SP_PUBLIC bool applyControlInvalid(NotNull<Node>, bool invalid);
SP_PUBLIC bool applyControlIndeterminate(NotNull<Node>, bool indeterminate);
SP_PUBLIC bool applyControlRequired(NotNull<Node>, bool required);
SP_PUBLIC bool applyControlDefault(NotNull<Node>, bool isDefault);

/* `:focus-visible` - focus that arrived by KEYBOARD and therefore wants to be seen. Written by
whoever owns the focus change, never by the widget: a widget knows it has focus and cannot know how
focus got to it, which is the whole difference this pseudo-class exists to draw. */
SP_PUBLIC bool applyControlFocusVisible(NotNull<Node>, bool visible);

/* The readers. A widget asks the component rather than keeping its own copy of the answer.

WHY NOT A CACHED POINTER, which would be the obvious optimisation: a Component keeps its payload in
28 bytes of IN-PLACE storage inside the container's hash set, so adding any other component to the
node can rehash the set and move this one. A pointer taken in init() would be dangling by the time
anybody trusted it. The lookup is a hash probe on a uint32 id and these are called on gestures, not
per frame per node. */
SP_PUBLIC bool isControlEnabled(const Node *);
SP_PUBLIC bool isControlChecked(const Node *);
SP_PUBLIC bool isControlReadOnly(const Node *);
SP_PUBLIC bool isControlInvalid(const Node *);

/* `:read-only` has TWO sources - the widget's own mode and a lock owning its value - and one bit.
While a lock is on it has the last word, but what the widget asked for is remembered, so unlocking
gives back the widget's answer instead of declaring the control writable. */
SP_PUBLIC bool applyControlReadOnly(NotNull<Node>, bool readOnly);

/* The lock, in terms of state alone: no style class, no tooltip, no widget interface. Those need to
know what a widget IS and live one layer up, in ui::setEditLock.

`ownerEnabled` is what the control answered before the lock took it away, captured by the caller
because only the caller knows how to ask. */
SP_PUBLIC bool lockControl(NotNull<Node>, uint32_t reasonCode, bool ownerEnabled);

// What the lock was holding, so the caller can put the control back the way it found it
struct ControlLockRelease {
	bool wasLocked = false;
	bool ownerEnabled = true;
	bool ownerReadOnly = false;
	bool ownsTooltip = false;
};

SP_PUBLIC ControlLockRelease unlockControl(NotNull<Node>);

SP_PUBLIC bool isControlLocked(const Node *);
SP_PUBLIC uint32_t getControlLockReason(const Node *);

// Record that the lock owns the hint it just installed
SP_PUBLIC void setControlOwnsTooltip(NotNull<Node>, bool);

/* Restore what the widget wanted, after the lock had to ask it something.

Taking a control away calls its setEnabled(false), which runs through resolveControlLock and records
the LOCK's own `false` as the owner's wish - so unlocking would restore "disabled" forever. The
caller captures the answer before it locks and writes it back with this. */
SP_PUBLIC void setControlOwnerEnabled(NotNull<Node>, bool);

/* Widget side: one line at the top of setEnabled(). Records what was asked for and answers what the
widget must actually apply - the lock has the last word while it is on. */
SP_PUBLIC bool resolveControlLock(NotNull<Node>, bool requested);

} // namespace stappler::xenolith

#endif // XENOLITH_APPLICATION_INPUT_XLINTERACTIVECOMPONENT_H_
