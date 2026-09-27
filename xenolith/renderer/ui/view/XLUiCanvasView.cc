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
#include "XLUiMenuPopup.h"
#include "XLUiMenuSource.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

namespace {

// The control's own metrics, not stylesheet-driven: a canvas may have no sheet in scope.
// ui::Button reserves 8pt on each side of its label, so a button below ~24pt clips its glyph.
constexpr float ZoomButtonSize = 28.0f;
constexpr float ZoomControlPadding = 3.0f;
constexpr float ZoomControlGap = 2.0f;
constexpr uint16_t ZoomFontSize = 14;

// The wider gap between "+" and the caller's items.
constexpr float ZoomControlGroupGap = 8.0f;

// The percentage button: room for "400%" and the arrow after it.
constexpr float ZoomValueWidth = 76.0f;

// The arrow glyph, and the colour of the control's marks.
constexpr float ZoomIconSize = 18.0f;
constexpr Color4F ZoomControlInk = Color4F(0.94f, 0.94f, 0.96f, 1.0f);

constexpr float DefaultZoomPresets[] = {0.25f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f, 4.0f};

// Above the world and anything the caller puts in it.
constexpr ZOrder ZoomControlZOrder = ZOrder(1'000);

// Below the world, which is at zero; this node draws nothing of its own.
constexpr ZOrder GridZOrder = ZOrder(-1);

} // namespace

bool CanvasView::init() { return init(sprt::geom::InteractiveZoom); }

bool CanvasView::init(const sprt::geom::ZoomLimits &limits) {
	if (!Node::init()) {
		return false;
	}

	_limits = limits;
	_zoomPresets = Vector<float>(std::begin(DefaultZoomPresets), std::end(DefaultZoomPresets));

	// The world's anchor stays (0,0): conversions here assume `screen = world * zoom + offset`.
	_world = addChild(Rc<Node>::create());
	_world->setName("canvas-world");
	_world->setAnchorPoint(Vec2(0.0f, 0.0f));

	setClipped(true);
	setZoomControlEnabled(true);
	return true;
}

void CanvasView::handleContentSizeDirty() {
	Node::handleContentSizeDirty();

	// The viewport needs no update (size is read on demand); the control follows its corner.
	layoutZoomControl();

	if (_grid) {
		_grid->setContentSize(_contentSize);
	}
}

void CanvasView::setGridEnabled(bool value) {
	if (value == (_grid != nullptr)) {
		return;
	}
	if (!value) {
		_grid->removeFromParent();
		_grid = nullptr;
		return;
	}
	_grid = addChild(Rc<basic2d::PixelGrid>::create(), GridZOrder);
	_grid->setName("canvas-grid");
	_grid->setAnchorPoint(Vec2(0.0f, 0.0f));
	_grid->setPosition(Vec2(0.0f, 0.0f));
	_grid->setContentSize(_contentSize);
	_grid->setMappingSource(_world);
}

void CanvasView::handleGlobalTransformDirty(const Mat4 &parentTransform) {
	Node::handleGlobalTransformDirty(parentTransform);

	/* The scale gesture deltas are divided by, per axis. Computed in the global phase so a density
	change on an ancestor is seen; zero falls back to one. */
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
	// The zoom is clamped; a valid offset depends on the content, which only the caller knows.
	_world->setScale(sprt::geom::clampZoom(view.zoom, _limits));
	_world->setPosition(view.offset);
	updateZoomControl();
}

void CanvasView::zoomAt(const Vec2 &anchor, float factor) {
	setViewport(sprt::geom::zoomAt(getViewport(), anchor, factor, _limits));
}

void CanvasView::zoomBy(float factor) {
	zoomAt(Vec2(_contentSize.width * 0.5f, _contentSize.height * 0.5f), factor);
}

void CanvasView::fit(const sprt::geom::Bounds &bounds, const sprt::geom::FitConfig &config,
		const sprt::geom::ZoomLimits &limits) {
	if (_contentSize.width <= 0.0f || _contentSize.height <= 0.0f) {
		// No surface size yet; the caller is responsible for retrying after layout.
		return;
	}

	// Applied directly, not through setViewport, which would clamp to the narrower gesture range.
	auto view = sprt::geom::fitBounds(bounds, Vec2(_contentSize.width, _contentSize.height), config,
			limits);
	_world->setScale(view.zoom);
	_world->setPosition(view.offset);
	updateZoomControl();
}

void CanvasView::setFitBounds(Function<sprt::geom::Bounds()> &&fn) { _fitBounds = sp::move(fn); }

void CanvasView::fit(sprt::geom::FitAxis axis) {
	if (!_fitBounds) {
		return;
	}

	sprt::geom::FitConfig config;
	config.axis = axis;
	fit(_fitBounds(), config);
}

void CanvasView::setZoom(float zoom) {
	// The world centre is read through the old viewport, before the new zoom is applied.
	auto view = getViewport();
	const Vec2 screenCentre(_contentSize.width * 0.5f, _contentSize.height * 0.5f);
	const Vec2 worldCentre = view.toWorld(screenCentre);

	view.zoom = sprt::geom::clampZoom(zoom, _limits);
	view.offset = screenCentre - worldCentre * view.zoom;
	setViewport(view);
}

Vec2 CanvasView::worldLocation(const Vec2 &sceneLocation) const {
	return getViewport().toWorld(convertToNodeSpace(sceneLocation));
}

void CanvasView::attachGestures(InputListener *listener) {
	if (!listener) {
		return;
	}

	// Pan on the middle and right buttons; the left one belongs to the caller.
	listener->addSwipeRecognizer(
			[this](const GestureSwipe &swipe) {
		if (swipe.event == GestureEvent::Activated) {
			panBy(swipe.delta);
		}
		return true;
	},
			InputSwipeInfo{
				makeButtonMask({InputMouseButton::MouseMiddle, InputMouseButton::MouseRight})});

	listener->addScrollRecognizer([this](const GestureScroll &scroll) {
		// The scroll amount is converted to notches: ZoomStepRatio is per notch, and one notch is
		// `InputScrollNotch` of the amount.
		zoomAt(convertToNodeSpace(scroll.input->currentLocation),
				sprt::geom::wheelZoomRatio(scroll.amount.y / sprt::window::InputScrollNotch,
						ZoomStepRatio));
		return true;
	});
}

void CanvasView::panBy(const Vec2 &sceneDelta) {
	// The delta is in scene units; divide by the world scale to move in this node's space.
	_world->setPosition(_world->getPosition().xy()
			+ Vec2(sceneDelta.x / _surfaceScale.x, sceneDelta.y / _surfaceScale.y));
}

void CanvasView::handleExit() {
	// The menu's close callback points here.
	closeZoomMenu();
	Node::handleExit();
}

/* The control is built and laid out by hand, without a LayoutSystem. It paints itself because
a canvas may have no stylesheet (an unstyled ui::Panel is opaque white); the `canvas-zoom` type
is registered so a sheet can override it. */
void CanvasView::setZoomControlEnabled(bool value) {
	if (value == (_zoomControl != nullptr)) {
		return;
	}

	if (!value) {
		closeZoomMenu();
		_zoomControl->removeFromParent(true);
		_zoomControl = nullptr;
		_zoomOut = _zoomIn = _zoomValue = nullptr;
		_zoomArrow = nullptr;
		_zoomItems.clear();
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

	// One step each way, the same step as the wheel.
	_zoomOut = _zoomControl->addChild(Rc<Button>::create(StringView("-"),
			[this] { performZoomAction(CanvasZoomRequest{CanvasZoomAction::StepOut}); }));
	_zoomOut->setName("canvas-zoom-out");
	_zoomIn = _zoomControl->addChild(Rc<Button>::create(StringView("+"),
			[this] { performZoomAction(CanvasZoomRequest{CanvasZoomAction::StepIn}); }));
	_zoomIn->setName("canvas-zoom-in");

	// The readout is the menu's button: presets and the fits drop out of it.
	_zoomValue = _zoomControl->addChild(Rc<Button>::create([this] {
		if (isZoomMenuOpen()) {
			closeZoomMenu();
		} else {
			openZoomMenu();
		}
	}));
	_zoomValue->setName("canvas-zoom-value");
	_zoomArrow = _zoomValue->addChild(Rc<basic2d::IconSprite>::create(), ZOrder(2));
	_zoomArrow->setName("canvas-zoom-arrow");
	_zoomArrow->setIconName(IconName::Navigation_arrow_drop_down_solid);
	_zoomArrow->setContentSize(Size2(ZoomIconSize, ZoomIconSize));
	_zoomArrow->setAnchorPoint(Anchor::MiddleRight);
	_zoomArrow->setColor(ZoomControlInk);

	for (auto *b : {_zoomOut, _zoomIn, _zoomValue}) {
		b->setType("button");
		b->addStyleClass("canvas-zoom-button");
		b->setPathColor(Color4B(0x3A, 0x3A, 0x46, 0xFF), true);
		b->setBorderRadius(3.0f);
		b->setLabelColor(ZoomControlInk);
		if (auto label = b->getLabel()) {
			label->setFontSize(ZoomFontSize);
		}
	}

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

	float width = ZoomControlPadding * 2.0f + ZoomButtonSize * 2.0f + ZoomValueWidth
			+ ZoomControlGap * 2.0f;
	for (auto &it : _zoomItems) { width += ZoomControlGroupGap + it.width; }

	const Size2 size(width, ZoomControlHeight);
	_zoomControl->setContentSize(size);

	// The corner is an anchor of this node's box and the margin runs inward from it.
	_zoomControl->setPosition(Vec2((_contentSize.width - size.width) * _zoomCorner.x
					+ _zoomMargin * (1.0f - 2.0f * _zoomCorner.x),
			(_contentSize.height - size.height) * _zoomCorner.y
					+ _zoomMargin * (1.0f - 2.0f * _zoomCorner.y)));

	const float inner = size.height - ZoomControlPadding * 2.0f;
	const float mid = size.height * 0.5f;

	// Placed left to right with one cursor.
	float x = ZoomControlPadding;
	auto place = [&](Node *node, float w, float gapAfter) {
		node->setAnchorPoint(Vec2(0.0f, 0.5f));
		node->setContentSize(Size2(w, inner));
		node->setPosition(Vec2(x, mid));
		x += w + gapAfter;
	};

	place(_zoomOut, ZoomButtonSize, ZoomControlGap);
	place(_zoomValue, ZoomValueWidth, ZoomControlGap);
	place(_zoomIn, ZoomButtonSize, ZoomControlGroupGap);
	for (auto &it : _zoomItems) { place(it.node, it.width, ZoomControlGroupGap); }

	_zoomArrow->setPosition(Vec2(ZoomValueWidth - 2.0f, inner * 0.5f));
}

void CanvasView::updateZoomControl() {
	if (!_zoomValue) {
		return;
	}

	// Guarded on the shown percentage: pans write the transform often, and relabeling is costly.
	const auto percent = int32_t(sprt::lroundf(_world->getScale().x * 100.0f));
	if (percent == _zoomShown) {
		return;
	}
	_zoomShown = percent;
	// Nudged left of centre: the arrow takes the right end of the button.
	_zoomValue->setString(toString(percent, "%    "));
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

void CanvasView::setZoomPresets(SpanView<float> presets) { _zoomPresets = presets.vec<Interface>(); }

void CanvasView::setZoomActionCallback(Function<bool(const CanvasZoomRequest &)> &&cb) {
	_zoomAction = sp::move(cb);
}

void CanvasView::performZoomAction(const CanvasZoomRequest &req) {
	if (_zoomAction && _zoomAction(req)) {
		return;
	}
	switch (req.action) {
	case CanvasZoomAction::StepIn: zoomBy(ZoomStepRatio); break;
	case CanvasZoomAction::StepOut: zoomBy(1.0f / ZoomStepRatio); break;
	case CanvasZoomAction::Preset: setZoom(req.zoom); break;
	case CanvasZoomAction::FitWidth: fit(sprt::geom::FitAxis::Width); break;
	case CanvasZoomAction::FitHeight: fit(sprt::geom::FitAxis::Height); break;
	}
}

void CanvasView::setZoomMenuTitles(StringView fitWidth, StringView fitHeight) {
	_fitWidthTitle = fitWidth.str<Interface>();
	_fitHeightTitle = fitHeight.str<Interface>();
}

void CanvasView::setZoomMenuPreferNative(bool value) { _zoomMenuNative = value; }

bool CanvasView::openZoomMenu() {
	if (!_zoomValue || _zoomMenu) {
		return false;
	}
	auto window = getSubWindowParent(this);
	if (!window) {
		return false;
	}

	auto source = Rc<MenuSource>::create();
	const auto current = int32_t(sprt::lroundf(_world->getScale().x * 100.0f));
	for (auto zoom : _zoomPresets) {
		const auto percent = int32_t(sprt::lroundf(zoom * 100.0f));
		const auto name = toString("zoom-", percent);
		auto item = source->addButton(name, toString(percent, "%"),
				[this, zoom](NotNull<MenuSourceButton>) {
			performZoomAction(CanvasZoomRequest{CanvasZoomAction::Preset, zoom});
		});
		if (item) {
			item->setChecked(percent == current);
		}
	}
	source->addSeparator();
	if (auto item = source->addButton(StringView("fit-width"), _fitWidthTitle,
				IconName::Action_swap_horiz_solid, [this](NotNull<MenuSourceButton>) {
		performZoomAction(CanvasZoomRequest{CanvasZoomAction::FitWidth});
	})) {
		item->setEnabled(!!_fitBounds);
	}
	if (auto item = source->addButton(StringView("fit-height"), _fitHeightTitle,
				IconName::Action_swap_vert_solid, [this](NotNull<MenuSourceButton>) {
		performZoomAction(CanvasZoomRequest{CanvasZoomAction::FitHeight});
	})) {
		item->setEnabled(!!_fitBounds);
	}

	MenuConfig config;
	config.idPrefix = String("canvas-zoom-menu");
	config.preferNative = _zoomMenuNative;
	config.style.minWidth = ZoomValueWidth;
	config.onClose = [this] { _zoomMenu = nullptr; };

	_zoomMenu = openMenuForNode(window, _zoomValue, source, sp::move(config), MenuSide::Below);
	return _zoomMenu != nullptr;
}

void CanvasView::closeZoomMenu() {
	if (auto menu = sp::move(_zoomMenu)) {
		_zoomMenu = nullptr;
		menu->dismiss();
	}
}

Node *CanvasView::addZoomControlItem(Rc<Node> &&node, float width) {
	if (!_zoomControl || !node) {
		return nullptr;
	}
	auto ret = _zoomControl->addChild(sp::move(node));
	_zoomItems.emplace_back(ZoomItem{ret, width});
	layoutZoomControl();
	return ret;
}

void CanvasView::clearZoomControlItems() {
	for (auto &it : _zoomItems) { it.node->removeFromParent(true); }
	_zoomItems.clear();
	layoutZoomControl();
}

} // namespace stappler::xenolith::ui
