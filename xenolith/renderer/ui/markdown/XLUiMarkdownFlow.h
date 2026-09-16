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

	// A Label the builder wrote itself (a bullet, an item number): in the flow, but it maps only
	// to the block it marks.
	Marker,

	// A block with no text of its own: a rule, a task checkbox.
	Atomic,
};

struct SP_PUBLIC MarkdownFlowEntry {
	Rc<Node> node;
	uint32_t textBegin = 0; // first position of this entry in the document's own numbering
	uint32_t textLength = 0; // UTF-16 code units of the Label's string; 0 for Atomic
	document::SourceSpan span; // the block this entry belongs to, whole
	MarkdownFlowKind kind = MarkdownFlowKind::Text;

	// The document node this entry was built from; the document outlives the tree and the flow.
	// Used to re-walk the source when inline styles are re-resolved on a stylesheet reload.
	const document::Node *source = nullptr;
};

/* The document in reading order, as one flat vector with continuous numbering, so a selection
can ask what lies between two points without walking the tree.

Entry i occupies `[textBegin, textBegin + textLength]`; the next entry starts one past that end,
the extra position being the block boundary. Positions index the Label's string (UTF-16), not
glyphs: `white-space: normal` collapses spaces only in layout. `getSourceRange` maps positions to
source bytes (for `document::writeMarkdownFragment`); `writeText` yields the displayed text. */
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

	/* Positions to source bytes. Edges move outward where the mapping is not exact: a rewritten
	run (entity, smart quote) is taken whole, a position between runs takes the nearer run's
	boundary, and an entry with no runs answers with its block. */
	Pair<uint32_t, uint32_t> getSourceRange(uint32_t begin, uint32_t end) const;

	// The text as it was read: the labels' own strings, with a break between blocks.
	void writeText(const Callback<void(StringView)> &, uint32_t begin, uint32_t end) const;

	// --- geometry: all in world coordinates; each entry converts through its own transform ---

	// The position under a point. A point between blocks or past the document takes the nearest
	// entry by vertical distance, then its nearest edge.
	uint32_t getPositionForPoint(Vec2 worldLocation) const;

	/* The caret for a position: its base (the baseline, below the text) in world coordinates, and
	its height; invalid (see Vec2::isValid) without geometry. A point on the word is
	`y + height`. */
	Pair<Vec2, float> getPointForPosition(uint32_t position) const;

	// The word and the whole block around a position: a double and a triple tap.
	Pair<uint32_t, uint32_t> getWordRange(uint32_t position) const;
	Pair<uint32_t, uint32_t> getBlockRange(uint32_t position) const;

	/* Paint a range: each Label gets its slice, other entries are cleared, so an empty range erases
	the selection. Atomic entries paint nothing. */
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
