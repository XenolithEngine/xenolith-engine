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

#ifndef STAPPLER_MARKDOWN_SPDOCMARKDOWNPROCESSOR_H_
#define STAPPLER_MARKDOWN_SPDOCMARKDOWNPROCESSOR_H_

#include "SPDocMarkdown.h"
#include "MMDHtmlProcessor.h"

namespace STAPPLER_VERSIONIZED stappler::mmd {

/* Turns the MultiMarkdown token tree into a document::Node tree.

It rides on HtmlProcessor because that class already holds every decision about WHAT a token
means - which tokens open a list, where a table header ends, how a reference link resolves - and
publishes the result through four hooks (pushNode / pushInlineNode / popNode / flushBuffer). Only
those four are overridden here; the html writing the base does into `buffer` becomes the text of
a Node instead of bytes in a stream.

SOURCE SPANS are the reason this class is not the legacy one verbatim. Two different questions
are answered separately:

 - an ELEMENT's span is the span of the token that made it. HtmlProcessor already passes that
   token to pushNode (and nullptr for wrappers it invents, like the `code` inside a `pre`), so
   the element case is a copy;

 - a TEXT run's span is the union of the INNERMOST tokens that actually appended to the buffer
   since the last flush. Innermost matters: exporting `## Header` walks the block token before
   the text token, and attributing the run to the block would make the text start at the `##`.
   exportToken() is therefore wrapped, and a token claims the growth only when no token below it
   already did. */
class SP_PUBLIC DocumentProcessor : public HtmlProcessor {
public:
	virtual ~DocumentProcessor() = default;

	virtual bool init(document::DocumentMarkdown *, document::DocumentData *);

protected:
	virtual void processHtml(const Content &, const StringView &, const Token &) override;

	// span bookkeeping: see the class comment
	virtual void exportToken(const CallbackStream &, token *t) override;

	void processStyle(const StringView &name, document::StyleList &, const StringView &);

	document::Node *makeNode(token *, const StringView &name, InitList &&, VecList &&);

	virtual void pushNode(token *, const StringView &name, InitList &&attr = InitList(),
			VecList && = VecList()) override;
	virtual void pushInlineNode(token *, const StringView &name, InitList &&attr = InitList(),
			VecList && = VecList()) override;
	virtual void popNode() override;
	virtual void flushBuffer() override;

	template <typename T>
	void processAttributes(const T &container, const StringView &name, document::StyleList &,
			const Callback<void(StringView, StringView)> &);

	// Span of the element a push hook is creating: the token's own range, widened to its mate
	// (a paired marker) and to `nodeSpanEnd` (markup the token does not cover), which it consumes.
	document::SourceSpan nodeSpanFor(token *);

	void extendTextSpan(token *);
	document::SourceSpan takeTextSpan();

	Vector<document::Node *> _nodeStack;
	document::DocumentMarkdown *_document = nullptr;
	document::DocumentData *_data = nullptr;
	document::PageContainer *_page = nullptr;
	uint32_t _tableIdx = 0;

	// union of the innermost token spans that produced what is currently in `buffer`
	uint32_t _textBegin = maxOf<uint32_t>();
	uint32_t _textEnd = 0;

	// set while unwinding exportToken(): a token below this one already claimed the text
	bool _textSpanClaimed = false;
};

} // namespace stappler::mmd

#endif // STAPPLER_MARKDOWN_SPDOCMARKDOWNPROCESSOR_H_
