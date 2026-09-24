/**
 Copyright (c) 2026 Stappler Team <admin@stappler.org>

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

#ifndef XENOLITH_RENDERER_UI_MENU_XLUICONTEXTMENU_H_
#define XENOLITH_RENDERER_UI_MENU_XLUICONTEXTMENU_H_

#include "XLUiMenuPopup.h"
#include "XLInputListener.h"
#include "XLNode.h"
#include "XLSystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class InputDispatcher;

}

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

class ContextMenuSystem;

// What the user did to ask for a menu, and where.
struct SP_PUBLIC ContextMenuRequest {
	// In the space of the target's owner, the node that declared the menu.
	Vec2 location;

	// The same point in world space, for a builder that has to talk to something else.
	Vec2 worldLocation;

	InputModifier modifiers = InputModifier::None;

	// A long press rather than a right click; the button is MouseLeft either way.
	bool fromTouch = false;
};

/** Declares that a node has a context menu; it has no input of its own.

    ui::setContextMenu(node, source);

    ui::setContextMenu(node, [this](const ui::ContextMenuRequest &req) -> Rc<ui::MenuSource> {
        return buildMenuAt(req.location);   // a menu that depends on where it was asked for
    });

A component, not a listener: the input lives once on the scene (ContextMenuSystem), which suits
many declaring nodes and virtualized views.

Targets are found through the frame's hit-test registry, like drop targets: a node publishes its
drawn rect during its visit, so paint order gives the topmost target and unvisited subtrees offer
nothing. The registry is one frame old when a press reads it.

A target whose `resolve` returns null blocks: the search stops at the topmost target, so a widget
can opt out of the menu of the region around it. */
struct SP_PUBLIC ContextMenuComponent {
	static ComponentId Id;

	// What the node offers for one request. Null means no menu, and blocks; see above.
	using Builder = Function<Rc<MenuSource>(const ContextMenuRequest &)>;

	// A fixed menu, shared rather than copied, so it can be updated between openings.
	Rc<MenuSource> source;

	// A menu built per request. The builder wins; the source is the fallback when it returns null.
	Builder builder;

	// Inflates the hit test on every side, in world units, like DropTargetComponent::padding.
	float padding = 0.0f;

	bool enabled = true;

	// What this target offers for `request`; callable directly, e.g. from tests.
	Rc<MenuSource> resolve(const ContextMenuRequest &) const;
};

/** The context-menu coordinator. One per scene, on the SceneContent.

    auto menus = ui::ContextMenuSystem::acquireForNode(this);
    menus->setMenuConfigCallback([](ui::MenuConfig &config) {
        config.stylesheetSource = String(s_css);
    });

Placed and found like DragSystem: `acquireForNode` installs it on `SceneContent`, `findForNode`
walks the parent chain. Targets are read from the window's hit-test registry; there is no roster
here.

Two listeners:
- opening is asked last (post-scene band) and swallows nothing;
- closing is asked first (pre-scene band), swallows the press, and is enabled only while a menu is
  up.

The dispatcher does not record whether an event was handled, so a widget that wants the right
button must either capture it (swallow Begin or take the pointer exclusively, which turns this
listener's event into a Cancel) or offer an empty ContextMenuComponent. A plain Processed does not
block: a ui::Button inside a panel must not stop the panel's menu.

The mouse opens on release (a tap recognizer), which leaves right-button drags possible. */
class SP_PUBLIC ContextMenuSystem : public System {
public:
	static uint64_t Id;

	/* After every widget in the scene (SceneContent's listener is at -1); between the tooltip
	dismiss listener (-0x2000) and the drag cursor layer (-0x4000). */
	static constexpr int32_t ListenerPriority = -0x3000;

	/* Priority of the dismiss listener: positive, in the pre-scene band, so the dismissing press
	is consumed before any widget acts on it. */
	static constexpr int32_t DismissListenerPriority = 0x2000;

	// How long a finger must rest before the menu opens.
	static constexpr TimeInterval DefaultLongPressInterval = TimeInterval::milliseconds(500);

	// Walks the parent chain. Use this everywhere except inside a visit.
	static ContextMenuSystem *findForNode(Node *);

	// findForNode, and if there is none, installs one on the scene's content node.
	static ContextMenuSystem *acquireForNode(Node *);

	virtual ~ContextMenuSystem() = default;

	virtual bool init() override;

	virtual void handleAdded(Node *) override;
	virtual void handleRemoved() override;
	virtual void handleEnter(Scene *) override;
	virtual void handleExit() override;

	virtual void handleVisitBegin(FrameInfo &) override;

	// Number of context-menu targets registered in the committed frame, from the hit-test registry.
	size_t getTargetCount() const;

	/* Customizes the config of every context menu opening: above all the stylesheet (a native popup
	does not inherit the parent window's ui::StyleSystem), plus style, id prefix and callbacks.
	Called with onActivate and onClose already set; replacing them is allowed. */
	void setMenuConfigCallback(Function<void(MenuConfig &)> &&);

	void setLongPressInterval(TimeInterval value) { _longPress = value; }
	TimeInterval getLongPressInterval() const { return _longPress; }

	// Turns the input off; `openAt` still works.
	virtual void setEnabled(bool) override;

	/* Open the menu for whatever is under `worldLocation`, as the pointer does. False when no
	target offered anything; a refusing topmost target does not fall through. */
	virtual bool openAt(const Vec2 &worldLocation, bool fromTouch = false,
			InputModifier = InputModifier::None);

	virtual void close();

	SubWindow *getMenu() const { return _menu; }
	bool isMenuOpen() const;

	// The node the open menu came from, or the last one if it has closed. Null before the first
	// opening.
	Node *getCurrentTarget() const { return _currentTarget; }

	InputListener *getListener() const { return _listener; }

	// Enabled only while a menu is open - see DismissListenerPriority.
	InputListener *getDismissListener() const { return _dismissListener; }

protected:
	// The topmost target at that point, or null. A target that offers nothing still stops the
	// search.
	Node *findTarget(const Vec2 &worldLocation) const;

	core::RenderServerChannel *getParentWindow() const;

	// The window's input dispatcher, which owns the hit-test registry. Null outside a scene
	InputDispatcher *getDispatcher() const;

	// Attaches the listeners once the owner is running, which is later than when they are added;
	// see the implementation.
	void attachListener();

	// the only place that enables the dismiss listener, only while a menu is up
	void updateDismissListener();

	Rc<InputListener> _listener;
	Rc<InputListener> _dismissListener;
	Rc<SubWindow> _menu;
	Rc<Node> _currentTarget;

	Function<void(MenuConfig &)> _configCallback;
	TimeInterval _longPress = DefaultLongPressInterval;

	// Which opening the handle belongs to; a late close must not drop the next menu's handle.
	uint64_t _generation = 0;
};

// Adds (or replaces) a context menu on `node` and ensures the scene has a coordinator.
SP_PUBLIC const ContextMenuComponent *setContextMenu(NotNull<Node>, Rc<MenuSource> &&);
SP_PUBLIC const ContextMenuComponent *setContextMenu(NotNull<Node>,
		ContextMenuComponent::Builder &&);

SP_PUBLIC const ContextMenuComponent *getContextMenu(NotNull<Node>);
SP_PUBLIC void setContextMenuEnabled(NotNull<Node>, bool);
SP_PUBLIC void removeContextMenu(NotNull<Node>);

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_MENU_XLUICONTEXTMENU_H_
