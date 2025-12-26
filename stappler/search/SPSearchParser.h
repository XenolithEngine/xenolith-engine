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

#ifndef STAPPLER_SEARCH_SPSEARCHPARSER_H_
#define STAPPLER_SEARCH_SPSEARCHPARSER_H_

#include "SPSearchEnum.h"
#include "SPStringView.h"
#include "SPMemory.h"

namespace STAPPLER_VERSIONIZED stappler::search {

using namespace mem_pool;

/* API based on postgresql full-text, but parser more correct with urls, emails and paths */

enum class ParserToken {
	AsciiWord,
	Word,
	NumWord,
	Email,
	Url,
	ScientificFloat,
	Version, // or ip-address, or some date
	Blank,
	NumHyphenatedWord,
	AsciiHyphenatedWord,
	HyphenatedWord,
	Path,
	Float,
	Integer,
	XMLEntity,
	Custom,
	HyphenatedWord_NumPart,
	HyphenatedWord_Part,
	HyphenatedWord_AsciiPart,
};

enum class UrlToken {
	Scheme,
	User,
	Password,
	Host,
	Port,
	Path,
	Query,
	Fragment,
	Blank,
};

using Language = ::SnowballLanguage;

enum class ParserStatus {
	Continue = 0, // just continue parsing
	PreventSubdivide = 1, // do not subdivide complex token (works with isComplexWord(ParserToken))
	Stop = 2, // stop parsing in place
};

enum class SearchRank {
	Unknown,
	D,
	C,
	B,
	A,
};

struct SP_PUBLIC SearchData {
	using Language = search::Language;
	using Rank = SearchRank;

	enum Type {
		Parse,
		Cast,
		ForceCast,
	};

	String buffer;
	Language language = Language::Unknown;
	Rank rank = Rank::D;

	StringView getLanguage() const;
};

enum class SearchOp : uint8_t {
	None,
	And,
	Or,
	Follow,
};

struct StemmerEnv;

SP_PUBLIC bool isStopword(const StringView &word, Language lang = Language::Unknown);
SP_PUBLIC bool isStopword(const StringView &word, StemmerEnv *);
SP_PUBLIC bool isStopword(const StringView &word, const StringView *);

SP_PUBLIC StringView getLanguageName(Language);
SP_PUBLIC Language parseLanguage(const StringView &);
SP_PUBLIC Language detectLanguage(const StringView &);

SP_PUBLIC StringView getParserTokenName(ParserToken);

SP_PUBLIC bool isWordPart(ParserToken);
SP_PUBLIC bool isComplexWord(ParserToken);

SP_PUBLIC void parseHtml(StringView, const Callback<void(StringView)> &);

SP_PUBLIC bool parseUrl(StringView &s, const Callback<void(StringViewUtf8, UrlToken)> &cb);
SP_PUBLIC bool parsePhrase(StringView, const Callback<ParserStatus(StringView, ParserToken)> &);

SP_PUBLIC StemmerEnv *getStemmer(Language lang);

SP_PUBLIC bool stemWord(StringView word, const Callback<void(StringView)> &, StemmerEnv *env);
SP_PUBLIC bool stemWord(StringView word, const Callback<void(StringView)> &, Language lang = Language::Unknown);

// lowercase, remove soft hyphens
SP_PUBLIC String normalizeWord(const StringView &str);

}

#endif /* STAPPLER_SEARCH_SPSEARCHPARSER_H_ */
