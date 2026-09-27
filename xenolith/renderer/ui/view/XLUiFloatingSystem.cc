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

#include "XLUiFloatingSystem.h"
#include "XLNode.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

namespace {

// Above whatever the owner holds, so a border is grabbed before the content under it
static constexpr auto HandleZOrder = ZOrder(30'000);

struct FloatingHandleDesc {
	FloatingEdge edge;
	StringView name;
	WindowCursor cursor;
};

static constexpr FloatingHandleDesc s_handles[] = {
	{FloatingEdge::Left, "floating-resize-left", WindowCursor::ResizeLeft},
	{FloatingEdge::Right, "floating-resize-right", WindowCursor::ResizeRight},
	{FloatingEdge::Bottom, "floating-resize-bottom", WindowCursor::ResizeBottom},
	{FloatingEdge::Top, "floating-resize-top", WindowCursor::ResizeTop},
	{FloatingEdge::Left | FloatingEdge::Bottom, "floating-resize-bottom-left",
		WindowCursor::ResizeBottomLeft},
	{FloatingEdge::Right | FloatingEdge::Bottom, "floating-resize-bottom-right",
		WindowCursor::ResizeBottomRight},
	{FloatingEdge::Left | FloatingEdge::Top, "floating-resize-top-left",
		WindowCursor::ResizeTopLeft},
	{FloatingEdge::Right | FloatingEdge::Top, "floating-resize-top-right",
		WindowCursor::ResizeTopRight},
};

} // namespace

bool FloatingSystem::init() {
	if (!System::init()) {
		return false;
	}
	setSystemFlags(SystemFlags::HandleOwnerEvents | SystemFlags::HandleNodeEvents);
	return true;
}

void FloatingSystem::handleAdded(Node *owner) {
	System::handleAdded(owner);

	owner->setAnchorPoint(Anchor::BottomLeft);
	owner->setOverlay(true);
	owner->addHitTestFlags(HitTestFlags::Occluder);

	buildHandles();
	placeHandles();
}

void FloatingSystem::handleRemoved() {
	setHeader(nullptr);
	if (_owner) {
		for (auto &it : _handles) { _owner->removeChild(it.node); }
		_owner->removeHitTestFlags(HitTestFlags::Occluder);
	}
	_handles.clear();
	System::handleRemoved();
}

void FloatingSystem::handleContentSizeDirty() {
	System::handleContentSizeDirty();
	placeHandles();
}

void FloatingSystem::handleLayoutInParent(Node *parent) {
	System::handleLayoutInParent(parent);

	// The parent was resized or the owner attached: the frame must still fit in it
	if (!isDragging()) {
		applyFrame(clampFrame(getFrame()));
	}
}

void FloatingSystem::setHeader(Node *header) {
	if (_header == header) {
		return;
	}

	if (_header && _headerListener) {
		_header->removeSystem(_headerListener);
	}
	_headerListener = nullptr;
	_header = header;

	if (!_header) {
		return;
	}

	_headerListener = Rc<InputListener>(_header->addSystem(Rc<InputListener>::create()));
	_headerListener->addSwipeRecognizer([this](const GestureSwipe &swipe) {
		switch (swipe.event) {
		case GestureEvent::Began:
			return handleDragBegin(Drag::Move, FloatingEdge::None, _headerListener);
		case GestureEvent::Activated: handleDrag(swipe.delta, swipe.density); return true;
		case GestureEvent::Ended:
		case GestureEvent::Cancelled: handleDragEnd(); return true;
		}
		return false;
	},
			InputSwipeInfo{makeButtonMask({InputMouseButton::Touch, InputMouseButton::MouseLeft}),
				MoveThreshold, true});
}

void FloatingSystem::setMinSize(const Size2 &size) {
	_minSize = size;
	if (_owner) {
		applyFrame(clampFrame(getFrame()));
	}
}

void FloatingSystem::setResizable(bool value) {
	_resizable = value;
	for (auto &it : _handles) { it.node->setVisible(_resizable); }
}

void FloatingSystem::setFrame(const Rect &frame) { applyFrame(clampFrame(frame)); }

Rect FloatingSystem::getFrame() const {
	if (!_owner) {
		return Rect();
	}
	return Rect(_owner->getPosition().xy(), _owner->getContentSize());
}

void FloatingSystem::setFrameChangedCallback(Function<void(const Rect &)> &&cb) {
	_frameChangedCallback = sp::move(cb);
}

void FloatingSystem::buildHandles() {
	if (!_owner || !_handles.empty()) {
		return;
	}

	for (auto &desc : s_handles) {
		auto node = _owner->addChild(Rc<Node>::create(), HandleZOrder);
		node->setName(desc.name);
		node->setAnchorPoint(Anchor::BottomLeft);
		node->setVisible(_resizable);

		auto listener = node->addSystem(Rc<InputListener>::create());
		listener->setCursor(desc.cursor);
		listener->addSwipeRecognizer(
				[this, edge = desc.edge, listener](const GestureSwipe &swipe) {
			switch (swipe.event) {
			case GestureEvent::Began: return handleDragBegin(Drag::Resize, edge, listener);
			case GestureEvent::Activated: handleDrag(swipe.delta, swipe.density); return true;
			case GestureEvent::Ended:
			case GestureEvent::Cancelled: handleDragEnd(); return true;
			}
			return false;
		},
				// threshold 0: a border follows the pointer from the first pixel
				InputSwipeInfo{makeButtonMask({InputMouseButton::Touch, InputMouseButton::MouseLeft}),
					0.0f, true});

		_handles.emplace_back(Handle{desc.edge, node});
	}
}

void FloatingSystem::placeHandles() {
	if (!_owner) {
		return;
	}

	const auto size = _owner->getContentSize();
	const float g = GrabWidth;
	const float corner = g * 3.0f;

	for (auto &it : _handles) {
		const bool left = hasFlag(it.edge, FloatingEdge::Left);
		const bool right = hasFlag(it.edge, FloatingEdge::Right);
		const bool bottom = hasFlag(it.edge, FloatingEdge::Bottom);
		const bool top = hasFlag(it.edge, FloatingEdge::Top);

		Rect rect;
		if ((left || right) && (bottom || top)) {
			rect.origin.x = left ? -g : size.width + g - corner;
			rect.origin.y = bottom ? -g : size.height + g - corner;
			rect.size = Size2(corner, corner);
		} else if (left || right) {
			rect.origin = Vec2(left ? -g : size.width - g, corner - g);
			rect.size = Size2(g * 2.0f, sprt::max(size.height - (corner - g) * 2.0f, 0.0f));
		} else {
			rect.origin = Vec2(corner - g, bottom ? -g : size.height - g);
			rect.size = Size2(sprt::max(size.width - (corner - g) * 2.0f, 0.0f), g * 2.0f);
		}

		it.node->setPosition(rect.origin);
		it.node->setContentSize(rect.size);
	}
}

void FloatingSystem::applyFrame(const Rect &frame) {
	if (!_owner) {
		return;
	}
	_owner->setPosition(frame.origin);
	_owner->setContentSize(frame.size);
}

Rect FloatingSystem::clampFrame(const Rect &frame) const {
	auto parent = _owner ? _owner->getParent() : nullptr;
	const auto area = parent ? parent->getContentSize() : Size2::ZERO;

	Rect ret = frame;
	ret.size.width = sprt::max(ret.size.width, _minSize.width);
	ret.size.height = sprt::max(ret.size.height, _minSize.height);

	// A parent not laid out yet has nothing to clamp against
	if (area.width <= 0.0f || area.height <= 0.0f) {
		return ret;
	}

	ret.size.width = sprt::min(ret.size.width, area.width);
	ret.size.height = sprt::min(ret.size.height, area.height);
	ret.origin.x = sprt::clamp(ret.origin.x, 0.0f, area.width - ret.size.width);
	ret.origin.y = sprt::clamp(ret.origin.y, 0.0f, area.height - ret.size.height);
	return ret;
}

Vec2 FloatingSystem::toParentDelta(const Vec2 &delta, float density) const {
	return (density > 0.0f) ? (delta / density) : delta;
}

bool FloatingSystem::handleDragBegin(Drag drag, FloatingEdge edge, InputListener *listener) {
	if (!_owner || isDragging() || (drag == Drag::Resize && !_resizable)) {
		return false;
	}

	_dragging = drag;
	_dragEdge = edge;
	_dragFrame = getFrame();

	// capture: the pointer leaves a thin border at once, and a header drag is no longer a tap
	listener->setExclusive();
	_owner->addStyleClass(drag == Drag::Move ? "moving" : "resizing");
	return true;
}

void FloatingSystem::handleDrag(const Vec2 &delta, float density) {
	if (!isDragging()) {
		return;
	}

	const auto d = toParentDelta(delta, density);

	if (_dragging == Drag::Move) {
		_dragFrame.origin += d;
		applyFrame(clampFrame(_dragFrame));
		return;
	}

	// A border moves alone: the opposite one stays where it is, even against the minimum
	auto &f = _dragFrame;
	if (hasFlag(_dragEdge, FloatingEdge::Left)) {
		f.origin.x += d.x;
		f.size.width -= d.x;
	} else if (hasFlag(_dragEdge, FloatingEdge::Right)) {
		f.size.width += d.x;
	}
	if (hasFlag(_dragEdge, FloatingEdge::Bottom)) {
		f.origin.y += d.y;
		f.size.height -= d.y;
	} else if (hasFlag(_dragEdge, FloatingEdge::Top)) {
		f.size.height += d.y;
	}

	float left = f.origin.x;
	float right = f.origin.x + f.size.width;
	float bottom = f.origin.y;
	float top = f.origin.y + f.size.height;

	auto parent = _owner->getParent();
	const auto area = parent ? parent->getContentSize() : Size2::ZERO;
	if (area.width > 0.0f && area.height > 0.0f) {
		left = sprt::max(left, 0.0f);
		bottom = sprt::max(bottom, 0.0f);
		right = sprt::min(right, area.width);
		top = sprt::min(top, area.height);
	}

	if (right - left < _minSize.width) {
		if (hasFlag(_dragEdge, FloatingEdge::Left)) {
			left = right - _minSize.width;
		} else {
			right = left + _minSize.width;
		}
	}
	if (top - bottom < _minSize.height) {
		if (hasFlag(_dragEdge, FloatingEdge::Bottom)) {
			bottom = top - _minSize.height;
		} else {
			top = bottom + _minSize.height;
		}
	}

	applyFrame(Rect(left, bottom, right - left, top - bottom));
}

void FloatingSystem::handleDragEnd() {
	if (!isDragging()) {
		return;
	}

	if (_owner) {
		_owner->removeStyleClass(_dragging == Drag::Move ? "moving" : "resizing");
	}
	_dragging = Drag::None;
	_dragEdge = FloatingEdge::None;

	if (_frameChangedCallback) {
		_frameChangedCallback(getFrame());
	}
}

} // namespace stappler::xenolith::ui
