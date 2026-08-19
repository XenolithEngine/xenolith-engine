/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

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


#include "SPSearchVocabulary.h"
#include "SPSearchConfiguration.h"

namespace STAPPLER_VERSIONIZED stappler::search {

static size_t Vocabulary_codepointLength(StringView str) {
	StringViewUtf8 r(str);
	size_t count = 0;
	while (!r.empty()) {
		r.readChar();
		++count;
	}
	return count;
}

static int Vocabulary_compare(StringView l, StringView r) { return sprt::detail::compare_c(l, r); }

static bool Vocabulary_startsWith(StringView str, StringView prefix) {
	return str.size() >= prefix.size()
			&& sprt::__constexpr_strcompare(str.data(), prefix.data(), prefix.size()) == 0;
}

uint32_t Vocabulary::distanceForQuery(StringView word) {
	auto len = Vocabulary_codepointLength(word);
	if (len <= 3) {
		// One edit on three characters reaches a large part of the alphabet: at this length
		// "fuzzy" stops separating anything and the result is noise, not tolerance.
		return 0;
	} else if (len <= 7) {
		return 1;
	}
	return 2;
}

Vocabulary::~Vocabulary() {
	_words.clear();
	if (_pool) {
		pool::destroy(_pool);
		_pool = nullptr;
	}
}

bool Vocabulary::init() {
	/* A pool with its OWN allocator, not one inherited from whatever was current here.

	`pool::create(pool::acquire())` is the idiom next door in Configuration, and it carries a trap
	for anything refcounted: the pool it inherits from is whatever frame the object happened to be
	constructed in, and a child pool cannot outlive its parent. A vocabulary is held by an Rc and
	will be read long after the frame that built it is gone, so it owns its memory outright. */
	_pool = pool::create();
	return _pool != nullptr;
}

void Vocabulary::add(StringView word) {
	if (word.empty()) {
		return;
	}
	_words.emplace_back(word.pdup(_pool));
	_built = false;
}

void Vocabulary::addAll(SpanView<StringView> words) {
	for (auto &it : words) { add(it); }
}

void Vocabulary::addPhrase(const Configuration &cfg, StringView phrase) {
	// The stem, not the word: the index stores stems, so a vocabulary of words would expand a
	// query into terms the index never contained and quietly return nothing.
	cfg.stemPhrase(phrase, [&](StringView, StringView stem, ParserToken) { add(stem); });
}

void Vocabulary::build() {
	sprt::sort(_words.begin(), _words.end(),
			[](StringView l, StringView r) { return Vocabulary_compare(l, r) < 0; });
	_words.erase(sprt::unique(_words.begin(), _words.end(),
						 [](StringView l, StringView r) { return Vocabulary_compare(l, r) == 0; }),
			_words.end());
	_built = true;
}

void Vocabulary::clear() {
	_words.clear();
	_built = false;
}

void Vocabulary::prefix(StringView value, const Callback<void(StringView)> &cb) const {
	if (!_built || value.empty() || !cb) {
		return;
	}

	auto it = sprt::lower_bound(_words.begin(), _words.end(), value,
			[](StringView l, StringView r) { return Vocabulary_compare(l, r) < 0; });

	while (it != _words.end() && Vocabulary_startsWith(*it, value)) {
		cb(*it);
		++it;
	}
}

void Vocabulary::near(StringView value, uint32_t k,
		const Callback<void(StringView, uint32_t)> &cb) const {
	if (!_built || value.empty() || !cb) {
		return;
	}

	if (k == 0) {
		auto it = sprt::lower_bound(_words.begin(), _words.end(), value,
				[](StringView l, StringView r) { return Vocabulary_compare(l, r) < 0; });
		if (it != _words.end() && Vocabulary_compare(*it, value) == 0) {
			cb(*it, 0);
		}
		return;
	}

	const auto length = Vocabulary_codepointLength(value);

	// Distance allocates its alignment in the ambient pool, and this runs it once per surviving
	// candidate. Collected first and reported after, so the scan's garbage is released in one
	// place instead of being handed to a callback that has no idea it is standing in a temporary.
	mem_std::Vector<sprt::pair<StringView, uint32_t>> found;

	memory::perform_temporary([&] {
		for (auto &it : _words) {
			auto candidateLength = Vocabulary_codepointLength(it);
			auto diff = (candidateLength > length) ? (candidateLength - length)
												   : (length - candidateLength);
			if (diff > k) {
				// A length difference of more than k needs at least that many inserts or deletes:
				// no alignment can bring it back under the cutoff, so do not pay for one.
				continue;
			}

			Distance d(value, it, k);
			auto dist = d.distance();
			if (dist <= k) {
				found.emplace_back(it, dist);
			}
		}
	});

	for (auto &it : found) { cb(it.first, it.second); }
}

void Vocabulary::doExpand(const SearchQuery &src, SearchQuery &dst, uint32_t k,
		bool perWord) const {
	dst.block = src.block;
	dst.op = src.op;
	dst.neg = src.neg;
	dst.offset = src.offset;
	dst.source = src.source;

	if (!src.args.empty()) {
		if (src.op == SearchOp::Follow) {
			/* A phrase is copied as it stands, without expanding its words.

			This is not caution, it is the shape of the matcher: SearchQuery_isMatch reads a Follow
			node's arguments as `it.value` directly (SPSearchQuery.cc), so every argument of a
			phrase HAS to be a leaf. Replacing one with an Or node would make the phrase look up
			the empty string and match nothing - a quoted query that silently stops working is
			worse than a quoted query that stays exact. */
			dst.value = src.value;
			dst.args = src.args;
			return;
		}

		dst.args.reserve(src.args.size());
		for (auto &it : src.args) {
			dst.args.emplace_back();
			doExpand(it, dst.args.back(), k, perWord);
		}
		return;
	}

	if (src.value.empty()) {
		dst.value = src.value;
		return;
	}

	auto distance = perWord ? distanceForQuery(src.value) : k;
	if (distance == 0) {
		dst.value = src.value;
		return;
	}

	Vector<StringView> variants;
	near(src.value, distance, [&](StringView word, uint32_t) {
		if (word != StringView(src.value)) {
			variants.emplace_back(word);
		}
	});

	if (variants.empty()) {
		/* Nothing to add - including the case where the vocabulary knows no word like this at all.
		The leaf stays exactly as it was: a term nothing matches has to keep failing, or "no
		results" quietly turns into "everything". */
		dst.value = src.value;
		return;
	}

	// The leaf becomes Or(what was typed, what it might have been). The negation, block and offset
	// stay on the Or node, where they applied to the leaf before.
	dst.op = SearchOp::Or;
	dst.value.clear();
	dst.args.reserve(variants.size() + 1);

	dst.args.emplace_back(SearchQuery(StringView(src.value), src.offset, src.source));
	for (auto &it : variants) { dst.args.emplace_back(SearchQuery(it, src.offset, src.source)); }
}

SearchQuery Vocabulary::expand(const SearchQuery &query, uint32_t k) const {
	SearchQuery ret;
	doExpand(query, ret, k, false);
	return ret;
}

SearchQuery Vocabulary::expand(const SearchQuery &query) const {
	SearchQuery ret;
	doExpand(query, ret, 0, true);
	return ret;
}

} // namespace stappler::search
