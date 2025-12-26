/**
 Copyright (c) 2024 Stappler LLC <admin@stappler.dev>
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

#ifndef CORE_SEARCH_SPSEARCHQUERY_H_
#define CORE_SEARCH_SPSEARCHQUERY_H_

#include "SPSearchParser.h"

namespace STAPPLER_VERSIONIZED stappler::search {

enum class Normalization : uint32_t {
	Default = 0,
	DocLengthLog = 1, // divides the rank by 1 + the logarithm of the document length
	DocLength = 2, // divides the rank by the document length
	UniqueWordsCount = 8, // divides the rank by the number of unique words in document
	UniqueWordsCountLog =
			16, // divides the rank by 1 + the logarithm of the number of unique words in document
	Self = 32 // divides the rank by itself + 1
};

SP_DEFINE_ENUM_AS_MASK(Normalization)

struct SP_PUBLIC RankingValues {
	float A = 1.0f;
	float B = 0.4f;
	float C = 0.2f;
	float D = 0.1f;

	// Linearly interpolated from first to last word in document
	// (so, last word score will be RANK * positionFactor
	// word in a middle: (RANK * (1.0 + positionFactor) / 2.0)
	// 1.0 - do not apply position-based score
	//
	// Not implemented in PostgreSQL engine, has no effect
	float positionFactor = 1.0f;

	float rank(SearchRank r) const {
		switch (r) {
		case SearchRank::A: return A; break;
		case SearchRank::B: return B; break;
		case SearchRank::C: return C; break;
		case SearchRank::D: return D; break;
		case SearchRank::Unknown: return D; break;
		}
		return D;
	}
};

struct SP_PUBLIC SearchVector {
	using MatchVector = Vector<Pair<size_t, SearchData::Rank>>;

	size_t documentLength = 0;
	Map<StringView, MatchVector> words;

	bool empty() const { return words.empty(); }
};

struct SP_PUBLIC SearchQuery {
	enum Block : uint8_t {
		None,
		Parentesis,
		Quoted,
	};

	enum Format {
		Stappler,
		Postgresql,
	};

	Block block = None;
	SearchOp op = SearchOp::None;
	bool neg = false;
	uint32_t offset = 0;
	String value;
	StringView source;
	Vector<SearchQuery> args;

	SearchQuery() = default;
	SearchQuery(StringView value, uint32_t offset = 1, StringView source = StringView());
	SearchQuery(SearchOp, StringView);

	bool empty() const {
		return (op == SearchOp::None && value.empty()) || (op != SearchOp::None && args.empty());
	}

	void clear();
	void encode(const Callback<void(StringView)> &, Format = Stappler) const;

	void describe(std::ostream &stream, size_t depth = 0) const;
	void foreach (const Callback<void(StringView value, StringView source)> &) const;

	bool isMatch(const SearchVector &) const;

	// used with opaque index format from `Configuration::encodeSearchVectorData`
	bool isMatch(const BytesView &) const;

	float rankQuery(const SearchVector &, Normalization = Normalization::Default,
			RankingValues = RankingValues()) const;

	// used with opaque index format from `Configuration::encodeSearchVectorData`
	float rankQuery(const BytesView &, Normalization = Normalization::Default,
			RankingValues = RankingValues()) const;

	void normalize();

	void decompose(const Callback<void(StringView)> &positive,
			const Callback<void(StringView)> &negative) const;
};

} // namespace stappler::search

#endif /* CORE_SEARCH_SPSEARCHQUERY_H_ */
