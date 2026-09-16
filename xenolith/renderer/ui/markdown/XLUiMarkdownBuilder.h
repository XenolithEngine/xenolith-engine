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

#ifndef XENOLITH_RENDERER_UI_MARKDOWN_XLUIMARKDOWNBUILDER_H_
#define XENOLITH_RENDERER_UI_MARKDOWN_XLUIMARKDOWNBUILDER_H_

#include "XLUiMarkdownRegistry.h"
#include "XLUiMarkdownFlow.h"
#include "XLUiMarkdownStyle.h"
#include "XLUiMarkdownImage.h"
#include "SPDocMarkdown.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

/* Turns a parsed Markdown document into a subtree of scene nodes.

A block that carries text is a Label, and each inline construct inside it (`strong`, `em`, `code`,
`a`) is a style range over that Label's string, never a node, so lines can break inside it. CSS
reaches inline appearance through probes resolved by MarkdownInlineResolver.

Blocks that need a box (background, padding, quote bar, table) are plain nodes with typed Labels
inside, since the resolver gives a Label only its width. Layout, spacing and colour come from the
stylesheet: each node gets `setType(<tag>)` plus a `md-<tag>` class. */
class SP_PUBLIC MarkdownBuilder final {
public:
	// The formatter counts characters in a uint16_t. A longer block is split into several labels,
	// with the same threshold and 1->N shaping headroom as ui::TextView.
	static constexpr uint32_t kMaxLabelChars = 8'000;

	// The controller maps a sheet's `font-family` to a range style's index; without one only that
	// property is dropped.
	MarkdownBuilder(NotNull<Node> root, NotNull<MarkdownRegistry>, const MarkdownInlineStyles &,
			font::FontController * = nullptr);

	// Build `page` (a document page root, i.e. the `body` node) under the root node given to the
	// constructor. Returns the number of blocks produced.
	uint32_t build(const document::Node &page);

	// A factory calling back into the builder: recurse into the document children of `source`,
	// producing blocks under `parent`.
	void buildChildren(Node *parent, const document::Node &source);

	// A factory calling back into the builder: fill `label` with the inline content of `source`,
	// with a style range per construct and a run map recording where each character came from.
	void buildText(basic2d::Label *label, const document::Node &source);

	// Verbatim text, for a block whose content is not inline markup (a code fence).
	void buildRawText(basic2d::Label *label, const document::Node &source);

	/* Re-resolve the inline style ranges of an already built Label against the current sheet.
	The source walk is repeated to recover range positions; the string, source map, reading order
	and selection are untouched. */
	void restyleText(basic2d::Label *label, const document::Node &source);

	// Put a node the factory filled itself (a list bullet) or a textless block (a rule, a task
	// checkbox) into the reading order. Labels the builder fills join on their own.
	uint32_t registerFlow(Node *, MarkdownFlowKind, const document::Node &source,
			uint32_t textLength = 0);

	MarkdownFlow *getFlow() const { return _flow; }

	// Every `id` the document declared, mapped to its reading position. Includes inline ids (a
	// footnote's reference site), which have no node of their own.
	const Map<String, uint32_t> &getAnchors() const { return _anchors; }

	// The span to attribute a node to: its own, or the nearest ancestor's. Parser-invented
	// wrappers (the `code` inside a `pre`) carry no span.
	static document::SourceSpan spanOf(const document::Node &source);

	// Create a Label already typed and classed for `tag`, and add it to `parent`.
	basic2d::Label *makeLabel(Node *parent, StringView tag);

	// Type + `md-<type>` class, the whole CSS identity of a produced node.
	static void applyIdentity(Node *, StringView type);

	// How an `![alt](src)` becomes something to draw. Unset means images are not shown; their box
	// is still reserved.
	void setImageResolver(MarkdownImageResolver *resolver) { _imageResolver = resolver; }

	const MarkdownInlineStyles &getInlineStyles() const { return _styles; }
	MarkdownRegistry *getRegistry() const { return _registry; }

	// The cascade the inline constructs were resolved through, including its probe count.
	MarkdownInlineResolver &getInlineResolver() { return _inlineResolver; }

	// The source text every SourceSpan indexes into; empty when the caller had no document.
	StringView getSource() const { return _source; }
	void setSource(StringView source) {
		_source = source;
		_flow->setSource(source);
	}

	uint32_t getBlockCount() const { return _blocks; }

protected:
	// One block, or one implicit block wrapping a run of loose inline children.
	void buildBlock(Node *parent, const document::Node &source);

	// The body of buildBlock; the wrapper bounds the block's pending anchor to its own subtree.
	void buildBlockContent(Node *parent, const document::Node &source);

	// One inline construct inside the block's string.
	struct TextRange {
		uint32_t start = 0;
		uint32_t count = 0;

		// The construct's kind: the key of the built-in table, used when no stylesheet answers.
		MarkdownInline kind = MarkdownInline::Text;

		// The enclosing inline tags, outermost first, joined by '>': the probe chain and its cache
		// key (`em` inside `strong` differs from a lone `em`).
		String chain;
	};

	// The text being assembled for one Label, with the range of each inline construct and the
	// source of each character.
	struct TextState {
		WideString text;
		Vector<TextRange> ranges;
		Vector<MarkdownRunMap::Run> runs;
		Vector<MarkdownRunMap::Link> links;

		// The inline tags open at this point of the descent, outermost first.
		Vector<StringView> chain;

		// Ids declared by inline elements, with their starting character. Resolved into document
		// positions in commitText.
		Vector<Pair<String, uint32_t>> anchors;

		// An image inside this block: its stand-in character, the box to keep and what to draw.
		struct Image {
			uint32_t charIndex = 0;
			Size2 size;
			Rc<Texture> texture;
			const document::Node *source = nullptr;
		};

		Vector<Image> images;

		// The alt text of each image, by the character standing for it.
		Vector<Pair<uint32_t, String>> objects;
	};

	void collectText(TextState &, const document::Node &source, MarkdownInline);

	// An `img` inside a block: one character in the string, one box in the line, one node over
	// it (see XLUiMarkdownImage.h).
	void collectImage(TextState &, const document::Node &source);

	// Build the sprites for the images collected into `state` and hang them on the Label.
	void commitImages(basic2d::Label *, TextState &);

	// Sort the ranges into composition order and apply them. Shared by build and restyle so the
	// winning range is the same in both.
	void applyInlineStyles(basic2d::Label *, Vector<TextRange> &);
	void appendValue(TextState &, const document::Node &value);
	void commitText(basic2d::Label *, TextState &, const document::Node &source);

	// Whether the rendered characters repeat the source bytes one for one; false when the parser
	// decoded an entity or applied smart typography.
	bool isVerbatim(WideStringView, document::SourceSpan) const;

	Rc<Node> _root;
	Rc<MarkdownRegistry> _registry;
	Rc<MarkdownFlow> _flow;
	MarkdownInlineStyles _styles;
	MarkdownInlineResolver _inlineResolver;
	MarkdownImageResolver *_imageResolver = nullptr;
	StringView _source;
	uint32_t _blocks = 0;

	Map<String, uint32_t> _anchors;

	// Ids of blocks entered but not yet placed: a block's id binds to the first flow entry inside
	// it. Trimmed when the block ends, so an id never attaches to the next block.
	Vector<String> _pendingAnchors;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_MARKDOWN_XLUIMARKDOWNBUILDER_H_
