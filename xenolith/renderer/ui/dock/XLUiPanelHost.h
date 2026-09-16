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

/** Something that parks panels: a ui::DockSystem, a ui::AccordionView. ui::PanelHandle talks to
it, and the registry calls it back when a panel moves away.

Not a Ref: implementers are already Refs through System or Node. Identity and lifetime are therefore
separate: a drag payload holds the raw PanelHost * and an Rc<Ref> from getPanelHostRef(). */
class SP_PUBLIC PanelHost {
public:
	virtual ~PanelHost() = default;

	virtual PanelRegistry *getPanelRegistry() const = 0;

	// this host as a Ref, for holding it alive
	virtual Ref *getPanelHostRef() = 0;

	virtual bool isPanelOpen(StringView id) const = 0;

	// Bring the panel forward: activate its tab, expand its section.
	virtual bool activatePanel(StringView id) = 0;

	/* The user pressed the panel's handle; ui::DockTab raises it right after activatePanel. Fires
	even when the tap changed nothing, and never on programmatic activation. The default does
	nothing. */
	virtual void handlePanelTapped(StringView id) { }

	// Take the panel out as closed by the user; the node survives in the registry.
	virtual bool closePanel(StringView id) = 0;

	/* Give the panel up without destroying its node or reporting it closed: it is moving elsewhere.
	Called only by PanelRegistry::acquireContent on the current host. Must not acquire content for
	the same id, directly or indirectly (asserted). */
	virtual void releasePanel(StringView id) = 0;

	/* Where a drag ghost for this host's panels is parked: inside this host's StyleResolver subtree
	(or it comes out unstyled) and outside any clipped scroll container. */
	virtual Node *getPanelDecoratorParent() const = 0;

	// forwards to the registry
	const DockPanelDescriptor *getPanelDescriptor(StringView id) const {
		auto registry = getPanelRegistry();
		return registry ? registry->getPanelDescriptor(id) : nullptr;
	}
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_DOCK_XLUIPANELHOST_H_
