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

#ifndef XENOLITH_APPLICATION_INPUT_XLFOCUSWITHIN_H_
#define XENOLITH_APPLICATION_INPUT_XLFOCUSWITHIN_H_

#include "XLInteractiveComponent.h"
#include "XLNode.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

/* `:focus-within` - the only interactive state a node does NOT carry in its InteractiveComponent,
and the reason is worth the extra type.

Every other flag belongs to a CONTROL. This one belongs to whatever happens to be above one: a
panel, a card, a toolbar, the layout root. InteractiveComponent defaults to Enabled, so giving these
containers one - which is what writing the bit there would do - switches `:enabled` on and
`:disabled` off for them, and only while focus is somewhere inside. A panel that matches `:disabled`
until the user tabs into it and stops matching afterwards is a worse bug than the missing feature:
it is the same trap already described in XLInteractiveComponent.h, walking.

PRESENCE IS THE STATE, and the counter is why the component can be trusted to disappear again.
Focus moves as a pair of events - the new chain is retained BEFORE the old one is released - so a
shared ancestor goes 1 -> 2 -> 1 and never blinks. Its style is not recomputed, and nothing below it
is either.

The bit itself still lives in document::InteractiveFlags, because that is what a selector asks
about; three places read the marker and fold it in - see XLUiStyleSheet.cc (matching) and
XLUiStyleResolver.cc (the restyle mask and the recursive-resolver whitelist). */
struct SP_PUBLIC FocusWithinComponent {
	static ComponentId Id;

	// How many focused descendants (or the node itself) are counting on it. Never 0 on a live
	// component: the last release removes it.
	int counter = 0;
};

// Does a rule asking for `:focus-within` match this node?
SP_PUBLIC bool hasFocusWithin(const Node *);

/* Move the marker from one chain of ancestors to another, `from` and `to` being the focused NODES
(either may be null). Retains the new chain first, so a common ancestor keeps its component and its
style throughout.

Walks to the scene root: `:focus-within` is a claim about ancestry, not about forms, and a stylesheet
is free to put the rule on any container above the field. */
SP_PUBLIC void updateFocusWithinChain(Node *from, Node *to);

} // namespace stappler::xenolith

#endif // XENOLITH_APPLICATION_INPUT_XLFOCUSWITHIN_H_
