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

#include "XLUiCanvasView.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

bool CanvasView::init() { return init(sprt::geom::InteractiveZoom); }

bool CanvasView::init(const sprt::geom::ZoomLimits &limits) {
	if (!Node::init()) {
		return false;
	}

	_limits = limits;

	// The world's anchor is (0,0) and stays there: `screen = world * zoom + offset` is only true of
	// a node whose origin is its bottom-left corner, and every conversion in this class assumes it.
	_world = addChild(Rc<Node>::create());
	_world->setName("canvas-world");
	_world->setAnchorPoint(Vec2(0.0f, 0.0f));

	setClipped(true);
	return true;
}

void CanvasView::handleContentSizeDirty() {
	Node::handleContentSizeDirty();

	// Nothing to re-apply: the viewport is the world's transform and the surface size is read off
	// this node when it is asked for. A canvas that mirrored the size into a field would have to
	// remember to do it here, which is the class of bug this arrangement removes.
}

sprt::geom::Viewport CanvasView::getViewport() const {
	sprt::geom::Viewport out;
	out.offset = _world->getPosition().xy();
	out.zoom = _world->getScale().x;
	out.screenSize = Vec2(_contentSize.width, _contentSize.height);
	return out;
}

void CanvasView::setViewport(const sprt::geom::Viewport &view) {
	// The ZOOM is clamped and the OFFSET is not, and the asymmetry is deliberate: a zoom has limits
	// this widget was told, while what a sensible offset would be depends on what is drawn - which
	// is exactly what this widget does not know.
	_world->setScale(sprt::geom::clampZoom(view.zoom, _limits));
	_world->setPosition(view.offset);
}

void CanvasView::zoomAt(const Vec2 &anchor, float factor) {
	// The order - clamp, then read the world anchor from the OLD view, then solve the offset - is
	// the whole content of sprt::geom::zoomAt, and getting it wrong is invisible until somebody scrolls
	// at a limit.
	setViewport(sprt::geom::zoomAt(getViewport(), anchor, factor, _limits));
}

void CanvasView::fit(const sprt::geom::Bounds &bounds, const sprt::geom::FitConfig &config,
		const sprt::geom::ZoomLimits &limits) {
	if (_contentSize.width <= 0.0f || _contentSize.height <= 0.0f) {
		// Framing into a surface that has no size yet centres the world in a 1x1 frame. The caller
		// carries the "do it again when the size is real" flag, because only the caller knows
		// whether it still wants to.
		return;
	}

	// Framing has a range of its own, wider than the gesture's, and it is applied HERE rather than
	// through setViewport - which would clamp the result back into the gesture range and undo the
	// fit for exactly the worlds that needed one.
	auto view = sprt::geom::fitBounds(bounds, Vec2(_contentSize.width, _contentSize.height), config,
			limits);
	_world->setScale(view.zoom);
	_world->setPosition(view.offset);
}

Vec2 CanvasView::worldLocation(const Vec2 &sceneLocation) const {
	return getViewport().toWorld(convertToNodeSpace(sceneLocation));
}

void CanvasView::attachGestures(InputListener *listener) {
	if (!listener) {
		return;
	}

	// Pan on the middle and right buttons. The left one belongs to the caller, whatever it means
	// there - which is the whole reason these go on the caller's listener rather than on one here.
	listener->addSwipeRecognizer(
			[this](const GestureSwipe &swipe) {
		if (swipe.event == GestureEvent::Activated) {
			_world->setPosition(_world->getPosition().xy() + swipe.delta);
		}
		return true;
	},
			InputSwipeInfo{
				makeButtonMask({InputMouseButton::MouseMiddle, InputMouseButton::MouseRight})});

	listener->addScrollRecognizer([this](const GestureScroll &scroll) {
		zoomAt(convertToNodeSpace(scroll.input->currentLocation),
				sprt::geom::wheelZoomFactor(-scroll.amount.y * WheelNotchPixels));
		return true;
	});
}

void CanvasView::setClipped(bool value) {
	_clipped = value;

	if (value) {
		if (!_scissor) {
			_scissor =
					addSystem(Rc<DynamicStateSystem>::create(DynamicStateApplyMode::ApplyForAll));
		}
		_scissor->enableScissor();
	} else if (_scissor) {
		_scissor->disableScissor();
	}
}

} // namespace stappler::xenolith::ui
