/**
 Copyright (c) 2024 Stappler LLC <admin@stappler.dev>

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

#include "SPSearchQuery.h"

namespace STAPPLER_VERSIONIZED stappler::search {

SearchQuery::SearchQuery(StringView w, uint32_t off, StringView source)
: offset(off), value(w.str<memory::PoolInterface>()), source(source) { }

SearchQuery::SearchQuery(SearchOp op, StringView w)
: op(op), value(w.str<memory::PoolInterface>()) { }

void SearchQuery::clear() {
	block = None;
	offset = 0;
	op = SearchOp::None;
	value.clear();
	args.clear();
}

static void SearchQuery_encode_Stappler(const Callback<void(StringView)> &cb,
		const SearchQuery *t) {
	if (t->args.empty()) {
		if (!t->value.empty()) {
			switch (t->block) {
			case SearchQuery::None: break;
			case SearchQuery::Parentesis: cb << "("; break;
			case SearchQuery::Quoted: cb << "\""; break;
			}

			if (t->neg) {
				cb << "!";
			}
			cb << t->value;

			switch (t->block) {
			case SearchQuery::None: break;
			case SearchQuery::Parentesis: cb << ")"; break;
			case SearchQuery::Quoted: cb << "\""; break;
			}
		}
	} else {
		if (!t->args.empty()) {
			switch (t->block) {
			case SearchQuery::None: break;
			case SearchQuery::Parentesis: cb << "("; break;
			case SearchQuery::Quoted: cb << "\""; break;
			}

			auto it = t->args.begin();
			if (t->neg) {
				cb << "!";
			}
			SearchQuery_encode_Stappler(cb, &(*it));
			++it;

			for (; it != t->args.end(); ++it) {
				cb << " ";
				switch (t->op) {
				case SearchOp::None:
				case SearchOp::And: break;
				case SearchOp::Or: cb << " | "; break;
				case SearchOp::Follow:
					if (t->offset > 1 && t->offset <= 5) {
						for (size_t i = 1; i < t->offset; ++i) { cb << "a "; }
					}
					break;
				}
				SearchQuery_encode_Stappler(cb, &(*it));
			}

			switch (t->block) {
			case SearchQuery::None: break;
			case SearchQuery::Parentesis: cb << ")"; break;
			case SearchQuery::Quoted: cb << "\""; break;
			}
		}
	}
}

static void SearchQuery_encode_Postgresql(const Callback<void(StringView)> &cb,
		const SearchQuery *t) {
	if (t->args.empty()) {
		if (!t->value.empty()) {
			switch (t->block) {
			case SearchQuery::None: break;
			case SearchQuery::Parentesis: cb << "("; break;
			case SearchQuery::Quoted: cb << "("; break;
			}

			if (t->neg) {
				cb << "!";
			}
			cb << t->value;
		}
	} else {
		if (!t->args.empty()) {
			if (t->neg) {
				cb << "!";
			}

			switch (t->block) {
			case SearchQuery::None: break;
			case SearchQuery::Parentesis: cb << "("; break;
			case SearchQuery::Quoted: cb << "("; break;
			}

			auto it = t->args.begin();
			SearchQuery_encode_Postgresql(cb, &(*it));
			++it;

			for (; it != t->args.end(); ++it) {
				switch (t->op) {
				case SearchOp::None: cb << " "; break;
				case SearchOp::And: cb << " & "; break;
				case SearchOp::Or: cb << " | "; break;
				case SearchOp::Follow:
					if (it->offset > 1 && it->offset <= 5) {
						cb << " <" << uint64_t(it->offset) << "> ";
					} else {
						cb << " <-> ";
					}
					break;
				}
				SearchQuery_encode_Postgresql(cb, &(*it));
			}

			switch (t->block) {
			case SearchQuery::None: break;
			case SearchQuery::Parentesis: cb << ")"; break;
			case SearchQuery::Quoted: cb << ")"; break;
			}
		}
	}
}

void SearchQuery::encode(const Callback<void(StringView)> &cb, Format fmt) const {
	switch (fmt) {
	case Stappler: SearchQuery_encode_Stappler(cb, this); break;
	case Postgresql: SearchQuery_encode_Postgresql(cb, this); break;
	}
}

static void SearchQuery_print(std::ostream &stream, const SearchQuery *t, uint16_t depth) {
	if (t->args.empty()) {
		for (size_t i = 0; i < depth; ++i) { stream << "  "; }

		switch (t->block) {
		case SearchQuery::None: break;
		case SearchQuery::Parentesis: stream << "(parentesis) "; break;
		case SearchQuery::Quoted: stream << "(quoted) "; break;
		}

		if (t->neg) {
			stream << "(not) ";
		}

		if (t->offset > 1) {
			stream << "<" << t->offset << "> ";
		}

		if (!t->value.empty()) {
			stream << "'" << t->value << "'";
		}
		stream << "\n";

	} else {
		for (size_t i = 0; i < depth; ++i) { stream << "  "; }
		stream << "-> ";

		if (t->neg) {
			stream << "(not) ";
		}

		switch (t->block) {
		case SearchQuery::None: break;
		case SearchQuery::Parentesis: stream << "(parentesis)"; break;
		case SearchQuery::Quoted: stream << "(quoted)"; break;
		}

		switch (t->op) {
		case SearchOp::None: stream << " (none)"; break;
		case SearchOp::And: stream << " (and)"; break;
		case SearchOp::Or: stream << " (or)"; break;
		case SearchOp::Follow: stream << " (follow)"; break;
		}
		stream << "\n";

		for (auto &it : t->args) { SearchQuery_print(stream, &it, depth + 1); }
	}
}

void SearchQuery::describe(std::ostream &stream, size_t depth) const {
	SearchQuery_print(stream, this, depth);
}

static void SearchQuery_foreach(const SearchQuery *t,
		const Callback<void(StringView value, StringView source)> &cb) {
	if (t->args.empty()) {
		if (!t->value.empty()) {
			cb(t->value, t->source);
		}
	} else {
		for (auto &it : t->args) { SearchQuery_foreach(&it, cb); }
	}
}

void SearchQuery::foreach (const Callback<void(StringView value, StringView source)> &cb) const {
	SearchQuery_foreach(this, cb);
}

template <typename SearchVectorType>
static auto SearchQuery_isMatch(const SearchVectorType &vec, StringView stem) -> const
		typename SearchVectorType::mapped_type * {
	auto it = vec.find(stem);
	if (it != vec.end()) {
		return &it->second;
	}
	return nullptr;
}

static void SearchQuery_foreachValueMatches(
		Vector< sprt::pair<SearchData::Rank, Vector<size_t>> > &path,
		const Vector<sprt::pair<size_t, SearchData::Rank>> &matches) {
	for (auto &it : matches) {
		auto &obj = path.emplace_back(
				sprt::pair<SearchData::Rank, Vector<size_t>>(it.second, Vector<size_t>()));
		obj.second.emplace_back(it.first);
	}
}

static SpanView<sprt::pair<size_t, SearchData::Rank>> SearchQuery_matchesToArray(
		const Vector<sprt::pair<size_t, SearchData::Rank>> &matches) {
	return matches;
}

static void SearchQuery_foreachValueMatches(
		Vector< sprt::pair<SearchData::Rank, Vector<size_t>> > &path, const Value &matches) {
	if (!matches.isArray()) {
		return;
	}

	auto it = matches.asArray().begin();
	auto end = matches.asArray().end();

	while (it != end) {
		auto pos = (it++)->getInteger();
		auto rank = (it++)->getInteger();

		auto &obj = path.emplace_back(
				sprt::pair<SearchData::Rank, Vector<size_t>>(SearchRank(rank), Vector<size_t>()));
		obj.second.emplace_back(pos);
	}
}

static SpanView<sprt::pair<Value, Value>> SearchQuery_matchesToArray(const Value &matches) {
	if (matches.isArray()) {
		auto &arr = matches.asArray();
		return SpanView<sprt::pair<Value, Value>>((const sprt::pair<Value, Value> *)arr.data(),
				arr.size() / 2);
	}
	return SpanView<sprt::pair<Value, Value>>();
}

static int64_t SearchQuery_toInt(const Value &val) { return val.getInteger(); }

static int64_t SearchQuery_toInt(const SearchData::Rank &val) { return toInt(val); }

static int64_t SearchQuery_toInt(const size_t &val) { return val; }

template <typename SearchVectorTypeValue>
static bool SearchQuery_isFollow(Vector< sprt::pair<SearchData::Rank, Vector<size_t>> > &path,
		const SearchVectorTypeValue *v2, size_t offset) {
	if (offset < 1) {
		offset = 1;
	}

	if (path.empty()) {
		// add all matches to follow path list
		SearchQuery_foreachValueMatches(path, *v2);
	} else {
		auto arr = SearchQuery_matchesToArray(*v2);

		// for every known path, check, if next word within range
		// if no next word found within range for path - remove path
		auto it = path.begin();
		while (it != path.end()) {
			auto &targetPosition = it->second.back();

			// find closest position of next word
			auto nextIt =
					std::lower_bound(arr.begin(), arr.end(), sprt::pair(targetPosition, it->first),
							[&](const auto &l, const sprt::pair<size_t, SearchData::Rank> &r) {
				if (SearchQuery_toInt(l.first) != SearchQuery_toInt(r.first)) {
					return SearchQuery_toInt(l.first) < SearchQuery_toInt(r.first);
				} else {
					return SearchQuery_toInt(l.second) < SearchQuery_toInt(r.second);
				}
			});
			if (nextIt != arr.end()) {
				// skip words with other ranks - follow line should have same rank
				// also, skip word decompositions (holds same positions in search vector)
				while (nextIt != arr.end()
						&& (SearchQuery_toInt(nextIt->first) == int64_t(targetPosition)
								|| SearchQuery_toInt(nextIt->second)
										!= SearchQuery_toInt(it->first))) {
					++nextIt;
				}

				if (nextIt != arr.end()
						&& (SearchQuery_toInt(nextIt->first) - targetPosition <= offset
								&& SearchQuery_toInt(nextIt->second)
										== SearchQuery_toInt(it->first))) {
#if 0
					// handle repeats - next word offset should be calculated from last word in repeat line
					auto possibleRepeat = nextIt;
					++ possibleRepeat;
					while (possibleRepeat->first == nextIt->first + 1) {
						nextIt = possibleRepeat;
						++ possibleRepeat;
					}
#endif
					it->second.emplace_back(SearchQuery_toInt(nextIt->first));
					++it;
					continue;
				}
			}
			it = path.erase(it);
		}
	}

	return !path.empty();
}

template <typename SearchVectorType>
static bool SearchQuery_isMatch(const SearchVectorType &vec, const SearchQuery &q) {
	if (!q.args.empty()) {
		switch (q.op) {
		case SearchOp::None:
		case SearchOp::And:
			for (auto &it : q.args) {
				if (!SearchQuery_isMatch(vec, it)) {
					return q.neg;
					break;
				}
			}
			return !q.neg;
			break;
		case SearchOp::Or:
			for (auto &it : q.args) {
				if (SearchQuery_isMatch(vec, it)) {
					return !q.neg;
					break;
				}
			}
			return q.neg;
			break;
		case SearchOp::Follow:
			Vector< sprt::pair<SearchData::Rank, Vector<size_t>> > path;
			for (auto &it : q.args) {
				auto tmp = SearchQuery_isMatch(vec, it.value);
				if (!tmp) {
					return q.neg;
				}

				if (!SearchQuery_isFollow(path, tmp, it.offset)) {
					return q.neg;
				}
			}
			return !q.neg;
			break;
		}
	} else if (!q.value.empty()) {
		auto v = SearchQuery_isMatch(vec, q.value);
		if (q.neg) {
			return v == nullptr;
		} else {
			return v != nullptr;
		}
	}
	return false;
}

bool SearchQuery::isMatch(const SearchVector &vec) const {
	return SearchQuery_isMatch(vec.words, *this);
}

bool SearchQuery::isMatch(const BytesView &blob) const {
	auto p = pool::create(pool::acquire());

	bool result = false;
	perform([&, this] {
		auto d = data::read<Interface>(blob);
		if (d.isArray() && d.size() == 3 && d.getInteger(0) == 1) {
			auto &dict = d.getDict(2);
			result = SearchQuery_isMatch(dict, *this);
		}
	}, p);

	pool::destroy(p);
	return result;
}

static SpanView<sprt::pair<size_t, SearchData::Rank>> SearchQuery_getWordInfo(
		const SearchVector &vec, StringView word) {
	auto it = vec.words.find(word);
	if (it != vec.words.end()) {
		return it->second;
	}
	return SpanView<sprt::pair<size_t, SearchData::Rank>>();
}

static SpanView<sprt::pair<Value, Value>> SearchQuery_getWordInfo(const Value::DictionaryType &vec,
		StringView word) {
	auto it = vec.find(word);
	if (it != vec.end()) {
		return SearchQuery_matchesToArray(it->second);
	}
	return SpanView<sprt::pair<Value, Value>>();
}

template <typename SearchVectorType>
static float SearchQuery_rankWord(const SearchVectorType &vec, StringView word, size_t docLength,
		const RankingValues &vals) {
	float accum = 0.0f;
	auto w = SearchQuery_getWordInfo(vec, word);

	for (auto &it : w) {
		auto wordPos = float(SearchQuery_toInt(it.first)) / float(docLength);

		accum += vals.rank(SearchRank(SearchQuery_toInt(it.second)))
				* math::lerp(1.0f, vals.positionFactor, wordPos);
	}
	return accum;
}

template <typename SearchVectorType>
static float SearchQuery_rankQuery(const SearchQuery &query, const SearchVectorType &vec,
		Normalization norm, const RankingValues &vals, size_t docLength, size_t wordsCount) {
	float accum = 0.0f;

	query.foreach ([&](StringView word, StringView source) {
		accum += SearchQuery_rankWord(vec, word, docLength, vals);
	});

	if ((norm & Normalization::DocLengthLog) != Normalization::Default) {
		accum /= 1.0f + std::log(float(docLength));
	}

	if ((norm & Normalization::DocLength) != Normalization::Default) {
		accum /= (float(docLength));
	}

	if ((norm & Normalization::UniqueWordsCount) != Normalization::Default) {
		accum /= (float(wordsCount));
	}

	if ((norm & Normalization::UniqueWordsCountLog) != Normalization::Default) {
		accum /= 1.0f + std::log(float(wordsCount));
	}

	if ((norm & Normalization::Self) != Normalization::Default) {
		accum /= accum + 1.0f;
	}

	return accum;
}

float SearchQuery::rankQuery(const SearchVector &vec, Normalization norm,
		RankingValues vals) const {
	return SearchQuery_rankQuery(*this, vec, norm, vals, vec.documentLength, vec.words.size());
}

float SearchQuery::rankQuery(const BytesView &blob, Normalization norm, RankingValues vals) const {
	auto p = pool::create(pool::acquire());

	float result = 0.0f;
	perform([&, this] {
		auto d = data::read<Interface>(blob);
		if (d.isArray() && d.size() == 3 && d.getInteger(0) == 1) {
			auto docLength = d.getInteger(1);
			auto &dict = d.getDict(2);
			auto wordsCount = dict.size();
			result = SearchQuery_rankQuery(*this, dict, norm, vals, docLength, wordsCount);
		}
	}, p);

	pool::destroy(p);
	return result;
}

void SearchQuery::normalize() {
	if (!args.empty() && neg) {
		switch (op) {
		case SearchOp::And:
			neg = false;
			op = SearchOp::Or;
			for (auto &it : args) { it.neg = !it.neg; }
			break;
		case SearchOp::Or:
			neg = false;
			op = SearchOp::And;
			for (auto &it : args) { it.neg = !it.neg; }
			break;
		default: break;
		}
	}
}

static void SearchQuery_decomposeDnf(const SearchQuery &q,
		const Callback<void(StringView)> &positive, const Callback<void(StringView)> &negative) {
	if (!q.value.empty()) {
		positive(q.value);
	} else {
		for (auto &it : q.args) { SearchQuery_decomposeDnf(it, positive, negative); }
	}
}

static void SearchQuery_decomposeCnf(const SearchQuery &q,
		const Callback<void(StringView)> &positive, const Callback<void(StringView)> &negative) {
	if (!q.value.empty()) {
		if (q.neg) {
			negative(q.value);
		} else {
			positive(q.value);
		}
	} else {
		for (auto &it : q.args) {
			switch (q.op) {
			case SearchOp::And:
			case SearchOp::Follow:
				if (!it.neg) {
					SearchQuery_decomposeDnf(it, positive, negative);
				} else {
					SearchQuery_decomposeCnf(it, positive, negative);
				}
				break;
			case SearchOp::Or:
				if (!it.value.empty()) {
					if (!it.neg) {
						positive(it.value);
					}
				} else {
					SearchQuery_decomposeDnf(it, positive, negative);
				}
				break;
			case SearchOp::None: break;
			}
		}
	}
}

void SearchQuery::decompose(const Callback<void(StringView)> &positive,
		const Callback<void(StringView)> &negative) const {
	if (!args.empty()) {
		for (auto &it : args) {
			switch (op) {
			case SearchOp::And:
			case SearchOp::Follow:
				if (!it.value.empty()) {
					if (it.neg) {
						negative(it.value);
					} else {
						positive(it.value);
					}
				} else if (it.neg) {
					SearchQuery_decomposeDnf(it, positive, negative);
				} else {
					SearchQuery_decomposeCnf(it, positive, negative);
				}
				break;
			case SearchOp::Or:
				if (!it.value.empty()) {
					if (!it.neg) {
						positive(it.value);
					}
				} else {
					SearchQuery_decomposeDnf(it, positive, negative);
				}
				break;
			case SearchOp::None: break;
			}
		}
	} else {
		if (neg) {
			negative(value);
		} else {
			positive(value);
		}
	}
}

} // namespace stappler::search
