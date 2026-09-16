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

/* Lays out and shapes only the visible part of a long document. Nodes all exist and are hidden,
not built lazily, so run maps, reading order, anchors and selection work off screen. Hiding saves
the per-frame cost:

  - `Node::wrapVisit` skips a hidden node before `processParentFlags`, where a Label shapes;
    `setVisible(false)` is the only gate that early (`display: none`, `visibility: hidden` and
    the scroll clip all act later);
  - a hidden child is not a flex item (`isDisplayed`), so it is never measured;
  - nothing of it is drawn or waits on the glyph atlas.

Two spacer nodes stand in for the hidden blocks above and below, so the column stays a plain CSS
flex column. Heights are measured, never estimated: blocks are shaped at the current width a chunk
per frame and the scroll range grows; estimates would feed back into the visible window (see
`tests/window/src/widgets/ScrollThrashLayout`). Enabled only past `kMinBlocks`. */
class SP_PUBLIC MarkdownVirtualizer final {
public:
	// Below this many top-level blocks the whole document is laid out.
	static constexpr uint32_t kMinBlocks = 400;

	// Blocks measured per frame.
	static constexpr uint32_t kMeasureChunk = 256;

	// How much beyond the viewport is kept materialized, as a fraction of the viewport height.
	static constexpr float kOverscan = 0.5f;

	// Adopt the blocks currently under `content`. Returns false when the document has fewer than
	// `threshold` blocks, in which case nothing is touched. A threshold of maxOf turns it off.
	bool init(NotNull<Node> content, uint32_t threshold = kMinBlocks);

	// Make every block visible and remove the spacers.
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

	// A block's measured advance: its height plus the layout's gap to the next one. Negative for
	// an index that is not a block.
	float getAdvance(uint32_t i) const { return i < _blocks.size() ? _blocks[i].advance : -1.0f; }

	// The block a node belongs to, or maxOf. The node may be the block itself or anything inside.
	uint32_t findBlock(const Node *) const;

	// Measure up to a block regardless of the measuring pass, so an anchor can be followed into an
	// unmeasured part of the document.
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

	// Frames the current chunk has been visible for. A Label measures unwrapped first and
	// re-shapes to its width after, so heights are read a frame later.
	uint32_t _chunkAge = 0;

	float _knownHeight = 0.0f;
	bool _enabled = false;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_MARKDOWN_XLUIMARKDOWNVIRTUAL_H_
