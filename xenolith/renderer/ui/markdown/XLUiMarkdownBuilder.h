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

THE ONE RULE THE WHOLE CLASS IS BUILT ON: a block that carries text IS a Label, and every inline
construct inside it is a STYLE RANGE over that Label's string. A node per inline would hand the
formatter a row of independent layouts with no way to break a line between them, and would cost a
cascade resolution per word. So `strong`, `em`, `code`, `a` and their kin never become nodes.

The consequence for a stylesheet is worth stating, because it is the reverse of the web: CSS
reaches the TEXT through the block's own node (`p`, `h1`, `td`), and the per-inline appearance
comes from MarkdownInlineStyles, not from a `strong { }` rule. Full cascade resolution for inlines
is a later milestone; the deltas are configurable meanwhile.

Blocks that need a BOX - a background, padding, a quote bar, a table - cannot be Labels: the style
resolver hands a Label its width and nothing else (no background, no content size). Those become
plain nodes with typed Labels inside.

The builder produces nodes only. Layout, spacing and colour all come from the stylesheet the view
attaches: every node is given `setType(<tag>)` plus a `md-<tag>` class, which is the whole of its
CSS identity. */
class SP_PUBLIC MarkdownBuilder final {
public:
	// The formatter counts characters in a uint16_t. A block past this is split into several
	// labels rather than silently truncated - the same threshold ui::TextView uses, with the
	// same headroom for 1->N shaping.
	static constexpr uint32_t kMaxLabelChars = 8'000;

	// The controller is what turns a `font-family` from the sheet into the index a range style
	// carries; without one the inline resolver drops that one property and keeps the rest.
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

	/* Resolve the inline appearance of a Label that is ALREADY BUILT, and change nothing else.

	A range style is baked into a Label when the text is committed, so a stylesheet that arrives
	afterwards restyles every block and leaves the words inside them as they were. This is the
	way back: the source walk is repeated to recover where each range was, the cascade is asked
	again, and the string, the source map, the reading order - and the selection painted over all
	three - are untouched. */
	void restyleText(basic2d::Label *label, const document::Node &source);

	/* Put a node the factory made itself into the reading order.

	Every Label the builder fills joins the flow on its own, but a factory that writes a node's
	text directly - a list bullet, an item number - or produces a block with no text at all - a
	rule, a task checkbox - has to say so, or the document's order will have a hole where the
	reader sees something. */
	uint32_t registerFlow(Node *, MarkdownFlowKind, const document::Node &source,
			uint32_t textLength = 0);

	MarkdownFlow *getFlow() const { return _flow; }

	/* Every `id` the document declared, mapped to the reading position it names.

	Both kinds are in here, and the second is why this is a map rather than a walk over the tree:
	a BLOCK's id becomes the name of its node (`li#fn_1`, a footnote's definition), but an
	INLINE's id has no node at all - the reference site a footnote returns to is a style range
	inside a paragraph, and nothing in the scene remembers it. */
	const Map<String, uint32_t> &getAnchors() const { return _anchors; }

	// The span to attribute a node to: its own, or the nearest ancestor that has one. A wrapper
	// the parser invented (the `code` inside a `pre`) carries no span, and the block around it is
	// the honest answer for everything the flow does with it.
	static document::SourceSpan spanOf(const document::Node &source);

	// Create a Label already typed and classed for `tag`, and add it to `parent`.
	basic2d::Label *makeLabel(Node *parent, StringView tag);

	// Type + `md-<type>` class, the whole CSS identity of a produced node.
	static void applyIdentity(Node *, StringView type);

	// How an `![alt](src)` becomes something to draw. Unset means images are not shown at all -
	// their box is still reserved, so the text around them does not move when one is supplied
	// later.
	void setImageResolver(MarkdownImageResolver *resolver) { _imageResolver = resolver; }

	const MarkdownInlineStyles &getInlineStyles() const { return _styles; }
	MarkdownRegistry *getRegistry() const { return _registry; }

	// The cascade the inline constructs were resolved through; also how a test reads how many
	// probes one document cost.
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

	// The body of buildBlock; the wrapper exists to bound the block's pending anchor to the
	// block's own subtree, however the body returns.
	void buildBlockContent(Node *parent, const document::Node &source);

	// One inline construct inside the block's string.
	struct TextRange {
		uint32_t start = 0;
		uint32_t count = 0;

		// What the construct IS - the key of the built-in table, used when no stylesheet is in
		// scope to answer for it.
		MarkdownInline kind = MarkdownInline::Text;

		// The inline tags enclosing this range, outermost first, joined by '>'. This is the shape
		// of the probe chain the cascade is asked about, and the key that answer is cached on -
		// `em` inside `strong` is not the same question as `em` on its own.
		String chain;
	};

	// The text being assembled for one Label, plus what the milestones after it need: which range
	// each inline construct covers, and where each character came from.
	struct TextState {
		WideString text;
		Vector<TextRange> ranges;
		Vector<MarkdownRunMap::Run> runs;
		Vector<MarkdownRunMap::Link> links;

		// The inline tags open at this point of the descent, outermost first.
		Vector<StringView> chain;

		// Ids declared by inline elements, with the character each one starts at. Resolved into
		// document positions in commitText, once the block has a place in the reading order.
		Vector<Pair<String, uint32_t>> anchors;

		// One image met inside this block: the character standing in for it, the box to keep, and
		// what to draw there once there is anything to draw.
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

	// An `img` met inside a block: one character in the string, one box in the line, one node
	// over it. See XLUiMarkdownImage.h for why it is not a node of its own in the flex flow.
	void collectImage(TextState &, const document::Node &source);

	// Build the sprites for the images collected into `state` and hang them on the Label.
	void commitImages(basic2d::Label *, TextState &);

	// Sort the collected ranges into composition order and hand each to the Label. Shared by the
	// first build and by a restyle, because "which range wins" must not differ between them.
	void applyInlineStyles(basic2d::Label *, Vector<TextRange> &);
	void appendValue(TextState &, const document::Node &value);
	void commitText(basic2d::Label *, TextState &, const document::Node &source);

	// Do the rendered characters repeat the source bytes one for one? False whenever the parser
	// transformed them - decoded an entity, or applied smart typography.
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

	// Ids of blocks entered but not yet placed: a block has no position of its own, so its id
	// binds to the first flow entry produced inside it. Trimmed when the block ends, so an id on
	// a block that produced nothing readable does not attach itself to the next one.
	Vector<String> _pendingAnchors;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_MARKDOWN_XLUIMARKDOWNBUILDER_H_
