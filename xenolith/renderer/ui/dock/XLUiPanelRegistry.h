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

/** Panel descriptors, built content and the current host of each panel, shared by every
container that parks panels (ui::DockSystem, ui::AccordionView).

 - descriptors are declared by the application, never derived from a saved layout;
 - content: `builder` runs at most once and the node is kept here across tab switches, moves and
   destruction of the container, so a panel keeps its state;
 - host claim: a panel is parked in exactly one host. acquireContent(id, forHost) tells the previous
   host to release it first, so a container never has to check.

PanelHost::releasePanel(id) must not acquire content for the same id (asserted); otherwise the
eviction chain would not terminate. */
class SP_PUBLIC PanelRegistry : public Ref {
public:
	virtual ~PanelRegistry() = default;

	virtual bool init();

	// --- descriptors -------------------------------------------------------

	// Register what a panel is. The content node is not built here.
	void registerPanel(DockPanelDescriptor &&);

	// Forget a panel: close it on its host and release its built node, if any.
	void unregisterPanel(StringView id);

	const DockPanelDescriptor *getPanelDescriptor(StringView id) const;

	const Map<String, DockPanelDescriptor> &getPanelDescriptors() const { return _descriptors; }

	// --- content and the host claim ----------------------------------------

	/* The panel's node, built on first use, transferred to `forHost`: the previous host is told to
	release it first, without touching the node. Null when the panel is unknown, the builder
	returned nothing, or the node would end up inside itself. */
	Node *acquireContent(StringView panelId, NotNull<PanelHost> forHost);

	// Which host currently holds a panel; null when it is parked nowhere.
	PanelHost *getHost(StringView panelId) const;

	// Every node built so far; a host uses it to detach panels before destroying a subtree.
	void foreachContent(const Callback<void(StringView, Node *)> &) const;

	// Hosts register while in the scene, so releaseHost knows which claims to sweep and a removed
	// host is never asked to release.
	void addHost(NotNull<PanelHost>);
	void removeHost(NotNull<PanelHost>);

	// Drop every claim `host` holds without touching the nodes; the panels keep their content.
	void releaseHost(NotNull<PanelHost>);

protected:
	// whether `node` is `container` or its descendant; keeps a container from parking itself
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
