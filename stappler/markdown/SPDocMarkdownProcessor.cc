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

#include "SPDocMarkdownProcessor.h"
#include "SPDocPageContainer.h"
#include "SPString.h"
#include "MMDCore.h"
#include "MMDToken.h"

namespace STAPPLER_VERSIONIZED stappler::mmd {

bool DocumentProcessor::init(document::DocumentMarkdown *doc, document::DocumentData *data) {
	_document = doc;
	_data = data;
	_page = _document->acquireRootPage();
	_nodeStack.push_back(_page->getRoot());
	return true;
}

void DocumentProcessor::processHtml(const Content &c, const StringView &str, const Token &t) {
	source = str;
	exportTokenTree(buffer, t);
	exportFootnoteList(buffer);
	exportCitationList(buffer);

	// A glossary reference is emitted as a link to `#gn_N` like any other; without this the
	// target it points at is never written, and `[?term]` renders as a link that goes nowhere.
	exportGlossaryList(buffer);
	flushBuffer();

	auto &headerStack = content->getHeaders();
	if (headerStack.empty()) {
		_page->finalize();
		return;
	}

	// One slot per header level, holding the record new sub-headers of that level attach to.
	document::DocumentContentRecord *levelList[8] = {nullptr};

	auto initLevel = rawLevelForHeader(headerStack.front());
	levelList[initLevel - 1] = &_data->tableOfContents;

	memory::context ctx(_data->pool);

	for (auto &it : headerStack) {
		auto level = rawLevelForHeader(it);
		if (level < 1 || level >= sizeof(levelList) / sizeof(levelList[0])) {
			continue;
		}

		if (auto record = levelList[level - 1]) {
			buffer.clear();
			exportTokenTree(buffer, it.getToken()->child);
			auto title = buffer.weak();
			auto id = labelFromHeader(source, it);

			record->childs.push_back(document::DocumentContentRecord{
				StringView(title).pdup(_data->pool), StringView(id).pdup(_data->pool)});
			levelList[level] = &record->childs.back();
		}
	}
	buffer.clear();

	_page->finalize();
}

void DocumentProcessor::claimTextSpan(token *t, const Callback<void()> &exportFn) {
	auto claimedBelow = _textSpanClaimed;
	_textSpanClaimed = false;

	auto before = buffer.weak().size();
	exportFn();

	if (!_textSpanClaimed && t && buffer.weak().size() > before) {
		// nothing below claimed the growth, so this token is the one that wrote it
		extendTextSpan(t);
		_textSpanClaimed = true;
	}

	_textSpanClaimed = _textSpanClaimed || claimedBelow;
}

void DocumentProcessor::exportToken(const CallbackStream &out, token *t) {
	claimTextSpan(t, [&] { HtmlProcessor::exportToken(out, t); });
}

void DocumentProcessor::exportTokenRaw(const CallbackStream &out, token *t) {
	claimTextSpan(t, [&] { HtmlProcessor::exportTokenRaw(out, t); });
}

void DocumentProcessor::exportTokenMath(const CallbackStream &out, token *t) {
	claimTextSpan(t, [&] { HtmlProcessor::exportTokenMath(out, t); });
}

document::SourceSpan DocumentProcessor::nodeSpanFor(token *t) {
	// A wrapper the base processor invented (the `code` inside a `pre`, a table's `colgroup`)
	// has no markup of its own, and an empty span says exactly that.
	auto end = nodeSpanEnd;
	nodeSpanEnd = 0;

	if (!t) {
		return document::SourceSpan();
	}

	end = sprt::max(end, t->start + t->len);

	// A paired marker (`**` of a strong, `*` of an emphasis) names only its own half; the element
	// owns everything up to its mate.
	if (t->mate && t->mate->start >= t->start) {
		end = sprt::max(end, t->mate->start + t->mate->len);
	}

	// A block token runs to the end of its line, trailing newline included. Keeping it would make
	// every reconstructed fragment carry a separator the caller adds itself, so the span stops at
	// the last character the element actually owns.
	end = sprt::min(end, uint32_t(source.size()));
	while (end > t->start
			&& (source[end - 1] == '\n' || source[end - 1] == '\r' || source[end - 1] == ' '
					|| source[end - 1] == '\t')) {
		--end;
	}

	return document::SourceSpan{t->start, end - t->start};
}

void DocumentProcessor::extendTextSpan(token *t) {
	_textBegin = sprt::min(_textBegin, t->start);
	_textEnd = sprt::max(_textEnd, t->start + t->len);
}

document::SourceSpan DocumentProcessor::takeTextSpan() {
	document::SourceSpan ret;
	if (_textBegin != maxOf<uint32_t>() && _textEnd > _textBegin) {
		ret.offset = _textBegin;
		ret.length = _textEnd - _textBegin;
	}
	_textBegin = maxOf<uint32_t>();
	_textEnd = 0;
	_textSpanClaimed = false;
	return ret;
}

void DocumentProcessor::processStyle(const StringView &name, document::StyleList &style,
		const StringView &styleData) {
	auto tmp = document::PageContainer::StringReader(styleData);
	_page->readStyle(style, tmp);
}

template <typename T>
void DocumentProcessor::processAttributes(const T &container, const StringView &name,
		document::StyleList &style, const Callback<void(StringView, StringView)> &cb) {
	for (auto &it : container) {
		if (it.first == "style") {
			processStyle(name, style, it.second);
		} else if (name == "img" && it.first == "src") {
			_page->addAsset(it.second);
		}
		cb(it.first, it.second);
	}
}

document::Node *DocumentProcessor::makeNode(token *t, const StringView &name, InitList &&attr,
		VecList &&vec) {
	auto node = new (_data->pool) document::Node(name);

	processAttributes(attr, name, node->getStyle(),
			[&](StringView key, StringView value) { node->setAttribute(key, value); });
	processAttributes(vec, name, node->getStyle(),
			[&](StringView key, StringView value) { node->setAttribute(key, value); });

	node->setSourceSpan(nodeSpanFor(t));

	if (name == "table" && node->getHtmlId().empty()) {
		++_tableIdx;
		node->setAttribute("id", string::toString<memory::PoolInterface>("__table:", _tableIdx));
	}

	return _nodeStack.back()->pushNode(node);
}

void DocumentProcessor::pushNode(token *t, const StringView &name, InitList &&attr, VecList &&vec) {
	memory::context ctx(_data->pool);
	flushBuffer();
	_nodeStack.push_back(makeNode(t, name, sp::move(attr), sp::move(vec)));
}

void DocumentProcessor::pushInlineNode(token *t, const StringView &name, InitList &&attr,
		VecList &&vec) {
	memory::context ctx(_data->pool);
	flushBuffer();
	makeNode(t, name, sp::move(attr), sp::move(vec));
}

void DocumentProcessor::popNode() {
	memory::context ctx(_data->pool);
	flushBuffer();
	_nodeStack.pop_back();
}

void DocumentProcessor::flushBuffer() {
	memory::context ctx(_data->pool);
	auto str = buffer.weak();
	auto span = takeTextSpan();
	if (!str.empty()) {
		StringView r(str);
		r.skipChars<StringView::CharGroup<CharGroupId::WhiteSpace>>();
		if (r.empty()) {
			// whitespace only: it separates two runs, and is dropped when there is nothing
			// to separate it from
			auto back = _nodeStack.back();
			if (back->hasValue() || !back->getNodes().empty()) {
				back->pushValue(StringView(" "), span);
			}
		} else {
			// Verbatim, trailing newline included. The buffer is flushed at every node boundary,
			// not once per block, so a newline dropped here is not a block separator but the
			// SPACE between two words - `...and a\n[link](...)` came out as `and alink`. What a
			// renderer does with it is a rendering question: collapsing whitespace is what
			// `white-space: normal` is for, and a code block needs the newline kept.
			_nodeStack.back()->pushValue(
					string::toUtf16Html<memory::PoolInterface>(StringView(str)), span);
		}
	}
	buffer.clear();
}

} // namespace stappler::mmd
