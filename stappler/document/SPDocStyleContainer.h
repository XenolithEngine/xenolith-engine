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

#ifndef CORE_DOCUMENT_SPDOSTYLECONTAINER_H_
#define CORE_DOCUMENT_SPDOSTYLECONTAINER_H_

#include "SPDocStyle.h"
#include "SPDocNode.h"

namespace STAPPLER_VERSIONIZED stappler::document {

class SP_PUBLIC StyleContainer : public memory::AllocPool,
								 public InterfaceObject<memory::PoolInterface> {
public:
	using HtmlIdentifier = chars::Compose<char32_t, chars::Range<char32_t, u'0', u'9'>,
			chars::Range<char32_t, u'A', u'Z'>, chars::Range<char32_t, u'a', u'z'>,
			chars::Chars<char32_t, u'_', u'-', u'!', u'/', u':'> >;

	using CssIdentifier = chars::Compose<char32_t, chars::Range<char32_t, u'0', u'9'>,
			chars::Range<char32_t, u'A', u'Z'>, chars::Range<char32_t, u'a', u'z'>,
			chars::Chars<char32_t, u'_', u'-', u'!', u'.', u',', u'*', u'#', u'@', '+', '-', '~',
					'>', '%'> >;

	using CssSelectorStart = chars::Compose<char32_t, CssIdentifier, chars::Chars<char32_t, u'['> >;

	using CssIdentifierExtended = chars::Compose<char32_t, CssIdentifier,
			chars::Chars<char32_t, '=', '|', '^', '$', ',', ':', '/', '?', '&'> >;

	using SingleQuote = chars::Chars<char32_t, u'\''>;
	using DoubleQuote = chars::Chars<char32_t, u'"'>;

	template <char32_t First, char32_t Second>
	using Range = chars::Range<char32_t, First, Second>;

	template <char16_t... Args>
	using Chars = chars::Chars<char32_t, Args...>;

	template <CharGroupId G>
	using Group = chars::CharGroup<char32_t, G>;

	using StringReader = StringViewUtf8;

	struct StyleBuffers {
		BufferTemplate<Interface> selector;
		BufferTemplate<Interface> name;
		BufferTemplate<Interface> value;

		auto getSelectorStream();
		auto getNameStream();
		auto getValueStream();
		void nameToLower();
		void valueToLower();
	};

	enum class StyleType {
		Css
	};

	static StringView resolveCssString(const StringView &origStr);
	static void readQuotedString(StringReader &s, String &str, char quoted);
	static void readCssParameter(const StringView &name, const StringView &value,
			const StyleCallback &cb, const StringCallback &strCb);

	StyleContainer(DocumentData *, StyleType = StyleType::Css);

	bool readStyle(StringReader &);
	bool readStyle(FileInfo);
	bool readStyle(StyleList &target, StringReader &);

	FontFace readFontFace(StyleBuffers &buffers, StringReader &s);

	MediaQuery::Query readMediaQuery(StyleBuffers &buffers, StringReader &s);

	MediaQuery::Vector<MediaQuery::Query> readMediaQueryList(StyleBuffers &buffers,
			StringReader &s);

	void resolveNodeStyle(StyleList &style, const Node &node, const SpanView<const Node *> &stack,
			const MediaParameters &media, const SpanView<bool> &resolved) const;

protected:
	void import(StringReader &);

	void readStyleParameters(const StringView &name, const StringView &value,
			const StyleCallback &);

	DocumentData *_document = nullptr;
	StyleType _type = StyleType::Css;
	Map<String, StyleList> _styles;
	Map<String, Vector<FontFace>> _fonts;
};

} // namespace stappler::document

#endif /* CORE_DOCUMENT_SPDOSTYLECONTAINER_H_ */
