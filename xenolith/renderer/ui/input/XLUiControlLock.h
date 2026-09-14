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

#ifndef XENOLITH_RENDERER_UI_INPUT_XLUICONTROLLOCK_H_
#define XENOLITH_RENDERER_UI_INPUT_XLUICONTROLLOCK_H_

#include "XLInteractiveComponent.h"
#include "XLUiConfig.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

/* Edit lock: a control that may not be written to because something else owns its value. This is
not a validation failure (`:invalid`); both may apply at once.

The state lives in InteractiveComponent (Locked flag, the owner's requested enabled value, the
reason code); this layer adds the `locked` style class (the CSS subset has no `:locked`), the
tooltip and the widget's setEnabled. */

// Interface a lockable control implements. Found with dynamic_cast; a node that does not implement
// it still gets the style class, the reason and the tooltip.
class SP_PUBLIC EditLockTarget {
public:
	virtual ~EditLockTarget() = default;

	virtual void setEnabled(bool) = 0;
	virtual bool isEnabled() const = 0;
};

/* Lock a node; the reason is a diagnostic code registered once by the calling module:

    static const uint32_t s_lockedByWire = diagnostic::registerMessage("value arrives on a wire");

Applies the `locked` style class, clears Enabled and sets ReadOnly (`:disabled` and `:read-only`
both match), installs a tooltip with the message if the node has none, and disables the widget.
Synchronous, so no edit slips through before the next frame. */
SP_PUBLIC void setEditLock(NotNull<Node>, uint32_t reasonCode);
SP_PUBLIC void clearEditLock(NotNull<Node>);

SP_PUBLIC bool isEditLocked(const Node *);

// The code, and the text it stands for
SP_PUBLIC uint32_t getEditLockReasonCode(const Node *);
SP_PUBLIC StringView getEditLockReason(const Node *);

// Widget side: one line at the top of setEnabled()
SP_PUBLIC bool resolveEditLock(NotNull<Node>, bool requested);

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_INPUT_XLUICONTROLLOCK_H_
