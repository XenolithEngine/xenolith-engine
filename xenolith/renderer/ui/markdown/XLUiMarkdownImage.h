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

#ifndef XENOLITH_RENDERER_UI_MARKDOWN_XLUIMARKDOWNIMAGE_H_
#define XENOLITH_RENDERER_UI_MARKDOWN_XLUIMARKDOWNIMAGE_H_

#include "XLUiMarkdownTypes.h"
#include "XL2dSprite.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

// What the widget knows about an image before anything has been loaded.
struct SP_PUBLIC MarkdownImageRequest {
	StringView src; // exactly as the document wrote it
	StringView alt;

	// `width`/`height` written in the markup, already resolved through the node's style; zero on
	// an axis the author left alone.
	Size2 declared;
};

/* What an image resolves to: a texture that may still be loading, and the box to reserve. The box
is needed while the paragraph is shaped; the texture can arrive later without a re-flow. */
struct SP_PUBLIC MarkdownImageSource {
	Rc<Texture> texture;
	Size2 size;
};

/* How a `src` becomes something to draw. The default (ui::MarkdownView) reads a local file
relative to the document, sizing it from the file header; URLs need an application resolver. */
using MarkdownImageResolver = Function<MarkdownImageSource(const MarkdownImageRequest &)>;

/* Keeps inline image nodes on the boxes their Label's layout reserved. The two are joined only by
the layout range index, so nodes are repositioned after every re-wrap. Attached to the Label. */
class SP_PUBLIC MarkdownImageSystem : public System {
public:
	virtual ~MarkdownImageSystem() = default;

	virtual bool init() override;

	// `objectIndex` indexes the Label's inline objects, which carry the formatter's range.
	void addImage(NotNull<Node>, uint32_t objectIndex);

	// The layout is current at this point of the visit: Label::processParentFlags re-shapes a
	// dirty label before its children are reached.
	virtual void handleVisitSelf(FrameInfo &, Node *, NodeVisitFlags) override;

	// Also repositions here: the box comes from the layout but is expressed against the node's
	// height, assigned in a different phase; without this a picture lags a frame after re-wrap.
	virtual void handleContentSizeDirty() override;

	// Move every picture onto its box. Idempotent; unmoved nodes are not touched, since setting a
	// position marks the subtree dirty.
	void reposition();

	uint32_t getImageCount() const { return uint32_t(_images.size()); }

	// Where the box of image `i` is, in the Label's own space; empty when it has no box yet.
	Rect getImageRect(uint32_t i) const;

protected:
	struct Entry {
		Rc<Node> node;
		uint32_t objectIndex = 0;
		Rect rect;
	};

	Vector<Entry> _images;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_MARKDOWN_XLUIMARKDOWNIMAGE_H_
