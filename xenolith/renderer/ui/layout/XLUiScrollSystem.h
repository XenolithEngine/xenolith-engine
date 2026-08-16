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

#ifndef XENOLITH_RENDERER_UI_LAYOUT_XLUISCROLLSYSTEM_H_
#define XENOLITH_RENDERER_UI_LAYOUT_XLUISCROLLSYSTEM_H_

#include "XLUiLayoutSystem.h"
#include "XLInputListener.h"
#include "XLDynamicStateSystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

/** Makes its owner a CSS scroll container.

Installed by ui::StyleResolver for any node whose `overflow-x` / `overflow-y` is not `visible`
(see OverflowComponent). Two modes, one system:

- `hidden` / `clip`: the box is clipped to a scissor rect and nothing else happens;
- `scroll` / `auto`: the LayoutSystem below lays the content out at its natural size, this system
  clips the box, slides the content inside it, and shows an overlay indicator.

It never writes a ContentSize - neither its own nor a child's. The LayoutSystem stays the sole
writer (see the ContentSize ownership rule in XLUiStyleResolver.cc); all this does is hand it a
translation to replay.

There is NO virtualization here: every child of a scroll container is a real node, always. A long
list belongs in ui::TreeView / ui::TableView, which build only the rows in the window.

It IS the InputListener rather than owning one, following ui::FormInputListener and DragSource.
That is not only tidier: Node::removeSystem calls handleRemoved() while holding an iterator into
the owner's system list, so a system that removed a second system of its own from there would
invalidate that iterator underneath the caller. */
class SP_PUBLIC ScrollSystem : public InputListener {
public:
	// Runs AFTER LayoutSystem in the layout-children phase, so it reads the extent that pass has
	// just produced. Systems are dispatched in ASCENDING priority order, so "after" is "+".
	static constexpr uint32_t ScrollDefaultPriority = LayoutSystem::LayoutDefaultPriority + 10;

	// When the indicator is shown. `Auto` follows the CSS value: an `overflow: scroll` axis with a
	// range keeps it, an `auto` axis fades it out when idle.
	enum class IndicatorMode {
		Auto,
		Always,
		Never,
	};

	virtual ~ScrollSystem() = default;

	// Takes the resolved overflow rather than defaulting to it, so the signature stays distinct from
	// InputListener::init(int32_t priority = 0). A zero-argument override would be hidden by that
	// one and silently never run - the same reason FormInputListener and DragSource take arguments.
	virtual bool init(document::Overflow x, document::Overflow y);

	virtual void handleAdded(Node *) override;
	virtual void handleRemoved() override;
	virtual void handleTransformDirty(const Mat4 &) override;
	virtual void handleLayoutChildren() override;
	virtual void update(const UpdateTime &) override;

	// Assign the resolved overflow. Idempotent; safe to call every style pass.
	void setOverflow(document::Overflow x, document::Overflow y);
	document::Overflow getOverflowX() const { return _overflowX; }
	document::Overflow getOverflowY() const { return _overflowY; }

	bool clipsX() const { return _overflowX != document::Overflow::Visible; }
	bool clipsY() const { return _overflowY != document::Overflow::Visible; }
	bool scrollsX() const {
		return _overflowX == document::Overflow::Scroll || _overflowX == document::Overflow::Auto;
	}
	bool scrollsY() const {
		return _overflowY == document::Overflow::Scroll || _overflowY == document::Overflow::Auto;
	}

	// Maximum scroll offset per axis, i.e. max(0, contentExtent - box). Zero on an axis that does
	// not scroll, and zero while the content fits.
	Size2 getScrollRange() const { return _range; }

	// Current offset, in CSS scroll orientation: x grows right, y grows DOWN.
	Vec2 getScrollPosition() const { return Vec2(float(_scrollX), float(_scrollY)); }
	void setScrollPosition(Vec2);
	void scrollBy(Vec2 delta);

	// Bring a descendant's box inside the scrollport, moving as little as possible. Returns false
	// when `node` is not a descendant of the owner.
	bool scrollNodeIntoView(NotNull<Node>, Padding = Padding());

	void setIndicatorMode(IndicatorMode);
	IndicatorMode getIndicatorMode() const { return _indicatorMode; }

	void setScrollCallback(Function<void(Vec2)> &&);

	// Where a wheel animation is heading, or the current position when none is running. Reading it
	// is how a second notch adds to the first instead of restarting from wherever the easing is.
	Vec2 getScrollTarget() const;

protected:
	using InputListener::init;

	// Tag of the wheel easing action on the owner. Using a tag rather than an Rc means "is one
	// running?" is always asked of the node itself, so a finished action can never leave a stale
	// pointer behind.
	static constexpr uint32_t WheelActionTag = 0x5C401101;

	// Ease to `target` over ScrollSystem_wheelDuration, replacing any easing already in flight.
	void scrollToAnimated(Vec2 target);

	// Commit an offset without touching the wheel easing. The easing itself drives this; every
	// other caller goes through setScrollPosition, which cancels the easing first - otherwise the
	// two would write the same offset from different directions on the same frame.
	void applyScrollPosition(Vec2);

	void updateClip();
	void updateIndicators();
	void commitOffset();
	Vec2 clampPosition(Vec2) const;
	bool handleScrollGesture(const GestureScroll &);
	bool handleSwipeGesture(const GestureSwipe &);

	// Accumulated in double, like ui::TextViewContainer: a long wheel session inside a large
	// content area accumulates visible float error otherwise.
	double _scrollX = 0.0;
	double _scrollY = 0.0;

	Size2 _range;

	// Coasting after a released touch. Zero for a mouse drag: a pointer released with the content
	// still moving under it reads as the scroller ignoring the button, and only a real touchscreen
	// says otherwise (InputModifier::Touch).
	Vec2 _velocity;

	// The destination of the wheel easing in flight. Only meaningful while an action carrying
	// WheelActionTag is running on the owner.
	Vec2 _wheelTarget;

	// World scale of the owner, recomputed on every transform change. Wheel and drag deltas arrive
	// in screen units and must be divided by it, or a scroller inside a scaled subtree moves at the
	// wrong rate (the same divisor basic2d::ScrollViewBase keeps as _globalScale).
	Vec2 _worldScale = Vec2(1.0f, 1.0f);

	document::Overflow _overflowX = document::Overflow::Visible;
	document::Overflow _overflowY = document::Overflow::Visible;

	IndicatorMode _indicatorMode = IndicatorMode::Auto;
	float _indicatorOpacity = 0.0f;
	float _indicatorIdle = 0.0f;

	DynamicStateSystem *_scissor = nullptr;
	Node *_indicatorV = nullptr;
	Node *_indicatorH = nullptr;

	Function<void(Vec2)> _scrollCallback;
};

// Walk the ancestor chain from `node` upward and ask every ScrollSystem on it to reveal `node`.
// The whole chain, outermost included - that is what makes a widget nested two scrollers deep
// actually become visible. This is the focus-follow entry point.
SP_PUBLIC void scrollIntoView(NotNull<Node>, Padding = Padding());

} // namespace stappler::xenolith::ui

#endif /* XENOLITH_RENDERER_UI_LAYOUT_XLUISCROLLSYSTEM_H_ */
