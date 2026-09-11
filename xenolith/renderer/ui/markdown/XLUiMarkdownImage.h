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

/* What an image resolves to: a texture that may still be loading, and the BOX to reserve for it.

The two are separate on purpose. The box has to be known while the paragraph is being shaped, and
the pixels do not - a texture arrives whenever the loop gets to it, and finds its place already
kept. That is the whole reason a Markdown document does not re-flow as its images appear. */
struct SP_PUBLIC MarkdownImageSource {
	Rc<Texture> texture;
	Size2 size;
};

/* How a `src` becomes something to draw.

The default (ui::MarkdownView) reads a local file: the path is resolved against the document's own
directory, its EXTENT is read from the file header - not by decoding it - and the decode itself is
left to the render loop. Anything else, a URL above all, is the application's business: this
widget deliberately depends on neither the network nor a storage backend, and an application that
wants remote images already has whichever of the two it prefers. */
using MarkdownImageResolver = Function<MarkdownImageSource(const MarkdownImageRequest &)>;

/* KEEPS THE PICTURES ON THE BOXES THE TEXT LEFT FOR THEM.

An inline image is a box the formatter reserved inside a Label's layout, and a scene node drawn
over that box. The two are joined only by the index of the layout range that reserved it, so
something has to look up where the box ended up and move the node there - after every re-wrap, at
every width, on every line the paragraph gains or loses.

It hangs on the Label itself, which is where both halves live. */
class SP_PUBLIC MarkdownImageSystem : public System {
public:
	virtual ~MarkdownImageSystem() = default;

	virtual bool init() override;

	// `objectIndex` indexes the Label's own inline objects, which is what carries the range the
	// formatter answered with.
	void addImage(NotNull<Node>, uint32_t objectIndex);

	// The layout is current at this point of the visit: Label::processParentFlags re-shapes a
	// dirty label before its children are reached.
	virtual void handleVisitSelf(FrameInfo &, Node *, NodeVisitFlags) override;

	/* Also here, and not only at the visit: a box's place is read out of the LAYOUT but expressed
	against the node's own height, and those two are assigned in different phases. Repositioning
	when the height lands is what keeps the picture from spending a frame off its box after every
	re-wrap. */
	virtual void handleContentSizeDirty() override;

	// Move every picture onto its box. Cheap and idempotent: a node whose box has not moved is
	// left alone, because writing a position marks a subtree dirty.
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
