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

#ifndef STAPPLER_MARKDOWN_SPDOCMARKDOWN_H_
#define STAPPLER_MARKDOWN_SPDOCMARKDOWN_H_

#include "SPDocument.h"
#include "SPDocPageContainer.h" // IWYU pragma: keep

namespace STAPPLER_VERSIONIZED stappler::document {

/* A Markdown document: a MultiMarkdown parse turned into the same html-shaped Node tree every
other format produces, plus ONE thing the other formats do not carry - the source text, kept
alive next to the tree.

That is what makes `Node::getSourceSpan()` usable: every span indexes into `getSource()`, so a
consumer holding a node can hand back the markup that produced it instead of the rendered text.
The tree, the spans and the source share the document's pool and die together.

Detection is deliberately asymmetric. A FILE is Markdown by its extension, and only a file with
a text-ish or missing extension is sniffed; BYTES are Markdown when the content type says so, or
when the text opens with something no other format would (an ATX header, MultiMarkdown metadata,
a `{{TOC}}`). Plain prose is not claimed on a guess - it would take plain text away from every
format that reads it legitimately. */
class SP_PUBLIC DocumentMarkdown : public Document {
public:
	static bool isMarkdown(StringView text);
	static bool isMarkdown(FileInfo);
	static bool isMarkdown(BytesView, StringView ct = StringView());

	// content types this format answers to
	static bool isMarkdownContentType(StringView ct);

	virtual ~DocumentMarkdown() = default;

	virtual bool init(FileInfo, StringView ct = StringView());
	virtual bool init(BytesView, StringView ct = StringView());
	virtual bool init(memory::pool_t *, FileInfo, StringView ct = StringView());
	virtual bool init(memory::pool_t *, BytesView, StringView ct = StringView());

	// The text the tree was parsed from, owned by the document's pool. Every SourceSpan in the
	// tree is a byte range in THIS view.
	StringView getSource() const { return _source; }

	PageContainer *acquireRootPage();

protected:
	virtual bool read(BytesView, StringView ct);

	StringView _source;
};

} // namespace stappler::document

#endif // STAPPLER_MARKDOWN_SPDOCMARKDOWN_H_
