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

// setOrUpdate, not update: the component must exist even when a bit is cleared, because its
// absence reads as state 0, i.e. also "not enabled".
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
	// Two sources, one bit. While locked the lock wins, but the widget's request is remembered
	// for unlockControl.
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
	// No component: the node was never written as a control (widgets publish `Enabled` via
	// applyControlEnabled() in init()).
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
			// The state before locking; read-only is read off the published bit.
			state->setControlFlag(ControlFlags::OwnerEnabled, ownerEnabled);
			state->setControlFlag(ControlFlags::OwnerReadOnly,
					sprt::hasFlag(state->state, InteractiveState::ReadOnly));
			state->setControlFlag(ControlFlags::Locked, true);
		}
		state->lockReason = reasonCode;
		return false; // the visible half is written below and dirties the style
	});

	// A locked control is disabled and read-only; both go through the writers.
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

	// Restore what the application last asked for, not "on".
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
	// Not locked: the common case
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
