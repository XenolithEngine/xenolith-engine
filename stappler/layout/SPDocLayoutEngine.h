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

#ifndef EXTRA_DOCUMENT_LAYOUT_SPDOCLAYOUTENGINE_H_
#define EXTRA_DOCUMENT_LAYOUT_SPDOCLAYOUTENGINE_H_

#include "SPDocLayoutBlock.h"
#include "SPDocAsset.h"
#include "SPFontHyphenMap.h"

namespace STAPPLER_VERSIONIZED stappler::document {

class LayoutResult;
struct LayoutBlock;

using ExternalAssetsMap = memory::StandartInterface::MapType<memory::StandartInterface::StringType,
		DocumentAssetMeta>;

class SP_PUBLIC LayoutEngine : public StyleInterface,
							   public InterfaceObject<memory::PoolInterface> {
public:
	LayoutEngine(Document *, std::function<Rc<font::FontFaceSet>(const FontStyleParameters &)> &&,
			const MediaParameters &, SpanView<StringView> spine = SpanView<StringView>());
	virtual ~LayoutEngine();

	void setExternalAssetsMeta(ExternalAssetsMap &&);
	void setHyphens(font::HyphenMap *);
	void setMargin(const Margin &);

	LayoutResult *getResult() const;

	const MediaParameters &getMedia() const;
	const MediaParameters &getOriginalMedia() const;
	void hookMedia(const MediaParameters &);
	void restoreMedia();
	bool isMediaHooked() const;

	Rc<font::FontFaceSet> getFont(const FontStyleParameters &);

	Document *getDocument() const;
	font::HyphenMap *getHyphens() const;

	void incrementNodeId(NodeId);
	NodeId getMaxNodeId() const;

	LayoutBlock *makeLayout(LayoutBlock::NodeInfo &&, bool disablePageBreaks);
	LayoutBlock *makeLayout(LayoutBlock::NodeInfo &&, LayoutBlock::PositionInfo &&);
	LayoutBlock *makeLayout(const Node *, Display = Display::None, bool disablePageBreaks = false);

	InlineContext *acquireInlineContext(float);

	bool isFileExists(StringView) const;
	const DocumentImage *getImage(StringView) const;

	const StyleList *compileStyle(const Node &);
	const StyleList *getStyle(const Node &) const;

	virtual bool resolveMediaQuery(MediaQueryId queryId) const override;
	virtual StringView resolveString(StringId) const override;

	virtual float getDensity() const override;
	virtual float getFontScale() const override;
	virtual Color4B getTextColor() const override;

	Float getNodeFloating(const Node &node) const;
	Display getNodeDisplay(const Node &node) const;
	Display getNodeDisplay(const Node &node, Display p) const;

	Display getLayoutContext(const Node &node);
	Display getLayoutContext(const Vector<Node *> &, Vector<Node *>::const_iterator, Display p);

	uint16_t getLayoutDepth() const;
	LayoutBlock *getTopLayout() const;

	void render();

	Pair<float, float> getFloatBounds(const LayoutBlock *l, float y, float height);
	font::Formatter::LinePosition getTextBounds(const LayoutBlock *l, uint16_t &linePos,
			uint16_t &lineHeight, float density, float parentPosY);

	void pushNode(const Node *);
	void popNode();

	void processChilds(LayoutBlock &l, const Node &);

protected:
	struct Data;

	// Default style, that can be redefined with css
	virtual void beginStyle(StyleList &, const Node &, SpanView<const Node *>,
			const MediaParameters &) const;

	// Default style, that can NOT be redefined with css
	virtual void endStyle(StyleList &, const Node &, SpanView<const Node *>,
			const MediaParameters &) const;

	Data *_data = nullptr;
};

} // namespace stappler::document

#endif /* EXTRA_DOCUMENT_LAYOUT_SPDOCLAYOUTENGINE_H_ */
