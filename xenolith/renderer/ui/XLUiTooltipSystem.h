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

#ifndef XENOLITH_RENDERER_UI_XLUITOOLTIPSYSTEM_H_
#define XENOLITH_RENDERER_UI_XLUITOOLTIPSYSTEM_H_

#include "XLUiSubWindow.h"
#include "XLUiSubWindowSession.h"
#include "XLInputListener.h"

#include <sprt/cxx/optional>

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

class TooltipSystem;
class TooltipTarget;

// Where the hint hangs off.
enum class TooltipAnchorMode {
	// The hovered node's own world rect. The hint lands in the same place however the pointer
	// wandered in, which is what a hint describing a WIDGET should do.
	NodeRect,

	// The pointer, as of the moment the delay elapsed. For a node that is not one thing - a canvas,
	// a map, a chart - where the hint describes what is UNDER the pointer, not the node.
	Pointer,
};

// Which materialization to ask SubWindow for.
enum class TooltipMode {
	// An in-scene overlay on the parent's SceneContent2d. The default, and not merely the portable
	// choice: a native tip costs a swapchain for a few hundred milliseconds of hint and takes the
	// pointer away from the node it describes, which turns the leave that should hide it into a
	// leave that fires immediately.
	Overlay,

	// A real subwindow where the platform advertises WindowCapabilities::Subwindows, an overlay
	// where it does not. Read the note on TooltipConfig::hideOnLeave before choosing this.
	Native,
};

// Where the hint opens relative to its anchor. Lays 1:1 into sprt::window::WindowPlacement, and is
// resolved by the same computeWindowPlacement both materializations use.
struct SP_PUBLIC TooltipPlacement {
	using WindowAnchor = sprt::window::WindowAnchor;
	using WindowPlacementAdjustment = sprt::window::WindowPlacementAdjustment;

	TooltipAnchorMode anchorMode = TooltipAnchorMode::NodeRect;

	// The point ON the anchor rect the hint attaches to. Y-DOWN, like the rest of WindowPlacement:
	// `Bottom` is the node's lower edge on screen.
	WindowAnchor anchor = WindowAnchor::Bottom;

	// WHICH EDGE OF THE HINT lands on that point - NOT the direction it opens, which is the reading
	// the name invites. `Top` puts the hint's top edge at the anchor, so the hint hangs BELOW it;
	// `Bottom` would put the hint above. See originForGravity in SPRuntimeWindowSupport.cc.
	WindowAnchor gravity = WindowAnchor::Top;

	// Y-down as well: positive y pushes the hint further down, away from a node it sits under.
	IVec2 offset = IVec2{0, 8};

	// FlipY first: a hint under a node near the bottom edge belongs above it, not slid up over the
	// node it describes.
	WindowPlacementAdjustment adjustment = WindowPlacementAdjustment::FlipY
			| WindowPlacementAdjustment::SlideX | WindowPlacementAdjustment::SlideY;
};

// What a factory is handed. Everything the hint could want to know about why it is being built.
struct SP_PUBLIC TooltipRequest {
	// The hovered node.
	Node *target = nullptr;

	// The target that declared the hint, so a factory shared by many nodes can read back whatever
	// it put there.
	TooltipTarget *source = nullptr;

	StringView text;

	// Never null; an empty Value when the target declared none.
	const Value *data = nullptr;

	// The target's world rect, as of the moment the delay elapsed.
	Rect nodeWorldRect;

	// World pointer position, same moment.
	Vec2 pointer;

	// The extent the surface was opened with. A factory that builds to a different size will be
	// clipped on the native path, so build to this.
	Extent2 size;
};

// Builds the hint's content. Runs on BOTH materializations, which is what keeps a caller portable.
using TooltipFactory =
		Function<Rc<basic2d::SceneLayout2d>(NotNull<SubWindow>, const TooltipRequest &)>;

// What a node declares about its hint.
struct SP_PUBLIC TooltipInfo {
	String text;

	// Free-form payload for a factory of your own. Ignored by the default one.
	Value data;

	// This node's own factory. Unset falls back to TooltipSystem::getDefaultFactory().
	TooltipFactory factory;

	// This node's own placement. Unset falls back to TooltipSystem's.
	sprt::optional<TooltipPlacement> placement;

	// Zero asks the system to size the hint (by measuring `text` for the default factory). A
	// factory building something other than a line of text must set this.
	Extent2 size = Extent2::ZERO;

	// Inflates the hover test, the same idea as InputListener::setTouchPadding: a thin target is
	// hard to rest a pointer on.
	float hoverPadding = 0.0f;
};

/** Declares that a node has a hint, and carries what that hint is.

	node->addSystem(Rc<TooltipTarget>::create("Save the document"));

	node->addSystem(Rc<TooltipTarget>::create(TooltipInfo{
		.text = "Save the document",
		.factory = [](NotNull<SubWindow>, const TooltipRequest &req) { ... },
	}));

IT IS AN InputListener, not a System owning one. A System cannot add a sibling system from its own
handleAdded/handleRemoved - Node::removeSystem runs those while holding an iterator into the very
list it would mutate. ScrollSystem solved this the same way, and the same choice pays a second time
here: InputListener::settlePointerState() already answers "is the pointer over me" with no event in
hand, so a target attached while the pointer is ALREADY resting on the node arms its delay instead
of waiting for the next move.

THE DELAY IS NOT HERE. This reports hover; TooltipSystem decides how long a pointer must rest. One
node's hint appearing sooner than its neighbour's is a bug, not a feature, so there is deliberately
no per-target override.

App-thread only. */
class SP_PUBLIC TooltipTarget : public InputListener {
public:
	virtual ~TooltipTarget() = default;

	// Both take arguments on purpose: InputListener::init(int32_t priority = 0) would HIDE a
	// zero-argument override, which then silently never runs. Same reason ScrollSystem,
	// FormInputListener and DragSource all take arguments.
	virtual bool init(TooltipInfo &&);
	virtual bool init(StringView text);

	virtual void handleExit() override;

	virtual void setInfo(TooltipInfo &&);
	const TooltipInfo &getInfo() const { return _info; }

	// Changing the text of a hint that is currently UP rebuilds it in place.
	virtual void setText(StringView);
	StringView getText() const { return _info.text; }

	virtual void setFactory(TooltipFactory &&);

	virtual void setPlacement(const TooltipPlacement &);
	virtual void clearPlacement();
	const sprt::optional<TooltipPlacement> &getPlacement() const { return _info.placement; }

	// True while the pointer is resting on the owner - which is not the same as the hint being up,
	// the delay may still be running.
	bool isHovered() const { return _hovered; }

	// The system this target reports to, or null before its first hover.
	TooltipSystem *getSystem() const { return _system; }

protected:
	using InputListener::init;

	// Shared tail of both init overloads.
	bool setup();

	// Walks the parent chain and installs a system if there is none. Deferred to the first hover
	// rather than done in handleAdded: the parent chain is not complete there, and the hover
	// callbacks that need it all run OUTSIDE a visit anyway. Same split as DragSystem's
	// findForNode vs. frame-stack registration.
	TooltipSystem *acquireSystem();

	TooltipInfo _info;

	// Raw: the system lives on the SceneContent above us and outlives every target under it.
	// Cleared in handleExit, which is also where we tell it we are going.
	TooltipSystem *_system = nullptr;
	bool _hovered = false;
};

// How the scene's hints behave. Everything here is a default a TooltipTarget may override, except
// the delay - see the note on TooltipTarget.
struct SP_PUBLIC TooltipConfig {
	using WindowCreationFlags = sprt::window::WindowCreationFlags;

	// How long the pointer must REST on a target. Every move within the node restarts it, so this
	// is dwell time, not time-since-entry.
	TimeInterval hoverDelay = TimeInterval::milliseconds(600);

	// How long the hint stays once shown. Zero means "until something takes it down" - a leave,
	// a popup, the scene going away. See the hideOnLeave note for when zero is a bad idea.
	TimeInterval hideDelay = TimeInterval();

	TooltipPlacement placement;

	TooltipMode mode = TooltipMode::Overlay;

	// --- the subwindow block ---

	// Used when neither the target nor the default measurement produced one.
	Extent2 defaultSize = Extent2(160, 34);
	Extent2 minExtent = Extent2::ZERO;

	// A hint is a hint: a width that runs to the window edge is a paragraph. Zero per dimension
	// means unconstrained, as everywhere else.
	Extent2 maxExtent = Extent2(360, 0);

	WindowCreationFlags flags = WindowCreationFlags::None;

	// Seeds the generated window id, for logs only.
	String idPrefix = String("tooltip");
	String title = String("Tip");

	// The pointer leaving the target hides the hint.
	//
	// IGNORED under TooltipMode::Native, and not as a simplification: a native tip takes the
	// pointer off the parent window, WindowState::Pointer drops, and the target reports a leave it
	// never had. There the hide timer is the only honest closer, so a Native config with a zero
	// hideDelay is given SubWindowSession::DefaultHideDelay rather than a hint that never goes away.
	bool hideOnLeave = true;

	// A press or a keystroke anywhere hides the hint. A hint is for a pointer at rest; the moment
	// the user does something they have stopped reading it.
	bool hideOnInput = true;
};

/** The scene's hint coordinator: one hover delay, one hint, one place to configure both.

	auto *tips = TooltipSystem::acquireForNode(node);
	tips->setHoverDelay(TimeInterval::milliseconds(400));

WHERE IT LIVES. On SceneContent, and acquireForNode puts it there if nobody did - so a widget can
carry a hint without the application having arranged anything. Found by walking the parent chain,
because everything that reaches it runs outside a visit.

WHY THE DELAY IS AN ACTION. Not just because dwell is naturally expressed as "start a timer, restart
it on every move". A running action makes Director::hasActiveInteractions() true, so the frame loop
stays awake and the delay actually elapses in an app that renders on demand. A looper timer would
fire on a thread that has stopped drawing. The action is tracked by TAG rather than by Rc, so "is
one running?" is always asked of the node and a finished action leaves nothing stale behind.

ONE HINT AT A TIME is not enforced here but by SubWindowSession, which owns the window's single tip
slot and drops it when a popup opens. This system is a client of that slot, not a second owner.

App-thread only. */
class SP_PUBLIC TooltipSystem : public System {
public:
	static uint64_t Id;

	// The dwell action on the owner.
	static constexpr uint32_t DelayActionTag = "XLUiTooltipDelay"_tag;

	// Deeply negative so the dismiss listener sits in the dispatcher's post-scene band and sees
	// what every widget already had its chance at. It swallows nothing.
	static constexpr int32_t DismissListenerPriority = -0x2000;

	// The nearest TooltipSystem at or above `node`.
	static TooltipSystem *findForNode(Node *);

	// findForNode, and if there is none, installs one on the scene's content node.
	static TooltipSystem *acquireForNode(Node *);

	// The stock hint: a Panel and a Label, typed and classed so a stylesheet can restyle it.
	static Rc<basic2d::SceneLayout2d> buildDefaultTooltip(NotNull<SubWindow>,
			const TooltipRequest &);

	// What the stock hint needs for `text`, clamped into the config's extents.
	static Extent2 measureDefaultTooltip(StringView text, const TooltipConfig &);

	virtual ~TooltipSystem() = default;

	virtual bool init() override;

	virtual void handleAdded(Node *) override;
	virtual void handleExit() override;

	virtual void setConfig(const TooltipConfig &);
	const TooltipConfig &getConfig() const { return _config; }

	// Shorthands for the fields worth changing on their own.
	virtual void setHoverDelay(TimeInterval);
	TimeInterval getHoverDelay() const { return _config.hoverDelay; }
	virtual void setPlacement(const TooltipPlacement &);
	const TooltipPlacement &getPlacement() const { return _config.placement; }
	virtual void setMode(TooltipMode);
	TooltipMode getMode() const { return _config.mode; }

	// The factory for targets that carry none. Setting an empty one restores the stock hint.
	virtual void setDefaultFactory(TooltipFactory &&);
	const TooltipFactory &getDefaultFactory() const { return _defaultFactory; }

	// Show `target`'s hint right now, skipping the delay. `pointerWorld` only matters under
	// TooltipAnchorMode::Pointer.
	virtual bool showFor(NotNull<TooltipTarget>, Vec2 pointerWorld);

	// Take the hint down and cancel any delay in flight. Idempotent.
	virtual void hide();

	bool isVisible() const;

	// The target whose hint is up, or null.
	TooltipTarget *getCurrentTarget() const { return _shown; }

	// The target the delay is running for, or null.
	TooltipTarget *getPendingTarget() const { return _pending; }

protected:
	friend class TooltipTarget;

	// --- called by TooltipTarget ---

	// Pointer entered the target, or moved within it. Both restart the dwell.
	void handleTargetHover(NotNull<TooltipTarget>, Vec2 pointerWorld);
	void handleTargetLeave(NotNull<TooltipTarget>);

	// The target is leaving the scene, whether or not it was hovered.
	void handleTargetGone(NotNull<TooltipTarget>);

	// The target's content changed under a hint that is already up.
	void handleTargetChanged(NotNull<TooltipTarget>);

	void armDelay();
	void cancelDelay();

	// The dwell elapsed. Builds and opens.
	void fire();

	bool present(NotNull<TooltipTarget>, Vec2 pointerWorld);

	const TooltipPlacement &placementFor(NotNull<TooltipTarget>) const;
	sprt::window::WindowPlacement makePlacement(const TooltipRequest &,
			const TooltipPlacement &) const;

	Rect getTargetWorldRect(NotNull<TooltipTarget>) const;

	AppWindow *getWindow() const;
	SubWindowSession *getSession() const;
	void updateDismissListener();

	TooltipConfig _config;
	TooltipFactory _defaultFactory;

	// Raw: a target lives under this node, and tells us on the way out. An Rc here would keep a
	// removed widget alive for as long as a hint that was never shown.
	TooltipTarget *_pending = nullptr;
	TooltipTarget *_shown = nullptr;

	Vec2 _pointer;
	Rc<SubWindow> _tip;
	Rc<InputListener> _dismissListener;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_XLUITOOLTIPSYSTEM_H_
