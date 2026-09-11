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

#ifndef XENOLITH_RENDERER_UI_MARKDOWN_XLUIMARKDOWNVIRTUAL_H_
#define XENOLITH_RENDERER_UI_MARKDOWN_XLUIMARKDOWNVIRTUAL_H_

#include "XLUiMarkdownTypes.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

/* ONLY THE PART OF THE DOCUMENT THE READER CAN SEE COSTS ANYTHING.

WHAT IS VIRTUALIZED, AND WHAT IS NOT. The nodes all exist - this hides them, it does not build them
lazily. That distinction is the whole reason the rest of the widget is untouched: the run maps, the
reading order, the anchors and the selection are all read off nodes, and every one of them keeps
working on a block that is off screen. What goes away is the expensive half, and it is the half
that is paid per FRAME rather than once:

  - a hidden node is skipped by `Node::wrapVisit` before `processParentFlags`, which is where a
    Label shapes its text. `setVisible(false)` is the ONLY gate that runs that early - `display:
    none` and `visibility: hidden` are both checked after it, and the scroll only clips the draw;
  - a hidden child is not an item of the flex layout (`isDisplayed`), so it is never measured -
    and measuring a Label is a full shaping pass of its own;
  - nothing is drawn for it, and no glyph of it holds the frame back waiting for the atlas.

Measured on ten thousand lines: twelve seconds of shaping for a screenful of text.

WHY SPACERS. A hidden child takes no space, so the document would collapse to what is on screen.
Two ordinary nodes stand in for what is hidden above and below, each as tall as the blocks it
replaces. The column stays an ordinary CSS flex column, so margins, gaps and alignment keep working
and none of it is reimplemented here.

HEIGHTS ARE MEASURED, NEVER ESTIMATED. A block's height is known only after its text is shaped at
the current width, so the blocks are measured in chunks, a chunk per frame, and the document's
scroll range grows as that proceeds. The alternative - guessing a height and correcting it when the
block is finally shown - is the loop that `tests/window/src/widgets/ScrollThrashLayout` exists to
pin: the correction moves the window, which shows a different block, which corrects again.

WHEN IT IS ON. Only past `kMinBlocks`. A document of forty blocks costs nothing to lay out whole,
and switching machinery on for it would mean two behaviours to keep correct instead of one. */
class SP_PUBLIC MarkdownVirtualizer final {
public:
	// Below this many top-level blocks the whole document is simply laid out, as it always was.
	static constexpr uint32_t kMinBlocks = 400;

	// Blocks measured per frame. Large enough that a long document settles in a second or two,
	// small enough that the frame it happens in is not the one the reader notices.
	static constexpr uint32_t kMeasureChunk = 256;

	// How much beyond the viewport is kept materialized, as a fraction of the viewport height.
	// A reader scrolling a page at a time lands inside what is already shaped.
	static constexpr float kOverscan = 0.5f;

	// Adopt the blocks currently under `content`. Returns false when the document has fewer than
	// `threshold` blocks, in which case nothing is touched. A threshold of maxOf turns it off.
	bool init(NotNull<Node> content, uint32_t threshold = kMinBlocks);

	// Give up everything: every block visible, the spacers gone. What a rebuild goes through.
	void clear();

	bool isEnabled() const { return _enabled; }

	// Called every frame while blocks remain unmeasured, and after every scroll.
	// Returns true while there is still work to do.
	bool update(float viewportHeight, float scrollY);

	// The document's height as far as it is known: measured blocks plus what is on screen.
	float getKnownHeight() const { return _knownHeight; }

	uint32_t getBlockCount() const { return uint32_t(_blocks.size()); }
	uint32_t getMeasuredCount() const { return _measured; }
	uint32_t getVisibleCount() const;

	// Where a block starts, in the document's own coordinates; maxOf when it is not measured yet.
	float getBlockTop(uint32_t index) const;

	// A block's measured advance: its height plus whatever the layout put between it and the next
	// one. Negative for an index that is not a block. Reported so a test can compare the heights a
	// virtualized document recorded against the ones a plain layout produces.
	float getAdvance(uint32_t i) const { return i < _blocks.size() ? _blocks[i].advance : -1.0f; }

	// The block a node belongs to, or maxOf. The node may be the block itself or anything inside.
	uint32_t findBlock(const Node *) const;

	// Bring a block on screen whatever the measuring pass has reached, so an anchor can be
	// followed into a part of the document that has not been measured yet.
	bool ensureMeasuredTo(uint32_t index);

protected:
	struct Block {
		Rc<Node> node;
		float top = 0.0f; // prefix sum of advances; valid while `measured`
		float advance = 0.0f; // height plus whatever the layout put between it and the next
		bool measured = false;
		bool visible = true;
	};

	void setSpacer(Node *spacer, float height);
	void collapse(Block &);
	void materialize(Block &);

	// Read back the geometry of the chunk that was made visible for the previous frame.
	void commitChunk();

	Rc<Node> _content;
	Rc<Node> _spacerTop;
	Rc<Node> _spacerBottom;

	Vector<Block> _blocks;

	uint32_t _measured = 0;
	uint32_t _chunkBegin = maxOf<uint32_t>();
	uint32_t _chunkEnd = maxOf<uint32_t>();

	/* Frames the current chunk has been visible for.

	A block cannot be measured on the frame it appears: the layout measures a Label unwrapped
	first, assigns it a width, and only then does the Label re-shape to it - so a paragraph that
	needs two lines reports one on that first pass. Read a frame later and it is settled. */
	uint32_t _chunkAge = 0;

	float _knownHeight = 0.0f;
	bool _enabled = false;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_MARKDOWN_XLUIMARKDOWNVIRTUAL_H_
