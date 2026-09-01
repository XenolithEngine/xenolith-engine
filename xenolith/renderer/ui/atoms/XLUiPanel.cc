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

#include "XLUiPanel.h"
#include "XLUiStyleResolver.h"
#include "XL2dScrollView.h"
#include "XL2dLayerRounded.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

ComponentId PanelStyleComponent::Id;

Panel::~Panel() { }

bool Panel::init() {
	if (!VectorSprite::init()) {
		return false;
	}

	setType("panel");
	addStyleClass("xl-ui-panel");
	setRenderingLevel(RenderingLevel::Surface);
	registerStyleAppliers("panel");
	return true;
}

void Panel::registerStyleAppliers(StringView type) {
	using document::ParameterName;

	// The appliers are the same for every surface atom, so a second registration for the same type
	// would only rebuild an identical callback. Registry and node graph both live on the app
	// thread, so a plain set is enough to keep this to once per type.
	static Set<String> s_registered;
	if (!s_registered.emplace(type.str<mem_std::Interface>()).second) {
		return;
	}

	StyleResolver::registerTypeApplier(type,
			[](StyleResolver &, Node *node, const ResolvedStyle &s, document::ParameterName name,
					const document::StyleValue &val) {
		if (auto p = dynamic_cast<Panel *>(node)) {
			return p->setStyleValue(s, name, val);
		}
		return false;
	},
			StyleResolver::makeParameterMask({
				ParameterName::CssBackgroundColor,
				ParameterName::CssOutlineColor,
				ParameterName::CssOutlineWidth,
				ParameterName::CssOutlineStyle,
				ParameterName::CssBorderTopLeftRadius,
				ParameterName::CssBorderTopRightRadius,
				ParameterName::CssBorderBottomRightRadius,
				ParameterName::CssBorderBottomLeftRadius,
				ParameterName::CmdReset,
			}));
}

void Panel::handleContentSizeDirty() {
	VectorSprite::handleContentSizeDirty();
	updateBackgroundImage();
}

void Panel::updateBackgroundImage() {
	if (_contentSize.width <= 0.0f || _contentSize.height <= 0.0f) {
		return;
	}

	// the resolved paint lives in a PanelStyleComponent; when the widget was never styled the
	// component is absent and the struct's own defaults (white fill, no outline, no radius) apply
	PanelStyleComponent defaultStyle;
	const PanelStyleComponent *style = &defaultStyle;
	if (auto c = getComponent<PanelStyleComponent>()) {
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
			writer.addBox(box.origin.x, box.origin.y, box.size.width, box.size.height,
					/* addBox TL = visual bottom-left  */ rbl,
					/* addBox TR = visual bottom-right */ rbr,
					/* addBox BR = visual top-right    */ rtr,
					/* addBox BL = visual top-left     */ rtl);
		} else {
			writer.addRect(box);
		}
	})
			.setFillColor(style->backgroundColor)
			.setStyle(vg::DrawFlags::Fill)
			// a hard-edged rect needs none of it; rounded corners and strokes do
			.setAntialiased(rounded || style->outlineWidth > 0.0f);

	if (style->outlineWidth > 0.0f && style->outlineStyle != document::BorderStyle::None) {
		path->setStyle(vg::DrawFlags::FillAndStroke)
				.setStrokeColor(style->outlineColor)
				.setStrokeWidth(style->outlineWidth);

		// CSS fixes neither the dash lengths nor the cap; these are the proportions browsers
		// settled on. A dot is a zero-length dash, which is only visible through a round cap.
		const float w = style->outlineWidth;
		switch (style->outlineStyle) {
		case document::BorderStyle::Dashed: {
			const float dashes[] = {w * 3.0f, w * 2.0f};
			path->setDashArray(SpanView<float>(dashes, 2));
			break;
		}
		case document::BorderStyle::Dotted: {
			const float dots[] = {0.0f, w * 2.0f};
			path->setLineCup(vg::LineCup::Round).setDashArray(SpanView<float>(dots, 2));
			break;
		}
		default: break;
		}
	}

	setImage(sp::move(image));
}

void Panel::updateStyle(const Callback<bool(NotNull<PanelStyleComponent>)> &cb) {
	bool changed = false;
	setOrUpdateComponent<PanelStyleComponent>([&](NotNull<PanelStyleComponent> c) {
		changed = cb(c);
		return changed;
	});
	if (changed) {
		markContentSizeDirty();
	}
}

void Panel::setPathColor(const Color4B &color, bool withOpacity) {
	// The alpha a colourless call keeps is the OWN layer's, not the one a stylesheet may have put
	// on the node a moment ago: this is the widget painting itself, and what it is amending is its
	// own previous paint.
	_ownPainted = true;
	_ownStyle.backgroundColor =
			withOpacity ? color : Color4B(color.r, color.g, color.b, _ownStyle.backgroundColor.a);

	updateStyle([&](NotNull<PanelStyleComponent> c) {
		if (c->backgroundColor == _ownStyle.backgroundColor) {
			return false;
		}
		c->backgroundColor = _ownStyle.backgroundColor;
		return true;
	});
}

Color4B Panel::getPathColor() const {
	if (auto c = getComponent<PanelStyleComponent>()) {
		return c->backgroundColor;
	}
	return PanelStyleComponent().backgroundColor;
}

void Panel::setBorderRadius(float radius) {
	_ownPainted = true;
	_ownStyle.borderRadiusTopLeft = _ownStyle.borderRadiusTopRight = radius;
	_ownStyle.borderRadiusBottomRight = _ownStyle.borderRadiusBottomLeft = radius;

	updateStyle([&](NotNull<PanelStyleComponent> c) {
		if (c->borderRadiusTopLeft == radius && c->borderRadiusTopRight == radius
				&& c->borderRadiusBottomRight == radius && c->borderRadiusBottomLeft == radius) {
			return false;
		}
		c->borderRadiusTopLeft = c->borderRadiusTopRight = radius;
		c->borderRadiusBottomRight = c->borderRadiusBottomLeft = radius;
		return true;
	});
}

float Panel::getBorderRadius() const {
	if (auto c = getComponent<PanelStyleComponent>()) {
		return c->borderRadiusTopLeft;
	}
	return PanelStyleComponent().borderRadiusTopLeft;
}

void Panel::setOutline(const Color4B &color, float width) {
	_ownPainted = true;
	_ownStyle.outlineColor = color;
	_ownStyle.outlineWidth = width;

	updateStyle([&](NotNull<PanelStyleComponent> c) {
		if (c->outlineColor == color && c->outlineWidth == width) {
			return false;
		}
		c->outlineColor = color;
		c->outlineWidth = width;
		return true;
	});
}

bool Panel::setStyleValue(const ResolvedStyle &, document::ParameterName name,
		const document::StyleValue &value) {
	using document::ParameterName;

	/* CmdReset arrives before the parameters of every style pass (it is not a CSS property, and no
	stylesheet can produce it). It undoes the previous pass in full: whatever this pass still
	declares is re-applied below, and whatever it no longer declares is gone - which is the only way
	a rule that stopped matching can be undone.

	IT UNDOES STYLING, NOT PAINT. What the widget put on itself through setPathColor and friends is
	not a declaration and no pass owns it, so the reset rewinds the component to that layer instead
	of dropping it; only a widget that never painted itself ends up with no component at all, which
	is the state the defaults in updateBackgroundImage stand for. Storing both layers in the one
	component (as this did) made them indistinguishable, and a widget painted from code lost its
	paint to the first resolver pass that touched it. */
	if (name == ParameterName::CmdReset) {
		if (_ownPainted) {
			updateStyle([&](NotNull<PanelStyleComponent> c) {
				if (*c == _ownStyle) {
					return false;
				}
				*c = _ownStyle;
				return true;
			});
		} else if (removeComponent<PanelStyleComponent>()) {
			markContentSizeDirty();
		}
		return true;
	}

	// all paint is stored in the PanelStyleComponent (created on the first styled attribute);
	// writes are equality-guarded so an unchanged value neither rebuilds nor re-dirties components
	bool known = true;
	updateStyle([&](NotNull<PanelStyleComponent> c) {
		// raw px magnitude of the metric (em/% are not resolved here)
		const float px = value.sizeValue.value;
		bool changed = false;
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
		case ParameterName::CssOutlineStyle:
			changed = c->outlineStyle != value.borderStyle;
			c->outlineStyle = value.borderStyle;
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
		slog().warn("ui::Panel", "Unknown style parameter: ", name);
		return false;
	}
	return true;
}

namespace {

// A Panel answering to a type of its own.
//
// A derived class rather than a free function because Panel::registerStyleAppliers is protected:
// the appliers route through Panel::setStyleValue, so whoever registers a type has to BE a Panel.
class ScrollIndicatorPanel : public Panel {
public:
	virtual bool init(StringView type) {
		if (!Panel::init()) {
			return false;
		}

		// Panel::init() made this a `panel`, and it is not one: a sheet's panel rules paint cards
		// and dialogs, and the scroll bar has no business inheriting them
		removeStyleClass("xl-ui-panel");
		setType(type);
		registerStyleAppliers(type);
		return true;
	}

protected:
	using Panel::init;
};

// Carry over what the node being replaced actually PAINTED with.
//
// Not the node's colour: LayerRounded keeps its fill in a path colour and leaves the node's own
// colour to mean opacity - the track is created black with alpha 0 - so copying that as a tint
// would multiply every colour a sheet later asks for by black. The fill goes to the fill, and the
// tint is reset to white, which leaves setIndicatorColor() meaning exactly what it meant before:
// on a white fill a tint IS the colour.
static void UiPanel_adoptIndicatorPaint(Node *from, ScrollIndicatorPanel *to) {
	if (auto layer = dynamic_cast<basic2d::LayerRounded *>(from)) {
		to->setPathColor(layer->getPathColor(), true);
		to->setBorderRadius(layer->getBorderRadius());
	} else {
		to->setPathColor(Color4B(from->getColor()), false);
	}
}

} // namespace

void useStyledScrollIndicator(NotNull<basic2d::ScrollView> view) {
	// Idempotent: TreeView and TableView each ask for this in init(), and an application that also
	// asks must not end up with a second pair of nodes
	if (dynamic_cast<Panel *>(view->getIndicatorNode())) {
		return;
	}

	auto track = Rc<ScrollIndicatorPanel>::create(StringView("scroll-indicator-track"));
	auto thumb = Rc<ScrollIndicatorPanel>::create(StringView("scroll-indicator"));
	if (!track || !thumb) {
		return;
	}

	// Read the old paint BEFORE the swap: setIndicator*Node re-imposes the view's own geometry and
	// identity on the new node, and the old one is gone by the time it returns
	UiPanel_adoptIndicatorPaint(view->getIndicatorTrackNode(), track);
	UiPanel_adoptIndicatorPaint(view->getIndicatorNode(), thumb);

	// The track first: it is the thumb's parent, and swapping it moves whatever thumb is current
	view->setIndicatorTrackNode(Rc<Node>(track.get()));
	view->setIndicatorNode(Rc<Node>(thumb.get()));

	// setIndicatorNode carried the old colour across as a tint, which for a Panel would multiply
	// the fill just adopted. The fill is the colour now
	thumb->setColor(Color4F::WHITE, false);
	track->setColor(Color4F::WHITE, false);
}

} // namespace stappler::xenolith::ui
