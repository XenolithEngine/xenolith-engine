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

/* WHY a control cannot be edited - and it is NOT a validation failure.

`:invalid` says "what is written here is wrong": the remedy is to fix the value, and an author who
reads it goes looking for a typo. A lock says "you may not write here at all", and its remedy is
somewhere else entirely - cut the wire that feeds this pin, declare the field, leave the read-only
view. A control that says the first when it means the second sends the author hunting for a mistake
they did not make, which is why the two are different words and both may be worn at once.

THE STATE ITSELF LIVES IN InteractiveComponent (see XLInteractiveComponent.h): the Locked flag, what
the widget last asked for, and the code of the sentence that says why. What lives HERE is the half
that has to know what a widget is - the `locked` style class, the hint, and the widget's own
setEnabled.

`locked` STAYS A STYLE CLASS, unlike `invalid` and the rest which became pseudo-classes: the CSS
subset has no `:locked` to become. It is the one such class that was never a crutch. */

// What a lockable control offers, and the whole of it.
//
// There is no ui::Widget base and there must not be one - a widget is composed, not subclassed - so
// the lock reaches the gate through the two methods nine widgets already declare with exactly these
// signatures. No state, one line each to answer.
//
// It is reached with a dynamic_cast, so a node that is NOT a control still takes the mark, the
// reason and the hint: a caption, a table row or a widget of your own can say "this cannot be
// edited" without having to become a control first.
class SP_PUBLIC EditLockTarget {
public:
	virtual ~EditLockTarget() = default;

	virtual void setEnabled(bool) = 0;
	virtual bool isEnabled() const = 0;
};

/* Lock a node, and say why with a diagnostic CODE.

A code, not a string: the sentence is the same in every instance of one situation, and the registry
that owns it (stappler::diagnostic) hands out one number for one text. Register the message as a
constant of the calling module:

    static const uint32_t s_lockedByWire = diagnostic::registerMessage("the value arrives on a wire");

Applies the `locked` style class, clears the Enabled bit and raises ReadOnly (a locked control IS
disabled and it may not be written to - `:disabled` and `:read-only` both match), installs a
ui::TooltipComponent carrying the message IF the node has none of its own, and asks the widget to stop
accepting edits. Synchronous: nothing here waits for the next frame, because a lock that arrives a
frame late is a lock that let one more edit through. */
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
