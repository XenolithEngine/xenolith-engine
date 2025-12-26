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

	virtual void setOverscrollColor(const Color4F &, bool withOpacity = false);
	virtual Color4F getOverscrollColor() const;

	virtual void setOverscrollVisible(bool value);
	virtual bool isOverscrollVisible() const;

	virtual void setIndicatorColor(const Color4B &, bool withOpacity = false);
	virtual Color4F getIndicatorColor() const;

	virtual void setIndicatorVisible(bool value);
	virtual bool isIndicatorVisible() const;

	virtual void setPadding(const Padding &) override;

	virtual void setOverscrollFrontOffset(float value);
	virtual float getOverscrollFrontOffset() const;

	virtual void setOverscrollBackOffset(float value);
	virtual float getOverscrollBackOffset() const;

	virtual void setIndicatorIgnorePadding(bool value);
	virtual bool isIndicatorIgnorePadding() const;

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

	virtual void onOverscroll(float delta) override;
	virtual void onScroll(float delta, bool finished) override;
	virtual void onTap(int count, Vec2 loc) override;
	virtual void onAnimationFinished() override;

	virtual void updateIndicatorPosition();
	virtual void updateIndicatorPosition(Node *indicator, float size, float value, bool actions,
			float min);

	virtual ScrollController::Item *getItemForNode(Node *) const;

	Overscroll *_overflowFront = nullptr;
	Overscroll *_overflowBack = nullptr;

	LayerRounded *_indicator = nullptr;
	bool _indicatorVisible = true;
	bool _indicatorIgnorePadding = false;

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
