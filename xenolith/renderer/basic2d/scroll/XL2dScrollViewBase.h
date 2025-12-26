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

#ifndef XENOLITH_RENDERER_BASIC2D_SCROLL_XL2DSCROLLVIEWBASE_H_
#define XENOLITH_RENDERER_BASIC2D_SCROLL_XL2DSCROLLVIEWBASE_H_

#include "XL2d.h" // IWYU pragma: keep
#include "XLNode.h"
#include "XL2dActionAcceleratedMove.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

class ScrollController;
class ActionAcceleratedMove;

class SP_PUBLIC ScrollViewBase : public Node {
public:
	using ScrollFilterCallback = std::function<float(float delta)>;
	using ScrollCallback = std::function<void(float delta, bool finished)>;
	using OverscrollCallback = std::function<void(float delta)>;

	enum Layout {
		Vertical,
		Horizontal
	};

	virtual ~ScrollViewBase() = default;

	virtual bool init(Layout l);
	virtual void setLayout(Layout l);

	virtual bool visitDraw(FrameInfo &, NodeVisitFlags parentFlags) override;

	virtual void handleEnter(Scene *) override;

	inline bool isVertical() const { return _layout == Vertical; }
	inline bool isHorizontal() const { return _layout == Horizontal; }
	virtual Layout getLayout() const { return _layout; }

	virtual void handleContentSizeDirty() override;
	virtual void handleTransformDirty(const Mat4 &) override;

	virtual void setEnabled(bool value);
	virtual bool isEnabled() const;

	virtual bool isInMotion() const;
	virtual bool isMoved() const;

	virtual void setScrollCallback(const ScrollCallback &cb);
	virtual const ScrollCallback &getScrollCallback() const;

	virtual void setOverscrollCallback(const OverscrollCallback &cb);
	virtual const OverscrollCallback &getOverscrollCallback() const;

	virtual Node *getRoot() const { return _root; }
	virtual InputListener *getInputListener() const { return _inputListener; }

	virtual bool addSystemItem(System *) override;

	template <typename T>
	auto setController(const Rc<T> &ptr) -> T * {
		setController(ptr.get());
		return ptr.get();
	}

	virtual void setController(ScrollController *c);
	virtual ScrollController *getController();

	virtual void setPadding(const Padding &);
	virtual const Padding &getPadding() const;

	virtual void setSpaceLimit(float, float padding = 0.0f);
	virtual float getSpaceLimit() const;
	virtual float getSpacePadding() const;

	virtual float getScrollableAreaOffset() const; // NaN by default
	virtual float getScrollableAreaSize() const; // NaN by default

	virtual Vec2 getPositionForNode(float scrollPos) const;
	virtual Size2 getContentSizeForNode(float size) const;
	virtual Vec2 getAnchorPointForNode() const;

	virtual float getNodeScrollSize(Size2) const;
	virtual float getNodeScrollPosition(Vec2) const;

	virtual bool addScrollNode(Node *, Vec2 pos, Size2 size, ZOrder z, StringView name);
	virtual void updateScrollNode(Node *, Vec2 pos, Size2 size, ZOrder z, StringView name);
	virtual bool removeScrollNode(Node *);

	virtual float getDistanceFromStart() const;

	virtual void setScrollMaxVelocity(float value);
	virtual float getScrollMaxVelocity() const;

	virtual Node *getFrontNode() const;
	virtual Node *getBackNode() const;

	virtual Vec2 convertFromScrollableSpace(const Vec2 &);
	virtual Vec2 convertToScrollableSpace(const Vec2 &);

	virtual Vec2 convertFromScrollableSpace(Node *, Vec2);
	virtual Vec2 convertToScrollableSpace(Node *, Vec2);

	// internal, use with care
	virtual float getScrollMinPosition() const; // NaN by default
	virtual float getScrollMaxPosition() const; // NaN by default
	virtual float getScrollLength() const; // NaN by default
	virtual float getScrollSize() const;

	virtual void setScrollRelativePosition(float value);
	virtual float getScrollRelativePosition() const;
	virtual float getScrollRelativePosition(float pos) const;

	virtual void setScrollPosition(float pos);
	virtual float getScrollPosition() const;

	virtual Vec2 getPointForScrollPosition(float pos);

	virtual void setScrollDirty(bool value);
	virtual void updateScrollBounds();

	// common overload points
	virtual void onDelta(float delta);
	virtual void onOverscrollPerformed(float velocity, float pos, float boundary);

	virtual bool onSwipeEventBegin(uint32_t id, Vec2 loc, Vec2 d, Vec2 v);
	virtual bool onSwipeEvent(uint32_t id, Vec2 loc, Vec2 d, Vec2 v);
	virtual bool onSwipeEventEnd(uint32_t id, Vec2 loc, Vec2 d, Vec2 v);

	virtual void onSwipeBegin();
	virtual bool onSwipe(float delta, float velocity, bool ended);
	virtual void onAnimationFinished();

	virtual Rc<ActionInterval> onSwipeFinalizeAction(float velocity);

	virtual void onScroll(float delta, bool finished);
	virtual void onOverscroll(float delta);
	virtual void onPosition();
	virtual void doSetScrollPosition(float pos);

	virtual void fixPosition();

	virtual bool onPressBegin(Vec2);
	virtual bool onLongPress(Vec2, TimeInterval, int count);
	virtual bool onPressEnd(Vec2, TimeInterval);
	virtual bool onPressCancel(Vec2, TimeInterval);

	virtual void onTap(int count, Vec2 loc);

protected:
	Layout _layout = Vertical;

	enum class Movement {
		None,
		Manual,
		Auto,
		Overscroll
	} _movement = Movement::None;

	float _scrollPosition = 0.0f; // current position of scrolling
	float _scrollSize = 0.0f; // size of scrolling area

	float _savedRelativePosition = nan();

	float _scrollMin = nan();
	float _scrollMax = nan();

	float _maxVelocity = nan();
	float _scrollSpaceLimit = nan();
	float _scrollSpacePadding = 0.0f;

	Vec2 _globalScale; // values to scale input gestures

	Node *_root = nullptr;
	ScrollFilterCallback _scrollFilter = nullptr;
	ScrollCallback _scrollCallback = nullptr;
	OverscrollCallback _overscrollCallback = nullptr;
	InputListener *_inputListener = nullptr;
	ScrollController *_controller;

	Rc<ActionInterval> _animationAction;
	Rc<ActionAcceleratedMove> _movementAction;

	bool _bounce = false;
	bool _scrollDirty = true;
	bool _animationDirty = false;

	Padding _paddingGlobal;

private:
	using Node::init;
};

} // namespace stappler::xenolith::basic2d

#endif /* XENOLITH_RENDERER_BASIC2D_SCROLL_XL2DSCROLLVIEWBASE_H_ */
