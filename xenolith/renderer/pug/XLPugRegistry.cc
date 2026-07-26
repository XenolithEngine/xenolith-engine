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

#include "XLPugRegistry.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::pugui {

Function<void()> BuilderContext::resolve(StringView name) const {
	if (resolveHandler && *resolveHandler) {
		return (*resolveHandler)(name);
	}
	return nullptr;
}

void BuilderContext::error(StringView msg) const {
	if (onError && *onError) {
		(*onError)(msg);
	} else {
		log::source().warn("pugui", msg);
	}
}

static bool parseFloat(const Value &v, float &out) {
	if (v.isInteger() || v.isDouble() || v.isBool() || v.isString()) {
		out = float(v.asDouble());
		return true;
	}
	return false;
}

static bool parseVec2(const Value &v, Vec2 &out) {
	if (v.isArray() && v.size() == 2) {
		out = Vec2(float(v.getDouble(0)), float(v.getDouble(1)));
		return true;
	}
	return false;
}

static bool parseAnchor(const Value &v, Vec2 &out) {
	if (parseVec2(v, out)) {
		return true;
	}
	if (v.isString()) {
		auto str = StringView(v.getString());
		if (str == "center" || str == "middle") {
			out = Anchor::Middle;
		} else if (str == "bottom-left") {
			out = Anchor::BottomLeft;
		} else if (str == "top-left") {
			out = Anchor::TopLeft;
		} else if (str == "bottom-right") {
			out = Anchor::BottomRight;
		} else if (str == "top-right") {
			out = Anchor::TopRight;
		} else if (str == "middle-left") {
			out = Anchor::MiddleLeft;
		} else if (str == "middle-right") {
			out = Anchor::MiddleRight;
		} else if (str == "middle-top") {
			out = Anchor::MiddleTop;
		} else if (str == "middle-bottom") {
			out = Anchor::MiddleBottom;
		} else {
			return false;
		}
		return true;
	}
	return false;
}

static bool parseColor(const Value &v, Color4F &out) {
	if (v.isString()) {
		Color4B color;
		if (sprt::geom::readColor(StringView(v.getString()), color)) {
			out = Color4F(color);
			return true;
		}
	}
	return false;
}

// float -> uniform; [t,r,b,l] -> per-side
static bool parsePadding(const Value &v, Padding &out) {
	float f = 0.0f;
	if (v.isArray() && v.size() == 4) {
		out.set(float(v.getDouble(0)), float(v.getDouble(1)), float(v.getDouble(2)),
				float(v.getDouble(3)));
		return true;
	} else if (parseFloat(v, f)) {
		out.set(f);
		return true;
	}
	return false;
}

static bool parseFlexDirection(StringView str, FlexDirection &out) {
	if (str == "row") {
		out = FlexDirection::Row;
	} else if (str == "row-reverse") {
		out = FlexDirection::RowReverse;
	} else if (str == "column") {
		out = FlexDirection::Column;
	} else if (str == "column-reverse") {
		out = FlexDirection::ColumnReverse;
	} else {
		return false;
	}
	return true;
}

static bool parseFlexWrap(StringView str, FlexWrap &out) {
	if (str == "nowrap") {
		out = FlexWrap::NoWrap;
	} else if (str == "wrap") {
		out = FlexWrap::Wrap;
	} else if (str == "wrap-reverse") {
		out = FlexWrap::WrapReverse;
	} else {
		return false;
	}
	return true;
}

static bool parseFlexJustify(StringView str, FlexJustify &out) {
	if (str == "flex-start" || str == "start") {
		out = FlexJustify::FlexStart;
	} else if (str == "flex-end" || str == "end") {
		out = FlexJustify::FlexEnd;
	} else if (str == "center") {
		out = FlexJustify::Center;
	} else if (str == "space-between") {
		out = FlexJustify::SpaceBetween;
	} else if (str == "space-around") {
		out = FlexJustify::SpaceAround;
	} else if (str == "space-evenly") {
		out = FlexJustify::SpaceEvenly;
	} else {
		return false;
	}
	return true;
}

static bool parseFlexAlign(StringView str, FlexAlign &out) {
	if (str == "auto") {
		out = FlexAlign::Auto;
	} else if (str == "flex-start" || str == "start") {
		out = FlexAlign::FlexStart;
	} else if (str == "flex-end" || str == "end") {
		out = FlexAlign::FlexEnd;
	} else if (str == "center") {
		out = FlexAlign::Center;
	} else if (str == "stretch") {
		out = FlexAlign::Stretch;
	} else if (str == "space-between") {
		out = FlexAlign::SpaceBetween;
	} else if (str == "space-around") {
		out = FlexAlign::SpaceAround;
	} else {
		return false;
	}
	return true;
}

static bool parseTextAlign(StringView str, font::TextAlign &out) {
	if (str == "left") {
		out = font::TextAlign::Left;
	} else if (str == "center") {
		out = font::TextAlign::Center;
	} else if (str == "right") {
		out = font::TextAlign::Right;
	} else if (str == "justify") {
		out = font::TextAlign::Justify;
	} else {
		return false;
	}
	return true;
}

static FlexLayoutInfo parseFlexLayoutInfo(const BuilderContext &ctx, const Value &attrs) {
	FlexLayoutInfo info;
	float f = 0.0f;
	for (auto &it : attrs.asDict()) {
		StringView name(it.first);
		auto &value = it.second;
		bool ok = true;
		if (name == "direction") {
			ok = value.isString() && parseFlexDirection(value.getString(), info.direction);
		} else if (name == "wrap") {
			ok = value.isString() && parseFlexWrap(value.getString(), info.wrap);
		} else if (name == "justify-content") {
			ok = value.isString() && parseFlexJustify(value.getString(), info.justifyContent);
		} else if (name == "align-items") {
			ok = value.isString() && parseFlexAlign(value.getString(), info.alignItems);
		} else if (name == "align-content") {
			ok = value.isString() && parseFlexAlign(value.getString(), info.alignContent);
		} else if (name == "gap") {
			if (value.isArray() && value.size() == 2) {
				info.rowGap = float(value.getDouble(0));
				info.columnGap = float(value.getDouble(1));
			} else if (parseFloat(value, f)) {
				info.rowGap = info.columnGap = f;
			} else {
				ok = false;
			}
		} else if (name == "row-gap") {
			ok = parseFloat(value, info.rowGap);
		} else if (name == "column-gap") {
			ok = parseFloat(value, info.columnGap);
		} else if (name == "padding") {
			ok = parsePadding(value, info.padding);
		} else {
			continue;
		}
		if (!ok) {
			ctx.error(toString("pug: flex: invalid value for '", name, "'"));
		}
	}
	return info;
}

// flex item parameters are collected into the FlexItemInfo component of any node;
// they only take effect when the parent runs a LayoutSystem in flex mode
static bool applyFlexItemAttribute(const BuilderContext &ctx, Node *node, StringView name,
		const Value &value) {
	FlexItemInfo info;
	if (auto current = LayoutSystem::getItem(node)) {
		info = *current;
	}

	bool ok = true;
	if (name == "flex-grow") {
		ok = parseFloat(value, info.grow);
	} else if (name == "flex-shrink") {
		ok = parseFloat(value, info.shrink);
	} else if (name == "flex-basis") {
		ok = parseFloat(value, info.basis);
	} else if (name == "cross-size") {
		ok = parseFloat(value, info.crossSize);
	} else if (name == "align-self") {
		ok = value.isString() && parseFlexAlign(value.getString(), info.alignSelf);
	} else if (name == "order") {
		info.order = int32_t(value.asInteger());
	} else if (name == "min-main") {
		ok = parseFloat(value, info.minMain);
	} else if (name == "max-main") {
		ok = parseFloat(value, info.maxMain);
	} else if (name == "margin") {
		ok = parsePadding(value, info.margin);
	} else {
		return false;
	}

	if (!ok) {
		ctx.error(toString("pug: invalid value for flex item attribute '", name, "'"));
		return true;
	}

	LayoutSystem::setItem(node, info);
	return true;
}

bool Registry::applyGenericAttribute(const BuilderContext &ctx, Node *node, StringView name,
		const Value &value) {
	Vec2 vec;
	Color4F color;
	float f = 0.0f;

	if (name == "position") {
		if (parseVec2(value, vec)) {
			node->setPosition(vec);
			return true;
		}
	} else if (name == "x") {
		if (parseFloat(value, f)) {
			node->setPositionX(f);
			return true;
		}
	} else if (name == "y") {
		if (parseFloat(value, f)) {
			node->setPositionY(f);
			return true;
		}
	} else if (name == "size" || name == "content-size") {
		if (parseVec2(value, vec)) {
			node->setContentSize(Size2(vec.x, vec.y));
			return true;
		}
	} else if (name == "anchor" || name == "anchor-point") {
		if (parseAnchor(value, vec)) {
			node->setAnchorPoint(vec);
			return true;
		}
	} else if (name == "color") {
		if (parseColor(value, color)) {
			node->setColor(color);
			return true;
		}
	} else if (name == "opacity") {
		if (parseFloat(value, f)) {
			node->setOpacity(f);
			return true;
		}
	} else if (name == "visible") {
		node->setVisible(value.asBool());
		return true;
	} else if (name == "scale") {
		if (parseVec2(value, vec)) {
			node->setScale(vec);
			return true;
		} else if (parseFloat(value, f)) {
			node->setScale(f);
			return true;
		}
	} else if (name == "rotation") {
		if (parseFloat(value, f)) {
			node->setRotation(f * float(M_PI) / 180.0f);
			return true;
		}
	} else if (name == "z-index" || name == "z-order") {
		node->setLocalZOrder(ZOrder(int16_t(value.asInteger())));
		return true;
	} else if (name == "id" || name == "name") {
		if (value.isString()) {
			// NodeIdentity::name doubles as the css #id for the style systems
			node->setName(value.getString());
			return true;
		}
	} else if (name == "tag") {
		node->setTag(uint64_t(value.asInteger()));
		return true;
	} else if (name == "class") {
		// css classes for the style systems (NodeIdentity::classes)
		if (value.isString()) {
			StringView(value.getString()).split<StringView::WhiteSpace>([&](StringView cl) {
				node->addStyleClass(cl);
			});
			return true;
		}
	} else if (name == "style") {
		// inline css declarations lost their backend in the StyleIdentity ->
		// NodeIdentity migration; report instead of dropping them silently
		if (value.isString()) {
			ctx.error("pug: inline `style` attribute is not supported yet");
			return true;
		}
	} else if (name.starts_with("data-")) {
		Value data(node->getDataValue());
		data.setValue(value, name.str<memory::StandartInterface>());
		node->setDataValue(move(data));
		return true;
	} else if (applyFlexItemAttribute(ctx, node, name, value)) {
		return true;
	}

	if (name == "position" || name == "x" || name == "y" || name == "size"
			|| name == "content-size" || name == "anchor" || name == "anchor-point"
			|| name == "color" || name == "opacity" || name == "scale" || name == "rotation"
			|| name == "id" || name == "name") {
		ctx.error(toString("pug: invalid value for attribute '", name, "'"));
		return true;
	}

	return false;
}

Rc<Registry> Registry::createDefault() {
	auto ret = Rc<Registry>::alloc();

	ret->set("node", TagFactory{
		.create = [](const BuilderContext &, StringView, const Value &) -> Rc<Node> {
			return Rc<Node>::create();
		},
	});

	ret->set("layer", TagFactory{
		.create = [](const BuilderContext &, StringView, const Value &attrs) -> Rc<Node> {
			Color4F color = Color4F::WHITE;
			parseColor(attrs.getValue("color"), color);
			return Rc<Layer>::create(color);
		},
	});

	ret->set("label", TagFactory{
		.create = [](const BuilderContext &, StringView, const Value &) -> Rc<Node> {
			return Rc<Label>::create();
		},
		.applyAttribute = [](const BuilderContext &ctx, Node *node, StringView name,
				const Value &value) -> bool {
			auto label = static_cast<Label *>(node);
			float f = 0.0f;
			if (name == "font-size") {
				if (parseFloat(value, f)) {
					label->setFontSize(uint16_t(f));
				}
				return true;
			} else if (name == "align") {
				font::TextAlign align = font::TextAlign::Left;
				if (value.isString() && parseTextAlign(value.getString(), align)) {
					label->setAlignment(align);
				} else {
					ctx.error(toString("pug: label: invalid align '", value.getString(), "'"));
				}
				return true;
			} else if (name == "width") {
				if (parseFloat(value, f)) {
					label->setWidth(f);
				}
				return true;
			}
			return false;
		},
		.applyText = [](const BuilderContext &, Node *node, StringView text) {
			static_cast<Label *>(node)->setString(text);
		},
	});

	TagFactory spriteFactory{
		.create = [](const BuilderContext &ctx, StringView, const Value &attrs) -> Rc<Node> {
			auto &src = attrs.getValue("src");
			if (src.isString()) {
				return Rc<VectorSprite>::create(FileInfo(src.getString()));
			}
			ctx.error("pug: sprite: missing 'src' attribute");
			return Rc<Node>::create();
		},
		.applyAttribute = [](const BuilderContext &, Node *, StringView name,
				const Value &) -> bool {
			return name == "src"; // consumed by create
		},
	};
	ret->set("sprite", TagFactory(spriteFactory));
	ret->set("image", move(spriteFactory));

	ret->set("flex", TagFactory{
		.create = [](const BuilderContext &ctx, StringView, const Value &attrs) -> Rc<Node> {
			auto node = Rc<Node>::create();
			node->addSystem(Rc<LayoutSystem>::create(parseFlexLayoutInfo(ctx, attrs)));
			return node;
		},
		.applyAttribute = [](const BuilderContext &, Node *, StringView name,
				const Value &) -> bool {
			// container parameters are consumed by create
			return name == "direction" || name == "wrap" || name == "justify-content"
					|| name == "align-items" || name == "align-content" || name == "gap"
					|| name == "row-gap" || name == "column-gap" || name == "padding";
		},
	});

	ret->set("button", TagFactory{
		.create = [](const BuilderContext &ctx, StringView, const Value &attrs) -> Rc<Node> {
			return Rc<Button>::create(ctx.resolve(attrs.getString("on-tap")));
		},
		.applyAttribute = [](const BuilderContext &, Node *node, StringView name,
				const Value &value) -> bool {
			if (name == "on-tap") {
				return true; // consumed by create
			} else if (name == "enabled") {
				static_cast<Button *>(node)->setEnabled(value.asBool());
				return true;
			}
			return false;
		},
	});

	ret->set("button-label", TagFactory{
		.create = [](const BuilderContext &ctx, StringView, const Value &attrs) -> Rc<Node> {
			return Rc<ButtonWithLabel>::create(StringView(),
					ctx.resolve(attrs.getString("on-tap")));
		},
		.applyAttribute = [](const BuilderContext &, Node *node, StringView name,
				const Value &value) -> bool {
			if (name == "on-tap") {
				return true; // consumed by create
			} else if (name == "enabled") {
				static_cast<ButtonWithLabel *>(node)->setEnabled(value.asBool());
				return true;
			}
			return false;
		},
		.applyText = [](const BuilderContext &, Node *node, StringView text) {
			static_cast<ButtonWithLabel *>(node)->setString(text);
		},
	});

	return ret;
}

void Registry::set(StringView tag, TagFactory &&factory) {
	auto it = _tags.find(tag);
	if (it != _tags.end()) {
		it->second = move(factory);
	} else {
		_tags.emplace(tag.str<memory::StandartInterface>(), move(factory));
	}
}

const TagFactory *Registry::get(StringView tag) const {
	auto it = _tags.find(tag);
	if (it != _tags.end()) {
		return &it->second;
	}
	return nullptr;
}

} // namespace stappler::xenolith::pugui
