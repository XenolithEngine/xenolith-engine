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

#include "XL2dLabel.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

namespace {

// The control's own metrics. Deliberately not stylesheet-driven defaults: a canvas with no sheet in
// scope must still be able to read its zoom and step it, which is the same argument ui::TreeView
// makes for the colours of its drag feedback.
// 28 and not 20, and that is not a matter of taste: ui::Button reserves 8pt on each side of its
// label, so a button narrower than about 24 hands its glyph a box of four points and the "+" comes
// out clipped and off-centre. The button is what carries :hover and :active, and it is worth being
// wide enough to keep.
constexpr float ZoomButtonSize = 28.0f;
constexpr float ZoomControlPadding = 3.0f;
constexpr float ZoomControlGap = 2.0f;
constexpr uint16_t ZoomFontSize = 14;

// Above the world, and above whatever a caller has put in it: this is chrome, and a document drawn
// over its own zoom readout would be a document nobody can zoom out of.
constexpr ZOrder ZoomControlZOrder = ZOrder(1'000);

} // namespace

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
	setZoomControlEnabled(true);
	return true;
}

void CanvasView::handleContentSizeDirty() {
	Node::handleContentSizeDirty();

	// Nothing to re-apply for the VIEWPORT: it is the world's transform and the surface size is read
	// off this node when it is asked for. A canvas that mirrored the size into a field would have to
	// remember to do it here, which is the class of bug this arrangement removes.
	//
	// The control is another matter - it hangs off a corner, and the corner has just moved.
	layoutZoomControl();
}

void CanvasView::handleGlobalTransformDirty(const Mat4 &parentTransform) {
	Node::handleGlobalTransformDirty(parentTransform);

	/* What a gesture delta has to be divided by. Gestures arrive in scene units - physical pixels,
	since the Scene scales its subtree by the density - and the world's position is in this node's
	own space; see the header note.

	THE GLOBAL PHASE AND NOT THE LOCAL ONE. handleTransformDirty fires when THIS node's transform
	moved within its parent, which a density change several ancestors up never does; the global one
	is the phase whose whole stated purpose is "global parameters (like pixel density) can be
	recalculated". Node::getInputDensity() is the same number reduced to its minimum axis, and both
	axes are kept here for the same reason ui::ScrollSystem keeps them. Zero would be a canvas that
	cannot be panned at all, so it degrades to one rather than to a division by zero. */
	Vec3 scale;
	parentTransform.decompose(&scale, nullptr, nullptr);
	const auto own = getScale();
	_surfaceScale = Vec2(scale.x * own.x, scale.y * own.y);
	if (_surfaceScale.x == 0.0f) {
		_surfaceScale.x = 1.0f;
	}
	if (_surfaceScale.y == 0.0f) {
		_surfaceScale.y = 1.0f;
	}
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
	updateZoomControl();
}

void CanvasView::zoomAt(const Vec2 &anchor, float factor) {
	// The order - clamp, then read the world anchor from the OLD view, then solve the offset - is
	// the whole content of sprt::geom::zoomAt, and getting it wrong is invisible until somebody scrolls
	// at a limit.
	setViewport(sprt::geom::zoomAt(getViewport(), anchor, factor, _limits));
}

void CanvasView::zoomBy(float factor) {
	zoomAt(Vec2(_contentSize.width * 0.5f, _contentSize.height * 0.5f), factor);
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
	updateZoomControl();
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
			// Divided by this node's world scale, and that division is the whole of the difference
			// between the picture following the pointer and sliding out from under it - see the
			// header note. The delta is in scene units; the world's position is in this node's.
			_world->setPosition(_world->getPosition().xy()
					+ Vec2(swipe.delta.x / _surfaceScale.x, swipe.delta.y / _surfaceScale.y));
		}
		return true;
	},
			InputSwipeInfo{
				makeButtonMask({InputMouseButton::MouseMiddle, InputMouseButton::MouseRight})});

	listener->addScrollRecognizer([this](const GestureScroll &scroll) {
		// DIVIDED BY THE NOTCH, and that division is not a taste. A Scroll event carries a scroll
		// AMOUNT rather than a count of clicks - a trackpad has no clicks to count - and one detent
		// of a wheel is worth `InputScrollNotch` of it. ZoomStepRatio is stated per NOTCH, so the
		// amount has to be turned into notches before it is an exponent. Handing the raw amount to
		// wheelZoomRatio raises the step to the tenth power: one click of the wheel came out as
		// 1.1^10, two and a half times the scale, which is what this widget shipped with.
		zoomAt(convertToNodeSpace(scroll.input->currentLocation),
				sprt::geom::wheelZoomRatio(scroll.amount.y / sprt::window::InputScrollNotch,
						ZoomStepRatio));
		return true;
	});
}

/* THE CONTROL IS BUILT BY HAND AND LAID OUT BY HAND, and both are deliberate.

By hand, because a `ui::LayoutSystem` here would make the control's box the answer to a flex pass
that nothing else in this widget takes part in, and because three children in a row at fixed sizes
is less code than the declaration of that pass. It is what ui::WindowDecorations does with its eight
grips, for the same reason.

With a paint of its own, because a canvas is routinely drawn with no stylesheet in scope at all -
four of them in this repository are - and a ui::Panel with nothing declared is opaque WHITE. The
type is registered so a sheet that DOES say `canvas-zoom { … }` replaces this outright. */
void CanvasView::setZoomControlEnabled(bool value) {
	if (value == (_zoomControl != nullptr)) {
		return;
	}

	if (!value) {
		_zoomControl->removeFromParent(true);
		_zoomControl = nullptr;
		_zoomOut = _zoomIn = nullptr;
		_zoomLabel = nullptr;
		_zoomShown = -1;
		return;
	}

	_zoomControl = addChild(Rc<Panel>::create(), ZoomControlZOrder);
	_zoomControl->setName("canvas-zoom");
	_zoomControl->setType("canvas-zoom");
	_zoomControl->removeStyleClass("xl-ui-panel");
	_zoomControl->addStyleClass("xl-ui-canvas-zoom");
	Panel::registerStyleAppliers("canvas-zoom");
	_zoomControl->setPathColor(Color4B(0x26, 0x26, 0x2E, 0xD8), true);
	_zoomControl->setBorderRadius(4.0f);
	_zoomControl->setAnchorPoint(Vec2(0.0f, 0.0f));
	_zoomControl->setContentSize(ZoomControlSize);

	// One step each way, the wheel's own step. A control that stepped by some figure of its own
	// would be the fifth copy of the constant this widget exists to have one of.
	_zoomOut = _zoomControl->addChild(
			Rc<Button>::create(StringView("-"), [this] { zoomBy(1.0f / ZoomStepRatio); }));
	_zoomOut->setName("canvas-zoom-out");
	_zoomIn = _zoomControl->addChild(
			Rc<Button>::create(StringView("+"), [this] { zoomBy(ZoomStepRatio); }));
	_zoomIn->setName("canvas-zoom-in");

	for (auto *b : {_zoomOut, _zoomIn}) {
		b->addStyleClass("canvas-zoom-button");
		b->setPathColor(Color4B(0x3A, 0x3A, 0x46, 0xFF), true);
		b->setBorderRadius(3.0f);
		b->setLabelColor(Color4F(0.94f, 0.94f, 0.96f, 1.0f));
		if (auto label = b->getLabel()) {
			label->setFontSize(ZoomFontSize);
		}
	}

	_zoomLabel = _zoomControl->addChild(Rc<basic2d::Label>::create());
	_zoomLabel->setName("canvas-zoom-value");
	_zoomLabel->setType("label");
	_zoomLabel->addStyleClass("canvas-zoom-value");
	_zoomLabel->setAlignment(basic2d::Label::TextAlign::Center);
	_zoomLabel->setFontSize(ZoomFontSize);
	_zoomLabel->setColor(Color4F(0.94f, 0.94f, 0.96f, 1.0f));

	layoutZoomControl();
	updateZoomControl();
}

void CanvasView::setZoomControlPlacement(const Vec2 &corner, float margin) {
	_zoomCorner = corner;
	_zoomMargin = margin;
	layoutZoomControl();
}

void CanvasView::layoutZoomControl() {
	if (!_zoomControl) {
		return;
	}

	const auto size = _zoomControl->getContentSize();

	// The corner is an anchor of THIS node's box and the margin runs INWARD from it, which is what
	// makes one pair of numbers describe all four corners: at (0,0) the margin adds, at (1,1) it
	// subtracts, and the control never hangs outside the surface it belongs to.
	_zoomControl->setPosition(Vec2((_contentSize.width - size.width) * _zoomCorner.x
					+ _zoomMargin * (1.0f - 2.0f * _zoomCorner.x),
			(_contentSize.height - size.height) * _zoomCorner.y
					+ _zoomMargin * (1.0f - 2.0f * _zoomCorner.y)));

	const float inner = size.height - ZoomControlPadding * 2.0f;
	const float labelWidth =
			size.width - ZoomControlPadding * 2.0f - ZoomButtonSize * 2.0f - ZoomControlGap * 2.0f;

	_zoomOut->setAnchorPoint(Vec2(0.0f, 0.5f));
	_zoomOut->setContentSize(Size2(ZoomButtonSize, inner));
	_zoomOut->setPosition(Vec2(ZoomControlPadding, size.height * 0.5f));

	_zoomIn->setAnchorPoint(Vec2(1.0f, 0.5f));
	_zoomIn->setContentSize(Size2(ZoomButtonSize, inner));
	_zoomIn->setPosition(Vec2(size.width - ZoomControlPadding, size.height * 0.5f));

	// The readout is CENTRED in the run between the buttons and given a fixed width, so the number
	// changing width does not walk the two buttons around under the pointer.
	_zoomLabel->setAnchorPoint(Vec2(0.5f, 0.5f));
	_zoomLabel->setWidth(labelWidth);
	_zoomLabel->setPosition(Vec2(size.width * 0.5f, size.height * 0.5f));
}

void CanvasView::updateZoomControl() {
	if (!_zoomLabel) {
		return;
	}

	// Guarded on the number that is SHOWN rather than on the zoom: a pan writes the world's
	// transform on every pointer move and changes no percentage, and rewriting a label is a text
	// layout.
	const auto percent = int32_t(sprt::lroundf(_world->getScale().x * 100.0f));
	if (percent == _zoomShown) {
		return;
	}
	_zoomShown = percent;
	_zoomLabel->setString(toString(percent, "%"));
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
