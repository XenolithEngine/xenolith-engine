/*

	Copyright © 2016 - 2017 Fletcher T. Penney.
	Copyright © 2017 Roman Katuntsev <sbkarr@stappler.org>


	The `MultiMarkdown 6` project is released under the MIT License..

	GLibFacade.c and GLibFacade.h are from the MultiMarkdown v4 project:

		https://github.com/fletcher/MultiMarkdown-4/

	MMD 4 is released under both the MIT License and GPL.


	CuTest is released under the zlib/libpng license. See CuTest.c for the text
	of the license.


	## The MIT License ##

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

*/

#ifndef STAPPLER_MARKDOWN_MMD_COMMON_MMDCONTENT_H_
#define STAPPLER_MARKDOWN_MMD_COMMON_MMDCONTENT_H_

#include "MMDToken.h"

namespace STAPPLER_VERSIONIZED stappler::mmd {

class SP_PUBLIC Content : public InterfaceObject<memory::PoolInterface> {
public:
	template <typename V>
	using Dict = typename Interface::DictionaryType<V>;

	template <typename V>
	using DictView = typename Interface::MapType<StringView, V>;

	struct Link : memory::AllocPool {
		using AttrVec = Vector<Pair<StringView, StringView>>;

		Token label;
		String label_text;
		String clean_text;
		String url;
		String title;
		AttrVec attributes;

		Link(const StringView &source, Token && label, String && url, const StringView & title, const StringView & attributes, bool clearUrl);
		Link(const StringView &source, Token && label);

		static void parseAttributes(AttrVec &, const StringView &);
	};

	struct Footnote : memory::AllocPool {
		enum Type {
			Abbreviation,
			Citation,
			Glossary,
			Note
		};

		Token label;
		Token content;
		String label_text;
		String clean_text;
		size_t count = maxOf<size_t>();
		bool free_para = false;
		bool reference = true;

		Footnote(const StringView &source, Token && label, Token && content, bool lowercase, Type);
	};

	static String labelFromString(const StringView &);
	static String cleanString(const StringView &, bool lowercase);

	static Link *explicitLink(const StringView &, const Extensions &, token * bracket, token * paren);

	void reset();

	Content(const Extensions &);

	const Extensions &getExtensions() const;
	void addExtension(Extensions::Value) const;

	void setQuotesLanguage(QuotesLanguage);
	QuotesLanguage getQuotesLanguage() const;

	void process(const StringView &);

	void emplaceMeta(StringView key, StringView value);
	void emplaceHtmlId(Token &&, const StringView &);

	const Vector<Token> &getHeaders() const;
	const Vector<Token> &getDefinitions() const;
	const Vector<Token> &getTables() const;

	const Vector<Footnote *> &getAbbreviations() const;
	const Vector<Footnote *> &getCitations() const;
	const Vector<Footnote *> &getGlossary() const;
	const Vector<Footnote *> &getFootnotes() const;
	const Vector<Link *> &getLinks() const;

	const Dict<String> &getMetaDict() const;

	Link *getLink(const StringView &) const;
	Footnote *getAbbreviation(const StringView &) const;
	Footnote *getCitation(const StringView &) const;
	Footnote *getFootnote(const StringView &) const;
	Footnote *getGlossary(const StringView &) const;
	StringView getMeta(const StringView &) const;

protected:
	void processDefinitions(const StringView &);
	void processDefinition(const StringView &, const Token &);
	bool extractDefinition(const StringView &, token **);

	void processHeaders(const StringView &);
	void processHeader(const StringView &, const Token &);

	void processTables(const StringView &);
	void processTable(const StringView &, const Token &);

	Extensions extensions = Extensions::None;
	QuotesLanguage quotes = QuotesLanguage::English;

	Vector<Token> headers;
	Vector<Token> definitions;
	Vector<Token> tables;

	Vector<Footnote *> abbreviation;
	Vector<Footnote *> citation;
	Vector<Footnote *> glossary;
	Vector<Footnote *> footnotes;
	Vector<Link *> links;

	Dict<String> meta;

	DictView<Content::Link *> linksView;
	DictView<Content::Footnote *> citationView;
	DictView<Content::Footnote *> footnotesView;
	DictView<Content::Footnote *> abbreviationView;
	DictView<Content::Footnote *> glossaryView;
};

SP_PUBLIC StringView text_inside_pair(const StringView & s, token * pair);
SP_PUBLIC auto url_accept(const char * source, size_t start, size_t max_len, size_t * end_pos, bool validate) -> Content::String;
SP_PUBLIC auto label_from_token(const StringView & s, token * t) -> Content::String;
SP_PUBLIC auto clean_string_from_range(const StringView & s, size_t start, size_t len, bool lowercase) -> Content::String;
SP_PUBLIC token * manual_label_from_header(token * h, const char * source);
SP_PUBLIC bool table_has_caption(token * t);

}

#endif /* STAPPLER_MARKDOWN_MMD_COMMON_MMDCONTENT_H_ */
