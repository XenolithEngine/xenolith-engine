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

#ifndef XENOLITH_RENDERER_UI_DOCK_XLUIPANELHOST_H_
#define XENOLITH_RENDERER_UI_DOCK_XLUIPANELHOST_H_

#include "XLUiPanelRegistry.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

/** Something that parks panels: a ui::DockSystem, a ui::AccordionView.

It is what a panel's HANDLE talks to (see ui::PanelHandle), so that one tab implementation serves
every kind of container, and what the registry calls back into when a panel is moved away.

A PURE INTERFACE WITH NO BASE. Not a Ref, deliberately: a DockSystem is a System and an AccordionView
is a Node, and both of those already are Refs - inheriting one here would give the second of them two
Ref bases and two refcounts. ui::EditLockTarget is the same shape for the same reason, mixed into
ui::Button beside a Panel. The consequence is that identity and lifetime travel separately: a drag
payload carries the raw PanelHost * to dispatch on and an Rc<Ref> from getPanelHostRef() to keep it
alive, because there is no cast between the two. */
class SP_PUBLIC PanelHost {
public:
	virtual ~PanelHost() = default;

	virtual PanelRegistry *getPanelRegistry() const = 0;

	// This host as a Ref, for whoever has to hold it alive - see the note above on why this cannot
	// simply be a base class.
	virtual Ref *getPanelHostRef() = 0;

	virtual bool isPanelOpen(StringView id) const = 0;

	// Bring the panel forward: activate its tab, expand its section.
	virtual bool activatePanel(StringView id) = 0;

	/* A PERSON pressed the panel's handle. ui::DockTab raises it, right after activatePanel, so a
	handler already sees the new active panel; a host whose handle is something else raises it from
	there.

	Separate from activation, and not a rename of it, for two reasons that pull in opposite
	directions:

	 - it fires on a tap that CHANGED NOTHING - the front tab of a stack, the only tab of a place -
	   which is exactly the click a collapsed rail has to answer to, and which activatePanel returns
	   early on;
	 - it does NOT fire on a programmatic activation - restoring a saved layout, a scripted command -
	   so an application can answer the CLICK without answering its own bookkeeping.

	Optional: the default does nothing, so a host that has no notion of being folded away (an
	AccordionView) need not know this exists. */
	virtual void handlePanelTapped(StringView id) { }

	// Take the panel out and treat it as closed by the user. The node survives in the registry, so
	// re-opening it brings back exactly what was there.
	virtual bool closePanel(StringView id) = 0;

	/* Give the panel up WITHOUT destroying its node and WITHOUT reporting it as closed: it is moving
	somewhere else. Called only by PanelRegistry::acquireContent, on the host that currently holds
	the panel, just before the new one takes it.

	MUST NOT acquire content for that same id, directly or through anything it calls. That is what
	terminates the eviction chain, and the registry asserts it. */
	virtual void releasePanel(StringView id) = 0;

	/* Where a drag ghost for one of this host's panels may be parked. Two constraints, and a node
	that satisfies only one of them is the wrong answer:

	 - INSIDE this host's StyleResolver subtree, or a decorator that takes its look from a stylesheet
	   comes out unstyled - a resolver only ever sees its own subtree;
	 - NOT inside a clipped scroll container, or the ghost vanishes at the first edge it crosses. */
	virtual Node *getPanelDecoratorParent() const = 0;

	// One forward to the registry. Not virtual: a vtable slot for it would buy nothing, and every
	// host would write the same line.
	const DockPanelDescriptor *getPanelDescriptor(StringView id) const {
		auto registry = getPanelRegistry();
		return registry ? registry->getPanelDescriptor(id) : nullptr;
	}
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_DOCK_XLUIPANELHOST_H_
