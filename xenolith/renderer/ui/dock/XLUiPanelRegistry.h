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

#ifndef XENOLITH_RENDERER_UI_DOCK_XLUIPANELREGISTRY_H_
#define XENOLITH_RENDERER_UI_DOCK_XLUIPANELREGISTRY_H_

#include "XLUiDockTypes.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

class PanelHost;

/** What a panel IS, and where it currently lives - for every container that parks panels.

It was DockSystem's two private maps until a second kind of container appeared. Sharing one of these
between a ui::DockSystem and a ui::AccordionView is what lets a panel be dragged from one into the
other and arrive with its node - its scroll position, its selection, its half-typed text - intact,
because there is exactly one node and both containers reach it through here.

THREE THINGS IT OWNS, and the third is the one that is easy to miss:

 - the DESCRIPTORS. What a panel is called, what it costs at minimum, how to build it. Declared once
   by the application, never derived from a saved layout;

 - the CONTENT. `builder` runs at most once, on first show, and the node is then kept alive here
   forever after - across tab switches, across moves between frames, across the destruction of the
   container it was parked in. A panel that rebuilt itself on a move would silently lose everything
   the user had done to it, and nothing in a layout dump would show it;

 - the HOST CLAIM. A panel node has ONE identity, so it can be parked in exactly one place. That is
   not something every drop slot can be trusted to remember, so it is enforced here instead:
   acquireContent(id, forHost) tells the previous host to release the panel before recording the new
   one. A container therefore never has to ask "does somebody else have this" - it just acquires.

RE-ENTRANCY. The eviction calls out into a host, which will restructure itself and may well acquire
another panel while it does so - a dock frame whose active tab just left shows the next one. That
terminates only because the ids differ, so the invariant is asserted rather than hoped for:
PanelHost::releasePanel(id) must never acquire content for that same id. */
class SP_PUBLIC PanelRegistry : public Ref {
public:
	virtual ~PanelRegistry() = default;

	virtual bool init();

	// --- descriptors -------------------------------------------------------

	// Register what a panel is. The content node is not built here.
	void registerPanel(DockPanelDescriptor &&);

	// Forget a panel entirely: it is closed on whatever host holds it, and its built node - if it
	// ever had one - is released.
	void unregisterPanel(StringView id);

	const DockPanelDescriptor *getPanelDescriptor(StringView id) const;

	const Map<String, DockPanelDescriptor> &getPanelDescriptors() const { return _descriptors; }

	// --- content and the host claim ----------------------------------------

	/* The node of a panel, built on first use and kept forever after, AND the transfer of that panel
	to `forHost`. When another host holds it, that host is told to release it first - structurally;
	the node itself is never touched, which is the whole point.

	Answers null when the panel is unknown, when its builder returned nothing, or when the node would
	end up inside itself (an accordion registered as one of its own panels). */
	Node *acquireContent(StringView panelId, NotNull<PanelHost> forHost);

	// Which host currently holds a panel; null when it is parked nowhere.
	PanelHost *getHost(StringView panelId) const;

	// Every node built so far. What a host walks when it has to detach the panels inside a subtree
	// it is about to destroy - it cannot just clean that subtree, because these nodes outlive it.
	void foreachContent(const Callback<void(StringView, Node *)> &) const;

	// A host announces itself while it is in the scene. Membership is only used to bound the claims
	// a releaseHost has to sweep, and to keep a host from being asked to release after it is gone.
	void addHost(NotNull<PanelHost>);
	void removeHost(NotNull<PanelHost>);

	// Drop every claim `host` holds WITHOUT touching the nodes: it is leaving the scene, and the
	// panels it was holding are now parked nowhere. They keep their content, so re-opening one
	// anywhere else brings back exactly what was there.
	void releaseHost(NotNull<PanelHost>);

protected:
	// Whether `node` is `container` or one of its descendants - the guard that keeps a container
	// from parking itself inside itself.
	static bool isInSubtree(const Node *node, const Node *container);

	Map<String, DockPanelDescriptor> _descriptors;
	Map<String, Rc<Node>> _content;

	// raw pointers: a host deregisters itself on the way out, and it is not a Ref anyway
	Map<String, PanelHost *> _hosts;
	Vector<PanelHost *> _attached;

	// asserts the re-entrancy invariant above; holds the id being released, empty when idle
	String _releasing;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_DOCK_XLUIPANELREGISTRY_H_
