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

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class InputDispatcher;

}

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

class TooltipSystem;
struct TooltipInfo;

// Where the hint hangs off.
enum class TooltipAnchorMode {
	// The hovered node's own world rect, independent of where the pointer entered.
	NodeRect,

	// The pointer, as of the moment the delay elapsed. For a node whose hint describes what is
	// under the pointer (a canvas, a map, a chart).
	Pointer,
};

// Which materialization to ask SubWindow for.
enum class TooltipMode {
	// An in-scene overlay on the parent's SceneContent2d. The default: a native tip costs a
	// swapchain and takes the pointer away from the node, firing the leave that hides it at once.
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

	// The point on the anchor rect the hint attaches to. Y-down, like WindowPlacement: `Bottom` is
	// the node's lower edge on screen.
	WindowAnchor anchor = WindowAnchor::Bottom;

	// Which edge of the hint lands on that point, not the opening direction: `Top` puts the hint's
	// top edge at the anchor, so it hangs below. See originForGravity in SPRuntimeWindowSupport.cc.
	WindowAnchor gravity = WindowAnchor::Top;

	// Y-down as well: positive y pushes the hint further down, away from a node it sits under.
	IVec2 offset = IVec2{0, 8};

	// FlipY first: a hint under a node near the bottom edge goes above it rather than sliding over
	// the node.
	WindowPlacementAdjustment adjustment = WindowPlacementAdjustment::FlipY
			| WindowPlacementAdjustment::SlideX | WindowPlacementAdjustment::SlideY;
};

// What a factory is handed.
struct SP_PUBLIC TooltipRequest {
	// The hovered node.
	Node *target = nullptr;

	// What the node declared. Never null while the factory runs; a copy taken when the hint was
	// built, not a pointer into a widget that may be gone on the native path.
	const TooltipInfo *info = nullptr;

	StringView text;

	// Never null; an empty Value when the target declared none.
	const Value *data = nullptr;

	// The target's world rect when the delay elapsed, in scene space (physical pixels, the space a
	// factory's nodes use). Not WindowPlacement space; see ui::placementAnchorRect.
	Rect nodeWorldRect;

	// Pointer position, same moment and the same scene space.
	Vec2 pointer;

	// The extent the surface was opened with; content built larger is clipped on the native path.
	Extent2 size;
};

// Builds the hint's content, on both materializations.
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

	// Inflates the hover test, like InputListener::setTouchPadding, for thin targets.
	float hoverPadding = 0.0f;
};

/** Declares that a node has a hint, and carries what that hint is.

    ui::setTooltip(node, "Save the document");

    ui::setTooltip(node, ui::TooltipInfo{
        .text = "Save the document",
        .factory = [](NotNull<SubWindow>, const TooltipRequest &req) { ... },
    });

The node publishes itself into the frame's hit-test registry (see HitTestFlags) and one listener
on the scene resolves which target the pointer rests on. The hover delay belongs to TooltipSystem
and has no per-node override.

App-thread only. */
struct SP_PUBLIC TooltipComponent {
	static ComponentId Id;

	TooltipInfo info;

	// A disabled hint is not found, for widgets that carry a hint only in some states.
	bool enabled = true;
};

// Attaches a hint to `node`, or replaces the one it has, and marks the node as a participant in the
// hit-test registry. The only supported way in: the flag caches this component's presence.
SP_PUBLIC const TooltipComponent *setTooltip(NotNull<Node>, TooltipInfo &&);
SP_PUBLIC const TooltipComponent *setTooltip(NotNull<Node>, StringView text);

SP_PUBLIC const TooltipComponent *getTooltip(NotNull<Node>);

// Changing the text of a hint that is currently up rebuilds it in place.
SP_PUBLIC void setTooltipText(NotNull<Node>, StringView);
SP_PUBLIC void setTooltipEnabled(NotNull<Node>, bool);

SP_PUBLIC void removeTooltip(NotNull<Node>);

// How the scene's hints behave. Everything here is a default a TooltipComponent may override,
// except the delay.
struct SP_PUBLIC TooltipConfig {
	using WindowCreationFlags = sprt::window::WindowCreationFlags;

	// How long the pointer must rest on a target. Every move within the node restarts it (dwell
	// time, not time since entry).
	TimeInterval hoverDelay = TimeInterval::milliseconds(600);

	// How long the hint stays once shown. Zero means until something takes it down (a leave, a
	// popup, the scene going away); see hideOnLeave.
	TimeInterval hideDelay = TimeInterval();

	TooltipPlacement placement;

	TooltipMode mode = TooltipMode::Overlay;

	// --- the subwindow block ---

	// Used when neither the target nor the default measurement produced one.
	Extent2 defaultSize = Extent2(160, 34);
	Extent2 minExtent = Extent2::ZERO;

	// Zero per dimension means unconstrained.
	Extent2 maxExtent = Extent2(360, 0);

	WindowCreationFlags flags = WindowCreationFlags::None;

	// Seeds the generated window id, for logs only.
	String idPrefix = String("tooltip");
	String title = String("Tip");

	// The pointer leaving the target hides the hint.
	//
	// Ignored under TooltipMode::Native: a native tip takes the pointer off the parent window, so
	// the target reports a false leave. There a zero hideDelay is replaced with
	// SubWindowSession::DefaultHideDelay.
	bool hideOnLeave = true;

	// A press or a keystroke anywhere hides the hint.
	bool hideOnInput = true;
};

/** The scene's hint coordinator: one hover delay, one hint, one place to configure both.

	auto *tips = TooltipSystem::acquireForNode(node);
	tips->setHoverDelay(TimeInterval::milliseconds(400));

Lives on SceneContent; acquireForNode installs it if missing. Found by walking the parent chain,
since callers run outside a visit.

The delay is a running action (tracked by tag), which keeps Director::hasActiveInteractions() true
so the delay elapses in an app that renders on demand. The single tip slot per window is owned by
SubWindowSession, which drops it when a popup opens; this system is a client of that slot.

App-thread only. */
class SP_PUBLIC TooltipSystem : public System {
public:
	static uint64_t Id;

	// The dwell action on the owner.
	static constexpr uint32_t DelayActionTag = "XLUiTooltipDelay"_tag;

	// The hover listener's priority: post-scene, since it only watches. It consumes nothing.
	static constexpr int32_t HoverListenerPriority = -0x1F00;

	// In the dispatcher's post-scene band, after every widget had its chance. It swallows nothing.
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
	virtual void handleRemoved() override;
	virtual void handleEnter(Scene *) override;
	virtual void handleExit() override;
	virtual void handleVisitBegin(FrameInfo &) override;

	// One hit-test query per frame while the scene has any hint: notices a node sliding out from
	// under a still pointer (a scrolling list, an animating panel).
	virtual void update(const UpdateTime &) override;

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
	virtual bool showFor(NotNull<Node>, Vec2 pointerWorld);

	// Take the hint down and cancel any delay in flight. Idempotent.
	virtual void hide();

	bool isVisible() const;

	// The node's hint changed (text, factory, or the component removed). Rebuilds a hint currently
	// up for it; otherwise a no-op. Public because the component setters are free functions.
	void handleNodeChanged(NotNull<Node>);

	// The node whose hint is up, or null.
	Node *getCurrentTarget() const { return _shown; }

	// The node the delay is running for, or null.
	Node *getPendingTarget() const { return _pending; }

	// The node the pointer is resting on, hint or no hint yet. Null when it is on none.
	Node *getHoveredTarget() const { return _hovered; }

	InputListener *getHoverListener() const { return _hoverListener; }

protected:
	/* Which node the pointer is resting on, from the frame's hit-test registry. `fromMove`: a real
	move restarts the dwell; a per-frame re-resolution must not, or a hint never appears in a scene
	that keeps drawing. */
	void resolveHover(const Vec2 &pointerWorld, bool fromMove);

	// Pointer entered the node, or moved within it. Both restart the dwell.
	void handleTargetHover(NotNull<Node>, Vec2 pointerWorld);
	void handleTargetLeave(NotNull<Node>);

	// The node is leaving the scene, or has stopped offering a hint.
	void handleTargetGone(NotNull<Node>);


	void armDelay();
	void cancelDelay();

	// The dwell elapsed. Builds and opens.
	void fire();

	bool present(NotNull<Node>, Vec2 pointerWorld);

	const TooltipPlacement &placementFor(NotNull<Node>) const;
	sprt::window::WindowPlacement makePlacement(const TooltipRequest &,
			const TooltipPlacement &) const;

	Rect getTargetWorldRect(NotNull<Node>) const;

	InputDispatcher *getDispatcher() const;

	// One listener on the owner for all hinted nodes, with only a move recognizer: it reports where
	// the pointer went and decides nothing.
	void updateHoverListener();

	core::RenderServerChannel *getWindow() const;
	SubWindowSession *getSession() const;
	void updateDismissListener();

	TooltipConfig _config;
	TooltipFactory _defaultFactory;

	// Held strongly: a component cannot report the node's exit, so a raw pointer could dangle.
	// `fire` then finds a detached node not running and declines.
	Rc<Node> _pending;
	Rc<Node> _shown;
	Rc<Node> _hovered;

	Vec2 _pointer;
	bool _hasPointer = false;

	Rc<SubWindow> _tip;
	Rc<InputListener> _dismissListener;
	Rc<InputListener> _hoverListener;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_XLUITOOLTIPSYSTEM_H_
