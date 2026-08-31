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
	// In the space of the TARGET'S OWNER, which is the node that declared the menu - so a view can
	// resolve which of its rows was asked about without converting anything itself.
	Vec2 location;

	// The same point in world space, for a builder that has to talk to something else.
	Vec2 worldLocation;

	InputModifier modifiers = InputModifier::None;

	// A long press rather than a right click. A builder may want to offer fewer, larger items to a
	// finger, and nothing else can tell it which it was: the button is MouseLeft either way.
	bool fromTouch = false;
};

/** Declares that a node has a context menu. Set it and forget it - it has no input of its own.

    ui::setContextMenu(node, source);

    ui::setContextMenu(node, [this](const ui::ContextMenuRequest &req) -> Rc<ui::MenuSource> {
        return buildMenuAt(req.location);   // a menu that depends on WHERE it was asked for
    });

WHY IT IS DATA AND NOT A LISTENER. A listener per element is the obvious implementation and the
wrong one: a menu is declared on many nodes and used on almost none of them, and in a virtualized
view the node that would carry the listener exists only while it is on screen. So the input lives
once, on the scene (ContextMenuSystem), and this only says WHAT the node offers - which is a fact
about the node, and a fact belongs in a Component.

HOW IT IS FOUND - through the frame's hit-test registry, the same one drop targets use. The node
publishes the rect it was DRAWN with, once per frame, from inside its own visit (see HitTestFlags):
the rect needs no re-deriving, registration order is paint order so the topmost is found by walking
the registry backwards, and a node that is not visited is not registered - which makes an invisible
or detached subtree stop offering a menu with no bookkeeping at all.

The registry is one frame old when a press reads it, exactly like the listener storage the
dispatcher resolves every event against.

A TARGET THAT OFFERS NOTHING BLOCKS. `resolve` returning null does not fall through to the target
underneath: the search stops at the topmost target, and that is how a widget says "no menu here"
inside a region that has one. Falling through would mean a right click on a control inside a panel
silently gets the panel's menu, which is never what the control wanted. */
struct SP_PUBLIC ContextMenuComponent {
	static ComponentId Id;

	// What the node offers for one request. Null means "no menu", and blocks - see the note above.
	using Builder = Function<Rc<MenuSource>(const ContextMenuRequest &)>;

	// A fixed menu. The source is shared, not copied: mutating it between openings is how an
	// application keeps one menu up to date.
	Rc<MenuSource> source;

	// A menu built per request. Both may be set; the builder wins, and the fixed source is the
	// fallback for a builder that returns null.
	Builder builder;

	// Inflates the hit test on every side, in world units. Same idea, and same reason, as
	// DropTargetComponent::padding and InputListener::setTouchPadding.
	float padding = 0.0f;

	bool enabled = true;

	// What this target offers for `request`. Public because that is the whole seam: a test asks it
	// directly, with no pointer and no window.
	Rc<MenuSource> resolve(const ContextMenuRequest &) const;
};

/** The context-menu coordinator. One per scene, on the SceneContent.

    auto menus = ui::ContextMenuSystem::acquireForNode(this);
    menus->setMenuConfigCallback([](ui::MenuConfig &config) {
        config.stylesheetSource = String(s_css);
    });

WHERE IT LIVES, and how it is reached: exactly as DragSystem. On `SceneContent`, put there by
`acquireForNode` if nobody did, and reached by `findForNode`, which walks the parent chain. Targets
do not come through here at all - a node publishes itself into the window's hit-test registry and
this system reads that registry, so there is no roster here to keep in step with the scene.

THE INPUT IS TWO LISTENERS, one at each end of the dispatcher's walk, because opening a menu and
closing one are opposite claims on the same press:

- OPENING is asked LAST. It sits in the post-scene band (negative priority), after every widget
  under the pointer has had its chance, and it swallows nothing;
- CLOSING is asked FIRST, in the pre-scene band, and swallows the press outright. A click that
  dismisses a menu is spent on the dismissal and reaches nothing else; the next one is an ordinary
  click. It is enabled only while a menu is up, so it registers nothing the rest of the time.

WHAT "IF NOBODY ELSE TOOK THE PRESS" REALLY MEANS. The dispatcher does not record whether an event
was handled - `InputEventState::Processed` does not even stop the walk - so that question cannot be
asked. It is answered by two mechanisms instead, and a widget that wants the right button to itself
must use one of them:

- CAPTURE. A listener that swallows the press (`setSwallowEvent(InputEventName::Begin)`) or takes
  the pointer exclusively - which is what DragSource does when a drag starts - turns this
  listener's event into a Cancel, and the menu does not open. That is the mechanism for "this
  gesture is mine";
- AN EMPTY TARGET. A node whose ContextMenuComponent offers nothing blocks the menu of the
  region it sits in. That is the mechanism for "this widget has no menu of its own either".

A plain Processed is deliberately invisible: ui::Button already answers a right tap and does not
swallow it, and a button inside a panel must not stop that panel's menu from opening.

THE MOUSE OPENS ON RELEASE. A tap recognizer, not a press one: press-then-release without moving
past the tap tolerance. That is what desktop applications do, and it is what leaves a right-button
DRAG possible - ui::CanvasView pans with one. */
class SP_PUBLIC ContextMenuSystem : public System {
public:
	static uint64_t Id;

	/* Deeply negative, so this is asked after every widget in the scene; SceneContent's own listener
	sits at -1. Between the tooltip's dismiss listener (-0x2000) and the drag's cursor layer
	(-0x4000): a menu decides later than a hint, and earlier than a layer that only paints a
	cursor. */
	static constexpr int32_t ListenerPriority = -0x3000;

	/* And the other end of the walk, for the listener that takes a menu down.

	Positive, so it lands in the PRE-scene band and is asked before any widget: a click that
	dismisses a menu must be spent on the dismissal and reach nothing else. Being asked last - which
	is right for opening a menu - would be useless for that, because by then the widget under the
	pointer has already acted on the very press that was meant to close the menu. */
	static constexpr int32_t DismissListenerPriority = 0x2000;

	// How long a finger must rest before the menu opens. Above the platform tap timings and below
	// the point where the user assumes nothing will happen.
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

	// How many context-menu targets the committed frame registered. Answered by the hit-test
	// registry, which is the only place that knows: this system keeps no roster of its own.
	size_t getTargetCount() const;

	/* Fills in what an application wants every context menu in this scene to look like: the
	stylesheet above all - a native popup is a scene of its own and does not inherit the parent
	window's ui::StyleSystem - plus the style, the id prefix and any callback of its own.

	Called for every opening, on a config this system has already filled with its own onActivate and
	onClose. Replacing those is allowed and is how an application learns what was chosen without
	going through each item's callback. */
	void setMenuConfigCallback(Function<void(MenuConfig &)> &&);

	void setLongPressInterval(TimeInterval value) { _longPress = value; }
	TimeInterval getLongPressInterval() const { return _longPress; }

	// Turns the input off without removing anything. `openAt` still works: a disabled system is one
	// the user cannot summon a menu from, not one that has no menus.
	virtual void setEnabled(bool) override;

	/* Open the menu for whatever is under `worldLocation` - the very call the pointer makes, minus
	the pointer.

	False when no target offered anything, which includes "the topmost target refused" - it does not
	then try the one below. */
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
	// The topmost node offering a menu at that point, or null when nothing was under it at all.
	// What it OFFERS is a separate question - a target that answers with nothing is a refusal, and
	// the search stops at it either way.
	Node *findTarget(const Vec2 &worldLocation) const;

	AppWindow *getAppWindow() const;

	// The window's input dispatcher, which owns the hit-test registry. Null outside a scene
	InputDispatcher *getDispatcher() const;

	// Puts the listeners on the owner once the owner is running - which is later than when they
	// are added. See the implementation: this is the one thing about this class that is not
	// obvious.
	void attachListener();

	// The dismiss listener answers only while a menu is up; this is the one place that decides it.
	void updateDismissListener();

	Rc<InputListener> _listener;
	Rc<InputListener> _dismissListener;
	Rc<SubWindow> _menu;
	Rc<Node> _currentTarget;

	Function<void(MenuConfig &)> _configCallback;
	TimeInterval _longPress = DefaultLongPressInterval;

	// Which opening the handle belongs to. See openAt: a close arriving after the next menu is up
	// must not take the new one's handle with it
	uint64_t _generation = 0;
};

// Adds (or replaces) a context menu on `node`, and makes sure the scene has a coordinator. The
// short way in: one call instead of a target plus an acquire.
SP_PUBLIC const ContextMenuComponent *setContextMenu(NotNull<Node>, Rc<MenuSource> &&);
SP_PUBLIC const ContextMenuComponent *setContextMenu(NotNull<Node>,
		ContextMenuComponent::Builder &&);

SP_PUBLIC const ContextMenuComponent *getContextMenu(NotNull<Node>);
SP_PUBLIC void setContextMenuEnabled(NotNull<Node>, bool);
SP_PUBLIC void removeContextMenu(NotNull<Node>);

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_MENU_XLUICONTEXTMENU_H_
