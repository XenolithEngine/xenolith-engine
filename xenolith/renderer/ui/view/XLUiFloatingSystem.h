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

#ifndef XENOLITH_RENDERER_UI_VIEW_XLUIFLOATINGSYSTEM_H_
#define XENOLITH_RENDERER_UI_VIEW_XLUIFLOATINGSYSTEM_H_

#include "XLInputListener.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

// Which borders a resize handle moves.
enum class FloatingEdge : uint8_t {
	None = 0,
	Left = 1 << 0,
	Right = 1 << 1,
	Bottom = 1 << 2,
	Top = 1 << 3,
};

SP_DEFINE_ENUM_AS_MASK(FloatingEdge)

/* A window inside the scene: its owner is moved by a header, resized by its borders and kept
within its parent, and drawn in the overlay pass above everything else.

It is not modal. The owner is an input occluder (HitTestFlags::Occluder): what is drawn under it
is not reached by the pointer there, while everything inside it works as usual. Nothing closes it
but its owner.

The owner is hung by its bottom-left corner; the frame is its rect in the parent's space. Eight
invisible handles along the borders (named `floating-resize-<edge>`) carry the resize cursors. */
class SP_PUBLIC FloatingSystem : public System {
public:
	// How far a handle reaches to either side of the border it moves
	static constexpr float GrabWidth = 4.0f;

	// Where a drag on the header starts moving the owner, so a press on a button there stays one
	static constexpr float MoveThreshold = 4.0f;

	virtual ~FloatingSystem() = default;

	virtual bool init() override;

	virtual void handleAdded(Node *owner) override;
	virtual void handleRemoved() override;
	virtual void handleContentSizeDirty() override;
	virtual void handleLayoutInParent(Node *) override;

	// The node the owner is dragged by, any descendant of the owner; null stops moving
	void setHeader(Node *);
	Node *getHeader() const { return _header; }

	void setMinSize(const Size2 &);
	const Size2 &getMinSize() const { return _minSize; }

	void setResizable(bool);
	bool isResizable() const { return _resizable; }

	// Kept within the parent and at least the minimum size
	void setFrame(const Rect &);
	Rect getFrame() const;

	// After a move or a resize by the user ended
	void setFrameChangedCallback(Function<void(const Rect &)> &&);

	bool isDragging() const { return _dragging != Drag::None; }

protected:
	enum class Drag {
		None,
		Move,
		Resize,
	};

	void buildHandles();
	void placeHandles();
	void applyFrame(const Rect &);
	Rect clampFrame(const Rect &) const;
	Vec2 toParentDelta(const Vec2 &delta, float density) const;

	bool handleDragBegin(Drag, FloatingEdge, InputListener *);
	void handleDrag(const Vec2 &delta, float density);
	void handleDragEnd();

	struct Handle {
		FloatingEdge edge = FloatingEdge::None;
		Node *node = nullptr;
	};

	Node *_header = nullptr;
	Rc<InputListener> _headerListener;
	Vector<Handle> _handles;

	Size2 _minSize = Size2(120.0f, 80.0f);
	bool _resizable = true;

	Drag _dragging = Drag::None;
	FloatingEdge _dragEdge = FloatingEdge::None;

	// The frame the drag is applied to, unrounded; the owner shows it clamped
	Rect _dragFrame;

	Function<void(const Rect &)> _frameChangedCallback;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_VIEW_XLUIFLOATINGSYSTEM_H_
