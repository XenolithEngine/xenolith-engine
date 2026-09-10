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

#ifndef XENOLITH_RENDERER_UI_MARKDOWN_XLUIMARKDOWNTYPES_H_
#define XENOLITH_RENDERER_UI_MARKDOWN_XLUIMARKDOWNTYPES_H_

#include "XLUiConfig.h" // IWYU pragma: keep
#include "XL2dLabel.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

/* The inline constructs a Markdown document can put INSIDE a block.

They are not nodes. A paragraph is one Label, and everything in this list is a style range over
that Label's string - which is what makes a line break inside a bold word work at all. A node per
inline would hand the formatter a sequence of independent layouts with nothing to break between.

`Text` is the absence of any of them and carries no style of its own. */
enum class MarkdownInline {
	Text,
	Strong, // **bold**
	Emphasis, // *italic*
	Code, // `code`
	Link, // [text](url)
	Strikethrough, // ~~text~~
	Insert, // {++text++}
	Mark, // ==text==
	Subscript, // ~text~
	Superscript, // ^text^

	Max
};

SP_PUBLIC StringView getMarkdownInlineName(MarkdownInline);

// The html tag a Markdown document uses for an inline construct; MarkdownInline::Max when the tag
// is not an inline one (it is a block, or something the builder does not know).
SP_PUBLIC MarkdownInline getMarkdownInlineForTag(StringView tag);

/* HOW EACH INLINE CONSTRUCT LOOKS, as a delta over the block's own text style.

A range style is a delta by construction: it names the parameters it changes and leaves the rest
to the block (`LabelBase::Style::merge`). So `code` inside a heading keeps the heading's size and
only swaps the family, which is exactly the behaviour a stylesheet would give an element - without
the block-per-inline node tree that the stylesheet route would require.

The table is a plain value so that a caller can hand a different one to each view, and
`MarkdownView` re-derives it from CSS custom properties on every style pass. */
struct SP_PUBLIC MarkdownInlineStyles {
	// The monospace family index, resolved from the FontController; `maxOf<uint32_t>()` (the
	// controller's own "unknown") leaves the family alone, so a build without the family still
	// renders code, just not monospaced.
	uint32_t monospaceFamily = maxOf<uint32_t>();

	Color3B codeColor = Color3B(0xB7, 0x1C, 0x1C);
	Color3B linkColor = Color3B(0x15, 0x65, 0xC0);
	Color3B markColor = Color3B(0x33, 0x33, 0x00);
	Color3B insertColor = Color3B(0x2E, 0x7D, 0x32);

	bool underlineLinks = true;

	// The delta for one construct; empty for `Text`.
	basic2d::Label::Style get(MarkdownInline) const;

	bool operator==(const MarkdownInlineStyles &) const = default;
};

/* WHERE EVERY CHARACTER OF A LABEL CAME FROM, in bytes of the document's source.

The map a selection needs to answer "what markup produced this?" - built once, while the text is
being assembled, because that is the only moment both halves are known. Nothing in this milestone
reads it; it is written now because writing it later means walking the document a second time.

Runs are ordered by `charStart`, do not overlap, and cover the whole string except where the
builder inserted characters of its own (a list marker, a synthetic separator) - those simply have
no run, and a lookup that lands between runs means "not from the source".

`verbatim` says whether the rendered characters are the source bytes one for one. They are not,
whenever the parser transformed the text: html entities are decoded, and smart typography turns
quotes, dashes and ellipses into single characters. Such a run can be mapped as a whole, never
sliced - which is why the flag is per run rather than per document. */
struct SP_PUBLIC MarkdownRunMap {
	static ComponentId Id;

	struct Run {
		uint32_t charStart = 0; // UTF-16 code units into the Label's string
		uint32_t charCount = 0;
		uint32_t srcOffset = 0; // bytes into DocumentMarkdown::getSource()
		uint32_t srcLength = 0;
		bool verbatim = true;

		bool operator==(const Run &) const = default;
	};

	// One link range, kept because the walk that emits the link's style range knows the target and
	// nothing downstream can recover it: a Label's layout records the resolved appearance of a
	// range, never the style it came from.
	struct Link {
		uint32_t charStart = 0;
		uint32_t charCount = 0;
		String href;
		String title;

		bool operator==(const Link &) const = default;
	};

	Vector<Run> runs;
	Vector<Link> links;

	// Where this Label sits in the document's reading order (ui::MarkdownFlow), and the global
	// position of its first character. Written when the flow is assembled, so that a Label found
	// by hit testing is located in the flow without a search.
	uint32_t flowIndex = maxOf<uint32_t>();
	uint32_t textBegin = 0;

	// The run covering `charIndex`, or nullptr between runs.
	const Run *findRun(uint32_t charIndex) const;

	// The link whose range covers `charIndex`, or nullptr.
	const Link *findLink(uint32_t charIndex) const;

	bool operator==(const MarkdownRunMap &) const = default;
};

/* HOW TALL IS THIS PARAGRAPH? - answered the way a document needs it answered.

A block of prose has no height of its own. It has a height once a width is chosen, and every
question a layout asks about it is really that question. The engine's measurement protocol carries
the width in `MeasureConstraints::maxWidth`, and a plain Label ignores it for a max-content request
- correctly, because the max-content WIDTH of a text is its unwrapped width. A flex column asks
exactly that question to size its items, so a document built out of plain labels comes out one line
per paragraph, with the rest of the text drawn over the block below it.

So a markdown block answers first, and answers with the shaping: it wraps at the offered width and
reports the extent that produced. `LayoutSystem::measureNode` takes the first system that answers,
and this one is registered before the Label's own - which still answers when no width is offered at
all, where an unwrapped measurement is the right one.

That is also why the wrap width is set here rather than at build time: the width belongs to the
container, and this is the moment the container says what it is. */
class SP_PUBLIC MarkdownTextSystem : public System {
public:
	// Before the Label's own measure system, which is added in its init() at the default priority.
	static constexpr uint32_t MarkdownTextPriority = System::DefaultPriority - 10;

	virtual ~MarkdownTextSystem() = default;

	virtual bool init() override;

	virtual bool handleMeasure(const MeasureConstraints &, Size2 &result) override;

	// The width the block was last measured at, and the height that width produced. Reported
	// rather than used: a stand asserting on wrapping needs to see both.
	float getWrapWidth() const { return _wrapWidth; }
	float getWrapHeight() const { return _wrapHeight; }

protected:
	float _wrapWidth = 0.0f;
	float _wrapHeight = -1.0f;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_MARKDOWN_XLUIMARKDOWNTYPES_H_
