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

#include "SPDocLayoutBlock.h"
#include "SPDocLayoutEngine.h"
#include "SPDocLayoutResult.h"
#include "SPFontFace.h"
#include "SPDocNode.h"

namespace STAPPLER_VERSIONIZED stappler::document {

static void Layout_NodeInfo_init(LayoutBlock::NodeInfo &nodeInfo, Display parentContext) {
	// correction of display style property
	// actual value may be different for default/run-in
	// <img> tag is block by default, but inline-block inside <p> or in inline context
	if (nodeInfo.block.display == Display::Default || nodeInfo.block.display == Display::RunIn) {
		if (nodeInfo.node->getHtmlName() == "__value__") {
			nodeInfo.block.display = Display::Inline;
		} else if (parentContext == Display::Block) {
			nodeInfo.block.display = Display::Block;
		} else if (parentContext == Display::Inline) {
			if (nodeInfo.node->getHtmlName() == "img") {
				nodeInfo.block.display = Display::InlineBlock;
			} else {
				nodeInfo.block.display = Display::Inline;
			}
		}
	}

	if (nodeInfo.block.display == Display::InlineBlock) {
		if (parentContext == Display::Block) {
			nodeInfo.block.display = Display::Block;
		}
	}
}

static int64_t Layout_getListItemCount(LayoutEngine *builder, const Node &node,
		const StyleList &s) {
	auto &nodes = node.getNodes();
	int64_t counter = 0;
	for (auto &n : nodes) {
		auto style = builder->compileStyle(*n);
		auto display = style->get(ParameterName::CssDisplay, builder);
		auto listStyleType = style->get(ParameterName::CssListStyleType, builder);

		if (!listStyleType.empty() && !display.empty()
				&& display[0].value.display == Display::ListItem) {
			auto type = listStyleType[0].value.listStyleType;
			switch (type) {
			case ListStyleType::Decimal:
			case ListStyleType::DecimalLeadingZero:
			case ListStyleType::LowerAlpha:
			case ListStyleType::LowerGreek:
			case ListStyleType::LowerRoman:
			case ListStyleType::UpperAlpha:
			case ListStyleType::UpperRoman: ++counter; break;
			default: break;
			}
		}
	}
	return counter;
}


LayoutBlock::NodeInfo::NodeInfo(const Node *n, const StyleList *s, const StyleInterface *r,
		Display parentContext)
: node(n), style(s), block(s->compileBlockModel(r)), context(block.display) {
	Layout_NodeInfo_init(*this, parentContext);
}

LayoutBlock::NodeInfo::NodeInfo(const Node *n, const StyleList *s, BlockModelParameters &&b,
		Display parentContext)
: node(n), style(s), block(move(b)), context(block.display) {
	Layout_NodeInfo_init(*this, parentContext);
}

Rect LayoutBlock::PositionInfo::getInsideBoundingBox() const {
	return Rect(-padding.left - margin.left, -padding.top - margin.top,
			size.width + padding.left + padding.right + margin.left + margin.right,
			(!std::isnan(size.height) ? (size.height + padding.top + padding.bottom + margin.top
												+ margin.bottom)
									  : nan()));
}
Rect LayoutBlock::PositionInfo::getBoundingBox() const {
	return Rect(position.x - padding.left - margin.left, position.y - padding.top - margin.top,
			size.width + padding.left + padding.right + margin.left + margin.right,
			(!std::isnan(size.height) ? (size.height + padding.top + padding.bottom + margin.top
												+ margin.bottom)
									  : nan()));
}
Rect LayoutBlock::PositionInfo::getPaddingBox() const {
	return Rect(position.x - padding.left, position.y - padding.top,
			size.width + padding.left + padding.right,
			(!std::isnan(size.height) ? (size.height + padding.top + padding.bottom) : nan()));
}
Rect LayoutBlock::PositionInfo::getContentBox() const {
	return Rect(position.x, position.y, size.width, size.height);
}

void LayoutBlock::applyStyle(LayoutEngine *b, const Node *node, const BlockModelParameters &block,
		PositionInfo &pos, const Size2 &parentSize, Display context, ContentRequest req) {
	auto &_media = b->getMedia();

	float width = _media.computeValueStrong(block.width, parentSize.width);
	float minWidth = _media.computeValueStrong(block.minWidth, parentSize.width);
	float maxWidth = _media.computeValueStrong(block.maxWidth, parentSize.width);

	float height = _media.computeValueStrong(block.height, parentSize.height);
	float minHeight = _media.computeValueStrong(block.minHeight, parentSize.height);
	float maxHeight = _media.computeValueStrong(block.maxHeight, parentSize.height);

	if (node && node->getHtmlName() == "img") {
		auto srcPtr = node->getAttribute("src");
		if (b->isFileExists(srcPtr)) {
			auto img = b->getImage(srcPtr);
			if (img && img->width > 0 && img->height > 0) {
				if (isnan(width) && isnan(height)) {
					if (context == Display::InlineBlock) {
						const float scale = 0.9f * _media.fontScale;

						width = img->width * scale;
						height = img->height * scale;
					} else {
						width = img->width;
						height = img->height;
					}
				} else if (isnan(width)) {
					width = height * (float(img->width) / float(img->height));
				} else if (isnan(height)) {
					height = width * (float(img->height) / float(img->width));
				}

				if (width > parentSize.width) {
					auto scale = parentSize.width / width;
					width *= scale;
					height *= scale;
				}
			}
		}

		if ((_media.flags & RenderFlags::PaginatedLayout) != RenderFlags::None) {
			auto scale =
					std::max(width / _media.surfaceSize.width, height / _media.surfaceSize.height)
					* 1.05;
			if (scale > 1.0f) {
				width /= scale;
				height /= scale;
			}
		}
	}

	pos.padding.right = _media.computeValueAuto(block.paddingRight, parentSize.width);
	pos.padding.left = _media.computeValueAuto(block.paddingLeft, parentSize.width);

	pos.margin.right = _media.computeValueStrong(block.marginRight, parentSize.width);
	pos.margin.left = _media.computeValueStrong(block.marginLeft, parentSize.width);

	if (!std::isnan(minWidth) && (width < minWidth || std::isnan(width))) {
		width = minWidth;
	}

	if (!std::isnan(maxWidth) && (width > maxWidth || std::isnan(width))) {
		if (node && node->getHtmlName() == "img") {
			auto scale = maxWidth / width;
			height *= scale;
		}
		width = maxWidth;
	}

	if (std::isnan(width) && req == ContentRequest::Normal) {
		if (std::isnan(pos.margin.right)) {
			pos.margin.right = 0;
		}
		if (std::isnan(pos.margin.left)) {
			pos.margin.left = 0;
		}
		width = parentSize.width - pos.padding.left - pos.padding.right - pos.margin.left
				- pos.margin.right;
	}

	if (!std::isnan(height)) {
		if (!std::isnan(minHeight) && height < minHeight) {
			height = minHeight;
		}

		if (!std::isnan(maxHeight) && height > maxHeight) {
			if (node && node->getHtmlName() == "img") {
				auto scale = maxHeight / height;
				width *= scale;
			}
			height = maxHeight;
		}

		pos.minHeight = nan();
		pos.maxHeight = height;
	} else {
		pos.minHeight = minHeight;
		pos.maxHeight = maxHeight;
	}

	if (block.floating == Float::None && block.display != Display::Inline
			&& block.display != Display::InlineBlock && block.display != Display::TableCell
			&& block.display != Display::TableColumn) {
		if (std::isnan(pos.margin.right) && std::isnan(pos.margin.left)) {
			float contentWidth = width + pos.padding.left + pos.padding.right;
			pos.margin.right = pos.margin.left = (parentSize.width - contentWidth) / 2.0f;
		} else if (std::isnan(pos.margin.right)) {
			float contentWidth = width + pos.padding.left + pos.padding.right + pos.margin.left;
			pos.margin.right = parentSize.width - contentWidth;
		} else if (std::isnan(pos.margin.left)) {
			float contentWidth = width + pos.padding.left + pos.padding.right + pos.margin.right;
			pos.margin.left = parentSize.width - contentWidth;
		}
	} else {
		if (std::isnan(pos.margin.right)) {
			pos.margin.right = 0;
		}
		if (std::isnan(pos.margin.left)) {
			pos.margin.left = 0;
		}
	}

	pos.size = Size2(width, height);
	pos.padding.top = _media.computeValueAuto(block.paddingTop, pos.size.width);
	pos.padding.bottom = _media.computeValueAuto(block.paddingBottom, pos.size.width);

	if (context == Display::TableCell || context == Display::TableColumn) {
		const float nBase = pos.size.width + pos.padding.top + pos.padding.bottom;
		pos.margin.top = _media.computeValueAuto(block.marginTop, nBase);
		pos.margin.bottom = _media.computeValueAuto(block.marginBottom, nBase);
	}
}

void LayoutBlock::applyStyle(LayoutBlock &l, const Size2 &size, Display d) {
	applyStyle(l.engine, l.node.node, l.node.block, l.pos, size, d, l.request);
}

LayoutBlock::LayoutBlock(LayoutEngine *b, NodeInfo &&n, bool d, uint16_t dpth)
: engine(b), node(move(n)), depth(dpth) {
	pos.disablePageBreak = d;
}

LayoutBlock::LayoutBlock(LayoutEngine *b, NodeInfo &&n, PositionInfo &&p, uint16_t dpth)
: engine(b), node(move(n)), pos(move(p)), depth(dpth) { }

LayoutBlock::LayoutBlock(LayoutEngine *b, const Node *n, const StyleList *s, Display ctx, bool d,
		uint16_t dpth)
: engine(b), node(n, s, b, ctx), depth(dpth) {
	pos.disablePageBreak = d;
}

LayoutBlock::LayoutBlock(LayoutEngine *b, const Node *n, const StyleList *s,
		BlockModelParameters &&block, Display ctx, bool d)
: engine(b), node(n, s, move(block), ctx) {
	pos.disablePageBreak = d;
}

LayoutBlock::LayoutBlock(LayoutBlock &l, const Node *n, const StyleList *s)
: engine(l.engine), node(n, s, l.engine, l.node.context) {
	pos.disablePageBreak = l.pos.disablePageBreak;
	request = l.request;
}

bool LayoutBlock::init(const Vec2 &parentPos, const Size2 &parentSize, float collapsableMarginTop) {
	auto &_media = engine->getMedia();

	applyStyle(engine, node.node, node.block, pos, parentSize, node.context);

	if ((_media.flags & RenderFlags::PaginatedLayout) != RenderFlags::None
			&& !pos.disablePageBreak) {
		const float pageHeight = _media.surfaceSize.height;
		const float nextPos = parentPos.y;
		if (node.block.pageBreakBefore == PageBreak::Always) {
			uint32_t curr = uint32_t(std::floor(nextPos / pageHeight));
			pos.padding.top += (curr + 1) * pageHeight - nextPos + 1.0f;
			collapsableMarginTop = 0;
		} else if (node.block.pageBreakBefore == PageBreak::Left) {
			uint32_t curr = uint32_t(std::floor(nextPos / pageHeight));
			pos.padding.top += (curr + ((curr % 2 == 1) ? 1 : 2)) * pageHeight - nextPos + 1.0f;
		} else if (node.block.pageBreakBefore == PageBreak::Right) {
			uint32_t curr = uint32_t(std::floor(nextPos / pageHeight));
			pos.padding.top += (curr + ((curr % 2 == 0) ? 1 : 2)) * pageHeight - nextPos + 1.0f;
		}
	}

	if (node.block.marginTop.metric == Metric::Units::Percent && node.block.marginTop.value < 0) {
		pos.position = Vec2(parentPos.x + pos.margin.left + pos.padding.left,
				parentPos.y + pos.padding.top);
		pos.disablePageBreak = true;
	} else {
		applyVerticalMargin(collapsableMarginTop, parentPos.y);
		if (isnan(parentPos.y)) {
			pos.position = Vec2(parentPos.x + pos.margin.left + pos.padding.left,
					pos.margin.top + pos.padding.top);
		} else {
			pos.position = Vec2(parentPos.x + pos.margin.left + pos.padding.left,
					parentPos.y + pos.margin.top + pos.padding.top);
		}
	}

	node.context = engine->getLayoutContext(*node.node);
	if (node.node->getHtmlName() == "ol" || node.node->getHtmlName() == "ul") {
		listItem = LayoutBlock::ListForward;
		if (!node.node->getAttribute("reversed").empty()) {
			listItem = LayoutBlock::ListReversed;
			auto counter = Layout_getListItemCount(engine, *node.node, *node.style);
			if (counter != 0) {
				listItemIndex = counter;
			}
		}
		auto startPtr = node.node->getAttribute("start");
		if (!startPtr.empty()) {
			startPtr.readInteger().unwrap([&](int64_t id) { listItemIndex = id; });
		}
	}

	return true;
}

bool LayoutBlock::finalize(const Vec2 &parentPos, float collapsableMarginTop) {
	finalizeInlineContext();
	processBackground(parentPos.y);
	processOutline();
	cancelInlineContext();

	processListItemBullet(parentPos.y);
	processRef();

	auto &_media = engine->getMedia();

	if (node.block.marginTop.metric == Metric::Units::Percent && node.block.marginTop.value < 0) {
		applyVerticalMargin(collapsableMarginTop, parentPos.y);
		if (isnan(parentPos.y)) {
			pos.position = Vec2(parentPos.x + pos.margin.left + pos.padding.left,
					pos.margin.top + pos.padding.top);
		} else {
			pos.position = Vec2(parentPos.x + pos.margin.left + pos.padding.left,
					parentPos.y + pos.margin.top + pos.padding.top);
		}
	}
	if ((_media.flags & RenderFlags::PaginatedLayout) != RenderFlags::None
			&& !pos.disablePageBreak) {
		const float pageHeight = _media.surfaceSize.height;
		if (node.block.pageBreakInside == PageBreak::Avoid) {
			uint32_t curr1 = uint32_t(std::floor(pos.position.y / pageHeight));
			uint32_t curr2 = uint32_t(std::floor((pos.position.y + pos.size.height) / pageHeight));

			if (curr1 != curr2) {
				float offset = curr2 * pageHeight - pos.position.y + 1.0f;
				updatePosition(offset);
				pos.padding.top += offset;
			}
		}

		if (node.block.pageBreakAfter == PageBreak::Always) {
			auto bbox = getBoundingBox();
			auto nextPos = bbox.origin.y + bbox.size.height;
			uint32_t curr = uint32_t(std::floor((nextPos - pos.margin.bottom) / pageHeight));
			pos.padding.bottom += (curr + 1) * pageHeight - nextPos + 1.0f;
		} else if (node.block.pageBreakAfter == PageBreak::Left) {
			auto bbox = getBoundingBox();
			auto nextPos = bbox.origin.y + bbox.size.height;
			uint32_t curr = uint32_t(std::floor((nextPos - pos.margin.bottom) / pageHeight));
			pos.padding.bottom += (curr + ((curr % 2 == 1) ? 1 : 2)) * pageHeight - nextPos + 1.0f;
		} else if (node.block.pageBreakAfter == PageBreak::Right) {
			auto bbox = getBoundingBox();
			auto nextPos = bbox.origin.y + bbox.size.height;
			uint32_t curr = uint32_t(std::floor((nextPos - pos.margin.bottom) / pageHeight));
			pos.padding.bottom += (curr + ((curr % 2 == 0) ? 1 : 2)) * pageHeight - nextPos + 1.0f;
		} else if (node.block.pageBreakAfter == PageBreak::Avoid) {
			auto bbox = getBoundingBox();
			auto nextPos = bbox.origin.y + bbox.size.height;
			auto scanPos = nextPos + pos.size.width;

			uint32_t curr = uint32_t(std::floor((nextPos - pos.margin.bottom) / pageHeight));
			uint32_t scan = uint32_t(std::floor((scanPos - pos.margin.bottom) / pageHeight));

			if (curr != scan) { }
		}
	}

	return true;
}

void LayoutBlock::finalizeChilds(float height) {
	if (!layouts.empty() && pos.collapsableMarginBottom > 0.0f) {
		auto &newL = *layouts.back();
		float collapsableMarginBottom =
				std::max(newL.pos.collapsableMarginBottom, newL.pos.margin.bottom);
		if (pos.collapsableMarginBottom > collapsableMarginBottom) {
			pos.margin.bottom -= collapsableMarginBottom;
		} else {
			pos.margin.bottom = 0;
		}
	}

	if (isnan(pos.size.height) || pos.size.height < height) {
		pos.size.height = height;
	}

	if (!isnan(pos.minHeight) && pos.size.height < pos.minHeight) {
		pos.size.height = pos.minHeight;
	}
}

void LayoutBlock::applyVerticalMargin(float collapsableMarginTop, float parentPos) {
	auto &_media = engine->getMedia();

	const float nBase = pos.size.width + pos.padding.top + pos.padding.bottom;
	pos.margin.top = _media.computeValueAuto(node.block.marginTop, nBase);
	pos.margin.bottom = _media.computeValueAuto(node.block.marginBottom, nBase);

	if (pos.padding.top == 0) {
		if (collapsableMarginTop >= 0 && pos.margin.top >= 0) {
			if ((_media.flags & RenderFlags::PaginatedLayout) != RenderFlags::None) {
				const float pageHeight = _media.surfaceSize.height;
				uint32_t curr1 = uint32_t(std::floor(parentPos / pageHeight));
				uint32_t curr2 =
						uint32_t(std::floor((parentPos - collapsableMarginTop) / pageHeight));

				if (curr1 != curr2) {
					float offset = parentPos - (curr1 * pageHeight);
					if (collapsableMarginTop > pos.margin.top) {
						pos.margin.top = 0;
					} else {
						pos.margin.top -= collapsableMarginTop;
					}
					pos.margin.top -= offset;
					collapsableMarginTop = 0;
				}
			}

			if (pos.margin.top >= 0) {
				if (collapsableMarginTop >= pos.margin.top) {
					pos.margin.top = 0;
					pos.collapsableMarginTop = collapsableMarginTop;
				} else {
					pos.margin.top -= collapsableMarginTop;
					pos.collapsableMarginTop = pos.collapsableMarginTop + pos.margin.top;
				}
			}
		}
	}

	if (pos.padding.bottom == 0) {
		pos.collapsableMarginBottom = pos.margin.bottom;
	}
}

Rect LayoutBlock::getBoundingBox() const { return pos.getBoundingBox(); }
Rect LayoutBlock::getPaddingBox() const { return pos.getPaddingBox(); }
Rect LayoutBlock::getContentBox() const { return pos.getContentBox(); }

void LayoutBlock::updatePosition(float p) {
	pos.position.y += p;
	for (auto &l : layouts) { l->updatePosition(p); }
}

void LayoutBlock::setBoundPosition(const Vec2 &p) {
	auto newPos = p;
	newPos.x += (pos.margin.left + pos.padding.left);
	newPos.y += (pos.margin.top + pos.padding.top);
	auto diff = p - getBoundingBox().origin;
	pos.position = newPos;

	for (auto &l : layouts) {
		auto nodePos = l->getBoundingBox().origin + diff;
		l->setBoundPosition(nodePos);
	}
}

InlineContext &LayoutBlock::makeInlineContext(float parentPosY, const Node &n) {
	if (context && !context->finalized) {
		return *context;
	}

	const auto density = engine->getMedia().density;
	context = engine->acquireInlineContext(density);

	if (request == ContentRequest::Normal) {
		context->setTargetLabel(engine->getResult()->emplaceLabel(*this, ZOrderLabel));
	}

	pos.origin = Vec2(roundf(pos.position.x * density), roundf(parentPosY * density));

	size_t count = 0, nodes = 0;
	n.foreach ([&](const Node &node, size_t level) {
		count += node.getValue().size();
		++nodes;
	});

	FontStyleParameters fStyle = node.style->compileFontStyle(engine);
	ParagraphLayoutParameters pStyle = node.style->compileParagraphLayout(engine);

	context->targetLabel->layout.reserve(count, nodes);
	InlineContext::initFormatter(*this, fStyle, pStyle, parentPosY, context->reader);

	LayoutBlock *parent = engine->getTopLayout();
	if (!inlineInitialized && parent && parent->listItem != LayoutBlock::ListNone
			&& node.block.display == Display::ListItem
			&& node.block.listStylePosition == ListStylePosition::Inside) {
		auto textStyle = node.style->compileTextLayout(engine);
		WideString str = getListItemBulletString();
		context->reader.read(fStyle, textStyle, str, 0, 0);
		inlineInitialized = true;
	}

	return *context;
}

float LayoutBlock::fixLabelPagination(Label &label) {
	uint16_t offset = 0;
	auto &media = engine->getMedia();
	const float density = media.density;
	if ((media.flags & RenderFlags::PaginatedLayout) != RenderFlags::None
			&& !pos.disablePageBreak) {
		const Vec2 origin(pos.origin.x / density, pos.origin.y / density);
		const float pageHeight = media.surfaceSize.height;
		for (auto &it : label.layout.lines) {
			Rect rect = label.layout.getLineRect(it, density, origin);
			if (!rect.equals(Rect::ZERO)) {
				rect.origin.y += offset / density;

				uint32_t curr1 = uint32_t(std::floor(rect.origin.y / pageHeight));
				uint32_t curr2 =
						uint32_t(std::floor((rect.origin.y + rect.size.height) / pageHeight));

				if (curr1 != curr2) {
					offset +=
							uint16_t((curr2 * pageHeight - rect.origin.y) * density) + 1 * density;
				}
			}

			if (offset > 0 && it.count > 0) {
				it.pos += offset;
			}
		}
	}
	return offset / density;
}

float LayoutBlock::finalizeInlineContext() {
	if (!context || context->finalized) {
		return 0.0f;
	}

	const float density = engine->getMedia().density;

	context->reader.finalize();

	pos.maxExtent = context->reader.getMaxLineX() / density;

	if (node.block.floating != Float::None
			&& (node.block.width.value == 0 || std::isnan(node.block.width.value))) {
		pos.size.width = pos.maxExtent;
	}

	float offset =
			(node.block.floating == Float::None) ? fixLabelPagination(*context->targetLabel) : 0;
	float fin = context->reader.getHeight() / density + offset;
	if (isnan(pos.size.height)) {
		pos.size.height = fin;
	} else {
		if (!isnan(pos.maxHeight)) {
			if (pos.size.height + fin < pos.maxHeight) {
				pos.size.height += fin;
			} else {
				pos.size.height = pos.maxHeight;
			}
		} else {
			pos.size.height += fin;
		}
	}
	context->targetLabel->height = fin;

	if (!inlineBlockLayouts.empty()) {
		const Vec2 origin(pos.origin.x / density, pos.origin.y / density);
		for (LayoutBlock *it : inlineBlockLayouts) {
			if (auto inl = context->alignInlineContext(*it, origin)) {
				layouts.push_back(inl);
			}
		}

		inlineBlockLayouts.clear();
	}

	return fin;
}

void LayoutBlock::cancelInlineContext() {
	if (!context || context->finalized) {
		return;
	}

	context->finalize(*this);
	context = nullptr;
}

void LayoutBlock::cancelInlineContext(Vec2 &p, float &height, float &collapsableMarginTop) {
	if (context && !context->finalized) {
		float inlineHeight = finalizeInlineContext();
		if (inlineHeight > 0.0f) {
			height += inlineHeight;
			p.y += inlineHeight;
			collapsableMarginTop = 0;
		}
		cancelInlineContext();
	}
}

void LayoutBlock::processBackground(float parentPosY) {
	if (node.node && (node.node->getHtmlName() == "body" || node.node->getHtmlName() == "html")) {
		return;
	}

	auto style = node.style->compileBackground(engine);
	if (style.backgroundImage.empty()) {
		auto srcPtr = node.node->getAttribute("src");
		if (!srcPtr.empty()) {
			style.backgroundImage = srcPtr;
		}
	}

	StringView src = style.backgroundImage;

	if (isnan(pos.size.height)) {
		pos.size.height = 0;
	}

	if (src.empty()) {
		if (style.backgroundColor.a != 0 && !isnan(pos.size.height)) {
			if (node.node->getHtmlName() != "hr") {
				background = engine->getResult()->emplaceBackground(*this,
						Rect(-pos.padding.left, -pos.padding.top,
								pos.size.width + pos.padding.horizontal(),
								pos.size.height + pos.padding.vertical()),
						style, ZOrderLabel);
				objects.emplace_back(background);
			} else {
				float density = engine->getMedia().density;

				uint16_t height = uint16_t(
						(pos.size.height + pos.padding.top + pos.padding.bottom) * density);
				uint16_t linePos = uint16_t(-pos.padding.top * density);

				auto posPair = engine->getTextBounds(this, linePos, height, density, parentPosY);

				background = engine->getResult()->emplaceBackground(*this,
						Rect(-pos.padding.left + posPair.offset / density, -pos.padding.top,
								posPair.width / density + pos.padding.left + pos.padding.right,
								pos.size.height + pos.padding.top + pos.padding.bottom),
						style, ZOrderBackground);
				objects.emplace_back(background);
			}
		}
	} else if (pos.size.width > 0 && pos.size.height == 0
			&& engine->getMedia().shouldRenderImages()) {
		float width = pos.size.width + pos.padding.horizontal();
		float height = pos.size.height;
		auto img = engine->getImage(src);

		uint16_t w = width;
		uint16_t h = height;

		if (img && img->width != 0 && img->height != 0) {
			w = img->width;
			h = img->height;
		}

		float ratio = float(w) / float(h);
		if (height == 0) {
			height = width / ratio;
		}

		if (!isnan(pos.maxHeight) && height > pos.maxHeight + pos.padding.vertical()) {
			height = pos.maxHeight + pos.padding.vertical();
		}

		if (!isnan(pos.minHeight) && height < pos.minHeight + pos.padding.vertical()) {
			height = pos.minHeight + pos.padding.vertical();
		}

		pos.size.height = height - pos.padding.vertical();
		background = engine->getResult()->emplaceBackground(*this,
				Rect(-pos.padding.left, -pos.padding.top, pos.size.width + pos.padding.horizontal(),
						pos.size.height + pos.padding.vertical()),
				style, ZOrderBackground);
		objects.emplace_back(background);
	} else {
		background = engine->getResult()->emplaceBackground(*this,
				Rect(-pos.padding.left, -pos.padding.top, pos.size.width + pos.padding.horizontal(),
						pos.size.height + pos.padding.vertical()),
				style, ZOrderBackground);
		objects.emplace_back(background);
	}
}

void LayoutBlock::processOutline(bool withBorder) {
	auto style = node.style->compileOutline(engine);
	if ((node.block.display != Display::Table
				|| node.block.borderCollapse == BorderCollapse::Separate)
			&& withBorder) {
		if (style.left.style != BorderStyle::None || style.top.style != BorderStyle::None
				|| style.right.style != BorderStyle::None
				|| style.bottom.style != BorderStyle::None) {

			engine->getResult()->emplaceBorder(*this,
					Rect(-pos.padding.left, -pos.padding.top,
							pos.size.width + pos.padding.left + pos.padding.right,
							pos.size.height + pos.padding.top + pos.padding.bottom),
					style, pos.size.width, ZOrderBorder);
		}
	}

	if (style.outline.style != BorderStyle::None) {
		float width = engine->getMedia().computeValueAuto(style.outline.width, pos.size.width);
		if (style.outline.color.a != 0 && width != 0.0f && !isnan(pos.size.height)) {
			objects.emplace_back(engine->getResult()->emplaceOutline(*this,
					Rect(-pos.padding.left, -pos.padding.top,
							pos.size.width + pos.padding.left + pos.padding.right,
							pos.size.height + pos.padding.top + pos.padding.bottom),
					style.outline.color, ZOrderOutline, width, style.outline.style));
		}
	}
}

void LayoutBlock::processRef() {
	auto hrefPtr = node.node->getAttribute("href");
	if (!hrefPtr.empty()) {
		auto targetPtr = node.node->getAttribute("target");
		if (targetPtr.empty()
				&& (node.node->getHtmlName() == "img" || node.node->getHtmlName() == "figure")) {
			targetPtr = node.node->getAttribute("type");
		}

		auto extent = isnan(pos.maxExtent) ? pos.size.width : pos.maxExtent;

		auto res = engine->getResult();
		link = res->emplaceLink(*this,
				Rect(-pos.padding.left, -pos.padding.top,
						extent + pos.padding.left + pos.padding.right,
						pos.size.height + pos.padding.top + pos.padding.bottom),
				hrefPtr, targetPtr, node.node->getValueRecursive());
		objects.emplace_back(link);

		if (background) {
			background->link = link;
			link->source = background;
		}
	}
}

static float Layout_requestWidth(LayoutBlock &l, const Node &, LayoutBlock::ContentRequest req);

static void Layout_processInlineNode(LayoutBlock &l, const Node &node, const StyleList *style) {
	auto &media = l.engine->getMedia();

	auto v = node.getValue();
	if (!v.empty()) {
		const float density = media.density;
		auto fstyle = style->compileFontStyle(l.engine);
		auto font = l.engine->getFont(fstyle);
		if (font) {
			auto metrics = font->getMetrics();
			InlineContext &ctx = l.makeInlineContext(0.0f, node);
			auto bstyle = style->compileInlineModel(l.engine);
			auto front = uint16_t((media.computeValueAuto(bstyle.marginLeft, l.pos.size.width,
										   metrics.height / density)
										  + media.computeValueAuto(bstyle.paddingLeft,
												  l.pos.size.width, metrics.height / density))
					* density);
			auto back = uint16_t((media.computeValueAuto(bstyle.marginRight, l.pos.size.width,
										  metrics.height / density)
										 + media.computeValueAuto(bstyle.paddingRight,
												 l.pos.size.width, metrics.height / density))
					* density);

			auto textStyle = style->compileTextLayout(l.engine);

			ctx.reader.read(fstyle, textStyle, node.getValue(), front, back);
		}
	}
}

static void Layout_processInlineBlockNode(LayoutBlock &l, const Node &node,
		const StyleList *style) {
	LayoutBlock newL(l.engine, &node, style);
	LayoutBlock::applyStyle(newL, Size2(0.0f, 0.0f), l.node.context);

	auto &_media = l.engine->getMedia();
	auto pushItem = [&](float width) {
		InlineContext &ctx = l.makeInlineContext(0.0f, node);

		float extraWidth = 0.0f;
		const float margin = newL.pos.margin.horizontal();
		const float padding = newL.pos.padding.horizontal();
		if (!isnan(margin)) {
			extraWidth += margin;
		}
		if (!isnan(padding)) {
			extraWidth += padding;
		}

		auto fstyle = style->compileFontStyle(l.engine);
		auto textStyle = style->compileTextLayout(l.engine);
		ctx.reader.read(fstyle, textStyle, (width + extraWidth) * _media.density,
				ctx.reader.getLineHeight());
	};

	float minWidth = _media.computeValueStrong(newL.node.block.minWidth, 0.0f);
	float maxWidth = _media.computeValueStrong(newL.node.block.maxWidth, 0.0f);

	switch (newL.request) {
	case LayoutBlock::ContentRequest::Minimize:
		if (!isnan(minWidth)) {
			pushItem(minWidth);
			return;
		} else if (!isnan(newL.pos.size.width)) {
			pushItem(newL.pos.size.width);
			return;
		}
		break;
	case LayoutBlock::ContentRequest::Maximize:
		if (!isnan(maxWidth)) {
			pushItem(maxWidth);
			return;
		} else if (!isnan(newL.pos.size.width)) {
			pushItem(newL.pos.size.width);
			return;
		}
		break;
	case LayoutBlock::ContentRequest::Normal: return; break;
	}

	pushItem(Layout_requestWidth(newL, node, newL.request));
}

static float Layout_processTableNode(LayoutBlock &l, const Node &node, const StyleList *style) {
	log::warn("LayoutBuilder", "Table nodes for requestLayoutWidth is not supported");
	return 0.0f;
}

static float Layout_processBlockNode(LayoutBlock &l, const Node &node, const StyleList *style) {
	LayoutBlock newL(l.engine, &node, style);
	LayoutBlock::applyStyle(newL, Size2(0.0f, 0.0f), l.node.context);

	auto &_media = l.engine->getMedia();
	float minWidth = _media.computeValueStrong(newL.node.block.minWidth, 0.0f);
	float maxWidth = _media.computeValueStrong(newL.node.block.maxWidth, 0.0f);

	float extraWidth = 0.0f;
	const float margin = newL.pos.margin.horizontal();
	const float padding = newL.pos.padding.horizontal();
	if (!isnan(margin)) {
		extraWidth += margin;
	}
	if (!isnan(padding)) {
		extraWidth += padding;
	}

	switch (newL.request) {
	case LayoutBlock::ContentRequest::Minimize:
		if (!isnan(minWidth)) {
			return minWidth + extraWidth;
		} else if (!isnan(newL.pos.size.width)) {
			return newL.pos.size.width + extraWidth;
		}
		break;
	case LayoutBlock::ContentRequest::Maximize:
		if (!isnan(maxWidth)) {
			return maxWidth + extraWidth;
		} else if (!isnan(newL.pos.size.width)) {
			return newL.pos.size.width + extraWidth;
		}
		break;
	case LayoutBlock::ContentRequest::Normal: return 0.0f; break;
	}

	return Layout_requestWidth(newL, node, newL.request) + extraWidth;
}

static float Layout_requestWidth(LayoutBlock &l, const Node &node,
		LayoutBlock::ContentRequest req) {
	l.request = req;

	float ret = 0.0f;
	auto &media = l.engine->getMedia();
	bool finalize = (l.context == nullptr);

	if (finalize) {
		l.node.context = Display::RunIn;
	}

	auto finalizeInline = [&] {
		if (l.context) {
			l.context->reader.finalize();
			ret = max(ret, l.context->reader.getMaxLineX() / media.density);
			l.context->finalize();
			l.context = nullptr;
		}
	};

	auto &nodes = node.getNodes();
	for (auto it = nodes.begin(); it != nodes.end(); ++it) {
		l.engine->pushNode((*it));
		auto style = l.engine->compileStyle(**it);

		l.node.context = l.engine->getLayoutContext(nodes, it, l.node.context);

		auto d = l.engine->getNodeDisplay(**it, l.node.context);
		auto f = l.engine->getNodeFloating(**it);

		if (f != Float::None) {
			log::warn("LayoutBuilder", "Floating nodes for requestLayoutWidth is not supported");
		} else {
			switch (d) {
			case Display::None: break;
			case Display::Inline:
				Layout_processInlineNode(l, **it, style);
				if (!(*it)->getNodes().empty()) {
					Layout_requestWidth(l, **it, req);
				}
				break;
			case Display::InlineBlock:
				Layout_processInlineBlockNode(l, **it, style);
				if (!(*it)->getNodes().empty()) {
					Layout_requestWidth(l, **it, req);
				}
				break;
			case Display::Table:
				finalizeInline();
				ret = max(ret, Layout_processTableNode(l, **it, style));
				break;
			default:
				finalizeInline();
				ret = max(ret, Layout_processBlockNode(l, **it, style));
				break;
			}
		}

		l.engine->popNode();
	}

	if (finalize) {
		finalizeInline();
		if (req == LayoutBlock::ContentRequest::Minimize) {
			// log::format("Min", "%f", ret);
		} else {
			// log::format("Max", "%f", ret);
		}
	}

	return ret;
}

float LayoutBlock::requestWidth(LayoutEngine *b, const LayoutBlock::NodeInfo &node,
		LayoutBlock::ContentRequest req, const MediaParameters &media) {
	b->hookMedia(media);
	LayoutBlock tmpLayout(b, LayoutBlock::NodeInfo(node), true);
	tmpLayout.node.context = Display::Block;
	auto ret = Layout_requestWidth(tmpLayout, *tmpLayout.node.node, req);
	b->restoreMedia();
	return ret;
}

} // namespace stappler::document
