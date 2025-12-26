/**
 Copyright (c) 2023-2024 Stappler LLC <admin@stappler.dev>
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#ifndef CORE_DOCUMENT_HTML_SPDOCHTML_H_
#define CORE_DOCUMENT_HTML_SPDOCHTML_H_

#include "SPDocument.h"
#include "SPDocPageContainer.h" // IWYU pragma: keep
#include "SPHtmlParser.h"

namespace STAPPLER_VERSIONIZED stappler::document {

class Node;

struct HtmlTag : html::Tag<StringView> {
	enum Type {
		Html,
		Head,
		Meta,
		Title,
		Base,
		Link,
		Style,
		Script,
		Body,
		Image,
		Special,
		Block,
	};

	Type type = Block;
	Node *node = nullptr;

	HtmlTag(StringView name) : Tag(name) { }

	explicit operator bool() const { return !name.empty(); }

	static Type getType(const StringView &tagName);
	static bool isForceUnclosed(const StringView &tagName);
};

struct HtmlReader {
	using Parser = html::Parser<HtmlReader, StringView, HtmlTag>;

	using Interface = memory::PoolInterface;

	template <typename T>
	using Vector = Interface::VectorType<T>;

	using String = Interface::StringType;

	static String encodePathString(StringView r);

	void onBeginTag(Parser &p, HtmlTag &tag);
	void onEndTag(Parser &p, HtmlTag &tag, bool isClosable);
	void onTagAttribute(Parser &p, HtmlTag &tag, StringView &name, StringView &value);
	void onTagAttributeList(Parser &p, HtmlTag &tag, StringView &data);
	void onPushTag(Parser &p, HtmlTag &tag);
	void onPopTag(Parser &p, HtmlTag &tag);
	void onInlineTag(Parser &p, HtmlTag &tag);
	void onTagContent(Parser &p, HtmlTag &tag, StringView &s);
	void onSchemeTag(Parser &p, StringView &name, StringView &value);
	void onCommentTag(Parser &p, StringView &data);

	HtmlReader(PageContainer *);

	PageContainer *page = nullptr;
	Vector<Node *> nodeStack;

	uint32_t _htmlTag = 0;
	uint32_t _bodyTag = 0;
	uint32_t _headTag = 0;
	uint32_t _pseudoId = 0;
};

class SP_PUBLIC DocumentHtml : public Document {
public:
	static bool isHtml(StringView);
	static bool isHtml(FileInfo);
	static bool isHtml(BytesView);

	virtual ~DocumentHtml() = default;

	virtual bool init(FileInfo, StringView ct = StringView());
	virtual bool init(BytesView, StringView ct = StringView());
	virtual bool init(memory::pool_t *, FileInfo, StringView ct = StringView());
	virtual bool init(memory::pool_t *, BytesView, StringView ct = StringView());

protected:
	virtual bool read(BytesView, StringView ct);
};

} // namespace stappler::document

#endif // CORE_DOCUMENT_HTML_SPDOCHTML_H_
