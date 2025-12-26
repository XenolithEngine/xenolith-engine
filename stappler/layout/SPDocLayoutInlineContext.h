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

#ifndef EXTRA_DOCUMENT_LAYOUT_SPDOCLAYOUTINLINECONTEXT_H_
#define EXTRA_DOCUMENT_LAYOUT_SPDOCLAYOUTINLINECONTEXT_H_

#include "SPDocLayoutObject.h"
#include "SPFontFormatter.h"
#include "SPDocNode.h"

namespace STAPPLER_VERSIONIZED stappler::document {

struct LayoutBlock;

struct SP_PUBLIC InlineContext : public memory::AllocPool,
								 public InterfaceObject<memory::PoolInterface> {
	using NodeCallback = Function<void(InlineContext &ctx)>;

	struct RefPosInfo {
		uint16_t firstCharId;
		uint16_t lastCharId;

		String target;
		String mode;
	};

	struct OutlinePosInfo {
		uint16_t firstCharId;
		uint16_t lastCharId;

		OutlineParameters style;
	};

	struct BackgroundPosInfo {
		uint16_t firstCharId;
		uint16_t lastCharId;

		BackgroundParameters background;
		Padding padding;
	};

	struct IdPosInfo {
		uint16_t firstCharId;
		uint16_t lastCharId;

		StringView id;
	};

	static void initFormatter(LayoutBlock &l, const FontStyleParameters &fStyle,
			const ParagraphLayoutParameters &pStyle, float parentPosY, font::Formatter &);
	static void initFormatter(LayoutBlock &l, float parentPosY, font::Formatter &);

	InlineContext(font::Formatter::FontCallback &&, float);

	void setTargetLabel(Label *);

	void pushNode(const Node *, const NodeCallback &);
	void popNode(const Node *);
	void finalize(LayoutBlock &);
	void finalize();
	void reset();

	LayoutBlock *alignInlineContext(LayoutBlock &inl, const Vec2 &);

	Vector<RefPosInfo> refPos;
	Vector<OutlinePosInfo> outlinePos;
	Vector<BackgroundPosInfo> backgroundPos;
	Vector<IdPosInfo> idPos;

	float density = 1.0f;
	float lineHeightMod = 1.0f;
	bool lineHeightIsAbsolute = false;
	uint16_t lineHeight = 0;

	Label phantomLabel;
	Label *targetLabel = nullptr;
	font::Formatter reader;
	bool finalized = false;

	Vector<Pair<const Node *, NodeCallback>> nodes;
};

} // namespace stappler::document

#endif /* EXTRA_DOCUMENT_LAYOUT_SPDOCLAYOUTINLINECONTEXT_H_ */
