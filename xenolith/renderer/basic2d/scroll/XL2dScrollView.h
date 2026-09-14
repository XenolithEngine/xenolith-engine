/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#ifndef XENOLITH_RENDERER_BASIC2D_SCROLL_XL2DSCROLLVIEW_H_
#define XENOLITH_RENDERER_BASIC2D_SCROLL_XL2DSCROLLVIEW_H_

#include "XL2dScrollViewBase.h"
#include "XL2dScrollController.h"
#include "XL2dVectorSprite.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

class LayerRounded;

class SP_PUBLIC ScrollView : public ScrollViewBase {
public:
	using TapCallback = Function<void(int count, const Vec2 &loc)>;
	using AnimationCallback = Function<void()>;

	class Overscroll : public VectorSprite {
	public:
		enum Direction {
			Top,
			Left,
			Bottom,
			Right
		};

		static constexpr float OverscrollEdge = 0.075f;
		static constexpr float OverscrollEdgeThreshold = 0.5f;
		static constexpr float OverscrollScale = 1.0f / 6.0f;
		static constexpr float OverscrollMaxHeight = 64.0f;

		virtual ~Overscroll() { }

		virtual bool init() override;
		virtual bool init(Direction);
		virtual void handleContentSizeDirty() override;
		virtual void update(const UpdateTime &time) override;
		virtual void handleEnter(Scene *) override;
		virtual void handleExit() override;

		virtual void setDirection(Direction);
		virtual Direction getDirection() const;

		void setProgress(float p);
		void incrementProgress(float dt);
		void decrementProgress(float dt);

	protected:
		using VectorSprite::init;

		void updateProgress(VectorImage *);

		bool _progressDirty = false;
		float _progress = 0.0f;
		uint64_t _delayStart = 0;
		Direction _direction = Direction::Top;
	};

	virtual ~ScrollView() = default;

	virtual bool init(Layout l) override;

	virtual void handleContentSizeDirty() override;
	virtual void handleEnter(Scene *) override;

	virtual void handleLayoutChildren() override;

	virtual void setOverscrollColor(const Color4F &, bool withOpacity = false);
	virtual Color4F getOverscrollColor() const;

	virtual void setOverscrollVisible(bool value);
	virtual bool isOverscrollVisible() const;

	// Bar thickness at rest and when a pointing device can aim at it, in points; the active one is
	// used whenever the window reports WindowState::InputPointer.
	static constexpr float IndicatorThicknessIdle = 3.0f;
	static constexpr float IndicatorThicknessActive = 8.0f;

	// Distance from the edge of the view, and the shortest a thumb is allowed to get however long
	// the content is.
	static constexpr float IndicatorInset = 2.0f;
	static constexpr float IndicatorMinLength = 20.0f;

	// Action tags on the thumb: the pulse to full on motion, and the settle back afterwards.
	static constexpr uint32_t IndicatorShowActionTag = 19;
	static constexpr uint32_t IndicatorSettleActionTag = 18;

	virtual void setIndicatorColor(const Color4B &, bool withOpacity = false);
	virtual Color4F getIndicatorColor() const;

	virtual void setIndicatorVisible(bool value);
	virtual bool isIndicatorVisible() const;

	/* Whether the bar settles back to invisible after the scroll stops. Auto (default) follows the
	attached devices: with a pointing device the bar stays, without one it fades like a touch
	scroller. */
	enum class IndicatorFade {
		Auto,
		Never,
		Always,
	};

	virtual void setIndicatorFade(IndicatorFade);
	IndicatorFade getIndicatorFade() const { return _indicatorFade; }
	bool isIndicatorFading() const;

	// What the bar settles to when it does not fade away. 0..1
	virtual void setIndicatorOpacity(float);
	float getIndicatorOpacity() const { return _indicatorOpacity; }

	// Whether the bar can be grabbed right now: it needs a pointing device and something to scroll.
	bool isIndicatorInteractive() const;

	virtual void setPadding(const Padding &) override;

	virtual void setOverscrollFrontOffset(float value);
	virtual float getOverscrollFrontOffset() const;

	virtual void setOverscrollBackOffset(float value);
	virtual float getOverscrollBackOffset() const;

	virtual void setIndicatorIgnorePadding(bool value);
	virtual bool isIndicatorIgnorePadding() const;

	/* The two nodes the bar is made of. Public for replacement (ui::useStyledScrollIndicator) and
	for tests that check the thumb against the scroll position. */
	Node *getIndicatorNode() const { return _indicator; }
	Node *getIndicatorTrackNode() const { return _indicatorTrack; }

	/* Replace either bar node, keeping what the view owns about it: geometry, identity, visibility,
	opacity and colour survive, the track keeps the thumb as its child and the input listener.
	Used by `ui::useStyledScrollIndicator()` to get a stylesheet-painted node. Thickness is not a
	style - `updateIndicatorPosition()` rewrites both content sizes on every scroll - so use
	setIndicatorThickness(); a sheet controls colour, radius, outline and `display: none`. */
	virtual void setIndicatorNode(Rc<Node> &&);
	virtual void setIndicatorTrackNode(Rc<Node> &&);

	virtual void setIndicatorThickness(float idle, float active);
	float getIndicatorThickness() const; // the one in force right now

	/* The strip along the scrolled edge the bar can occupy, which overlays must keep clear of: its
	inset plus the widest thickness (the bar swells with a pointing device). Zero while the
	content fits. */
	float getIndicatorReservedSize() const;

	/* Where the thumb sits along its track, 0..1, and the exact inverse of the expression
	updateIndicatorPosition() uses. Both handle the degenerate cases (no range yet, content fits)
	the same way. */
	virtual float getIndicatorRelativePosition() const;
	virtual void setIndicatorRelativePosition(float);

	virtual void setTapCallback(const TapCallback &);
	virtual const TapCallback &getTapCallback() const;

	virtual void setAnimationCallback(const AnimationCallback &);
	virtual const AnimationCallback &getAnimationCallback() const;

	virtual void update(const UpdateTime &time) override;

	enum class Adjust {
		None,
		Front,
		Back
	};

	virtual void runAdjustPosition(float pos, float factor = 1.0f);
	virtual void runAdjust(float pos, float factor = 1.0f);
	virtual void scheduleAdjust(Adjust, float value);

	virtual Value save() const;
	virtual void load(const Value &);

public:
	virtual Rc<ActionProgress> resizeNode(Node *, float newSize, float duration,
			Function<void()> &&cb = nullptr);
	virtual Rc<ActionProgress> resizeNode(ScrollController::Item *, float newSize, float duration,
			Function<void()> &&cb = nullptr);

	virtual Rc<ActionProgress> removeNode(Node *, float duration, Function<void()> &&cb = nullptr,
			bool disable = false);
	virtual Rc<ActionProgress> removeNode(ScrollController::Item *, float duration,
			Function<void()> &&cb = nullptr, bool disable = false);

protected:
	virtual void doSetScrollPosition(float pos) override;

	// The bar is recomputed whenever the scroll bounds are: a virtualized list (TreeView,
	// TableView) lays out before its items arrive via ScrollController::onScrollPosition.
	virtual void updateScrollBounds() override;

	virtual void onOverscroll(float delta) override;
	virtual void onScroll(float delta, bool finished) override;
	virtual void onTap(int count, Vec2 loc) override;
	virtual void onAnimationFinished() override;

	virtual void updateIndicatorPosition();
	virtual void updateIndicatorPosition(Node *indicator, float size, float value, bool actions,
			float min);

	// Re-apply everything that depends on the attached input devices: thickness, whether the
	// listener answers, and where the bar settles. Redraws immediately.
	virtual void updateIndicatorInteractive();

	// Whether the window reports a pointing device, from either of the two ways of learning it.
	void setIndicatorHasPointer(bool);

	// Track geometry, read back off the two nodes so the drag is the exact inverse of the
	// placement.
	float getIndicatorTravel() const;
	float getIndicatorRelativeForLocation(const Vec2 &trackLocation, float grab) const;

	virtual bool handleIndicatorDragBegin(const Vec2 &location);
	virtual void handleIndicatorDragMove(const Vec2 &location);
	virtual void handleIndicatorDragEnd();
	virtual bool handleIndicatorTap(const Vec2 &location);
	virtual void handleIndicatorHover(bool);

	virtual ScrollController::Item *getItemForNode(Node *) const;

	Overscroll *_overflowFront = nullptr;
	Overscroll *_overflowBack = nullptr;

	// The bar the user can grab, and the strip it runs in. The track carries the listener (see
	// init() in the .cc) and the thumb is its child, so they move as one.
	Node *_indicatorTrack = nullptr;
	Node *_indicator = nullptr;

	bool _indicatorVisible = true;
	bool _indicatorIgnorePadding = false;

	// Geometry of the bar, in points; fields because thickness depends on input devices and callers
	// may override it.
	float _indicatorThicknessIdle = IndicatorThicknessIdle;
	float _indicatorThicknessActive = IndicatorThicknessActive;
	float _indicatorInset = IndicatorInset;
	float _indicatorMinLength = IndicatorMinLength;

	// Whether the window reports a pointing device. Drives the thickness, whether the bar can be
	// grabbed at all, and - through IndicatorFade::Auto - whether it stays on screen.
	bool _indicatorHasPointer = false;

	// Style class both nodes carry while the bar is grabbable: the thickness is written by the
	// widget, so this is how a stylesheet rule matches the active state.
	static constexpr StringView IndicatorActiveClass = StringView("active");

	InputListener *_indicatorListener = nullptr;
	IndicatorFade _indicatorFade = IndicatorFade::Auto;

	// What the track fades to under the pointer. A widget-side default; a rule on
	// `scroll-indicator-track:hover` replaces it.
	float _indicatorTrackOpacity = 0.25f;

	// Where the pointer went down on the bar. Vec2::INVALID (NaNs) until a press is seen, so test
	// with isValid(), never ==.
	Vec2 _indicatorPress = Vec2::INVALID;
	float _indicatorGrab = 0.0f;
	bool _indicatorDragging = false;
	bool _indicatorHovered = false;

	float _overscrollFrontOffset = 0.0f;
	float _overscrollBackOffset = 0.0f;

	TapCallback _tapCallback = nullptr;
	AnimationCallback _animationCallback = nullptr;

	Adjust _adjust = Adjust::None;
	float _adjustValue = 0.0f;
	float _indicatorOpacity = 0.5f;
};

} // namespace stappler::xenolith::basic2d

#endif /* XENOLITH_RENDERER_BASIC2D_SCROLL_XL2DSCROLLVIEW_H_ */
