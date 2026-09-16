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

/* The inline constructs a Markdown document can put inside a block. They are style ranges over the
block's Label string, not nodes, so a line can break inside them. `Text` carries no style. */
// The character an image occupies in a block's string: U+FFFC, the object replacement character.
// It gives a picture a place in the reading order: a caret, selection and source map treat it as
// one character.
constexpr char16_t MarkdownObjectChar = u'￼';

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

/* The built-in look of each inline construct, as a delta over the block's text style
(`LabelBase::Style::merge`): `code` inside a heading keeps the heading's size and only swaps the
family. A plain value, so each view may have its own table. */
struct SP_PUBLIC MarkdownInlineStyles {
	// The monospace family index from the FontController; `maxOf<uint32_t>()` (unknown) leaves the
	// family unchanged, so code renders in the block's face.
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

/* Where each character of a Label came from, in bytes of the document source; built while the
text is assembled.

Runs are ordered by `charStart`, do not overlap, and cover the string except builder-inserted
characters (a list marker, a separator), which have no run. `verbatim` is false when the parser
transformed the text (entities, smart typography); such a run maps only as a whole. */
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

	// A link range and its target, which a Label's layout does not retain.
	struct Link {
		uint32_t charStart = 0;
		uint32_t charCount = 0;
		String href;
		String title;

		bool operator==(const Link &) const = default;
	};

	// The alt text of each inline object (U+FFFC), by its character; a plain-text copy puts it in
	// place of the object.
	Vector<Pair<uint32_t, String>> objects;

	Vector<Run> runs;
	Vector<Link> links;

	// This Label's entry in ui::MarkdownFlow and the global position of its first character, so a
	// hit-tested Label is located without a search.
	uint32_t flowIndex = maxOf<uint32_t>();
	uint32_t textBegin = 0;

	// The run covering `charIndex`, or nullptr between runs.
	const Run *findRun(uint32_t charIndex) const;

	// The link whose range covers `charIndex`, or nullptr.
	const Link *findLink(uint32_t charIndex) const;

	// The alt text of the object standing at `charIndex`, or empty.
	StringView findObjectText(uint32_t charIndex) const;

	bool operator==(const MarkdownRunMap &) const = default;
};

/* Measures a markdown block at the offered width. A plain Label answers a max-content request
with its unwrapped width, so a flex column would size each paragraph to one line. This system is
registered before the Label's own and, when `MeasureConstraints::maxWidth` is set, wraps at that
width and reports the resulting extent; without a width the Label answers itself. */
class SP_PUBLIC MarkdownTextSystem : public System {
public:
	// Before the Label's own measure system, which is added in its init() at the default priority.
	static constexpr uint32_t MarkdownTextPriority = System::DefaultPriority - 10;

	virtual ~MarkdownTextSystem() = default;

	virtual bool init() override;

	virtual bool handleMeasure(const MeasureConstraints &, Size2 &result) override;

	// The width the block was last measured at, and the height it produced.
	float getWrapWidth() const { return _wrapWidth; }
	float getWrapHeight() const { return _wrapHeight; }

protected:
	float _wrapWidth = 0.0f;
	float _wrapHeight = -1.0f;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_MARKDOWN_XLUIMARKDOWNTYPES_H_
