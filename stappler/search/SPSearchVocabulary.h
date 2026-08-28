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


#ifndef STAPPLER_SEARCH_SPSEARCHVOCABULARY_H_
#define STAPPLER_SEARCH_SPSEARCHVOCABULARY_H_

#include "SPSearchQuery.h"
#include "SPSearchDistance.h"

namespace STAPPLER_VERSIONIZED stappler::search {

class Configuration;

/* The set of words an index actually contains, and the two questions a typo-tolerant search asks
of it: what starts with this, and what is within k edits of this.

---- why this exists at all, instead of a fuzzy term in the query ------------------------------------

`SearchQuery` has no prefix term and no fuzziness flag: `isMatch` does an exact lookup of the stem,
`encode(Postgresql)` emits only the boolean operators, and `decompose` hands the SQLite backend two
flat lists of words to turn into row ids. Adding a fuzzy leaf means editing all four of those at
once, and every backend that consumes them afterwards.

Expansion answers the same need without touching any of it: the parsed query is rewritten into an
ordinary `Or` of the CONCRETE words the vocabulary knows, and everything downstream keeps working
unchanged - matching on a `SearchVector`, ranking on an encoded blob, the SQLite word-id prefilter,
and the PostgreSQL `tsquery` text. What the user typed is not what the index stores; making that
translation explicit is the whole job.

---- the cost, stated -------------------------------------------------------------------------------

Expansion is linear in the vocabulary per query word: a length prefilter, then an edit distance with
a cutoff for what survives it. That is right for the tens of thousands of words a palette or a
project index holds, and wrong for millions. If it ever has to be, `near` is the only thing that
changes; nothing outside this class knows how the candidates were found. */
class SP_PUBLIC Vocabulary : public Ref {
public:
	// How many edits to allow for a word of this length.
	//
	// One rule in one place, because the alternative is every call site inventing its own and the
	// same query behaving differently in two pickers. Short words get no tolerance at all: at three
	// characters a single edit reaches most of the alphabet, and "fuzzy" stops meaning anything.
	static uint32_t distanceForQuery(StringView);

	virtual ~Vocabulary();

	virtual bool init();

	// Words are copied into this object's own storage, so the caller may add views into anything.
	void add(StringView word);
	void addAll(SpanView<StringView>);

	// Every stem the configuration produces for this phrase. This is the symmetric half of
	// `Configuration::makeSearchVector`: index and vocabulary have to agree about what a word IS,
	// or expansion produces terms the index never stored.
	void addPhrase(const Configuration &, StringView phrase);

	// Sorts and deduplicates. Queries before this return nothing rather than a wrong answer.
	void build();

	bool isBuilt() const { return _built; }
	size_t size() const { return _words.size(); }
	void clear();

	// Words starting with the given prefix, in ascending code point order.
	void prefix(StringView, const Callback<void(StringView)> &) const;

	// Words within `k` edits, with the distance that was measured. `k == 0` degenerates to an
	// exact lookup and does no distance work at all.
	void near(StringView, uint32_t k, const Callback<void(StringView word, uint32_t distance)> &) const;

	// Rewrites every leaf of the query into Or(leaf, near-words...), preserving negation, blocks,
	// offsets and the original source text of the leaf.
	//
	// A leaf the vocabulary cannot improve on is left exactly as it was - including a leaf no word
	// matches, which must stay in the query so that "no results" remains the answer instead of
	// quietly becoming "everything".
	SearchQuery expand(const SearchQuery &, uint32_t k) const;

	// The same, choosing k per leaf with `distanceForQuery`.
	SearchQuery expand(const SearchQuery &) const;

protected:
	void doExpand(const SearchQuery &src, SearchQuery &dst, uint32_t k, bool perWord) const;

	bool _built = false;

	// The words are copied here and the views point into it, so a vocabulary outlives whatever it
	// was built from. Its own pool rather than the ambient one: this object is refcounted and its
	// lifetime has nothing to do with the pool that happened to be current when it was created.
	memory::pool_t *_pool = nullptr;

	// mem_std, not the pool Vector the rest of this module uses: the vector itself has to survive
	// independently of any pool frame, while only the characters it points at live in _pool.
	mem_std::Vector<StringView> _words;
};

} // namespace stappler::search

#endif /* STAPPLER_SEARCH_SPSEARCHVOCABULARY_H_ */
