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


#ifndef XENOLITH_RENDERER_UI_VIEW_XLUIMARQUEE_H_
#define XENOLITH_RENDERER_UI_VIEW_XLUIMARQUEE_H_

#include "XLUiRowSelection.h"
#include "XLUiPanel.h"
#include "XLUiDragScrollSystem.h"
#include "XL2dScrollViewBase.h"
#include "XLInputListener.h"
#include "XLHotkey.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

// Where a band stands. Content space is the scroll root's (it moves with the rows), or the host's
// when there is no scroll.
struct SP_PUBLIC MarqueeEvent {
	Vec2 origin; // the press, content space
	Vec2 point; // the pointer, content space
	Rect rect; // between them, content space
	Rect hostRect; // the same band in the host's space, not clamped to the viewport
	Vec2 press; // world, where the button went down
	Vec2 location; // world, the pointer now
	InputModifier modifiers = InputModifier::None; // as they were at the press
	ListSelectionOp op = ListSelectionOp::Replace; // getListSweepOp(modifiers)
};

struct SP_PUBLIC MarqueeSlots {
	// Whether the drag becomes a band; false leaves it to the scene. Required.
	Function<bool(const MarqueeEvent &)> begin;

	// The band moved, or the content moved under it.
	Function<void(const MarqueeEvent &)> update;

	// The release (`commit`), or a cancel: Escape, a lost pointer, disabling, leaving the scene.
	Function<void(const MarqueeEvent &, bool commit)> end;

	// The host's part the band lives in, host space: a press outside it starts none, and the band is
	// drawn cut to it. Default: the scroll's box, else the host's.
	Function<Rect()> viewport;
};

/** A rubber band over a list: a mouse drag sweeps a rectangle instead of scrolling, the owner
learns what it covers as it moves, and the release or a cancel ends it.

    host->addSystem(Rc<ui::MarqueeSystem>::create(ui::MarqueeSlots{...}, scroll));

The system decides nothing about the items: hits, the preview and the commit are the slots'.
ListSweep and applyListSweep (XLUiRowSelection.h) give the rules a list applies; TableView and
TreeView carry one of these already (setMarqueeEnabled).

The listener is asked before the scene, so it takes a drag from the scroll view and from the rows.
It stands back from a finger (InputModifier::Touch, which pans), from a press outside the viewport,
from a press on something drawn over the host, and from a press on a DragSource inside the host.
The band's origin is kept in content space, so it stays with its rows while they scroll; near an
edge, and beyond it, the scroll is pulled. Escape cancels. */
class SP_PUBLIC MarqueeSystem : public InputListener {
public:
	static constexpr int32_t DefaultPriority = 1;

	// Above a view's insertion line and drop highlight.
	static constexpr ZOrder DefaultBandZOrder = ZOrder(65);

	virtual ~MarqueeSystem() = default;

	virtual bool init(MarqueeSlots &&, basic2d::ScrollViewBase *scroll = nullptr);

	virtual void handleExit() override;
	virtual void update(const UpdateTime &) override;

	// Off cancels a band in progress.
	virtual void setEnabled(bool) override;

	// The edge pull, as DragScrollSystem's.
	void setEdge(float);
	float getEdge() const { return _edge; }
	void setSpeed(float);
	float getSpeed() const { return _speed; }

	void setBandZOrder(ZOrder);

	basic2d::ScrollViewBase *getScroll() const { return _scroll; }

	bool isActive() const { return _active; }

	// The band in progress; meaningless while idle.
	const MarqueeEvent &getEvent() const { return _event; }

	// In content space; ZERO while idle.
	Rect getRect() const;

	// As drawn, in the host's space, cut to the viewport; ZERO while nothing is drawn.
	Rect getBandRect() const;

	Rect getViewport() const;

	// The node the band is drawn with: type `marquee`, class `xl-ui-marquee`. Null before a band.
	Panel *getBand() const { return _band; }

	// Asks the owner again with the pointer where it is: the content was rebuilt under the band.
	void refresh();

	// Ends the band without a commit. The drag stays captured until it is released.
	void cancel();

protected:
	static constexpr uint32_t RenderActionTag = "XLUiMarqueeRender"_tag;

	bool handleSwipe(const GestureSwipe &);
	bool handleBack();

	// Whether a press may start a band; see the class comment.
	bool canBeginAt(const Vec2 &world) const;

	Node *getContent() const;
	void updateBand(bool force);
	void layoutBand();
	void pull(float dt);
	void finish(bool commit);

	MarqueeSlots _slots;
	basic2d::ScrollViewBase *_scroll = nullptr;
	EdgeScroller _scroller;
	Rc<Panel> _band;
	Rect _bandRect;

	MarqueeEvent _event;
	float _edge = DragScrollSystem::DefaultEdge;
	float _speed = DragScrollSystem::DefaultSpeed;
	ZOrder _bandZOrder = DefaultBandZOrder;
	bool _active = false;
	bool _captured = false; // the drag is ours until it ends, even after a cancel
	bool _pulling = false;
};

} // namespace stappler::xenolith::ui

#endif /* XENOLITH_RENDERER_UI_VIEW_XLUIMARQUEE_H_ */
