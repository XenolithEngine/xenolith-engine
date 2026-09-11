/**
 Copyright (c) 2026 Stappler Team <admin@stappler.org>

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

#ifndef STAPPLER_MARKDOWN_SPDOCMARKDOWNMARKUP_H_
#define STAPPLER_MARKDOWN_SPDOCMARKDOWNMARKUP_H_

#include "SPDocMarkdown.h"
#include "SPDocNode.h"

namespace STAPPLER_VERSIONIZED stappler::document {

/* THE MARKUP BEHIND A RANGE OF A DOCUMENT.

A selection is a range of rendered characters; what a user copying it expects back is the markup
that produced them, not the text that was drawn. Every node of a Markdown parse carries the byte
range of the source that made it (`Node::getSourceSpan`), so the answer is a slice of the source -
except at the two ENDS, where the range can cut a construct in half and leave `**bold` open.

That is the whole of this file: the middle of a fragment is the source, byte for byte, and only
the edges are reasoned about. Nothing is reassembled from the tree, which is why indentation of
nested lists, blank lines, table pipes, quote markers and reference definitions come back exactly
as they were written - and why "select everything" returns the source itself. */
enum class MarkdownMarkup {
	// The slice as it stands, moved out to whole code points. What a user asking for the raw
	// source expects: no invention, and no repair either.
	Raw,

	// The same slice with the constructs the edges cut through closed up again. See §"WHAT AN
	// EDGE DOES" in the implementation for which construct is repaired, which is taken whole,
	// and which is left alone.
	Normalized,
};

/* Write the markup for the byte range [begin, end) of `source`.

`root` is the document's content root (`DocumentMarkdown::getRoot()->getRoot()`), and `source` its
`getSource()`; the range is in bytes of that same view. Both are clamped, so a caller that computed
an edge from a run map cannot walk off the end. */
SP_PUBLIC void writeMarkdownFragment(const Callback<void(StringView)> &out, const Node &root,
		StringView source, uint32_t begin, uint32_t end,
		MarkdownMarkup = MarkdownMarkup::Normalized);

SP_PUBLIC void writeMarkdownFragment(const Callback<void(StringView)> &out,
		const DocumentMarkdown &, uint32_t begin, uint32_t end,
		MarkdownMarkup = MarkdownMarkup::Normalized);

/* Write the VISIBLE text of the same range: what the document says, with none of how it is
written. Text runs are taken from the tree rather than from the source, because the parser decoded
entities and applied smart typography on the way in; a run whose characters are not the source
bytes one for one is taken whole or not at all. */
SP_PUBLIC void writeMarkdownText(const Callback<void(StringView)> &out, const Node &root,
		StringView source, uint32_t begin, uint32_t end);

SP_PUBLIC void writeMarkdownText(const Callback<void(StringView)> &out, const DocumentMarkdown &,
		uint32_t begin, uint32_t end);

} // namespace stappler::document

#endif // STAPPLER_MARKDOWN_SPDOCMARKDOWNMARKUP_H_
