/**
Copyright (c) 2020-2022 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>

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

#include "SPSearchConfiguration.h"
#include "SPHtmlParser.h"
#include "SPValid.h"
#include "SPData.h"
#include <inttypes.h>

namespace STAPPLER_VERSIONIZED stappler::search {

static StemmerEnv *Configuration_makeLocalConfig(StemmerEnv *orig);
static void Stemmer_Reader_run(StringView origin,
		Function<void(const StringView &, const Callback<void()> &cancelCb)> &&cb);
static void *StemmerEnv_getUserData(StemmerEnv *);

static bool stemWordDefault(Language lang, StemmerEnv *env, ParserToken tok, StringView word,
		const Callback<void(StringView)> &cb, const StringView *stopwords) {
	switch (tok) {
	case ParserToken::AsciiWord:
	case ParserToken::AsciiHyphenatedWord:
	case ParserToken::HyphenatedWord_AsciiPart:
	case ParserToken::Word:
	case ParserToken::HyphenatedWord:
	case ParserToken::HyphenatedWord_Part:
		switch (lang) {
		case Language::Simple: {
			auto str = normalizeWord(word);
			if (stopwords && isStopword(str, stopwords)) {
				return false;
			}
			cb(str);
			break;
		}
		default: {
			auto str = normalizeWord(word);
			if (stopwords && isStopword(str, stopwords)) {
				return false;
			}
			return stemWord(str, cb, env);
			break;
		}
		}
		break;

	case ParserToken::NumWord:
	case ParserToken::NumHyphenatedWord:
	case ParserToken::HyphenatedWord_NumPart: {
		auto str = normalizeWord(word);
		if (stopwords && isStopword(str, stopwords)) {
			return false;
		}
		cb(str);
		break;
	}

	case ParserToken::Email: {
		auto str = normalizeWord(word);
		valid::validateEmail(str);
		if (stopwords && isStopword(str, stopwords)) {
			return false;
		}
		cb(str);
		break;
	}

	case ParserToken::Url: {
		auto str = normalizeWord(word);
		valid::validateUrl(str);
		if (stopwords && isStopword(str, stopwords)) {
			return false;
		}
		cb(str);
		break;
	}

	case ParserToken::Version:
	case ParserToken::Path:
	case ParserToken::ScientificFloat: {
		auto str = normalizeWord(word);
		if (stopwords && isStopword(str, stopwords)) {
			return false;
		}
		cb(str);
		break;
	}

	case ParserToken::Float:
	case ParserToken::Integer: {
		cb(word);
		break;
	}

	case ParserToken::Custom: {
		StringViewUtf8 tmp(word);
		auto num = tmp.readChars<StringViewUtf8::CharGroup<CharGroupId::Numbers>>();
		if (num.size() == 2) {
			if (tmp.is(':') || tmp.is('-') || tmp.is(u'–')) {
				bool cond = tmp.is('-') || tmp.is(u'–');
				String str;
				str.reserve(word.size());
				if (cond) {
					StringViewUtf8 word2(word);
					while (!word2.empty()) {
						auto r = word2.readUntil<StringViewUtf8::CharGroup<CharGroupId::WhiteSpace>,
								StringViewUtf8::Chars<u'–'>, StringViewUtf8::Chars<':'>>();
						if (!r.empty()) {
							str.append(r.data(), r.size());
						}
						if (word2.is(u'–')) {
							str.emplace_back('-');
							++word2;
						} else if (word2.is(':')) {
							str.emplace_back(':');
							++word2;
						} else {
							auto space = word2.readChars<
									StringViewUtf8::CharGroup<CharGroupId::WhiteSpace>>();
							if (cond && !space.empty() && !r.empty()) {
								str.emplace_back('/');
							}
						}
					}
				} else {
					while (!word.empty()) {
						auto r = word.readUntil<
								StringViewUtf8::CharGroup<CharGroupId::WhiteSpace>>();
						if (!r.empty()) {
							str.append(r.data(), r.size());
						}
					}
				}
				cb(string::tolower<Interface>(StringView(str)));
				return true;
			}
		}
		auto str = normalizeWord(word);
		if (stopwords && isStopword(str, stopwords)) {
			return false;
		}
		cb(str);
		break;
	}
	case ParserToken::XMLEntity:
	case ParserToken::Blank: return false; break;
	}
	return true;
}

struct Configuration::Data : AllocBase {
	pool_t *pool = nullptr;
	std::atomic<uint32_t> refCount = 1;
	Language language = Language::Simple;
	StemmerEnv *primary = nullptr;
	StemmerEnv *secondary = nullptr;

	Map<ParserToken, StemmerCallback> stemmers;

	PreStemCallback preStem;
	const StringView *customStopwords = nullptr;

	Data(pool_t *p, Language lang)
	: pool(p)
	, language(lang)
	, primary(search::getStemmer(language))
	, secondary(search::getStemmer(
			  (lang == Language::Simple) ? Language::Simple : Language::English)) { }
};

Configuration::Configuration() : Configuration(Language::English) { }

Configuration::Configuration(Language lang) {
	pool::initialize();
	auto p = pool::create(pool::acquire());
	perform([&, this] { data = new (p) Data(p, lang); }, p);
}

Configuration::~Configuration() {
	if (data->refCount.fetch_sub(1) == 1) {
		data->~Data();
		pool::destroy(data->pool);
		pool::terminate();
	}
}

void Configuration::setLanguage(Language lang) {
	perform([&, this] {
		auto prev = data->language;
		auto prevSec = (prev == Language::Simple) ? Language::Simple : Language::English;
		auto newSec = (lang == Language::Simple) ? Language::Simple : Language::English;
		data->language = lang;
		data->primary = search::getStemmer(data->language);
		if (prevSec != newSec) {
			data->secondary = search::getStemmer(newSec);
		}
	}, data->pool);
}

Language Configuration::getLanguage() const { return data->language; }

void Configuration::setStemmer(ParserToken tok, StemmerCallback &&cb) {
	perform([&, this] { data->stemmers.emplace(tok, move(cb)); }, data->pool);
}

Configuration::StemmerCallback Configuration::getStemmer(ParserToken tok) const {
	auto it = data->stemmers.find(tok);
	if (it != data->stemmers.end()) {
		return it->second;
	}

	return StemmerCallback([&, lang = data->language, env = getEnvForToken(tok),
								   stopwords = data->customStopwords](StringView word,
								   const Callback<void(StringView)> &cb) -> bool {
		return stemWordDefault(lang, env, tok, word, cb, stopwords);
	});
}

void Configuration::setCustomStopwords(const StringView *w) { data->customStopwords = w; }

const StringView *Configuration::getCustomStopwords() const { return data->customStopwords; }

void Configuration::setPreStem(PreStemCallback &&cb) {
	perform([&, this] { data->preStem = move(cb); }, data->pool);
}
const Configuration::PreStemCallback &Configuration::getPreStem() const { return data->preStem; }

void Configuration::stemPhrase(const StringView &str, const StemWordCallback &cb) const {
	parsePhrase(str, [&, this](StringView word, ParserToken tok) {
		if (data->preStem != nullptr && !isWordPart(tok)) {
			auto ret = data->preStem(word, tok);
			if (!ret.empty()) {
				for (auto &it : ret) {
					auto str = normalizeWord(it);
					cb(word, str, tok);
				}
				return isComplexWord(tok) ? ParserStatus::PreventSubdivide : ParserStatus::Continue;
			}
		}
		stemWord(word, tok, cb);
		return ParserStatus::Continue;
	});
}

size_t Configuration::makeSearchVector(SearchVector &vec, StringView str, SearchData::Rank rank,
		size_t counter, const Callback<void(StringView, StringView, ParserToken)> &cb) const {
	if (str.empty()) {
		return counter;
	}

	auto pushWord = [&](StringView s) -> const StringView * {
		++vec.documentLength;
		auto it = vec.words.find(s);
		if (it == vec.words.end()) {
			return &vec.words
							.emplace(s.pdup(vec.words.get_allocator()),
									SearchVector::MatchVector({pair(counter, rank)}))
							.first->first;
		} else {
			auto value = pair(counter, rank);
			auto iit = std::lower_bound(it->second.begin(), it->second.end(), value,
					[&](const Pair<size_t, SearchData::Rank> &l,
							const Pair<size_t, SearchData::Rank> &r) {
				if (l.first != r.first) {
					return l.first < r.first;
				} else {
					return toInt(l.second) < toInt(r.second);
				}
			});
			if (iit == it->second.end()) {
				it->second.emplace_back(value);
			} else if (*iit != value) {
				it->second.emplace(iit, value);
			}
			return &it->first;
		}
	};

	parsePhrase(str, [&, this](StringView word, ParserToken tok) {
		if (tok != ParserToken::Blank && !isWordPart(tok)) {
			++counter;
		}

		if (data->preStem != nullptr && !isWordPart(tok)) {
			auto ret = data->preStem(word, tok);
			if (ret.size() == 1) {
				auto str = normalizeWord(ret.back());
				if (auto sPtr = pushWord(str)) {
					if (cb != nullptr) {
						cb(*sPtr, word, tok);
					}
					return isComplexWord(tok) ? ParserStatus::PreventSubdivide
											  : ParserStatus::Continue;
				}
			} else if (!ret.empty()) {
				for (auto &it : ret) {
					auto str = normalizeWord(it);
					pushWord(str);
				}
				return isComplexWord(tok) ? ParserStatus::PreventSubdivide : ParserStatus::Continue;
			}
		}

		stemWord(word, tok, [&](StringView w, StringView s, ParserToken tok) {
			if (!s.empty()) {
				if (auto sPtr = pushWord(s)) {
					if (cb != nullptr) {
						cb(*sPtr, word, tok);
					}
				}
			}
		});
		return ParserStatus::Continue;
	});

	return counter;
}

String Configuration::encodeSearchVectorPostgres(const SearchVector &vec,
		SearchData::Rank rank) const {
	StringStream ret;
	for (auto &it : vec.words) {
		if (!ret.empty()) {
			ret << " ";
		}

		StringView r(it.first);
		ret << "'";
		while (!r.empty()) {
			auto v = r.readUntil<StringView::Chars<'\''>>();
			if (!v.empty()) {
				ret << v;
			}
			if (r.is('\'')) {
				ret << "''";
				++r;
			}
		}
		ret << "':";
		for (auto &v : it.second) {
			if (ret.weak().back() != ':') {
				ret << ",";
			}
			ret << v.first;
			auto r = v.second;
			if (r == SearchRank::Unknown) {
				r = rank;
			}
			switch (r) {
			case SearchRank::A: ret << 'A'; break;
			case SearchRank::B: ret << 'B'; break;
			case SearchRank::C: ret << 'C'; break;
			case SearchRank::D:
			case SearchRank::Unknown: break;
			}
		}
	}
	return ret.str();
}

Bytes Configuration::encodeSearchVectorData(const SearchVector &data, SearchData::Rank rank) const {
	data::cbor::Encoder<Interface> enc(true);
	data::cbor::_writeArrayStart(enc, 3);
	data::cbor::_writeInt(enc, 1); // version
	data::cbor::_writeInt(enc, data.documentLength); // version
	data::cbor::_writeMapStart(enc, data.words.size());
	for (auto &it : data.words) {
		enc.write(it.first);
		_writeArrayStart(enc, it.second.size() * 2);
		for (auto &iit : it.second) {
			data::cbor::_writeInt(enc, iit.first);
			data::cbor::_writeInt(enc,
					toInt((iit.second == SearchData::Rank::Unknown) ? rank : iit.second));
		}
	}
	auto result = enc.data();
	auto r = data::compress<Interface>(result.data(), result.size(),
			data::EncodeFormat::Compression::LZ4HCCompression, true);
	if (r.empty()) {
		return result;
	}
	return r;
}

void Configuration::stemHtml(const StringView &str, const StemWordCallback &cb) const {
	parseHtml(str, [&, this](StringView str) { stemPhrase(str, cb); });
}

bool Configuration::stemWord(const StringView &word, ParserToken tok,
		const StemWordCallback &cb) const {
	auto it = data->stemmers.find(tok);
	if (it != data->stemmers.end()) {
		return it->second(word, [&](StringView stem) { cb(word, stem, tok); });
	} else {
		return stemWordDefault(data->language, getEnvForToken(tok), tok, word,
				[&](StringView stem) { cb(word, stem, tok); }, data->customStopwords);
	}
}

StemmerEnv *Configuration::getEnvForToken(ParserToken tok) const {
	switch (tok) {
	case ParserToken::AsciiWord:
	case ParserToken::AsciiHyphenatedWord:
	case ParserToken::HyphenatedWord_AsciiPart:
		if (data->secondary) {
			if (memory::pool::acquire() == StemmerEnv_getUserData(data->secondary)) {
				return data->secondary;
			} else {
				return Configuration_makeLocalConfig(data->secondary);
			}
		}
		break;
	case ParserToken::Word:
	case ParserToken::HyphenatedWord:
	case ParserToken::HyphenatedWord_Part:
		if (data->primary) {
			if (memory::pool::acquire() == StemmerEnv_getUserData(data->primary)) {
				return data->primary;
			} else {
				return Configuration_makeLocalConfig(data->primary);
			}
		}
		break;

	case ParserToken::NumWord:
	case ParserToken::NumHyphenatedWord:
	case ParserToken::HyphenatedWord_NumPart:
	case ParserToken::Email:
	case ParserToken::Url:
	case ParserToken::Version:
	case ParserToken::Path:
	case ParserToken::Integer:
	case ParserToken::Float:
	case ParserToken::ScientificFloat:
	case ParserToken::XMLEntity:
	case ParserToken::Custom:
	case ParserToken::Blank: return nullptr;
	}
	return nullptr;
}

String Configuration::makeHeadline(const HeadlineConfig &cfg, const StringView &origin,
		const Vector<String> &stemList) const {
	memory::PoolInterface::StringStreamType result;
	result.reserve(
			origin.size() + (cfg.startToken.size() + cfg.stopToken.size()) * stemList.size());

	bool isOpen = false;
	StringViewUtf8 r(origin);
	StringView dropSep;

	parsePhrase(origin, [&, this](StringView word, ParserToken tok) {
		auto status = ParserStatus::Continue;
		if (tok == ParserToken::Blank
				|| !stemWord(word, tok, [&](StringView word, StringView stem, ParserToken tok) {
			auto it = std::lower_bound(stemList.begin(), stemList.end(), stem);
			if (it != stemList.end() && *it == stem) {
				if (!isOpen) {
					result << cfg.startToken;
					isOpen = true;
				} else if (!dropSep.empty()) {
					result << dropSep;
					dropSep.clear();
				}
				if (isComplexWord(tok)) {
					status = ParserStatus::PreventSubdivide;
				}
			} else {
				if (isOpen) {
					result << cfg.stopToken;
					isOpen = false;
					if (!dropSep.empty()) {
						result << dropSep;
						dropSep.clear();
					}
				}
				if (isComplexWord(tok)) {
					return;
				}
			}
			result << word;
		})) {
			if (isOpen) {
				if (!dropSep.empty()) {
					dropSep =
							StringView(dropSep.data(), word.data() + word.size() - dropSep.data());
				} else {
					dropSep = word;
				}
			} else {
				result << word;
			}
		}
		return status;
	});

	if (isOpen) {
		result << cfg.stopToken;
		isOpen = false;
	}

	return result.str();
}

String Configuration::makeHtmlHeadlines(const HeadlineConfig &cfg, const StringView &origin,
		const Vector<String> &stemList, size_t count) const {
	return makeHeadlines(cfg,
			[&](const Function<bool(const StringView &, const StringView &)> &cb) {
		Stemmer_Reader_run(origin, [&](const StringView &str, const Callback<void()> &cancelCb) {
			if (!cb(str, StringView())) {
				cancelCb();
			}
		});
	}, stemList, count);
}

String Configuration::makeHeadlines(const HeadlineConfig &cfg,
		const Callback<void(const Function<bool(const StringView &frag, const StringView &tag)>)>
				&cb,
		const Vector<String> &stemList, size_t count) const {

	using SplitTokens =
			StringViewUtf8::MatchCompose< StringViewUtf8::MatchCharGroup<CharGroupId::WhiteSpace>,
					StringViewUtf8::MatchChars<'-', u'—', u'\'', u'«', u'»', u'’', u'“', '(', ')',
							'"', ',', '*', ':', ';', '/', '\\'>>;

	using TrimToken = StringView::Chars<'.'>;

	struct WordIndex {
		StringView word;
		uint16_t index;
		uint16_t selectedCount; // selected words in block
		uint16_t allWordsCount; // all words in block;
		const WordIndex *end;
	};

	WordIndex *topIndex = nullptr;
	StringStream ret;
	ret.reserve(1_KiB);

	auto rateWord = [&](WordIndex &index, WordIndex *list, size_t listCount) {
		index.end = &index;
		index.selectedCount = 1;
		index.allWordsCount = 1;
		while (listCount > 0) {
			uint16_t offset = list->index - index.index;
			if (offset < cfg.maxWords) {
				++index.selectedCount;
				index.allWordsCount = offset;
				index.end = list;
			} else {
				break;
			}

			++list;
			--listCount;
		}

		if (!topIndex || index.selectedCount > topIndex->selectedCount
				|| (index.selectedCount == topIndex->selectedCount
						&& index.allWordsCount < topIndex->allWordsCount)) {
			topIndex = &index;
		}
	};

	auto writeFragmentWords = [&](StringStream &out, const StringView &str,
									  const std::array<WordIndex, 32> &words,
									  const WordIndex *word) {
		bool isOpen = false;
		for (auto it = word; it < word->end; ++it) {
			if (!isOpen) {
				out << cfg.startToken;
			}
			out << it->word;
			auto next = it + 1;
			if (next <= word->end && it->index + 1 == next->index) {
				isOpen = true;
			} else {
				isOpen = false;
				out << cfg.stopToken;
			}

			if (next <= word->end) {
				out << StringView(it->word.data() + it->word.size(),
						next->word.data() - (it->word.data() + it->word.size()));
			}
		}

		if (!isOpen) {
			out << cfg.startToken;
		}
		out << word->end->word;
		out << cfg.stopToken;
		isOpen = false;
	};

	auto makeFragmentPrefix = [&](StringStream &out, const StringView &str, size_t numWords,
									  size_t allWords) {
		if (numWords == allWords) {
			out << str;
			return;
		} else if (numWords == 0) {
			return;
		}

		StringViewUtf8 r(str);
		while (!r.empty() && numWords > 0) {
			r.backwardSkipChars<SplitTokens>();
			auto tmp = StringViewUtf8(r.backwardReadUntil<SplitTokens>());

			auto tmpR = tmp;
			tmpR.trimChars<TrimToken>();

			if (sprt::unicode::getUtf16Length(tmpR) > cfg.shortWord) {
				--numWords;
			}
		}

		if (!r.empty()) {
			out << cfg.separator << " ";
		}

		out << StringView(r.data() + r.size(), (str.data() + str.size()) - (r.data() + r.size()));
	};

	auto makeFragmentSuffix = [&](StringStream &out, const StringView &str, size_t numWords,
									  size_t allWords) {
		if (numWords == allWords) {
			out << str;
			return;
		} else if (numWords == 0) {
			return;
		}

		StringViewUtf8 r(str);
		while (!r.empty() && numWords > 0) {
			auto sep = StringViewUtf8(r.readChars<SplitTokens>());
			auto tmp = StringViewUtf8(r.readUntil<SplitTokens>());

			auto tmpR = tmp;
			tmpR.trimChars<TrimToken>();

			out << sep << tmp;

			if (sprt::unicode::getUtf16Length(tmpR) > cfg.shortWord) {
				--numWords;
			}
		}

		if (!r.empty()) {
			out << " " << cfg.separator;
		}
	};

	auto makeFragment = [&](StringStream &out, const StringView &str, const StringView &tagId,
								const std::array<WordIndex, 32> &words, const WordIndex *word,
								size_t idx) {
		out << cfg.startFragment;
		auto prefixView = StringView(str.data(), word->word.data() - str.data());
		auto suffixView = StringView(word->end->word.data() + word->end->word.size(),
				str.data() + str.size() - (word->end->word.data() + word->end->word.size()));
		if (idx < cfg.maxWords) {
			// write whole block
			out << prefixView;
			writeFragmentWords(out, str, words, word);
			out << suffixView;
		} else if (word->allWordsCount < cfg.minWords) {
			// make offsets
			size_t availStart = word->index;
			size_t availEnd = idx - word->end->index - 1;
			size_t diff = (cfg.minWords - word->allWordsCount) + 1;

			if (availStart >= diff / 2 && availEnd >= diff / 2) {
				makeFragmentPrefix(out, prefixView, diff / 2, word->index);
				writeFragmentWords(out, str, words, word);
				makeFragmentSuffix(out, suffixView, diff / 2, idx - word->end->index - 1);
			} else if (availStart < diff / 2 && availEnd < diff / 2) {
				out << prefixView;
				writeFragmentWords(out, str, words, word);
				out << suffixView;
			} else if (availStart < diff / 2) {
				out << prefixView;
				writeFragmentWords(out, str, words, word);
				makeFragmentSuffix(out, suffixView, diff - availStart - 1,
						idx - word->end->index - 1);
			} else if (availEnd < diff / 2) {
				makeFragmentPrefix(out, prefixView, diff - availEnd - 1, word->index);
				writeFragmentWords(out, str, words, word);
				out << suffixView;
			}
		} else {
			// try minimal offsets
			makeFragmentPrefix(out, prefixView, 1, word->index);
			writeFragmentWords(out, str, words, word);
			makeFragmentSuffix(out, suffixView, 1, idx - word->end->index - 1);
		}
		out << cfg.stopFragment;

		if (cfg.fragmentCallback) {
			cfg.fragmentCallback(out.weak(), tagId);
		}
	};

	cb([&, this](const StringView &str, const StringView &fragmentTag) -> bool {
		std::array<WordIndex, 32> wordsMatch;
		uint16_t wordCount = 0;
		uint16_t idx = 0;

		bool enabledComplex = false;
		parsePhrase(str, [&, this](StringView word, ParserToken tok) {
			auto status = ParserStatus::Continue;
			if (tok != ParserToken::Blank && sprt::unicode::getUtf16Length(word) > cfg.shortWord
					&& wordCount < 32) {
				if (enabledComplex) {
					if (isWordPart(tok)) {
						wordsMatch[wordCount] = WordIndex{word, idx};
						++wordCount;
						++idx;
						return status;
					} else {
						enabledComplex = false;
					}
				}
				stemWord(word, tok, [&](StringView word, StringView stem, ParserToken tok) {
					auto it = std::lower_bound(stemList.begin(), stemList.end(), stem);
					if (it != stemList.end() && string::detail::caseCompare_u(*it, stem) == 0) {
						if (isComplexWord(tok)) {
							enabledComplex = true;
						} else {
							wordsMatch[wordCount] = WordIndex{word, idx};
							++wordCount;
						}
					}
				});
				++idx;
			}
			return status;
		});

		if (wordCount == 0) {
			return true;
		}

		for (size_t i = 0; i < wordCount; ++i) {
			rateWord(wordsMatch[i], &wordsMatch[i + 1], wordCount - 1 - i);
		}

		if (topIndex && count > 0) {
			if (cfg.fragmentCallback) {
				StringStream out;
				makeFragment(out, str, fragmentTag, wordsMatch, topIndex, idx);
				ret << out.weak();
			} else {
				makeFragment(ret, str, fragmentTag, wordsMatch, topIndex, idx);
				--count;
			}
		}

		if (count == 0) {
			return false;
		}

		topIndex = nullptr;
		return true;
	});
	return ret.str();
}

Vector<String> Configuration::stemQuery(const SearchQuery &query) const {
	Vector<String> queryList;
	doStemQuery(queryList, query);
	return queryList;
}

void Configuration::doStemQuery(Vector<String> &queryList, const SearchQuery &query) const {
	if (!query.value.empty()) {
		emplace_ordered(queryList, query.value);
	}
	for (auto &it : query.args) { doStemQuery(queryList, it); }
}

struct Configuration_ParserControl {
	Vector<SearchQuery *> stack;
	StringView error;
	bool neg = false;
	bool success = true;
	bool strict = false;

	bool popNeg() {
		auto tmp = neg;
		neg = false;
		return tmp;
	}

	void pushNeg() { neg = !neg; }
};

static bool Configuration_parseQueryBlank(Configuration_ParserControl &control, StringView r) {
	auto makeShift = [&](SearchQuery *q, SearchOp op) {
		SearchQuery tmp = move(*q);
		q->clear();
		q->op = op;
		q->args.emplace_back(move(tmp));
	};

	while (!r.empty()) {
		auto q = control.stack.back();
		r.skipUntil<StringView::Chars<'"', '|', '!', '(', ')'>>();
		if (q->block == SearchQuery::Quoted) {
			// ignore any punctuation within quotes
			if (r[0] != '"') {
				++r;
				continue;
			}
		}
		switch (r[0]) {
		case '"':
			if (q->block != SearchQuery::Quoted) {
				if (q->op == SearchOp::None) {
					q->op = SearchOp::Follow;
					q->block = SearchQuery::Quoted;
				} else {
					if (q->op == SearchOp::Or) {
						makeShift(q, SearchOp::And);
					}
					auto &top = q->args.emplace_back();
					top.op = SearchOp::Follow;
					top.block = SearchQuery::Quoted;
					top.neg = control.popNeg();
					control.stack.emplace_back(&top);
				}
			} else {
				control.stack.pop_back();
			}
			break;
		case '|':
			if (q->op == SearchOp::Or) {
				auto &top = q->args.emplace_back();
				control.stack.emplace_back(&top);
			} else if (q->op == SearchOp::And && q->args.size() <= 1) {
				q->op = SearchOp::Or;
				auto &top = q->args.emplace_back();
				control.stack.emplace_back(&top);
			} else if (q->op != SearchOp::None || (q->op == SearchOp::None && !q->value.empty())) {
				makeShift(q, SearchOp::Or);
				auto &top = q->args.emplace_back();
				control.stack.emplace_back(&top);
			}
			break;
		case '!': {
			control.pushNeg();
			break;
		}
		case '(':
			if (q->block == SearchQuery::None) {
				if (q->op == SearchOp::None) {
					q->op = SearchOp::And;
					q->block = SearchQuery::Parentesis;
				} else {
					if (q->op == SearchOp::Or) {
						makeShift(q, SearchOp::And);
					}
					auto &top = q->args.emplace_back();
					top.op = SearchOp::And;
					top.block = SearchQuery::Parentesis;
					top.neg = control.popNeg();
					control.stack.emplace_back(&top);
				}
			} else {
				control.error = StringView("Invalid '(' token within block");
				if (control.strict) {
					return false;
				}
			}
			break;
		case ')':
			if (q->block == SearchQuery::Parentesis) {
				control.stack.pop_back();
			} else {
				control.error = StringView("Invalid ')' outside of parenthesis");
				if (control.strict) {
					return false;
				}
			}
			break;
		default: break;
		}

		++r;
	}
	return true;
}

static bool Configuration_parseQueryWord(Configuration_ParserControl &control, StringView word,
		uint32_t offset = 0, StringView source = StringView()) {
	auto q = control.stack.back();

	if (q->op == SearchOp::None) {
		if (q->value.empty()) {
			q->value = word.str<memory::PoolInterface>();
			q->source = source;
			q->offset = offset;
			q->neg = control.popNeg();
			if (control.stack.size() > 1) {
				control.stack.pop_back();
			}
		} else {
			control.error = StringView("Invalid element");
			if (control.strict) {
				return false;
			}
		}
	} else if (q->op == SearchOp::And || q->op == SearchOp::Follow) {
		auto &v = q->args.emplace_back(SearchQuery(word, offset, source));
		v.neg = control.popNeg();
	} else {
		SearchQuery tmp = move(*q);
		q->clear();
		q->op = SearchOp::And;
		q->args.emplace_back(move(tmp));
		q->args.emplace_back(SearchQuery(word, offset, source));
	}
	return true;
}

SearchQuery Configuration::parseQuery(StringView str, bool strict, StringView *err) const {
	SearchQuery query;
	query.op = SearchOp::And;

	Configuration_ParserControl control;
	control.stack.emplace_back(&query);
	control.strict = strict;

	uint32_t prev = 0;
	uint32_t counter = 0;
	auto ret = search::parsePhrase(str, [&, this](StringView word, ParserToken tok) {
		auto status = isComplexWord(tok) ? ParserStatus::PreventSubdivide : ParserStatus::Continue;
		if (tok == ParserToken::Blank) {
			if (!Configuration_parseQueryBlank(control, word)) {
				return ParserStatus::Stop;
			}
		} else {
			++counter;
			if (data->preStem && !isWordPart(tok)) {
				auto ret = data->preStem(word, tok);
				if (!ret.empty()) {
					uint32_t offset = counter - prev;
					prev = counter;
					for (auto &it : ret) {
						auto str = normalizeWord(it);
						if (!Configuration_parseQueryWord(control, str, offset, word)) {
							return ParserStatus::Stop;
						}
					}
					return isComplexWord(tok) ? ParserStatus::PreventSubdivide
											  : ParserStatus::Continue;
				}
			}
			stemWord(word, tok, [&](StringView w, StringView s, ParserToken tok) {
				if (!s.empty()) {
					if (!Configuration_parseQueryWord(control, s, counter - prev, w)) {
						status = ParserStatus::Stop;
					}
					prev = counter;
				}
			});
		}
		return status;
	});

	if (!ret) {
		if (err) {
			*err = control.error;
		}
		return SearchQuery();
	}

	if (control.stack.back()->block == SearchQuery::Block::Quoted
			&& control.stack.back()->op == SearchOp::Follow && control.stack.back()->args.empty()) {
		auto v = control.stack.back();
		control.stack.pop_back();
		if (!control.stack.empty()) {
			if (&control.stack.back()->args.back() == v) {
				control.stack.back()->args.pop_back();
			}
		}
	}

	query.normalize();
	return query;
}

bool Configuration::isMatch(const SearchVector &vec, StringView q) const {
	auto query = parseQuery(q);
	return query.isMatch(vec);
}

bool Configuration::isMatch(const SearchVector &vec, const SearchQuery &query) const {
	return query.isMatch(vec);
}
} // namespace stappler::search
