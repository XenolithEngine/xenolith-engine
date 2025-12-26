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

#ifndef EXTRA_DOCUMENT_LAYOUT_SPDOCLAYOUTBLOCK_H_
#define EXTRA_DOCUMENT_LAYOUT_SPDOCLAYOUTBLOCK_H_

#include "SPDocLayoutFloatContext.h"
#include "SPDocLayoutInlineContext.h"
#include "SPDocNode.h"
#include "SPFontFormatter.h"

namespace STAPPLER_VERSIONIZED stappler::document {

class LayoutEngine;
struct Object;

struct SP_PUBLIC LayoutBlock : public memory::AllocPool,
							   public InterfaceObject<memory::PoolInterface> {
	using ContentRequest = font::Formatter::ContentRequest;

	struct NodeInfo {
		const Node *node = nullptr;
		const StyleList *style = nullptr;

		BlockModelParameters block;
		Display context = Display::Block;

		NodeInfo() = default;
		NodeInfo(const Node *, const StyleList *, const StyleInterface *, Display = Display::None);
		NodeInfo(const Node *, const StyleList *, BlockModelParameters &&, Display = Display::None);
	};

	struct PositionInfo {
		Padding padding;
		Margin margin;

		Vec2 position;
		Vec2 origin;
		Size2 size;

		float minHeight = stappler::nan();
		float maxHeight = stappler::nan();
		float maxExtent = stappler::nan();

		float collapsableMarginTop = 0.0f;
		float collapsableMarginBottom = 0.0f;

		bool disablePageBreak = false;

		Rect getInsideBoundingBox() const;
		Rect getBoundingBox() const;
		Rect getPaddingBox() const;
		Rect getContentBox() const;
	};

	static void applyStyle(LayoutEngine *b, const Node *, const BlockModelParameters &,
			PositionInfo &, const Size2 &size, Display = Display::Block,
			ContentRequest = ContentRequest::Normal);

	static void applyStyle(LayoutBlock &, const Size2 &size, Display);

	static float requestWidth(LayoutEngine *b, const NodeInfo &n, ContentRequest,
			const MediaParameters &);

	LayoutBlock(LayoutEngine *, NodeInfo &&, bool disablePageBreak, uint16_t = 0);
	LayoutBlock(LayoutEngine *, NodeInfo &&, PositionInfo &&, uint16_t = 0);

	LayoutBlock(LayoutEngine *, const Node *, const StyleList *, Display = Display::None,
			bool disablePageBreak = false, uint16_t = 0);
	LayoutBlock(LayoutEngine *, const Node *, const StyleList *, BlockModelParameters &&,
			Display = Display::None, bool disablePageBreak = false);

	LayoutBlock(LayoutBlock &, const Node *, const StyleList *);

	LayoutBlock(LayoutBlock &&) = delete;
	LayoutBlock &operator=(LayoutBlock &&) = delete;

	LayoutBlock(const LayoutBlock &) = delete;
	LayoutBlock &operator=(const LayoutBlock &) = delete;

	bool init(const Vec2 &, const Size2 &, float collapsableMarginTop);
	bool finalize(const Vec2 &, float collapsableMarginTop);
	void finalizeChilds(float height);

	void applyVerticalMargin(float collapsableMarginTop, float pos);

	Rect getBoundingBox() const;
	Rect getPaddingBox() const;
	Rect getContentBox() const;

	void updatePosition(float pos);
	void setBoundPosition(const Vec2 &pos);

	void initFormatter(const FontStyleParameters &fStyle, const ParagraphLayoutParameters &pStyle,
			float parentPosY, font::Formatter &reader, bool initial);
	void initFormatter(float parentPosY, font::Formatter &reader, bool initial);

	float fixLabelPagination(Label &label);

	InlineContext &makeInlineContext(float parentPosY, const Node &node);
	float finalizeInlineContext();
	void cancelInlineContext();
	void cancelInlineContext(Vec2 &, float &height, float &collapsableMarginTop);

	void processBackground(float parentPosY);
	void processListItemBullet(float);
	void processOutline(bool withBorder = true);
	void processRef();

	WideString getListItemBulletString();

	LayoutEngine *engine = nullptr;

	NodeInfo node;
	PositionInfo pos;

	Vector<Object *> objects;
	Vector<LayoutBlock *> layouts;

	int64_t listItemIndex = 1;
	enum : uint8_t {
		ListNone,
		ListForward,
		ListReversed
	} listItem = ListNone;

	bool inlineInitialized = false;
	InlineContext *context = nullptr;
	Vector<LayoutBlock *> inlineBlockLayouts;
	size_t charBinding = 0;
	ContentRequest request = ContentRequest::Normal;

	uint16_t depth = 0;
	Background *background = nullptr;
	Link *link = nullptr;
};

} // namespace stappler::document

#endif /* EXTRA_DOCUMENT_LAYOUT_SPDOCLAYOUTBLOCK_H_ */
