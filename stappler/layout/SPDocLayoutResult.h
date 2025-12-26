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

#ifndef EXTRA_DOCUMENT_LAYOUT_SPDOCLAYOUTRESULT_H_
#define EXTRA_DOCUMENT_LAYOUT_SPDOCLAYOUTRESULT_H_

#include "SPDocLayoutObject.h"

namespace STAPPLER_VERSIONIZED stappler::document {

struct LayoutBlock;

struct SP_PUBLIC LayoutPageData {
	Margin margin;
	Rect viewRect; // rectangle in scroll view
	Rect texRect; // rectangle in prepared layout
	size_t num;
	bool isSplit;
};

struct SP_PUBLIC LayoutBoundIndex {
	size_t idx = maxOf<size_t>();
	size_t level = 0;
	float start;
	float end;
	int64_t page;
	StringView label;
	StringView href;
};

class SP_PUBLIC LayoutResult : public Ref {
public:
	virtual ~LayoutResult();

	bool init(const MediaParameters &, Document *doc);

	void storeFont(font::FontFaceSet *);

	const MediaParameters &getMedia() const;
	Document *getDocument() const;

	void pushIndex(StringView, Vec2);
	void finalize();

	void setBackgroundColor(Color4B);
	Color4B getBackgroundColor() const;

	void setContentSize(Size2);
	Size2 getContentSize() const;

	Size2 getSurfaceSize() const;

	SpanView<Object *> getObjects() const;
	SpanView<Link *> getRefs() const;
	SpanView<LayoutBoundIndex> getBounds() const;

	const Vec2 *getIndexPoint(StringView) const;

	size_t getNumPages() const;

	LayoutPageData getPageData(size_t idx, float offset) const;

	LayoutBoundIndex getBoundsForPosition(float) const;

	Background *emplaceBackground(const LayoutBlock &, const Rect &, const BackgroundParameters &,
			ZOrder zIndex);
	PathObject *emplaceOutline(const LayoutBlock &, const Rect &, const Color4B &, ZOrder zIndex,
			float = 0.0f, BorderStyle = BorderStyle::None);
	void emplaceBorder(LayoutBlock &, const Rect &, const OutlineParameters &, float width,
			ZOrder zIndex);
	PathObject *emplacePath(const LayoutBlock &, ZOrder zIndex);
	Label *emplaceLabel(const LayoutBlock &, ZOrder zIndex, bool isBullet = false);
	Link *emplaceLink(const LayoutBlock &, const Rect &, StringView, StringView, WideStringView);

	const Object *getObject(size_t size) const;
	const Label *getLabelByHash(StringView, size_t idx) const;

protected:
	struct Data;

	void processContents(const DocumentContentRecord &rec, size_t level);

	Data *_data = nullptr;
};

} // namespace stappler::document

#endif /* EXTRA_DOCUMENT_LAYOUT_SPDOCLAYOUTRESULT_H_ */
