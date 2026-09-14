/**
Copyright (c) 2017 Roman Katuntsev <sbkarr@stappler.org>

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

#ifndef STAPPLER_MARKDOWN_MMD_PROCESSORS_MMDHTMLPROCESSOR_H_
#define STAPPLER_MARKDOWN_MMD_PROCESSORS_MMDHTMLPROCESSOR_H_

#include "MMDProcessor.h"

namespace STAPPLER_VERSIONIZED stappler::mmd {

class SP_PUBLIC HtmlProcessor : public Processor {
public:
	using InitList = sprt::initializer_list<Pair<StringView, StringView>>;
	using VecList = Vector<Pair<StringView, StringView>>;

	HtmlProcessor();
	virtual ~HtmlProcessor() { }

	virtual bool init(const CallbackStream *);
	virtual void process(const Content &, const StringView &, const Token &);

protected:
	virtual void processMeta(const StringView &, const StringView &);
	virtual void processHtml(const Content &, const StringView &, const Token &);

	virtual void pad(const CallbackStream &, uint16_t num);
	virtual void printHtml(const CallbackStream &, const StringView &);
	virtual void printLocalizedChar(const CallbackStream &, uint16_t type);

	virtual bool shouldWriteMeta(const StringView &);

	virtual void startCompleteHtml(const Content &c);
	virtual void endCompleteHtml();

	virtual void exportToken(const CallbackStream &, token *t);
	virtual void exportTokenTree(const CallbackStream &, token *t);

	virtual void exportTokenRaw(const CallbackStream &, token *t);
	virtual void exportTokenTreeRaw(const CallbackStream &, token *t);

	virtual void makeTokenHash(const Callback<void(const StringView &)> &, token *t);
	virtual void makeTokenTreeHash(const Callback<void(const StringView &)> &, token *t);

	virtual void exportTokenMath(const CallbackStream &, token *t);
	virtual void exportTokenTreeMath(const CallbackStream &, token *t);

	virtual void exportFootnoteList(const CallbackStream &);
	virtual void exportGlossaryList(const CallbackStream &);
	virtual void exportCitationList(const CallbackStream &);

	virtual void exportBlockquote(const CallbackStream &, token *t);
	virtual void exportDefinition(const CallbackStream &, token *t);
	virtual void exportDefList(const CallbackStream &, token *t);
	virtual void exportDefTerm(const CallbackStream &, token *t);
	virtual void exportFencedCodeBlock(const CallbackStream &,token *t);
	virtual void exportIndentedCodeBlock(const CallbackStream &, token *t);
	virtual void exportHeader(const CallbackStream &, token *t);
	virtual void exportHr(const CallbackStream &);
	virtual void exportHtml(const CallbackStream &, token *t);
	virtual void exportListBulleted(const CallbackStream &, token *t);
	virtual void exportListEnumerated(const CallbackStream &, token *t);
	virtual void exportListItem(const CallbackStream &, token *t, bool tight);
	virtual void exportDefinitionBlock(const CallbackStream &, token *t);
	virtual void exportHeaderText(const CallbackStream &, token *t, uint8_t level);
	virtual void exportTable(const CallbackStream &, token *t);
	virtual void exportTableHeader(const CallbackStream &, token *t);
	virtual void exportTableSection(const CallbackStream &, token *t);
	virtual void exportTableCell(const CallbackStream &, token *t);
	virtual void exportTableRow(const CallbackStream &, token *t);
	virtual void exportToc(const CallbackStream &, token *t);
	virtual void exportTocEntry(const CallbackStream &, size_t &counter, uint16_t level);

	virtual void exportBacktick(const CallbackStream &, token *t);
	virtual void exportPairBacktick(const CallbackStream &, token *t);
	virtual void exportPairAngle(const CallbackStream &, token *t);
	virtual void exportPairBracketImage(const CallbackStream &, token *t);
	virtual void exportPairBracketAbbreviation(const CallbackStream &, token *t);
	virtual void exportPairBracketCitation(const CallbackStream &, token *t);
	virtual void exportPairBracketFootnote(const CallbackStream &, token *t);
	virtual void exportPairBracketGlossary(const CallbackStream &, token *t);
	virtual void exportPairBracketVariable(const CallbackStream &, token *t);

	virtual void exportCriticAdd(const CallbackStream &, token *t);
	virtual void exportCriticDel(const CallbackStream &, token *t);
	virtual void exportCriticCom(const CallbackStream &, token *t);
	virtual void exportCriticHi(const CallbackStream &, token *t);
	virtual void exportCriticPairSubDel(const CallbackStream &, token *t);
	virtual void exportCriticPairSubAdd(const CallbackStream &, token *t);

	virtual void exportMath(const CallbackStream &, token *t);
	virtual void exportSubscript(const CallbackStream &, token *t);
	virtual void exportSuperscript(const CallbackStream &, token *t);
	virtual void exportLineBreak(const CallbackStream &, token *t);

	virtual void exportLink(const CallbackStream &, token * text, Content::Link * link);
	virtual void exportImage(const CallbackStream &, token * text, Content::Link * link, bool is_figure);

	virtual void pushHtmlEntity(const CallbackStream &, token *t);

	virtual void pushNode(token *t, const StringView &name, InitList &&attr = InitList(), VecList && = VecList()) = 0;
	virtual void pushInlineNode(token *t, const StringView &name, InitList &&attr = InitList(), VecList && = VecList()) = 0;
	virtual void popNode() = 0;
	virtual void flushBuffer() = 0;

protected:
	virtual void pushHtmlEntityText(const CallbackStream &out, StringView r, token *t = nullptr);

	/* End offset (in the source) of the markup of the element the NEXT pushNode/pushInlineNode
	creates, when that markup reaches further than the token naming it - a link's label token
	stops at `]` while the element also owns `(url)`. Zero means "the token says it all".

	A consumer that records source positions reads it in its push hook and clears it there; a
	consumer that does not (HtmlOutputProcessor) never looks. */
	uint32_t nodeSpanEnd = 0;

	bool spExt = false;
	bool safeMath = false;
	uint8_t html_header_level = maxOf<uint8_t>();
	const CallbackStream *output = nullptr;
	StringStream buffer;
	uint32_t figureId = 0;
};

}

#endif /* STAPPLER_MARKDOWN_MMD_PROCESSORS_MMDHTMLPROCESSOR_H_ */
