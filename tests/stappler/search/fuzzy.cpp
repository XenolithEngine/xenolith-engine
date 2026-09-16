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


// stappler_search, the approximate-matching half: the subsequence matcher, the code-point-to-UTF-16
// conversion a highlight needs, the edit distance used as a threshold, and the vocabulary that turns
// a typo into an ordinary Or of words the index actually contains.
//
// The matcher section is a port of the golden assertions that guarded this code at its previous
// address (studio_edit's palette). It is repeated verbatim on purpose: the scores are a promise to
// every caller that the same query highlights the same characters, and a move is exactly the moment
// that promise is easiest to break by accident.

#include "SPCommon.h"
#include "SPMemory.h"
#include "SPSearchFuzzy.h"
#include "SPSearchVocabulary.h"
#include "SPSearchConfiguration.h"
#include "SPSearchIndex.h"

#include "../tests.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;
using stappler::test::checkEq;

namespace {

int32_t scoreOf(StringView query, StringView target) {
	search::FuzzyMatch m;
	search::fuzzyMatch(query, target, m);
	return m.matched ? m.score : -1;
}

mem_std::String indicesOf(StringView query, StringView target) {
	search::FuzzyMatch m;
	search::fuzzyMatch(query, target, m);
	if (!m.matched) {
		return mem_std::String("-");
	}
	mem_std::String out;
	for (auto i : m.indices) {
		if (!out.empty()) {
			out.append(",");
		}
		out.append(mem_std::toString(i));
	}
	return out;
}

// The ranges as a label would receive them: UTF-16 offsets and lengths.
mem_std::String rangesOf(StringView query, StringView target) {
	search::FuzzyMatch m;
	search::fuzzyMatch(query, target, m);
	if (!m.matched) {
		return mem_std::String("-");
	}
	mem_std::String out;
	search::makeHighlightRanges(target, m.indices, [&](size_t start, size_t length) {
		if (!out.empty()) {
			out.append(" ");
		}
		out.append(mem_std::toString(start, "+", length));
	});
	return out;
}

} // namespace

void performSearchFuzzyTests() {
	sprt::cout << "\n== stappler search fuzzy tests ==\n";

	// ---- the matcher: what it matches ---------------------------------------------------------

	{
		search::FuzzyMatch m;
		search::fuzzyMatch(StringView(), StringView("Transform"), m);
		check(m.matched && m.score == 0 && m.indices.empty(),
				"search-fuzzy: an empty query matches anything, neutrally");
	}

	checkEq(indicesOf(StringView("tra"), StringView("Transform")), "0,1,2",
			"search-fuzzy: a contiguous prefix matches at the front");
	checkEq(indicesOf(StringView("tf"), StringView("Transform")), "0,5",
			"search-fuzzy: and a scattered subsequence matches where its characters are");

	check(scoreOf(StringView("TRANS"), StringView("transform")) > 0
					&& scoreOf(StringView("trans"), StringView("TRANSFORM")) > 0,
			"search-fuzzy: matching ignores case in both directions");

	check(scoreOf(StringView("xyz"), StringView("Transform")) < 0,
			"search-fuzzy: characters that are not a subsequence do not match");
	check(scoreOf(StringView("mr"), StringView("Transform")) < 0,
			"search-fuzzy: ... and order counts - the r of Transform comes before its m");

	check(scoreOf(StringView("tra"), StringView("Transform"))
					> scoreOf(StringView("tro"), StringView("Transform")),
			"search-fuzzy: a prefix scores above a scattered match");
	check(scoreOf(StringView("sca"), StringView("Scatter"))
					> scoreOf(StringView("sct"), StringView("Scatter")),
			"search-fuzzy: a contiguous run scores above a gappy one of the same length");

	checkEq(indicesOf(StringView("gn"), StringView("getName")), "0,3",
			"search-fuzzy: a query hits the word starts of a camelCase name");
	check(scoreOf(StringView("gn"), StringView("getName"))
					> scoreOf(StringView("gn"), StringView("segments")),
			"search-fuzzy: ... and beats the same characters inside a word");

	check(search::isWordStart(StringView("getName"), 3),
			"search-fuzzy: the N of getName begins a word");
	check(!search::isWordStart(StringView("segments"), 4),
			"search-fuzzy: ... and the n of segments does not");
	check(search::isWordStart(StringView("scene.findByField"), 6)
					&& search::isWordStart(StringView("scene.findByField"), 10),
			"search-fuzzy: a dot and a camelCase step are both word starts");

	// ---- the score is a number, and can be written down ---------------------------------------

	// t: index 0, first character of the query at the start of the target, a word start.
	//    10 base + 50 word + 80 first + 40 decay = 180
	check(scoreOf(StringView("t"), StringView("Transform")) == 180,
			"search-fuzzy: one character at the very start scores base + word + first + decay");

	// "tra": 180, then r at 1 (10 + 60 consecutive + 39 decay = 109), then a at 2
	//        (10 + 60 + 38 = 108) -> 397
	check(scoreOf(StringView("tra"), StringView("Transform")) == 397,
			"search-fuzzy: ... and a run adds the consecutive bonus with the decay");

	// "tf": 180, then f at 5, neither consecutive nor a word start (10 + 35 = 45) -> 225
	check(scoreOf(StringView("tf"), StringView("Transform")) == 225,
			"search-fuzzy: a gap loses the consecutive bonus and some of the decay");

	check(scoreOf(StringView("n"), StringView("getName")) == 10 + 50 + 37,
			"search-fuzzy: a word start away from the front scores base + word + decay, exactly");

	// Index 13 is the first position where the reference's float decay, 10*max(0, 4 - 0.1*i), drops
	// BELOW the exact 40 - i. This is the check that actually holds the integer scaling in place.
	check(scoreOf(StringView("x"), StringView("aaaaaaaaaaaaax")) == 10 + 27,
			"search-fuzzy: the decay is exact at index 13, where a float one would already be a "
			"point short");

	// ---- text that is not ASCII ----------------------------------------------------------------

	{
		constexpr StringView Name("узел.взятьЦвет");

		checkEq(indicesOf(StringView("узел"), Name), "0,1,2,3",
				"search-fuzzy: a Cyrillic query matches a Cyrillic name by CHARACTER, not by byte");
		checkEq(indicesOf(StringView("вЦ"), Name), "5,10",
				"search-fuzzy: ... and the indices are character positions in the original");

		check(search::isWordStart(Name, 5),
				"search-fuzzy: a Cyrillic character after a dot begins a word");
		check(search::isWordStart(Name, 10),
				"search-fuzzy: ... and so does a Cyrillic lowercase-to-uppercase step");
		check(!search::isWordStart(Name, 6),
				"search-fuzzy: ... while a character inside a word does not");

		check(scoreOf(StringView("ВЗЯТЬ"), Name) > 0,
				"search-fuzzy: Cyrillic case folding works in the matcher too");

		// The stated limit, asserted so that nobody later "fixes" it by reaching for compareFolded
		// and silently breaking the indices.
		check(scoreOf(StringView("ss"), StringView("groß")) < 0,
				"search-fuzzy: the matcher folds one character at a time, so ss does not match ß");
		check(sprt::unicode::compareFolded(StringView("gross"), StringView("groß")) == 0,
				"search-fuzzy: ... which is what compareFolded is for, and it is a different "
				"question");
	}

	// ---- from code points to what a label counts in --------------------------------------------

	{
		// Latin only: code points, UTF-16 units and bytes all agree, so this pins the run merging
		// rather than the conversion.
		checkEq(rangesOf(StringView("tra"), StringView("Transform")), "0+3",
				"search-fuzzy: consecutive matches become one range");
		checkEq(rangesOf(StringView("tf"), StringView("Transform")), "0+1 5+1",
				"search-fuzzy: ... and a gap splits it into two");

		// Two astral characters, one per emoji: every index after them differs by one between code
		// points and UTF-16 units, which is the whole reason this conversion exists.
		constexpr StringView Emoji("🔴🔵Blend");

		checkEq(indicesOf(StringView("bl"), Emoji), "2,3",
				"search-fuzzy: the matcher counts the emoji as one character each");
		checkEq(rangesOf(StringView("bl"), Emoji), "4+2",
				"search-fuzzy: ... and the label range starts at 4, because each emoji is two "
				"UTF-16 units");

		check(search::codepointToUtf16Offset(Emoji, 0) == 0
						&& search::codepointToUtf16Offset(Emoji, 1) == 2
						&& search::codepointToUtf16Offset(Emoji, 2) == 4,
				"search-fuzzy: an astral character advances the UTF-16 offset by two");
		check(search::codepointToUtf16Offset(Emoji, 100) == 9,
				"search-fuzzy: past the end is the total length, not an overrun");

		// Cyrillic is two bytes per character but one UTF-16 unit: a conversion that had been
		// written against bytes would pass every ASCII check and fail here.
		check(search::codepointToUtf16Offset(StringView("узел"), 4) == 4,
				"search-fuzzy: a two-byte character is still one UTF-16 unit");
	}

	// ---- the edit distance as a threshold ------------------------------------------------------

	{
		search::Distance exact(StringView("transform"), StringView("transform"));
		check(exact.distance() == 0, "search-fuzzy: a word is at distance zero from itself");

		// A transposition is two substitutions in plain Levenshtein - edlib is not Damerau, and the
		// vocabulary's k has to be read with that in mind.
		search::Distance swapped(StringView("transfrom"), StringView("transform"));
		check(swapped.distance() == 2,
				"search-fuzzy: a transposition costs two, because this is Levenshtein, not Damerau");

		search::Distance cut(StringView("transfrom"), StringView("transform"), 1);
		check(cut.distance() == maxOf<uint32_t>(),
				"search-fuzzy: past the cutoff the answer is 'farther than you asked', not a "
				"number");
	}

	// ---- the vocabulary ------------------------------------------------------------------------

	{
		auto vocab = Rc<search::Vocabulary>::create();
		vocab->addAll(SpanView<StringView>({StringView("transform"), StringView("transfer"),
			StringView("transformation"), StringView("scatter"), StringView("scene")}));
		vocab->add(StringView("scene")); // a duplicate, to prove build() collapses it
		vocab->build();

		check(vocab->size() == 5, "search-fuzzy: build deduplicates the vocabulary");

		{
			mem_std::String out;
			vocab->prefix(StringView("trans"), [&](StringView word) {
				if (!out.empty()) {
					out.append(",");
				}
				out.append(word.data(), word.size());
			});
			checkEq(out, "transfer,transform,transformation",
					"search-fuzzy: a prefix query answers in code point order");
		}

		{
			mem_std::String out;
			vocab->near(StringView("transfrom"), 2, [&](StringView word, uint32_t distance) {
				if (!out.empty()) {
					out.append(",");
				}
				out.append(word.data(), word.size());
				out.append("=");
				out.append(mem_std::toString(distance));
			});
			checkEq(out, "transform=2",
					"search-fuzzy: a transposed word is found at distance two, and nothing else is");
		}

		{
			size_t count = 0;
			vocab->near(StringView("transfrom"), 1, [&](StringView, uint32_t) { ++count; });
			check(count == 0, "search-fuzzy: ... and not at all within one edit");
		}

		check(search::Vocabulary::distanceForQuery(StringView("abc")) == 0
						&& search::Vocabulary::distanceForQuery(StringView("abcd")) == 1
						&& search::Vocabulary::distanceForQuery(StringView("abcdefgh")) == 2,
				"search-fuzzy: the tolerance a word earns is a function of its length, in one "
				"place");
		check(search::Vocabulary::distanceForQuery(StringView("узел")) == 1,
				"search-fuzzy: ... measured in characters, so a Cyrillic word of four earns one");
	}

	// ---- expansion: a typo becomes an ordinary Or ----------------------------------------------

	{
		// Simple: no stemming, so the words in the query are the words in the vocabulary and the
		// test is about expansion rather than about the Russian stemmer.
		search::Configuration cfg(search::Language::Simple);

		auto vocab = Rc<search::Vocabulary>::create();
		vocab->addAll(SpanView<StringView>(
				{StringView("transform"), StringView("scatter"), StringView("scene")}));
		vocab->build();

		{
			auto query = cfg.parseQuery(StringView("transfrom"));
			auto expanded = vocab->expand(query);

			// parseQuery always returns a root And node holding the leaves, so the leaf that got
			// expanded is one level down.
			check(expanded.op == search::SearchOp::And && expanded.args.size() == 1
							&& expanded.args.front().op == search::SearchOp::Or
							&& expanded.args.front().args.size() == 2,
					"search-fuzzy: a leaf the vocabulary can improve becomes an Or of two words");

			mem_std::String positive;
			expanded.decompose(
					[&](StringView word) {
				if (!positive.empty()) {
					positive.append(",");
				}
				positive.append(word.data(), word.size());
			},
					nullptr);
			checkEq(positive, "transfrom,transform",
					"search-fuzzy: ... and decompose reports what was typed AND what it might have "
					"been, which is what the SQLite prefilter reads");
		}

		{
			// A word nothing in the vocabulary resembles has to keep failing: an expansion that
			// dropped it would turn "no results" into "everything".
			auto query = cfg.parseQuery(StringView("quaternion"));
			auto expanded = vocab->expand(query);
			check(expanded.args.size() == 1 && expanded.args.front().args.empty()
							&& StringView(expanded.args.front().value) == StringView("quaternion"),
					"search-fuzzy: a term nothing resembles is left exactly as it was");
		}

		{
			// A phrase is copied verbatim: SearchQuery_isMatch reads a Follow node's arguments as
			// leaves, so an Or inside one would look up the empty string and match nothing.
			auto query = cfg.parseQuery(StringView("\"transfrom scatter\""));
			auto expanded = vocab->expand(query);

			bool leaves = expanded.args.size() == 1
					&& expanded.args.front().op == search::SearchOp::Follow
					&& !expanded.args.front().args.empty();
			if (leaves) {
				for (auto &it : expanded.args.front().args) {
					if (!it.args.empty()) {
						leaves = false;
					}
				}
			}
			check(leaves,
					"search-fuzzy: a quoted phrase is not expanded, because a Follow node's "
					"arguments have to stay leaves");
		}
	}

	// ---- a vocabulary outlives the frame that built it ------------------------------------------

	{
		/* Built inside a temporary pool and read after it is gone.

		This is the shape of a bug that cost a day: `pool::create(pool::acquire())` - the idiom
		Configuration uses next door - makes the new pool a CHILD of whatever frame is running, and
		a child pool cannot outlive its parent. Anything refcounted is read long after the frame
		that constructed it has ended, so it has to own its memory outright. */
		Rc<search::Vocabulary> vocab;
		memory::perform_temporary([&] {
			vocab = Rc<search::Vocabulary>::create();
			vocab->addAll(SpanView<StringView>(
					{StringView("transform"), StringView("scatter"), StringView("scene")}));
			vocab->build();
		});

		mem_std::String out;
		vocab->near(StringView("transfrom"), 2, [&](StringView word, uint32_t) {
			out.append(word.data(), word.size());
		});
		checkEq(out, "transform",
				"search-fuzzy: a vocabulary still answers after the pool it was built in is gone");
	}

	// ---- SearchIndex: minMatch is a promise, not a parameter that is ignored --------------------

	{
		auto index = Rc<search::SearchIndex>::create();
		index->add(StringView("alpha beta"), 1, 0);
		index->add(StringView("alpha gamma"), 2, 0);

		auto any = index->performSearch(StringView("alpha beta"), 1);
		check(any.nodes.size() == 2, "search-fuzzy: minMatch of one is 'any word of the request'");

		auto both = index->performSearch(StringView("alpha beta"), 2);
		check(both.nodes.size() == 1 && both.nodes.front().node->id == 1,
				"search-fuzzy: ... and minMatch of two drops the node that answers only one");

		// "al" prefix-matches both tokens of node 3, but they answer the SAME word of the request,
		// and one word answered twice is still one word answered.
		index->add(StringView("alpha alright"), 3, 0);

		auto twice = index->performSearch(StringView("al beta"), 2);
		bool hasThird = false;
		for (auto &it : twice.nodes) {
			if (it.node->id == 3) {
				hasThird = true;
			}
		}
		check(!hasThird,
				"search-fuzzy: two tokens answering the same word of the request count once");
	}
}

} // namespace STAPPLER_VERSIONIZED stappler
