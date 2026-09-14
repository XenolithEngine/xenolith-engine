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

/* Marker for `:focus-within`. Kept out of InteractiveComponent: containers above a control would
otherwise get a default Enabled state and change `:enabled`/`:disabled` matching while focused.

Presence is the state; the counter removes the component on the last release. The bit is still
document::InteractiveFlags - see XLUiStyleSheet.cc (matching) and XLUiStyleResolver.cc (restyle
mask and the recursive-resolver whitelist). */
struct SP_PUBLIC FocusWithinComponent {
	static ComponentId Id;

	// How many focused descendants (or the node itself) are counting on it. Never 0 on a live
	// component: the last release removes it.
	int counter = 0;
};

// Does a rule asking for `:focus-within` match this node?
SP_PUBLIC bool hasFocusWithin(const Node *);

/* Moves the marker from the ancestor chain of `from` to that of `to` (focused nodes, either may
be null), up to the scene root. Retains the new chain first, so a shared ancestor keeps its
style. */
SP_PUBLIC void updateFocusWithinChain(Node *from, Node *to);

} // namespace stappler::xenolith

#endif // XENOLITH_APPLICATION_INPUT_XLFOCUSWITHIN_H_
