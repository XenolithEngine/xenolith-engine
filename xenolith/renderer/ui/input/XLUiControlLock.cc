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

#include "XLUiControlLock.h"

#include "XLUiTooltipSystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

// The narrower second word for a control that is disabled BECAUSE something else owns its value.
// A class rather than a pseudo-class, because the CSS subset has no `:locked`.
static constexpr auto s_editLockClass = StringView("locked");

bool isEditLocked(const Node *node) { return isControlLocked(node); }

uint32_t getEditLockReasonCode(const Node *node) { return getControlLockReason(node); }

StringView getEditLockReason(const Node *node) {
	return diagnostic::getMessage(getControlLockReason(node));
}

bool resolveEditLock(NotNull<Node> node, bool requested) {
	return resolveControlLock(node, requested);
}

void setEditLock(NotNull<Node> node, uint32_t reasonCode) {
	// Asked BEFORE the lock takes the control away: only this layer knows how to ask a widget, and
	// the very next call goes through resolveEditLock, which would otherwise record the LOCK's own
	// `false` as the owner's wish - and unlocking would then restore "disabled" forever.
	bool ownerEnabled = true;
	auto target = dynamic_cast<EditLockTarget *>(node.get());
	if (target) {
		ownerEnabled = target->isEnabled();
	}

	const bool wasLocked = isControlLocked(node);

	/* The widget is taken away FIRST, while it still believes it is enabled.

	Order matters and it cost a check to find out: lockControl clears the Enabled bit, and a widget
	whose setEnabled(false) then sees itself already disabled returns early - taking its blur() and
	its "drop the gesture" with it. Locked but still holding the keyboard is exactly the state a
	lock exists to prevent. */
	if (target && !wasLocked) {
		target->setEnabled(false);
	}

	lockControl(node, reasonCode, ownerEnabled);

	// ...and put back what the control WANTED. The call above ran through resolveEditLock, which
	// records the lock's own `false` as the owner's wish - and unlocking would then restore
	// "disabled" forever.
	setControlOwnerEnabled(node, ownerEnabled);

	node->addStyleClass(s_editLockClass);

	auto reason = diagnostic::getMessage(reasonCode);
	if (!reason.empty()) {
		if (getTooltip(node)) {
			// Someone else's hint. Changing its text would silently destroy what the application
			// put there, and clearing the lock would then take away a hint the lock never owned -
			// so it is left exactly as it is, and the reason stays readable through
			// getEditLockReason() for whoever lays the control out.
			if (isControlLocked(node)
					&& node->getComponent<InteractiveComponent>()->hasControlFlag(
							ControlFlags::OwnsTooltip)) {
				setTooltipText(node, reason);
			}
		} else {
			setTooltip(node, reason);
			setControlOwnsTooltip(node, true);
		}
	}
}

void clearEditLock(NotNull<Node> node) {
	auto released = unlockControl(node);
	if (!released.wasLocked) {
		return;
	}

	node->removeStyleClass(s_editLockClass);

	if (released.ownsTooltip) {
		removeTooltip(node);
	}

	if (auto target = dynamic_cast<EditLockTarget *>(node.get())) {
		target->setEnabled(released.ownerEnabled);
	}
}

} // namespace stappler::xenolith::ui
