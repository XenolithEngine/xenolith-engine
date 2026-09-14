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

// Marks a control disabled because something else owns its value; a class, since the CSS subset
// has no `:locked`.
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
	// Read the owner's enabled state before locking; resolveEditLock would otherwise record the
	// lock's own `false` as the owner's wish.
	bool ownerEnabled = true;
	auto target = dynamic_cast<EditLockTarget *>(node.get());
	if (target) {
		ownerEnabled = target->isEnabled();
	}

	const bool wasLocked = isControlLocked(node);

	/* Disable the widget before lockControl clears the Enabled bit: otherwise setEnabled(false)
	returns early and skips its blur() and gesture cancel. */
	if (target && !wasLocked) {
		target->setEnabled(false);
	}

	lockControl(node, reasonCode, ownerEnabled);

	// restore the owner's wish, which the setEnabled(false) above overwrote via resolveEditLock
	setControlOwnerEnabled(node, ownerEnabled);

	node->addStyleClass(s_editLockClass);

	auto reason = diagnostic::getMessage(reasonCode);
	if (!reason.empty()) {
		if (getTooltip(node)) {
			// A tooltip the lock does not own is left untouched; the reason stays available
			// through getEditLockReason().
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
