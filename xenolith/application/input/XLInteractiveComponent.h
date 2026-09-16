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

/* Control bookkeeping that is not visible to stylesheets (InteractiveFlags is exactly the set of
pseudo-classes). `OwnerEnabled` and `OwnerReadOnly` keep what the widget last asked for while a
lock overrides it, so unlocking restores the application's choice. */
enum class ControlFlags : uint32_t {
	None = 0,

	// Something else owns this control's value: it may not be edited, and `lockReason` says why.
	Locked = 1 << 0,

	OwnerEnabled = 1 << 1,
	OwnerReadOnly = 1 << 2,

	// The lock installed the hint and may remove it; an application's own hint is never touched.
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

	// Bookkeeping, not style. Default: unlocked, and the widget considers itself enabled.
	ControlFlags flags = ControlFlags::OwnerEnabled;

	// Why this control is locked, as a code in the diagnostic registry; NoMessage when it is not.
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

	// Bookkeeping changes nothing a selector can see, so it never reports "dirty".
	void setControlFlag(ControlFlags flag, bool value) {
		flags = value ? (flags | flag) : (flags & ~flag);
	}
};

/* The writers: every bit a stylesheet can ask about is written here and nowhere else. A node
without InteractiveComponent reads as state 0 and matches `:disabled`, so widgets call
applyControlEnabled once from init(). */
SP_PUBLIC bool applyControlEnabled(NotNull<Node>, bool enabled);
SP_PUBLIC bool applyControlChecked(NotNull<Node>, bool checked);
SP_PUBLIC bool applyControlInvalid(NotNull<Node>, bool invalid);
SP_PUBLIC bool applyControlIndeterminate(NotNull<Node>, bool indeterminate);
SP_PUBLIC bool applyControlRequired(NotNull<Node>, bool required);
SP_PUBLIC bool applyControlDefault(NotNull<Node>, bool isDefault);

/* `:focus-visible` - focus that arrived by keyboard. Written by whoever owns the focus change,
not by the widget, which cannot know how focus got to it. */
SP_PUBLIC bool applyControlFocusVisible(NotNull<Node>, bool visible);

/* The readers. A widget asks the component rather than keeping its own copy or a cached pointer:
component payloads are stored in place in a hash set, so adding another component can move it. */
SP_PUBLIC bool isControlEnabled(const Node *);
SP_PUBLIC bool isControlChecked(const Node *);
SP_PUBLIC bool isControlReadOnly(const Node *);
SP_PUBLIC bool isControlInvalid(const Node *);

/* `:read-only` has two sources - the widget's own mode and a lock. While locked the lock wins, but
the widget's request is remembered and restored on unlock. */
SP_PUBLIC bool applyControlReadOnly(NotNull<Node>, bool readOnly);

/* The lock, in terms of state alone: style class, tooltip and widget interface are handled in
ui::setEditLock. `ownerEnabled` is the control's enabled state before locking, captured by the
caller. */
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

/* Restore the widget's enabled wish after locking: setEnabled(false) during lock goes through
resolveControlLock and records the lock's `false`, so the caller writes its captured answer back. */
SP_PUBLIC void setControlOwnerEnabled(NotNull<Node>, bool);

/* Widget side: call at the top of setEnabled(). Records the request and returns what the widget
must apply; the lock wins while it is on. */
SP_PUBLIC bool resolveControlLock(NotNull<Node>, bool requested);

} // namespace stappler::xenolith

#endif // XENOLITH_APPLICATION_INPUT_XLINTERACTIVECOMPONENT_H_
