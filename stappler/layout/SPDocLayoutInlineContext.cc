/**
 Copyright (c) 2024 Stappler LLC <admin@stappler.dev>
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#include "SPDocLayoutInlineContext.h"
#include "SPDocLayoutBlock.h"
#include "SPDocLayoutEngine.h"
#include "SPDocLayoutResult.h"
#include "SPFontFace.h"
#include "SPFontFormatter.h"

namespace STAPPLER_VERSIONIZED stappler::document {

void InlineContext::initFormatter(LayoutBlock &l, const FontStyleParameters &fStyle,
		const ParagraphLayoutParameters &pStyle, float parentPosY, font::Formatter &reader) {
	auto &_media = l.engine->getMedia();

	auto baseFont = l.engine->getFont(fStyle);
	auto fontMetrics = baseFont->getMetrics();

	float _lineHeightMod = 1.0f;
	bool _lineHeightIsAbsolute = false;
	float _lineHeight = fontMetrics.height;
	uint16_t width = uint16_t(roundf(l.pos.size.width * _media.density));

	if (pStyle.lineHeight.metric == Metric::Units::Em
			|| pStyle.lineHeight.metric == Metric::Units::Percent
			|| pStyle.lineHeight.metric == Metric::Units::Auto) {
		if (pStyle.lineHeight.value > 0.0f) {
			_lineHeightMod = pStyle.lineHeight.value;
		}
	} else if (pStyle.lineHeight.metric == Metric::Units::Px) {
		_lineHeight = roundf(pStyle.lineHeight.value * _media.density);
		_lineHeightIsAbsolute = true;
	}

	if (!_lineHeightIsAbsolute) {
		_lineHeight = uint16_t(fontMetrics.height * _lineHeightMod);
	}

	reader.setFontCallback([e = l.engine](const FontStyleParameters &f) { return e->getFont(f); });

	if (l.request != LayoutBlock::ContentRequest::Normal) {
		reader.setRequest(l.request);
		reader.setLinePositionCallback(nullptr);
	} else {
		reader.setRequest(LayoutBlock::ContentRequest::Normal);
		reader.setLinePositionCallback(
				[l = &l, parentPosY](uint16_t &pos, uint16_t &height, float density) {
			return l->engine->getTextBounds(l, pos, height, density, parentPosY);
		});
		if (width > 0) {
			reader.setWidth(width);
		}
	}

	reader.setTextAlignment(pStyle.textAlign);
	if (_lineHeightIsAbsolute) {
		reader.setLineHeightAbsolute(_lineHeight);
	} else {
		reader.setLineHeightRelative(_lineHeightMod);
	}

	if (auto h = l.engine->getHyphens()) {
		reader.setHyphens(h);
	}

	reader.begin(uint16_t(roundf(_media.computeValueAuto(pStyle.textIndent, l.pos.size.width,
										 fontMetrics.height / _media.density)
						 * _media.density)),
			0);
}

void InlineContext::initFormatter(LayoutBlock &l, float parentPosY, font::Formatter &reader) {
	FontStyleParameters fStyle = l.node.style->compileFontStyle(l.engine);
	ParagraphLayoutParameters pStyle = l.node.style->compileParagraphLayout(l.engine);
	initFormatter(l, fStyle, pStyle, parentPosY, reader);
}

InlineContext::InlineContext(font::Formatter::FontCallback &&cb, float d) {
	targetLabel = &phantomLabel;
	reader.setFontCallback(sp::move(cb));
	reader.reset(&targetLabel->layout);
	density = d;

	idPos.clear();
}

void InlineContext::setTargetLabel(Label *label) {
	targetLabel = label;
	reader.reset(&targetLabel->layout);
}

void InlineContext::pushNode(const Node *node, const NodeCallback &cb) {
	if (!finalized) {
		nodes.emplace_back(pair(node, cb));
	}
}

void InlineContext::popNode(const Node *node) {
	if (!finalized && !nodes.empty()) {
		if (nodes.back().first == node) {
			if (nodes.back().second) {
				nodes.back().second(*this);
			}
			nodes.pop_back();
		} else {
			for (auto it = nodes.begin(); it != nodes.end(); it++) {
				if (it->first == node) {
					if (it->second) {
						it->second(*this);
					}
					nodes.erase(it);
					break;
				}
			}
		}
	}
}

void InlineContext::finalize(LayoutBlock &l) {
	auto res = l.engine->getResult();
	const float density = l.engine->getMedia().density;
	const Vec2 origin(l.pos.origin.x / density - l.pos.position.x,
			l.pos.origin.y / density - l.pos.position.y);

	for (auto &it : backgroundPos) {
		targetLabel->layout.getLabelRects([&](geom::Rect rect) {
			l.objects.emplace_back(
					res->emplaceBackground(l, rect, it.background, ZOrderBackground));
		}, it.firstCharId, it.lastCharId, density, origin, it.padding);
	}

	for (auto &it : outlinePos) {
		targetLabel->layout.getLabelRects([&](geom::Rect rect) {
			if (it.style.left.style != BorderStyle::None || it.style.top.style != BorderStyle::None
					|| it.style.right.style != BorderStyle::None
					|| it.style.bottom.style != BorderStyle::None) {
				res->emplaceBorder(l, rect, it.style, 1.0f, ZOrderBorder);
			}
			if (it.style.outline.style != BorderStyle::None) {
				l.objects.emplace_back(
						res->emplaceOutline(l, rect, it.style.outline.color, ZOrderBorder,
								l.engine->getMedia().computeValueAuto(it.style.outline.width, 1.0f),
								it.style.outline.style));
			}
		}, it.firstCharId, it.lastCharId, density, origin);
	}

	for (auto &it : refPos) {
		WideString text;
		targetLabel->layout.str([&](char32_t ch) { unicode::utf16Encode(text, ch); },
				it.firstCharId, it.lastCharId);
		targetLabel->layout.getLabelRects([&](geom::Rect rect) {
			l.objects.emplace_back(res->emplaceLink(l, rect, it.target, it.mode, text));
		}, it.firstCharId, it.lastCharId, density, origin);
	}

	for (auto &it : idPos) { res->pushIndex(it.id, Vec2(0.0f, l.pos.origin.y / density)); }

	float final = targetLabel->height;
	targetLabel->bbox = Rect(origin.x, origin.y, l.pos.size.width, final);
	if (targetLabel != &phantomLabel) {
		if (l.engine->isMediaHooked()) {
			auto &orig = l.engine->getOriginalMedia();
			auto &media = l.engine->getMedia();
			auto scale = media.fontScale / orig.fontScale;
			if (scale < 0.75f) {
				targetLabel->preview = true;
			}
		}
		l.objects.emplace_back(targetLabel);
	}
	finalize();
}

void InlineContext::finalize() {
	if (!nodes.empty()) {
		for (auto it = nodes.rbegin(); it != nodes.rend(); it++) {
			if (it->second) {
				it->second(*this);
			}
		}
		nodes.clear();
	}
	refPos.clear();
	idPos.clear();
	outlinePos.clear();
	backgroundPos.clear();
	phantomLabel.layout.clear();
	targetLabel = &phantomLabel;
	finalized = true;
}

void InlineContext::reset() {
	finalized = false;
	reader.reset(&phantomLabel.layout);
}

LayoutBlock *InlineContext::alignInlineContext(LayoutBlock &inl, const Vec2 &origin) {
	const font::RangeLayoutData &r = targetLabel->layout.ranges.at(inl.charBinding);
	const font::CharLayoutData &c = targetLabel->layout.chars.at(r.start + r.count - 1);
	auto line = targetLabel->layout.getLine(r.start + r.count - 1);
	if (line) {
		// Calculations for baseline are ALWAYS integer, ignore compiler complaints.
		int16_t baseline = (int16_t(r.metrics.size) - int16_t(r.metrics.height));

		switch (r.align) {
		case VerticalAlign::Baseline:
			inl.setBoundPosition(origin
					+ Vec2(c.pos / density,
							(line->pos - inl.pos.size.height * density + baseline) / density));
			break;
		case VerticalAlign::Sub:
			inl.setBoundPosition(origin
					+ Vec2(c.pos / density,
							(line->pos - inl.pos.size.height * density
									+ int16_t(baseline - r.metrics.descender / 2))
									/ density));
			break;
		case VerticalAlign::Super:
			inl.setBoundPosition(origin
					+ Vec2(c.pos / density,
							(line->pos - inl.pos.size.height * density
									+ int16_t(baseline - r.metrics.ascender / 2))
									/ density));
			break;
		case VerticalAlign::Middle:
			inl.setBoundPosition(origin
					+ Vec2(c.pos / density,
							(line->pos - (r.height + inl.pos.size.height * density) / 2)
									/ density));
			break;
		case VerticalAlign::Top:
			inl.setBoundPosition(origin + Vec2(c.pos / density, (line->pos - r.height) / density));
			break;
		case VerticalAlign::Bottom:
			inl.setBoundPosition(origin
					+ Vec2(c.pos / density, (line->pos - inl.pos.size.height * density) / density));
			break;
		}

		return &inl;
	}
	return nullptr;
}

} // namespace stappler::document
