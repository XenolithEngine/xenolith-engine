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

#include "XLUiEditLock.h"
#include "XLUiTooltipSystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

ComponentId EditLockComponent::Id;

// The narrower second word for a control that is disabled BECAUSE something else owns its value.
// A plain class, because the CSS subset has no `:locked` and no attribute selectors - the same
// reason `invalid` is one.
static constexpr auto s_editLockClass = StringView("locked");
static constexpr auto s_controlDisabledClass = StringView("disabled");

bool applyControlEnabled(NotNull<Node> node, bool enabled) {
	if (enabled) {
		node->removeStyleClass(s_controlDisabledClass);
	} else {
		node->addStyleClass(s_controlDisabledClass);
	}

	// setOrUpdate rather than update: the component has to EXIST even for an enabled control, or
	// the resolver reads state 0 and `:disabled` matches something that is not disabled.
	return node->setOrUpdateComponent<InteractiveComponent>(
				   [&](NotNull<InteractiveComponent> state) {
		return state->updateState(enabled ? (state->state | InteractiveState::Enabled)
										  : (state->state & ~InteractiveState::Enabled));
	}) != nullptr;
}

bool applyControlChecked(NotNull<Node> node, bool checked) {
	return node->setOrUpdateComponent<InteractiveComponent>(
				   [&](NotNull<InteractiveComponent> state) {
		return state->updateState(checked ? (state->state | InteractiveState::Checked)
										  : (state->state & ~InteractiveState::Checked));
	}) != nullptr;
}

bool isEditLocked(const Node *node) {
	return node ? node->getComponent<EditLockComponent>() != nullptr : false;
}

StringView getEditLockReason(const Node *node) {
	if (node) {
		if (auto c = node->getComponent<EditLockComponent>()) {
			return c->reason;
		}
	}
	return StringView();
}

bool resolveEditLock(NotNull<Node> node, bool requested) {
	// Nothing to compose with: the overwhelmingly common case, and it must cost a pointer test.
	auto c = node->getComponent<EditLockComponent>();
	if (!c) {
		return requested;
	}

	node->updateComponent<EditLockComponent>([&](NotNull<EditLockComponent> lock) {
		if (lock->ownerEnabled == requested) {
			return false;
		}
		lock->ownerEnabled = requested;
		return true;
	});

	// Remembered, but not obeyed while the lock is on.
	return false;
}

void setEditLock(NotNull<Node> node, StringView reason) {
	const bool wasLocked = isEditLocked(node);

	node->setOrUpdateComponent<EditLockComponent>([&](NotNull<EditLockComponent> lock) {
		if (!wasLocked) {
			// What the control is being taken away FROM, recorded before the lock takes it.
			if (auto target = dynamic_cast<EditLockTarget *>(node.get())) {
				lock->ownerEnabled = target->isEnabled();
			}
		}
		if (StringView(lock->reason) == reason) {
			return !wasLocked;
		}
		lock->reason = reason.str<Interface>();
		return true;
	});

	node->addStyleClass(s_editLockClass);
	applyControlEnabled(node, false);

	if (!reason.empty()) {
		if (auto tooltip = node->getSystemByType<TooltipTarget>()) {
			// Someone else's hint. Changing its text would silently destroy what the application
			// put there, and clearing the lock would then take away a hint the lock never owned -
			// so it is left exactly as it is, and the reason stays readable through
			// getEditLockReason() for whoever lays the control out.
			if (isEditLocked(node) && node->getComponent<EditLockComponent>()->ownsTooltip) {
				tooltip->setText(reason);
			}
		} else {
			node->addSystem(Rc<TooltipTarget>::create(reason));
			node->updateComponent<EditLockComponent>([](NotNull<EditLockComponent> lock) {
				lock->ownsTooltip = true;
				return true;
			});
		}
	}

	if (auto target = dynamic_cast<EditLockTarget *>(node.get())) {
		// What the application wanted, captured before the lock takes the control away - because
		// the very next call goes through resolveEditLock, which would otherwise record the LOCK's
		// own `false` as the owner's wish and unlocking would then restore "disabled" forever.
		const bool ownerEnabled = node->getComponent<EditLockComponent>()->ownerEnabled;
		target->setEnabled(false);
		node->updateComponent<EditLockComponent>([&](NotNull<EditLockComponent> lock) {
			if (lock->ownerEnabled == ownerEnabled) {
				return false;
			}
			lock->ownerEnabled = ownerEnabled;
			return true;
		});
	}
}

void clearEditLock(NotNull<Node> node) {
	auto c = node->getComponent<EditLockComponent>();
	if (!c) {
		return;
	}

	const bool ownerEnabled = c->ownerEnabled;
	const bool ownsTooltip = c->ownsTooltip;

	node->removeComponent<EditLockComponent>();
	node->removeStyleClass(s_editLockClass);

	if (ownsTooltip) {
		if (auto tooltip = node->getSystemByType<TooltipTarget>()) {
			node->removeSystem(tooltip);
		}
	}

	// What the APPLICATION last asked for, not "on": a control it had disabled for its own reasons
	// stays disabled.
	applyControlEnabled(node, ownerEnabled);
	if (auto target = dynamic_cast<EditLockTarget *>(node.get())) {
		target->setEnabled(ownerEnabled);
	}
}

} // namespace stappler::xenolith::ui
