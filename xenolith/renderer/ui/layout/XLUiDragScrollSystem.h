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

/** Scrolls the node it is on while a drag rests near its edge.

    DragScrollSystem::acquireForNode(scrollView);

A list you cannot drag past the bottom of is a list you cannot drop into the part you cannot see.
This is the general answer to that, for any scroller and any drag - not a feature of one widget.

WHY IT LIVES HERE. It has to know both a live drag and a scroller, and `renderer/ui` is the only
layer that can see both: the drag layer sits BELOW basic2d and cannot reference a ScrollView, and
basic2d has no idea what a drop target is. So DropTarget cannot carry this, however natural that
would read.

WHICH SCROLLER. Either of the two the engine has - a `basic2d::ScrollViewBase` the system is placed
on, or a `ui::ScrollSystem` on that node - resolved ONCE when the system is added and thereafter
reached through two callbacks. The type question is asked at attach time and never in the loop.

WHAT IT SCROLLS FOR. By default only a drag whose current target is inside this node's subtree
(Scope::TargetInside). Anything else would mean a dock tab dragged across a panel scrolls every list
it happens to pass over, which is not help, it is noise.

AND THE PART THAT IS EASY TO MISS: after each nudge it calls DragSystem::refreshDrag(). Drag events
arrive only on pointer MOTION, so a pointer held still at the edge would leave the drop position
pointing at a row that has since scrolled away - the insertion line frozen over nothing. */
class SP_PUBLIC DragScrollSystem : public System {
public:
	// Matching ui::TextViewContainer's edge pull, so the two behave alike where a user meets both.
	static constexpr float DefaultSpeed = 300.0f; // points per second, at the very edge
	static constexpr float DefaultEdge = 48.0f; // width of the band that pulls

	enum class Scope {
		// Only while the drag's current target is this node or inside it. The default, and what
		// keeps a scroller from reacting to a drag that has nothing to do with it.
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

	// Width of the pulling band, in points. Clamped to a third of the box, so a short list does not
	// become one band from edge to edge with no neutral middle.
	virtual void setEdge(float);
	float getEdge() const { return _edge; }

	virtual void setScope(Scope);
	Scope getScope() const { return _scope; }

	// True while a drag is actually being pulled by this system.
	bool isScrolling() const { return _scrolling; }

protected:
	static constexpr uint32_t RenderActionTag = "XLUiDragScrollRender"_tag;

	// Resolve which of the two scrollers the owner is, once. Empty callbacks mean "not a scroller",
	// and then this system does nothing at all.
	void resolveScroller();

	// How much room is left in each direction, and the nudge itself. Both in the CSS orientation -
	// x grows right, y grows DOWN - which is ui::ScrollSystem's, so the basic2d adapter is the one
	// that has to say which way is which.
	Function<Vec2()> _range;
	Function<void(Vec2)> _scrollBy;

	DragSystem *_drag = nullptr;
	float _speed = DefaultSpeed;
	float _edge = DefaultEdge;
	Scope _scope = Scope::TargetInside;
	bool _scrolling = false;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_LAYOUT_XLUIDRAGSCROLLSYSTEM_H_
