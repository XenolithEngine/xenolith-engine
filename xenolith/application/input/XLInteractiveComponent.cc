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

#include "XLInteractiveComponent.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

ComponentId InteractiveComponent::Id;

namespace {

// setOrUpdate, not update: the component has to EXIST even when a bit is being cleared, because its
// absence reads as state 0 - indistinguishable from "cleared" for this bit and also "not enabled"
// for the next one.
static bool applyStateFlag(NotNull<Node> node, InteractiveState flag, bool value) {
	return node->setOrUpdateComponent<InteractiveComponent>(
				   [&](NotNull<InteractiveComponent> state) {
		return state->updateState(value ? (state->state | flag) : (state->state & ~flag));
	}) != nullptr;
}

} // namespace

bool applyControlEnabled(NotNull<Node> node, bool enabled) {
	return applyStateFlag(node, InteractiveState::Enabled, enabled);
}

bool applyControlChecked(NotNull<Node> node, bool checked) {
	return applyStateFlag(node, InteractiveState::Checked, checked);
}

bool applyControlInvalid(NotNull<Node> node, bool invalid) {
	return applyStateFlag(node, InteractiveState::Invalid, invalid);
}

bool applyControlIndeterminate(NotNull<Node> node, bool indeterminate) {
	return applyStateFlag(node, InteractiveState::Indeterminate, indeterminate);
}

bool applyControlRequired(NotNull<Node> node, bool required) {
	return applyStateFlag(node, InteractiveState::Required, required);
}

bool applyControlDefault(NotNull<Node> node, bool isDefault) {
	return applyStateFlag(node, InteractiveState::Default, isDefault);
}

bool applyControlFocusVisible(NotNull<Node> node, bool visible) {
	return applyStateFlag(node, InteractiveState::FocusVisible, visible);
}

bool applyControlReadOnly(NotNull<Node> node, bool readOnly) {
	// Two sources, one bit. While a lock is on it has the last word - it is the stronger claim -
	// but what the widget asked for is remembered, exactly as setEnabled's answer is, so that
	// unlockControl can give it back instead of declaring the field writable.
	if (isControlLocked(node)) {
		node->updateComponent<InteractiveComponent>([&](NotNull<InteractiveComponent> state) {
			state->setControlFlag(ControlFlags::OwnerReadOnly, readOnly);
			return false; // bookkeeping: nothing a selector can see moved
		});
		return applyStateFlag(node, InteractiveState::ReadOnly, true);
	}
	return applyStateFlag(node, InteractiveState::ReadOnly, readOnly);
}

namespace {

static bool hasState(const Node *node, InteractiveState flag) {
	if (auto ic = node ? node->getComponent<InteractiveComponent>() : nullptr) {
		return sprt::hasFlag(ic->state, flag);
	}
	// No component at all: the node was never written as a control. `Enabled` is the only bit whose
	// absence would be a lie about a widget that simply forgot to publish itself - and that is
	// caught by applyControlEnabled() in init(), not by guessing here.
	return false;
}

} // namespace

bool isControlEnabled(const Node *node) { return hasState(node, InteractiveState::Enabled); }

bool isControlChecked(const Node *node) { return hasState(node, InteractiveState::Checked); }

bool isControlReadOnly(const Node *node) { return hasState(node, InteractiveState::ReadOnly); }

bool isControlInvalid(const Node *node) { return hasState(node, InteractiveState::Invalid); }

bool isControlLocked(const Node *node) {
	if (auto ic = node ? node->getComponent<InteractiveComponent>() : nullptr) {
		return ic->hasControlFlag(ControlFlags::Locked);
	}
	return false;
}

uint32_t getControlLockReason(const Node *node) {
	if (auto ic = node ? node->getComponent<InteractiveComponent>() : nullptr) {
		return ic->hasControlFlag(ControlFlags::Locked) ? ic->lockReason : diagnostic::NoMessage;
	}
	return diagnostic::NoMessage;
}

bool lockControl(NotNull<Node> node, uint32_t reasonCode, bool ownerEnabled) {
	const bool wasLocked = isControlLocked(node);

	node->setOrUpdateComponent<InteractiveComponent>([&](NotNull<InteractiveComponent> state) {
		if (!wasLocked) {
			// What the control is being taken away FROM, recorded before the lock takes it - and
			// the read-only answer it was giving, read off the bit it already publishes rather
			// than through one more method on every widget.
			state->setControlFlag(ControlFlags::OwnerEnabled, ownerEnabled);
			state->setControlFlag(ControlFlags::OwnerReadOnly,
					sprt::hasFlag(state->state, InteractiveState::ReadOnly));
			state->setControlFlag(ControlFlags::Locked, true);
		}
		state->lockReason = reasonCode;
		return false; // the visible half is written below, and only that is worth a restyle
	});

	// A locked control IS disabled, and it may not be written to. Both are what a stylesheet asks
	// about, so both go through the writers.
	applyControlEnabled(node, false);
	applyStateFlag(node, InteractiveState::ReadOnly, true);
	return !wasLocked;
}

ControlLockRelease unlockControl(NotNull<Node> node) {
	ControlLockRelease ret;

	auto ic = node->getComponent<InteractiveComponent>();
	if (!ic || !ic->hasControlFlag(ControlFlags::Locked)) {
		return ret;
	}

	ret.wasLocked = true;
	ret.ownerEnabled = ic->hasControlFlag(ControlFlags::OwnerEnabled);
	ret.ownerReadOnly = ic->hasControlFlag(ControlFlags::OwnerReadOnly);
	ret.ownsTooltip = ic->hasControlFlag(ControlFlags::OwnsTooltip);

	node->updateComponent<InteractiveComponent>([](NotNull<InteractiveComponent> state) {
		state->setControlFlag(ControlFlags::Locked, false);
		state->setControlFlag(ControlFlags::OwnsTooltip, false);
		state->lockReason = diagnostic::NoMessage;
		return false;
	});

	// What the APPLICATION last asked for, not "on": a control it had disabled for its own reasons
	// stays disabled, and one that was read-only stays read-only.
	applyControlEnabled(node, ret.ownerEnabled);
	applyStateFlag(node, InteractiveState::ReadOnly, ret.ownerReadOnly);
	return ret;
}

void setControlOwnerEnabled(NotNull<Node> node, bool value) {
	node->updateComponent<InteractiveComponent>([&](NotNull<InteractiveComponent> state) {
		state->setControlFlag(ControlFlags::OwnerEnabled, value);
		return false;
	});
}

void setControlOwnsTooltip(NotNull<Node> node, bool value) {
	node->setOrUpdateComponent<InteractiveComponent>([&](NotNull<InteractiveComponent> state) {
		state->setControlFlag(ControlFlags::OwnsTooltip, value);
		return false;
	});
}

bool resolveControlLock(NotNull<Node> node, bool requested) {
	// Nothing to compose with: the overwhelmingly common case, and it costs a pointer test
	if (!isControlLocked(node)) {
		return requested;
	}

	// Remembered, but not obeyed while the lock is on.
	node->updateComponent<InteractiveComponent>([&](NotNull<InteractiveComponent> state) {
		state->setControlFlag(ControlFlags::OwnerEnabled, requested);
		return false;
	});
	return false;
}

} // namespace stappler::xenolith
