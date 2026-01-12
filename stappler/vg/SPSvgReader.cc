/**
Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>

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

#include "SPSvgReader.h"

namespace STAPPLER_VERSIONIZED stappler::vg {

static Mat4 svg_parseTransform(StringView &r) {
	Mat4 ret(Mat4::IDENTITY);
	while (!r.empty()) {
		r.skipChars<StringView::CharGroup<CharGroupId::WhiteSpace>>();
		if (r.is("matrix(")) {
			r += "matrix("_len;
			float values[6] = {0};

			uint16_t i = 0;
			for (; i < 6; ++i) {
				r.skipChars<StringView::CharGroup<CharGroupId::WhiteSpace>,
						StringView::Chars<','>>();
				if (!r.readFloat().grab(values[i])) {
					break;
				}
			}

			if (i != 6) {
				break;
			}

			ret *= Mat4(values[0], values[1], values[2], values[3], values[4], values[5]);

		} else if (r.is("translate(")) {
			r += "translate("_len;

			float tx = 0.0f, ty = 0.0f;
			r.skipChars<StringView::CharGroup<CharGroupId::WhiteSpace>>();
			if (!r.readFloat().grab(tx)) {
				break;
			}

			r.skipChars<StringView::CharGroup<CharGroupId::WhiteSpace>, StringView::Chars<','>>();
			if (!r.is(')')) {
				if (!r.readFloat().grab(ty)) {
					break;
				}
			}

			ret.m[12] += tx;
			ret.m[13] += ty;

		} else if (r.is("scale(")) {
			r += "scale("_len;

			float sx = 0.0f, sy = 0.0f;
			r.skipChars<StringView::CharGroup<CharGroupId::WhiteSpace>>();
			if (!r.readFloat().grab(sx)) {
				break;
			}

			r.skipChars<StringView::CharGroup<CharGroupId::WhiteSpace>, StringView::Chars<','>>();
			if (!r.is(')')) {
				if (!r.readFloat().grab(sy)) {
					break;
				}
			}

			ret.scale(sx, (sy == 0.0f) ? sx : sy, 1.0f);

		} else if (r.is("rotate(")) {
			r += "rotate("_len;

			float angle = 0.0f;
			float cx = 0.0f, cy = 0.0f;

			r.skipChars<StringView::CharGroup<CharGroupId::WhiteSpace>>();
			if (!r.readFloat().grab(angle)) {
				break;
			}

			r.skipChars<StringView::CharGroup<CharGroupId::WhiteSpace>, StringView::Chars<','>>();
			if (!r.is(')')) {
				if (!r.readFloat().grab(cx)) {
					break;
				}

				r.skipChars<StringView::CharGroup<CharGroupId::WhiteSpace>,
						StringView::Chars<','>>();

				if (!r.readFloat().grab(cy)) {
					break;
				}
			}

			if (cx == 0.0f && cy == 0.0f) {
				ret.rotateZ(math::to_rad(angle));
			} else {
				// optimize matrix translate operations
				ret.m[12] += cx;
				ret.m[13] += cy;

				ret.rotateZ(math::to_rad(angle));

				ret.m[12] -= cx;
				ret.m[13] -= cy;
			}

		} else if (r.is("skewX(")) {
			r += "skewX("_len;

			float angle = 0.0f;
			r.skipChars<StringView::CharGroup<CharGroupId::WhiteSpace>>();
			if (!r.readFloat().grab(angle)) {
				break;
			}
			ret *= Mat4(1, 0, tanf(math::to_rad(angle)), 1, 0, 0);

		} else if (r.is("skewY(")) {
			r += "skewY("_len;

			float angle = 0.0f;
			r.skipChars<StringView::CharGroup<CharGroupId::WhiteSpace>>();
			if (!r.readFloat().grab(angle)) {
				break;
			}

			ret *= Mat4(1, tanf(math::to_rad(angle)), 0, 1, 0, 0);
		}
		r.skipChars<StringView::CharGroup<CharGroupId::WhiteSpace>>();
		if (!r.is(')')) {
			break;
		} else {
			++r;
		}
	}
	return ret;
}

static Rect svg_readViewBox(StringView &r) {
	float values[4] = {0};

	uint16_t i = 0;
	for (; i < 4; ++i) {
		r.skipChars<StringView::CharGroup<CharGroupId::WhiteSpace>, StringView::Chars<','>>();
		if (!r.readFloat().grab(values[i])) {
			return Rect();
		}
	}

	return Rect(values[0], values[1], values[2], values[3]);
}

static float svg_readCoordValue(StringView &source, float origin) {
	Metric m;
	m.metric = Metric::Px;
	if (m.readStyleValue(source, false, true)) {
		switch (m.metric) {
		case Metric::Px: return m.value; break;
		case Metric::Percent: return m.value * origin; break;
		default: break;
		}
	}
	return nan();
}

static void svg_readPointCoords(PathWriter &target, StringView &source) {
	float x, y;
	while (!source.empty()) {
		source.skipChars<StringView::CharGroup<CharGroupId::WhiteSpace>, StringView::Chars<','>>();
		if (!source.readFloat().grab(x)) {
			return;
		}

		source.skipChars<StringView::CharGroup<CharGroupId::WhiteSpace>, StringView::Chars<','>>();
		if (!source.readFloat().grab(y)) {
			return;
		}

		if (target.empty()) {
			target.moveTo(x, y);
		} else {
			target.lineTo(x, y);
		}
	}
}

VectorPath &SvgTag::getPath() { return rpath; }

PathWriter &SvgTag::getWriter() {
	if (!writer) {
		writer = rpath.getWriter();
	}
	return writer;
}

SvgReader::SvgReader() { }

void SvgReader::onBeginTag(Parser &p, Tag &tag) {
	if (!p.tagStack.empty()) {
		tag.rpath.setParams(p.tagStack.back().rpath.getParams());
	}

	if (tag.name.equals("rect")) {
		tag.shape = SvgTag::Rect;
	} else if (tag.name.equals("circle")) {
		tag.shape = SvgTag::Circle;
	} else if (tag.name.equals("ellipse")) {
		tag.shape = SvgTag::Ellipse;
	} else if (tag.name.equals("line")) {
		tag.shape = SvgTag::Line;
	} else if (tag.name.equals("polyline")) {
		tag.shape = SvgTag::Polyline;
	} else if (tag.name.equals("polygon")) {
		tag.shape = SvgTag::Polygon;
	} else if (tag.name.equals("use")) {
		tag.shape = SvgTag::Use;
		tag.mat = Mat4::IDENTITY;
	} else if (tag.name.equals("g")) {
		tag.shape = SvgTag::None;
		tag.mat = Mat4::IDENTITY;
	} else if (tag.name.equals("path")) {
		tag.shape = SvgTag::Path;
	}
}

void SvgReader::onEndTag(Parser &p, Tag &tag, bool isClosed) {
	if (tag.name.equals("svg")) {
		_squareLength = sqrtf((_width * _width + _height * _height) / 2.0f);
	}

	switch (tag.shape) {
	case SvgTag::Rect:
		if (!isnan(tag.mat.m[0]) && !isnan(tag.mat.m[1]) && !isnan(tag.mat.m[2])
				&& tag.mat.m[2] > 0.0f && !isnan(tag.mat.m[3]) && tag.mat.m[3] > 0.0f
				&& (isnan(tag.mat.m[4]) || tag.mat.m[4] >= 0.0f)
				&& (isnan(tag.mat.m[5]) || tag.mat.m[5] >= 0.0f)) {
			tag.getWriter().addRect(tag.mat.m[0], tag.mat.m[1], tag.mat.m[2], tag.mat.m[3],
					tag.mat.m[4], tag.mat.m[5]);
		}
		break;
	case SvgTag::Circle:
		if (!isnan(tag.mat.m[0]) && !isnan(tag.mat.m[1]) && !isnan(tag.mat.m[2])
				&& tag.mat.m[2] >= 0.0f) {
			tag.getWriter().addCircle(tag.mat.m[0], tag.mat.m[1], tag.mat.m[2]);
		}
		break;
	case SvgTag::Ellipse:
		if (!isnan(tag.mat.m[0]) && !isnan(tag.mat.m[1]) && !isnan(tag.mat.m[2])
				&& !isnan(tag.mat.m[3]) && tag.mat.m[2] >= 0.0f && tag.mat.m[3] >= 0.0f) {
			tag.getWriter().addEllipse(tag.mat.m[0], tag.mat.m[1], tag.mat.m[2], tag.mat.m[3]);
		}
		break;
	case SvgTag::Line:
		if (!isnan(tag.mat.m[0]) && !isnan(tag.mat.m[1]) && !isnan(tag.mat.m[2])
				&& !isnan(tag.mat.m[3])) {
			tag.getWriter().moveTo(tag.mat.m[0], tag.mat.m[1]);
			tag.getWriter().lineTo(tag.mat.m[2], tag.mat.m[3]);
		}
		break;
	case SvgTag::Polygon:
		if (!tag.getWriter().empty()) {
			tag.getWriter().closePath();
		}
		break;
	default: break;
	}
}

void SvgReader::onStyleParameter(Tag &tag, StringReader &name, StringReader &value) {
	if (name.equals("opacity")) {
		value.readFloat().unwrap([&](float op) {
			if (op <= 0.0f) {
				tag.getPath().setFillOpacity(0);
				tag.getPath().setStrokeOpacity(0);
			} else if (op >= 1.0f) {
				tag.getPath().setFillOpacity(255);
				tag.getPath().setStrokeOpacity(255);
			} else {
				tag.getPath().setFillOpacity(255 * op);
				tag.getPath().setStrokeOpacity(255 * op);
			}
		});
	} else if (name.equals("fill")) {
		if (value.equals("none")) {
			tag.getPath().setStyle(tag.getPath().getStyle() & (~DrawFlags::Fill));
		} else {
			Color3B color;
			if (readColor(value, color)) {
				tag.getPath().setFillColor(color, true);
				tag.getPath().setStyle(tag.getPath().getStyle() | DrawFlags::Fill);
			}
		}
	} else if (name.equals("fill-rule")) {
		if (value.equals("nonzero")) {
			tag.getPath().setWindingRule(Winding::NonZero);
		} else if (value.equals("evenodd")) {
			tag.getPath().setWindingRule(Winding::EvenOdd);
		}
	} else if (name.equals("fill-opacity")) {
		value.readFloat().unwrap([&](float op) {
			if (op <= 0.0f) {
				tag.getPath().setFillOpacity(0);
			} else if (op >= 1.0f) {
				tag.getPath().setFillOpacity(255);
			} else {
				tag.getPath().setFillOpacity(255 * op);
			}
		});
	} else if (name.equals("stroke")) {
		if (value.equals("none")) {
			tag.getPath().setStyle(tag.getPath().getStyle() & (~DrawFlags::Stroke));
		} else {
			Color3B color;
			if (readColor(value, color)) {
				tag.getPath().setStrokeColor(color, true);
				tag.getPath().setStyle(tag.getPath().getStyle() | DrawFlags::Stroke);
			}
		}
	} else if (name.equals("stroke-opacity")) {
		value.readFloat().unwrap([&](float op) {
			if (op <= 0.0f) {
				tag.getPath().setStrokeOpacity(0);
			} else if (op >= 1.0f) {
				tag.getPath().setStrokeOpacity(255);
			} else {
				tag.getPath().setStrokeOpacity(255 * op);
			}
		});
	} else if (name.equals("stroke-width")) {
		auto val = svg_readCoordValue(value, _squareLength);
		if (!isnan(val)) {
			tag.getPath().setStrokeWidth(val);
		}
	} else if (name.equals("stroke-linecap")) {
		if (value.equals("butt")) {
			tag.getPath().setLineCup(LineCup::Butt);
		} else if (value.equals("round")) {
			tag.getPath().setLineCup(LineCup::Round);
		} else if (value.equals("square")) {
			tag.getPath().setLineCup(LineCup::Square);
		}
	} else if (name.equals("stroke-linejoin")) {
		if (value.equals("miter")) {
			tag.getPath().setLineJoin(LineJoin::Miter);
		} else if (value.equals("round")) {
			tag.getPath().setLineJoin(LineJoin::Round);
		} else if (value.equals("bevel")) {
			tag.getPath().setLineJoin(LineJoin::Bevel);
		}
	} else if (name.equals("stroke-miterlimit")) {
		value.readFloat().unwrap([&](float op) {
			if (op > 1.0f) {
				tag.getPath().setMiterLimit(op);
			}
		});
	} else if (name.equals("width") && tag.name.equals("svg")) {
		auto val = svg_readCoordValue(value, 0.0f);
		if (!isnan(val)) {
			_width = val;
		}
	} else if (name.equals("height") && tag.name.equals("svg")) {
		auto val = svg_readCoordValue(value, 0.0f);
		if (!isnan(val)) {
			_height = val;
		}
	}
}

void SvgReader::onStyle(Tag &tag, StringReader &value) {
	while (!value.empty()) {
		auto n = value.readUntil<StringReader::Chars<':'>>();
		n.trimChars<StringReader::WhiteSpace>();
		if (value.is(':')) {
			++value;
			auto v = value.readUntil<StringReader::Chars<';'>>();
			if (value.is(';')) {
				++value;
			}
			if (!n.empty() && !v.empty()) {
				onStyleParameter(tag, n, v);
			}
		}
	}
}

void SvgReader::onTagAttribute(Parser &p, Tag &tag, StringReader &name, StringReader &value) {
	if (tag.name.equals("svg")) {
		if (name.equals<StringCaseComparator>("height")) {
			auto val = svg_readCoordValue(value, 0.0f);
			if (!isnan(val)) {
				_height = val;
			}
		} else if (name.equals<StringCaseComparator>("width")) {
			auto val = svg_readCoordValue(value, 0.0f);
			if (!isnan(val)) {
				_width = val;
			}
		} else if (name.equals<StringCaseComparator>("viewbox")) {
			_viewBox = svg_readViewBox(value);
		} else if (name.equals<StringCaseComparator>("style")) {
			onStyle(tag, value);
		}
		return;
	} else if (tag.name.equals("path")) {
		if (name.equals("d")) {
			tag.getPath().init(value);
		}
	}

	if (name.equals("fill") || name.equals("fill-rule") || name.equals("fill-opacity")
			|| name.equals("stroke") || name.equals("stroke-opacity") || name.equals("stroke-width")
			|| name.equals("stroke-linecap") || name.equals("stroke-linejoin")
			|| name.equals("stroke-miterlimit") || name.equals("opacity")) {
		onStyleParameter(tag, name, value);
	} else if (name.equals("transform") && tag.shape != SvgTag::Use && tag.shape != SvgTag::None) {
		tag.getPath().applyTransform(svg_parseTransform(value));
	} else if (name.equals("style")) {
		onStyle(tag, value);
	} else if (name.equals("id")) {
		tag.id = value;
	} else {
		switch (tag.shape) {
		case SvgTag::Rect:
			if (name.equals("x")) {
				tag.mat.m[0] = svg_readCoordValue(value, _width);
			} else if (name.equals("y")) {
				tag.mat.m[1] = svg_readCoordValue(value, _height);
			} else if (name.equals("width")) {
				tag.mat.m[2] = svg_readCoordValue(value, _width);
			} else if (name.equals("height")) {
				tag.mat.m[3] = svg_readCoordValue(value, _height);
			} else if (name.equals("rx")) {
				tag.mat.m[4] = svg_readCoordValue(value, _width);
			} else if (name.equals("ry")) {
				tag.mat.m[5] = svg_readCoordValue(value, _height);
			}
			break;
		case SvgTag::Circle:
			if (name.equals("cx")) {
				tag.mat.m[0] = svg_readCoordValue(value, _width);
			} else if (name.equals("cy")) {
				tag.mat.m[1] = svg_readCoordValue(value, _height);
			} else if (name.equals("r")) {
				tag.mat.m[2] = svg_readCoordValue(value, _width);
			}
			break;
		case SvgTag::Ellipse:
			if (name.equals("cx")) {
				tag.mat.m[0] = svg_readCoordValue(value, _width);
			} else if (name.equals("cy")) {
				tag.mat.m[1] = svg_readCoordValue(value, _height);
			} else if (name.equals("rx")) {
				tag.mat.m[2] = svg_readCoordValue(value, _width);
			} else if (name.equals("ry")) {
				tag.mat.m[3] = svg_readCoordValue(value, _height);
			}
			break;
		case SvgTag::Line:
			if (name.equals("x1")) {
				tag.mat.m[0] = svg_readCoordValue(value, _width);
			} else if (name.equals("y1")) {
				tag.mat.m[1] = svg_readCoordValue(value, _height);
			} else if (name.equals("x2")) {
				tag.mat.m[2] = svg_readCoordValue(value, _width);
			} else if (name.equals("y2")) {
				tag.mat.m[3] = svg_readCoordValue(value, _height);
			}
			break;
		case SvgTag::Polyline:
			if (name.equals("points")) {
				svg_readPointCoords(tag.getWriter(), value);
			}
			break;
		case SvgTag::Polygon:
			if (name.equals("points")) {
				svg_readPointCoords(tag.getWriter(), value);
			}
			break;
		case SvgTag::Use:
		case SvgTag::None:
			if (name.equals("x")) {
				tag.mat.translate(svg_readCoordValue(value, _width), 0.0f, 0.0f);
			} else if (name.equals("y")) {
				tag.mat.translate(0.0f, svg_readCoordValue(value, _width), 0.0f);
			} else if (name.equals("transform")) {
				tag.mat.multiply(svg_parseTransform(value));
			} else if (name.equals("id")) {
				tag.id = value;
			} else if (name.equals("xlink:href") || name.equals("href")) {
				tag.ref = value;
			}
			break;
		default: break;
		}
	}
}

void SvgReader::onPushTag(Parser &p, Tag &tag) {
	if (tag.name == "defs") {
		_defs = true;
	}
}
void SvgReader::onPopTag(Parser &p, Tag &tag) {
	if (tag.name == "defs") {
		_defs = false;
	} else if (tag.shape != Tag::Shape::None) {
		emplacePath(tag);
	}
}

void SvgReader::onInlineTag(Parser &p, Tag &tag) { emplacePath(tag); }

void SvgReader::emplacePath(Tag &tag) {
	if (tag.shape == Tag::Shape::Use) {
		StringView ref(tag.ref);
		if (ref.is('#')) {
			++ref;
		}
		auto pathIt = _paths.find(ref);
		if (pathIt != _paths.end()) {
			if (_defs) {
				if (!tag.id.empty()) {
					VectorPath npath(pathIt->second);
					npath.applyTransform(tag.mat);
					_paths.emplace(tag.id.str<Interface>(), move(npath));
				}
			} else {
				if (tag.mat.isIdentity()) {
					_drawOrder.emplace_back(PathXRef{ref.str<Interface>()});
				} else {
					_drawOrder.emplace_back(
							PathXRef{ref.str<Interface>(), Interface::StringType(), tag.mat});
				}
			}
		}
	} else if (tag.rpath) {
		Interface::StringType idStr;
		StringView id(tag.id);
		if (id.empty()) {
			idStr = mem_std::toString("auto-", _nextId);
			++_nextId;
			id = idStr;
		}

		_paths.emplace(id.str<Interface>(), move(tag.rpath));
		if (!_defs) {
			_drawOrder.emplace_back(PathXRef{id.str<Interface>()});
		}
	}
}

} // namespace stappler::vg
