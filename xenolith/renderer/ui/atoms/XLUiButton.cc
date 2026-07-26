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

#include "XLUiButton.h"
#include "XL2dIconSprite.h"
#include "XLUiStyleResolver.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

// Register the per-attribute style appliers for nodes of type "button" once, the first time a
// Button is constructed. This is the "resolve styles by type, without a per-instance callback"
// hook: the recursive StyleResolver on the button applies these instead of the generic defaults.
static void ensureButtonStyleAppliers() {
	using document::ParameterName;
	static bool once = [] {
		StyleResolver::registerTypeApplier("button",
				[](StyleResolver &res, Node *node, const ResolvedStyle &s,
						document::ParameterName name, const document::StyleValue &val) {
			if (auto btn = dynamic_cast<Button *>(node)) {
				return btn->setStyleValue(s, name, val);
			}
			return false;
		},
				StyleResolver::makeParameterMask({
					ParameterName::CssBackgroundColor,
					ParameterName::CssOutlineColor,
					ParameterName::CssOutlineWidth,
					ParameterName::CssBorderTopLeftRadius,
					ParameterName::CssBorderTopRightRadius,
					ParameterName::CssBorderBottomRightRadius,
					ParameterName::CssBorderBottomLeftRadius,
					ParameterName::CmdReset,
				}));
		return true;
	}();
	(void)once;
}

ComponentId ButtonStyleComponent::Id;

Button::~Button() { }

bool Button::init(Function<void()> &&cb) {
	if (!VectorSprite::init()) {
		return false;
	}

	ensureButtonStyleAppliers();

	_callback = cb;

	setType("button");
	addStyleClass("xl-ui-button");
	setRenderingLevel(RenderingLevel::Surface);

	_label = addChild(Rc<basic2d::Label>::create(), ZOrder(1));
	_label->setType("label");
	_label->addStyleClass("xl-ui-button-label");
	_label->setVisible(false);

	_icon = addChild(Rc<basic2d::IconSprite>::create(), ZOrder(2));
	_icon->setType("icon");
	_icon->addStyleClass("xl-ui-button-icon");
	_icon->setVisible(false);

	_listener = addSystem(Rc<InputListener>::create());
	_listener->addMouseOverRecognizer([this](const GestureData &data) {
		switch (data.event) {
		case GestureEvent::Began:
			setOrUpdateComponent<InteractiveComponent>([](NotNull<InteractiveComponent> state) {
				return state->handleHover(1); //
			});
			break;
		case GestureEvent::Activated: break;
		case GestureEvent::Ended:
		case GestureEvent::Cancelled:
			setOrUpdateComponent<InteractiveComponent>([](NotNull<InteractiveComponent> state) {
				return state->handleHover(-1); //
			});
			break;
		}
		return true;
	}, false);

	addSystem(Rc<StyleResolver>::create(true));

	return true;
}

void Button::handleContentSizeDirty() {
	VectorSprite::handleContentSizeDirty();
	updateBackgroundImage();
	// label / icon are positioned by CSS layout (e.g. `button { display: flex }`) applied through
	// the recursive StyleResolver - the button no longer places them itself
}

void Button::updateBackgroundImage() {
	if (_contentSize.width <= 0.0f || _contentSize.height <= 0.0f) {
		return;
	}

	// the resolved paint lives in a ButtonStyleComponent; when the button was never styled the
	// component is absent and the struct's own defaults (white fill, no outline, no radius) apply
	ButtonStyleComponent defaultStyle;
	const ButtonStyleComponent *style = &defaultStyle;
	if (auto c = getComponent<ButtonStyleComponent>()) {
		style = c;
	}

	auto image = Rc<VectorImage>::create(_contentSize);

	// inset the rect by half the stroke width so the outline is not clipped at the node's edges
	const float inset = style->outlineWidth > 0.0f ? style->outlineWidth * 0.5f : 0.0f;
	const Rect box(inset, inset, _contentSize.width - inset * 2.0f,
			_contentSize.height - inset * 2.0f);

	// shrink each corner radius by the inset so the OUTER edge of the stroke keeps the requested
	// radius; addBox() itself clamps each corner to the half-box and resolves adjacent overlap
	auto outer = [&](float r) { return r > 0.0f ? sprt::max(r - inset, 0.0f) : 0.0f; };
	const float rtl = outer(style->borderRadiusTopLeft);
	const float rtr = outer(style->borderRadiusTopRight);
	const float rbr = outer(style->borderRadiusBottomRight);
	const float rbl = outer(style->borderRadiusBottomLeft);
	const bool rounded = rtl > 0.0f || rtr > 0.0f || rbr > 0.0f || rbl > 0.0f;

	auto path = image->addPath();
	path->openForWriting([&](PathWriter &writer) {
		if (rounded) {
			writer.addBox(box.origin.x, box.origin.y, box.size.width, box.size.height, rtl, rtr,
					rbr, rbl);
		} else {
			writer.addRect(box);
		}
	})
			.setFillColor(style->backgroundColor)
			.setStyle(vg::DrawFlags::Fill);

	if (style->outlineWidth > 0.0f) {
		path->setStyle(vg::DrawFlags::FillAndStroke)
				.setStrokeColor(style->outlineColor)
				.setStrokeWidth(style->outlineWidth)
				.setAntialiased(true);
	}

	setImage(sp::move(image));
}

bool Button::setStyleValue(const ResolvedStyle &style, document::ParameterName name,
		const document::StyleValue &value) {
	using document::ParameterName;

	// `-xl-reset` (CmdReset): drop the styling so the fallback defaults take over on the next rebuild
	if (name == ParameterName::CmdReset) {
		if (removeComponent<ButtonStyleComponent>()) {
			markContentSizeDirty();
		}
		return true;
	}

	// all button paint is stored in the ButtonStyleComponent (created on first styled attribute);
	// writes are equality-guarded so an unchanged value neither rebuilds nor re-dirties components
	bool known = true;
	bool changed = false;
	setOrUpdateComponent<ButtonStyleComponent>([&](NotNull<ButtonStyleComponent> c) {
		// raw px magnitude of the metric (em/% are not resolved here)
		const float px = value.sizeValue.value;
		switch (name) {
		case ParameterName::CssBackgroundColor:
			changed = c->backgroundColor != value.color4;
			c->backgroundColor = value.color4;
			break;
		case ParameterName::CssOutlineColor:
			changed = c->outlineColor != value.color4;
			c->outlineColor = value.color4;
			break;
		case ParameterName::CssOutlineWidth:
			changed = c->outlineWidth != px;
			c->outlineWidth = px;
			break;
		case ParameterName::CssBorderTopLeftRadius:
			changed = c->borderRadiusTopLeft != px;
			c->borderRadiusTopLeft = px;
			break;
		case ParameterName::CssBorderTopRightRadius:
			changed = c->borderRadiusTopRight != px;
			c->borderRadiusTopRight = px;
			break;
		case ParameterName::CssBorderBottomRightRadius:
			changed = c->borderRadiusBottomRight != px;
			c->borderRadiusBottomRight = px;
			break;
		case ParameterName::CssBorderBottomLeftRadius:
			changed = c->borderRadiusBottomLeft != px;
			c->borderRadiusBottomLeft = px;
			break;
		default: known = false; break;
		}
		return changed;
	});

	if (!known) {
		slog().warn("ui::Button", "Unknown style parameter: ", name);
		return false;
	}
	if (changed) {
		markContentSizeDirty();
	}
	return true;
}

void Button::setString(StringView str) {
	if (_label) {
		_label->setString(str);
		_label->setVisible(!str.empty());
	}
}

StringView Button::getString() const {
	if (_label) {
		return _label->getString8();
	}
	return StringView();
}

void Button::setIcon(IconName name) {
	if (_icon) {
		_icon->setIconName(name);
		_icon->setVisible(name != IconName::None);
	}
}

IconName Button::getIcon() const {
	if (_icon) {
		return _icon->getIconName();
	}
	return IconName::None;
}


} // namespace stappler::xenolith::ui
