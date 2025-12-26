/**
Copyright (c) 2020-2022 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

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

#ifndef STAPPLER_SEARCH_SPSEARCHCONFIGURATION_H_
#define STAPPLER_SEARCH_SPSEARCHCONFIGURATION_H_

#include "SPSearchQuery.h"

namespace STAPPLER_VERSIONIZED stappler::search {

class SP_PUBLIC Configuration : public memory::AllocPool {
public:
	using StemmerCallback = Function<bool(StringView, const Callback<void(StringView)> &)>;
	using StemWordCallback = Callback<void(StringView, StringView, ParserToken)>;

	using PreStemCallback = Function<Vector<StringView>(StringView, ParserToken)>;

	using WordMap = memory::dict<StringView, StringView>;

	struct HeadlineConfig {
		static constexpr size_t DefaultMaxWords = 24;
		static constexpr size_t DefaultMinWords = 12;
		static constexpr size_t DefaultShortWord = 3;

		StringView startToken = StringView("<b>");
		StringView stopToken = StringView("</b>");

		StringView startFragment = StringView("<div>");;
		StringView stopFragment = StringView("</div>");
		StringView separator = StringView("…");

		size_t maxWords = DefaultMaxWords;
		size_t minWords = DefaultMinWords;
		size_t shortWord = DefaultShortWord;

		Function<void(StringView, StringView)> fragmentCallback;
	};

	Configuration();
	Configuration(Language);
	virtual ~Configuration();

	void setLanguage(Language);
	Language getLanguage() const;

	void setStemmer(ParserToken, StemmerCallback &&);
	StemmerCallback getStemmer(ParserToken) const;

	void setCustomStopwords(const StringView *);
	const StringView *getCustomStopwords() const;

	void setPreStem(PreStemCallback &&);
	const PreStemCallback &getPreStem() const;

	void stemPhrase(const StringView &, const StemWordCallback &) const;
	void stemHtml(const StringView &, const StemWordCallback &) const;

	bool stemWord(const StringView &, ParserToken, const StemWordCallback &) const;

	String makeHeadline(const HeadlineConfig &, const StringView &origin, const Vector<String> &stemList) const;

	String makeHtmlHeadlines(const HeadlineConfig &, const StringView &origin, const Vector<String> &stemList, size_t count = 1) const;

	String makeHeadlines(const HeadlineConfig &, const Callback<void(const Function<bool(const StringView &frag, const StringView &tag)>)> &producer,
			const Vector<String> &stemList, size_t count = 1) const;

	Vector<String> stemQuery(const SearchQuery &query) const;

	size_t makeSearchVector(SearchVector &, StringView phrase, SearchData::Rank rank = SearchData::Rank::Unknown, size_t counter = 0,
			const Callback<void(StringView, StringView, ParserToken)> & = nullptr) const;

	// encode for postgres textual representation
	String encodeSearchVectorPostgres(const SearchVector &, SearchData::Rank rank = SearchData::Rank::Unknown) const;

	// encode for binary blob
	Bytes encodeSearchVectorData(const SearchVector &, SearchData::Rank rank = SearchData::Rank::Unknown) const;

	SearchQuery parseQuery(StringView, bool strict = false, StringView *err = nullptr) const;

	bool isMatch(const SearchVector &, StringView) const;
	bool isMatch(const SearchVector &, const SearchQuery &) const;

protected:
	struct Data;

	StemmerEnv *getEnvForToken(ParserToken) const;

	void doStemQuery(Vector<String> &, const SearchQuery &query) const;

	Data *data;
};

}

#endif /* STAPPLER_SEARCH_SPSEARCHCONFIGURATION_H_ */
