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

	MarkdownBuilder(NotNull<Node> root, NotNull<MarkdownRegistry>, const MarkdownInlineStyles &);

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

	/* Put a node the factory made itself into the reading order.

	Every Label the builder fills joins the flow on its own, but a factory that writes a node's
	text directly - a list bullet, an item number - or produces a block with no text at all - a
	rule, a task checkbox - has to say so, or the document's order will have a hole where the
	reader sees something. */
	uint32_t registerFlow(Node *, MarkdownFlowKind, const document::Node &source,
			uint32_t textLength = 0);

	MarkdownFlow *getFlow() const { return _flow; }

	// The span to attribute a node to: its own, or the nearest ancestor that has one. A wrapper
	// the parser invented (the `code` inside a `pre`) carries no span, and the block around it is
	// the honest answer for everything the flow does with it.
	static document::SourceSpan spanOf(const document::Node &source);

	// Create a Label already typed and classed for `tag`, and add it to `parent`.
	basic2d::Label *makeLabel(Node *parent, StringView tag);

	// Type + `md-<type>` class, the whole CSS identity of a produced node.
	static void applyIdentity(Node *, StringView type);

	const MarkdownInlineStyles &getInlineStyles() const { return _styles; }
	MarkdownRegistry *getRegistry() const { return _registry; }

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

	// The text being assembled for one Label, plus the two things a milestone from now needs:
	// which range each inline construct covers, and where each character came from.
	struct TextState {
		WideString text;
		Vector<Pair<Pair<uint32_t, uint32_t>, MarkdownInline>> ranges;
		Vector<MarkdownRunMap::Run> runs;
		Vector<MarkdownRunMap::Link> links;
	};

	void collectText(TextState &, const document::Node &source, MarkdownInline);
	void appendValue(TextState &, const document::Node &value);
	void commitText(basic2d::Label *, TextState &, const document::Node &source);

	// Do the rendered characters repeat the source bytes one for one? False whenever the parser
	// transformed them - decoded an entity, or applied smart typography.
	bool isVerbatim(WideStringView, document::SourceSpan) const;

	Rc<Node> _root;
	Rc<MarkdownRegistry> _registry;
	Rc<MarkdownFlow> _flow;
	MarkdownInlineStyles _styles;
	StringView _source;
	uint32_t _blocks = 0;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_MARKDOWN_XLUIMARKDOWNBUILDER_H_
