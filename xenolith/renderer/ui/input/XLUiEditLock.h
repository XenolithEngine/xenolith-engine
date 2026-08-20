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

#ifndef XENOLITH_RENDERER_UI_INPUT_XLUIEDITLOCK_H_
#define XENOLITH_RENDERER_UI_INPUT_XLUIEDITLOCK_H_

#include "XLUiInteractiveComponent.h"
#include "XLNode.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

/* WHY a control cannot be edited - and it is NOT a validation failure.

`invalid` says "what is written here is wrong": the remedy is to fix the value, and an author who
reads it goes looking for a typo. A lock says "you may not write here at all", and its remedy is
somewhere else entirely - cut the wire that feeds this pin, declare the field, leave the read-only
view. A control that says the first when it means the second sends the author hunting for a mistake
they did not make, which is why the two are different words and both may be worn at once.

PRESENCE IS THE LOCK: a control nobody locked carries no component at all.

`ownerEnabled` is what the widget's own setEnabled() last asked for. Enablement and the lock are two
independent SOURCES of one effect - a control locked because a wire owns its value AND disabled
because its whole panel is inactive has to stay unusable until BOTH are cleared - so unlocking
restores what the application wanted rather than switching on something it had switched off for
reasons of its own. Same idea as InteractiveComponent's counters, narrowed to two named sources so
that the REASON survives. */
struct SP_PUBLIC EditLockComponent {
	static ComponentId Id;

	String reason;
	bool ownerEnabled = true;

	// The hint was installed by the lock, so it is the lock's to take away again. A hint the
	// application put there itself is never touched.
	bool ownsTooltip = false;
};

/* What a lockable control offers, and the whole of it.

There is no ui::Widget base and there must not be one - a widget is composed, not subclassed - so
the lock reaches the gate through the two methods nine widgets already declare with exactly these
signatures. No state, one line each to answer. The same shape, and the same reason, as
ui::TextHistoryTarget.

It is reached with a dynamic_cast, so a node that is NOT a control still takes the mark, the reason
and the hint: a caption, a table row or a widget of your own can say "this cannot be edited" without
having to become a control first. */
class SP_PUBLIC EditLockTarget {
public:
	virtual ~EditLockTarget() = default;

	virtual void setEnabled(bool) = 0;
	virtual bool isEnabled() const = 0;
};

/* Lock a node, and say why. The reason may be empty - "locked, no reason given" is a worse answer
than a reason but a better one than a control that is simply dead.

Applies the `locked` style class, clears InteractiveState::Enabled (so `:disabled` matches too - a
locked control IS disabled, and `locked` is the narrower second word for WHY), installs a
ui::TooltipTarget carrying the reason IF the node has none of its own, and asks the widget to stop
accepting edits. Synchronous: nothing here waits for the next frame, because a lock that arrives a
frame late is a lock that let one more edit through. */
SP_PUBLIC void setEditLock(NotNull<Node>, StringView reason);
SP_PUBLIC void clearEditLock(NotNull<Node>);

SP_PUBLIC bool isEditLocked(const Node *);
SP_PUBLIC StringView getEditLockReason(const Node *);

/* Widget side: one line at the top of setEnabled(). Records what was asked for and answers what the
widget must actually apply - the lock has the last word while it is on. */
SP_PUBLIC bool resolveEditLock(NotNull<Node>, bool requested);

/* The SINGLE writer of InteractiveState::Enabled and of the `disabled` style class.

Six widgets carried a copy of these three lines inside updateInteractiveState() and two more inline
in setEnabled(); five of the nine added the class and four did not, so half the kit could not be
painted by a stylesheet at all. Worse, a node with no InteractiveComponent reads as state 0, and
`:disabled` is "not :enabled" - so a widget that never wrote the component matched `:disabled` while
it was perfectly ENABLED. Calling this once from init() is what closes that. */
SP_PUBLIC bool applyControlEnabled(NotNull<Node>, bool enabled);

// The same, for the bit `:checked` reads. Here for the same reason: a widget that keeps its checked
// state only in a style class is invisible to the pseudo-class that exists for it.
SP_PUBLIC bool applyControlChecked(NotNull<Node>, bool checked);

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_INPUT_XLUIEDITLOCK_H_
