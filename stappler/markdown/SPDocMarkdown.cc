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

#include "SPDocMarkdown.h"
#include "SPDocMarkdownProcessor.h"
#include "SPDocFormat.h"
#include "SPFilesystem.h"
#include "MMDEngine.h"

namespace STAPPLER_VERSIONIZED stappler::document {

SP_USED static Format s_markdownFormat([](memory::pool_t *, FileInfo str, StringView ct) -> bool {
	return DocumentMarkdown::isMarkdownContentType(ct) || DocumentMarkdown::isMarkdown(str);
}, [](memory::pool_t *p, FileInfo str, StringView ct) -> Rc<Document> {
	return Rc<DocumentMarkdown>::create(p, str, ct);
}, [](memory::pool_t *, BytesView str, StringView ct) -> bool {
	return DocumentMarkdown::isMarkdown(str, ct);
}, [](memory::pool_t *p, BytesView str, StringView ct) -> Rc<Document> {
	return Rc<DocumentMarkdown>::create(p, str, ct);
}, 0);

bool DocumentMarkdown::isMarkdownContentType(StringView ct) {
	return ct == "text/markdown" || ct == "text/x-markdown" || ct == "text/x-multimarkdown";
}

bool DocumentMarkdown::isMarkdown(StringView str) {
	str.skipChars<StringView::CharGroup<CharGroupId::WhiteSpace>>();

	if (str.is("#") || str.is("{{TOC}}") || str.is("Title:")) {
		return true;
	}

	str.skipUntilString("\n#", true);
	return str.is("\n#");
}

bool DocumentMarkdown::isMarkdown(BytesView data, StringView ct) {
	if (isMarkdownContentType(ct)) {
		return true;
	}
	return isMarkdown(StringView(reinterpret_cast<const char *>(data.data()), data.size()));
}

bool DocumentMarkdown::isMarkdown(FileInfo path) {
	auto ext = filepath::lastExtension(path.path);
	if (ext == "md" || ext == "markdown" || ext == "mdown" || ext == "mmd") {
		return true;
	}

	if (ext == "text" || ext == "txt" || ext.empty()) {
		uint8_t buf[512] = {0};
		auto len = filesystem::readIntoBuffer(buf, path, 0, 512);
		if (len > 0) {
			return isMarkdown(StringView(reinterpret_cast<const char *>(buf), len));
		}
	}

	return false;
}

bool DocumentMarkdown::init(FileInfo path, StringView ct) {
	if (!Document::init()) {
		return false;
	}

	auto data = filesystem::readIntoMemory<memory::PoolInterface>(path);
	return read(data, ct);
}

bool DocumentMarkdown::init(BytesView data, StringView ct) {
	if (!Document::init()) {
		return false;
	}

	return read(data, ct);
}

bool DocumentMarkdown::init(memory::pool_t *pool, FileInfo path, StringView ct) {
	if (!Document::init(pool)) {
		return false;
	}

	auto data = filesystem::readIntoMemory<memory::PoolInterface>(path);
	return read(data, ct);
}

bool DocumentMarkdown::init(memory::pool_t *pool, BytesView data, StringView ct) {
	if (!Document::init(pool)) {
		return false;
	}

	return read(data, ct);
}

bool DocumentMarkdown::read(BytesView data, StringView ct) {
	if (data.empty()) {
		return false;
	}

	memory::context ctx(_pool);

	// The tree keeps byte ranges into this text, so it has to outlive the parse: it is copied
	// into the document's pool, not borrowed from the caller's buffer.
	_source = StringView(reinterpret_cast<const char *>(data.data()), data.size()).pdup(_pool);

	// The engine parses in a pool of its own; the nodes it produces for us are allocated in the
	// document's pool by the processor.
	mmd::Engine e;
	e.init(_source, mmd::StapplerExtensions);
	e.process([&](const mmd::Content &c, const StringView &s, const mmd::Token &t) {
		mmd::DocumentProcessor p;
		p.init(this, _data);
		p.process(c, s, t);
	});

	_data->type = ct.empty() ? StringView("text/markdown") : ct.pdup(_data->pool);

	return !_data->pages.empty();
}

PageContainer *DocumentMarkdown::acquireRootPage() {
	memory::context ctx(_pool);
	if (_data->pages.empty()) {
		auto page = new (_pool) PageContainer(_data);
		_data->pages.emplace(StringView("/"), page);
		_data->spine.emplace_back(SpineFile{StringView("/"), true});
	}

	return _data->pages.begin()->second;
}

} // namespace stappler::document
