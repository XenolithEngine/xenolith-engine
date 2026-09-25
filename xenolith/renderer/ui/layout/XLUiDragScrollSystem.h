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

#ifndef XENOLITH_RENDERER_UI_LAYOUT_XLUIDRAGSCROLLSYSTEM_H_
#define XENOLITH_RENDERER_UI_LAYOUT_XLUIDRAGSCROLLSYSTEM_H_

#include "XLSystem.h"
#include "XLDragSystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

/* A scroller as an edge pull sees it: the room left before and after the position, and the nudge.
Both in CSS orientation (y grows down); the basic2d adapter converts. */
struct SP_PUBLIC EdgeScroller {
	Function<Vec2()> range;
	Function<void(Vec2)> scrollBy;

	bool empty() const { return !range || !scrollBy; }
};

// A basic2d::ScrollViewBase, else a ui::ScrollSystem on the node; empty when it is neither.
SP_PUBLIC EdgeScroller makeEdgeScroller(Node *);

/* The pull on a point at `y` of a box `height` tall, in node space (y up): from -1 at the top edge
to 0 at the inner side of the band, and from 0 to 1 at the bottom, the band clamped to a third of
the box. Outside the box the pull is full with `beyond`, and none without. */
SP_PUBLIC float getEdgeScrollRamp(float y, float height, float edge, bool beyond);

/** Scrolls the node it is on while a drag rests near its edge.

    DragScrollSystem::acquireForNode(scrollView);

It lives in renderer/ui because the drag layer sits below basic2d and cannot see a scroller.
The scroller (a basic2d::ScrollViewBase or a ui::ScrollSystem on the node) is resolved once, on
add. After each nudge it calls DragSystem::refreshDrag(): drag events arrive only on pointer motion,
so a still pointer would otherwise keep a stale drop position. */
class SP_PUBLIC DragScrollSystem : public System {
public:
	// Matches ui::TextViewContainer's edge pull.
	static constexpr float DefaultSpeed = 300.0f; // points per second, at the very edge
	static constexpr float DefaultEdge = 48.0f; // width of the band that pulls

	enum class Scope {
		// Only while the drag's current target is this node or inside it. The default.
		TargetInside,

		// Any live drag whose pointer is over this node. For a scroller that accepts drops through
		// something other than a DropTarget of its own.
		AnyDrag,
	};

	// Adds one if the node has none. Idempotent, like DragSystem::acquireForNode.
	static DragScrollSystem *acquireForNode(NotNull<Node>);

	virtual ~DragScrollSystem() = default;

	virtual bool init() override;

	virtual void handleAdded(Node *) override;
	virtual void handleRemoved() override;
	virtual void handleEnter(Scene *) override;
	virtual void handleExit() override;

	virtual void update(const UpdateTime &) override;

	virtual void setSpeed(float);
	float getSpeed() const { return _speed; }

	// Width of the pulling band, in points. Clamped to a third of the box.
	virtual void setEdge(float);
	float getEdge() const { return _edge; }

	virtual void setScope(Scope);
	Scope getScope() const { return _scope; }

	// True while a drag is actually being pulled by this system.
	bool isScrolling() const { return _scrolling; }

protected:
	static constexpr uint32_t RenderActionTag = "XLUiDragScrollRender"_tag;

	// Which scroller the owner is; an empty one means "not a scroller", and nothing runs.
	EdgeScroller _scroller;

	DragSystem *_drag = nullptr;
	float _speed = DefaultSpeed;
	float _edge = DefaultEdge;
	Scope _scope = Scope::TargetInside;
	bool _scrolling = false;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_LAYOUT_XLUIDRAGSCROLLSYSTEM_H_
