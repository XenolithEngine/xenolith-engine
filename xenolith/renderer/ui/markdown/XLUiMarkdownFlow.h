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

#ifndef XENOLITH_RENDERER_UI_MARKDOWN_XLUIMARKDOWNFLOW_H_
#define XENOLITH_RENDERER_UI_MARKDOWN_XLUIMARKDOWNFLOW_H_

#include "XLUiMarkdownTypes.h"
#include "SPDocMarkdownMarkup.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

enum class MarkdownFlowKind {
	// A Label whose characters came from the source, and which therefore carries a run map.
	Text,

	// A Label the builder wrote itself: a bullet, an item number. It is read, so it is in the
	// flow; it is not source, so it maps to the block it marks and to nothing finer.
	Marker,

	// A block with no text of its own - a rule, a task checkbox, later an image.
	Atomic,
};

struct SP_PUBLIC MarkdownFlowEntry {
	Rc<Node> node;
	uint32_t textBegin = 0; // first position of this entry in the document's own numbering
	uint32_t textLength = 0; // UTF-16 code units of the Label's string; 0 for Atomic
	document::SourceSpan span; // the block this entry belongs to, whole
	MarkdownFlowKind kind = MarkdownFlowKind::Text;

	/* The document node this entry was built from. A pointer, not a reference held: the document
	outlives every tree built from it, and the flow is discarded with the tree.

	It is here so that the inline appearance can be resolved AGAIN - a stylesheet reload has to
	restyle ranges that were baked into a Label at build time, and re-walking the source is what
	recovers where each range was. Rebuilding the tree instead would work too, and would throw
	away the selection. */
	const document::Node *source = nullptr;
};

/* THE DOCUMENT IN READING ORDER, as one flat vector.

A tree answers "what is inside what". A selection asks something else entirely - "what lies between
these two points" - and the tree cannot answer it without a walk per question. So the order a
reader reads in is recorded once, while the tree is being built, as a sequence of entries with a
continuous numbering across all of them.

POSITIONS. Entry i occupies `[textBegin, textBegin + textLength]`, and the next entry starts one
past that end: the extra position is the boundary between two blocks, so a range that crosses it
is a range that crosses a paragraph break. The numbers index the LABEL'S STRING, not the glyphs on
screen - `white-space: normal` collapses runs of spaces during layout while the string stays
verbatim, and `Label::getCharIndex` answers in the string's own units too.

WHAT IT IS FOR. Two questions, and everything else in the milestones ahead is built on them:
`getSourceRange` turns a range of positions into a range of source bytes (which
`document::writeMarkdownFragment` turns into markup), and `writeText` turns it into the text that
was on screen. */
class SP_PUBLIC MarkdownFlow : public Ref {
public:
	virtual ~MarkdownFlow() = default;

	// The source every span and every run map indexes into.
	void setSource(StringView source) { _source = source; }
	StringView getSource() const { return _source; }

	// Append in reading order; returns the new entry's index. The builder is the only caller.
	uint32_t emplace(Node *, MarkdownFlowKind, document::SourceSpan, uint32_t textLength,
			const document::Node *source = nullptr);

	const Vector<MarkdownFlowEntry> &getEntries() const { return _entries; }

	// The last readable position, i.e. the end of the last entry.
	uint32_t getLength() const;

	// The entry a position falls in; nullptr only when the flow is empty or the position is past
	// its end. Positions are contiguous, so this never answers "between entries".
	const MarkdownFlowEntry *findByPosition(uint32_t) const;

	// O(1) through the run map the builder leaves on every entry's Label.
	const MarkdownFlowEntry *findByNode(const Node *) const;

	/* Positions to source bytes.

	The edges move OUTWARD wherever the mapping is not exact: a run the parser rewrote (an entity,
	a smart quote) has no per character mapping and is taken whole, a position between two runs
	belongs to a character the builder inserted and takes the nearer run's boundary, and an entry
	with no runs at all (a marker) answers with the block it marks. */
	Pair<uint32_t, uint32_t> getSourceRange(uint32_t begin, uint32_t end) const;

	// The text as it was read: the labels' own strings, with a break between blocks.
	void writeText(const Callback<void(StringView)> &, uint32_t begin, uint32_t end) const;

	/* --- geometry, which is what a selection asks of the reading order ---

	The flow answers these rather than the widget, because it is the only thing that holds both the
	order and the nodes. Everything here works in WORLD coordinates: a pointer event arrives in
	them, and every entry converts for itself - the tree between the view and a Label is several
	nodes deep and a scroll offset sits in the middle of it. */

	// The position under a point. A point inside an entry answers exactly; a point between blocks
	// or past the document takes the nearest entry by vertical distance and then its nearest edge,
	// which is what a reader dragging past the end of a line expects.
	uint32_t getPositionForPoint(Vec2 worldLocation) const;

	/* The caret for a position: where its BASE stands, in world coordinates, and how tall it is.
	Invalid (see Vec2::isValid) when the position has no geometry.

	The base is the baseline, which is what a Label answers with and where a handle hangs from -
	and it is BELOW the text, not inside it. A caller looking for a point the reader would call
	"on this word" wants `y + height`; a caller drawing something under the line wants `y`. */
	Pair<Vec2, float> getPointForPosition(uint32_t position) const;

	// The word and the whole block around a position: a double and a triple tap.
	Pair<uint32_t, uint32_t> getWordRange(uint32_t position) const;
	Pair<uint32_t, uint32_t> getBlockRange(uint32_t position) const;

	/* Paint a range: slice it into each entry's own coordinates and hand the slice to the Label.
	Entries outside the range are cleared, so this is also how a selection is erased (an empty
	range clears everything). Only labels are painted; an atomic entry has nothing to paint. */
	void applySelection(uint32_t begin, uint32_t end) const;

	// Colour for every label of the document; a Label keeps its own.
	void setSelectionColor(const Color4F &) const;

	// The link whose range covers a position, or nullptr.
	const MarkdownRunMap::Link *findLink(uint32_t position) const;

protected:
	// The label of an entry, or nullptr when the entry paints no text.
	static basic2d::Label *labelOf(const MarkdownFlowEntry &);

	uint32_t mapEdge(const MarkdownFlowEntry &, uint32_t position, bool leftEdge) const;

	Vector<MarkdownFlowEntry> _entries;
	StringView _source;
	uint32_t _next = 0;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_MARKDOWN_XLUIMARKDOWNFLOW_H_
