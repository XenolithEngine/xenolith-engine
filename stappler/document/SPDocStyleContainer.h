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

	// a selector may start with a pseudo-class (`:root { ... }`), so ':' has to count as a
	// selector start even though it is not a CssIdentifier character
	using CssSelectorStart =
			chars::Compose<char32_t, CssIdentifier, chars::Chars<char32_t, u'[', u':'> >;

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

	// CSS combinator between a compound and the compound to its right ('target' side)
	enum class SelectorCombinator {
		Descendant, // 'A B'   - A is some ancestor of B
		Child, // 'A > B' - A is the immediate parent of B
		AdjacentSibling, // 'A + B' - A is the immediately-preceding sibling of B
		GeneralSibling, // 'A ~ B' - A is some preceding sibling of B
	};

	// One structural test: the CSS An+B formula against a node's 1-based index among its
	// siblings. Every structural pseudo-class reduces to this, so the compound stores a list
	// of tests instead of a flag per selector:
	//   :first-child   -> {0, 1}            :last-child  -> {0, 1, fromEnd}
	//   :only-child    -> both of the above :nth-child(odd) -> {2, 1}
	//   :nth-of-type(n) -> {1, 0, ofType}   (the same tests with a same-type sibling filter)
	struct NthTest {
		int32_t a = 0;
		int32_t b = 0;
		bool fromEnd = false; // :nth-last-child / :nth-last-of-type - count from the end
		bool ofType = false; // :nth-of-type - count only siblings sharing the node's type

		bool operator==(const NthTest &) const = default;

		// does a 1-based index within a sibling run of `total` satisfy this test?
		bool matches(uint32_t index, uint32_t total) const {
			int32_t i = fromEnd ? int32_t(total) - int32_t(index) + 1 : int32_t(index);
			if (a == 0) {
				return i == b;
			}
			int32_t d = i - b;
			return (d % a) == 0 && (d / a) >= 0;
		}
	};

	/* One argument of `:not()` / `:is()` / `:where()`: a compound with NO combinators and no
	functional pseudo-class of its own.

	It is a flat type rather than a CompoundSelector, and that is what the restriction buys. A
	compound holding compounds would be an incomplete type inside a pool vector; routing around it
	(an argument pool plus indices) would mean handing the whole ComplexSelector to the client's
	matchCompound, i.e. changing the seam between this header and every consumer - for a nesting
	the parser refuses anyway.

	Structural tests (`:is(:first-child)`) are refused for the same reason they are not here: they
	live in NthTest and are checked by a separate pass over the sibling run. */
	struct SelectorArg {
		String tag;
		String id;
		Vector<String> classes;
		bool universal = false;
		uint32_t pseudoRequire = 0;
		uint32_t pseudoForbid = 0;

		// packed (id, class, type) triple of THIS argument, computed once while parsing: `:not()`
		// takes its argument's specificity and `:is()` the largest of its own
		uint32_t specificity = 0;

		bool matchesPseudo(uint32_t state) const {
			return (state & pseudoRequire) == pseudoRequire && (state & pseudoForbid) == 0;
		}
	};

	// One `:is(...)` or `:where(...)`: at least one option must match. `:where()` is the same test
	// with its specificity thrown away - which is the whole reason it exists.
	struct SelectorMatchAny {
		Vector<SelectorArg> options;
		bool zeroSpecificity = false; // `:where()`
	};

	// a single compound (tag/id/classes/pseudo) with its leading combinator; strings are
	// owned (pool-backed) so they stay valid for the container's lifetime
	struct CompoundSelector {
		// relation of this compound to the compound on its right; unused for the target ([0])
		SelectorCombinator combinator = SelectorCombinator::Descendant;
		String tag; // element/tag name; empty when universal or absent
		String id; // '#id' without the marker; empty when absent
		Vector<String> classes; // '.class' names without the marker
		bool universal = false; // explicit '*'
		uint32_t pseudoRequire = 0; // InteractiveFlags bits that must be SET on the node
		uint32_t pseudoForbid = 0; // InteractiveFlags bits that must be CLEAR (e.g. :disabled)

		// structural pseudo-classes; ALL of these must pass
		Vector<NthTest> nth;
		bool requireEmpty = false; // :empty - the node has no children
		bool requireRoot = false; // :root - the node owns the nearest style scope

		// `:not(...)` - NONE of these may match. An argument made only of interactive bits never
		// lands here: it is folded into pseudoForbid while parsing, so `:not(:hover)` costs nothing
		Vector<SelectorArg> negations;

		// `:is(...)` / `:where(...)` - each list must have at least one matching option
		Vector<SelectorMatchAny> anyOf;

		// specificity contributed by the two above, as a packed triple. Kept ready-made because a
		// functional pseudo-class contributes id/class/type counts, not just "one more class"
		uint32_t extraId = 0;
		uint32_t extraClass = 0;
		uint32_t extraType = 0;

		// does `state` (InteractiveFlags bits) satisfy this compound's pseudo-class requirements?
		bool matchesPseudo(uint32_t state) const {
			return (state & pseudoRequire) == pseudoRequire && (state & pseudoForbid) == 0;
		}

		// is there anything here beyond tag/id/classes, i.e. something the plain string-keyed
		// rule store cannot express?
		bool hasPredicates() const {
			return pseudoRequire != 0 || pseudoForbid != 0 || !nth.empty() || requireEmpty
					|| requireRoot || !negations.empty() || !anyOf.empty();
		}

		// number of predicates counting as a class for CSS specificity
		uint32_t countPseudoSpecificity() const {
			return uint32_t(__builtin_popcount(pseudoRequire))
					+ uint32_t(__builtin_popcount(pseudoForbid)) + uint32_t(nth.size())
					+ (requireEmpty ? 1u : 0u) + (requireRoot ? 1u : 0u);
		}
	};

	// a full selector with combinators; compounds are ordered RIGHT-TO-LEFT so that
	// compounds[0] is the target/key compound (the part a node is bucketed by)
	struct ComplexSelector {
		Vector<CompoundSelector> compounds;
		StyleList style;
		// OR of token bits required by every Descendant/Child ancestor compound; used to
		// reject non-matching descendant selectors in O(1) before the backtracking walk
		uint64_t ancestorFilterBits = 0;
		uint32_t specificity = 0; // packed CSS specificity of the whole selector
		uint32_t order = 0; // source order within the container (cascade tie-break)
	};

	// a simple string-keyed rule (`*`, `tag`, `.cls`, `tag.cls`, `#id`, `tag#id`)
	struct SimpleRule {
		StyleList style;
		uint32_t specificity = 0; // packed CSS specificity of the key
		uint32_t order = 0; // source order within the container (cascade tie-break)
	};

	// a rule matched against a node, carried through the CSS cascade sort. Matched rules are
	// applied in ASCENDING (specificity, order) so the most specific / latest declaration wins.
	struct MatchedRule {
		const StyleList *style = nullptr;
		SpanView<bool> media; // resolved @media bits of the owning sheet (per-rule media filter)
		// string table of the sheet that parsed the rule; the raw text of its custom properties
		// and of its deferred var() declarations is indexed into THIS table, not the nearest
		// sheet's, so it has to travel with the rule
		SpanView<StringView> strings;
		uint32_t specificity = 0;
		uint64_t order = 0; // (scopeRank << 32) | source-order; higher wins ties
	};

	// pack CSS specificity components (a=id, b=class+attr+pseudo, c=type) into one comparable
	// value: a << 16 | b << 8 | c (each clamped to 8 bits)
	static uint32_t packSpecificity(uint32_t idCount, uint32_t classCount, uint32_t typeCount);

	// sort matched rules into CSS cascade order: ascending (specificity, then order), so a
	// later apply/merge lets the most specific / latest declaration win. Insertion sort keeps
	// it stable for tiny match sets; works with any random-access vector of MatchedRule.
	template <typename Vec>
	static void sortMatchedRules(Vec &v) {
		for (size_t i = 1; i < v.size(); ++i) {
			MatchedRule x = v[i];
			size_t j = i;
			while (j > 0
					&& (v[j - 1].specificity > x.specificity
							|| (v[j - 1].specificity == x.specificity
									&& v[j - 1].order > x.order))) {
				v[j] = v[j - 1];
				--j;
			}
			v[j] = x;
		}
	}
	// specificity of a simple string-keyed selector (one compound: '*', 'tag', '.cls', '#id', ...)
	static uint32_t specificityOfSimpleKey(StringView);

	// Bloom bit for a selector token; `kind`: 0 = tag, 1 = class, 2 = id. MUST be
	// computed identically at parse time and at match time so bit sets are comparable.
	static uint64_t selectorTokenBit(uint32_t kind, StringView token);

	// true if the (normalized, comma-split) selector text must go through the structured
	// complex-selector path instead of the simple-key store: it contains a combinator OR an
	// interactive pseudo-class (`:hover`, ...), neither of which the simple keys can express
	static bool selectorNeedsStructured(StringView);

	// split a selector list on top-level commas only, stepping over `(...)`, `[...]` and
	// quoted spans (a naked split would cut `:not(.a, .b)` and `[attr="a,b"]` in half)
	static void splitSelectorList(StringView, const Callback<void(StringView)> &);

	// does any parsed rule use a structural pseudo-class? Consumers gate their sibling-order
	// invalidation on this, so a sheet without them pays nothing.
	bool hasStructuralSelectors() const { return _hasStructuralSelectors; }

	// does any parsed rule declare a custom property or reference one with var()? Lets the
	// cascade skip its custom-property pass entirely for an ordinary sheet.
	bool hasCustomProperties() const { return _hasCustomProperties; }

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

	// Evaluate the structural part of a compound (An+B / :empty / :root). Kept here rather
	// than in each `Access` so the An+B arithmetic exists exactly once; the adapter only
	// answers the primitive queries.
	template <typename NodeT, typename Access>
	static bool matchStructural(const CompoundSelector &c, NodeT node, const Access &access) {
		if (c.requireRoot && !access.isRoot(node)) {
			return false;
		}
		if (c.requireEmpty && !access.isEmpty(node)) {
			return false;
		}
		for (auto &test : c.nth) {
			uint32_t index = 0, total = 0;
			if (!access.siblingIndex(node, test.ofType, index, total)
					|| !test.matches(index, total)) {
				return false;
			}
		}
		return true;
	}

	// Right-to-left combinator match. `Access` adapts an arbitrary node model, providing:
	//   bool matchCompound(NodeT, const CompoundSelector &)
	//   NodeT parent(NodeT)  / NodeT prevSibling(NodeT)  / bool valid(NodeT)
	//   bool siblingIndex(NodeT, bool sameTypeOnly, uint32_t &index, uint32_t &total)
	//       - 1-based index among the parent's children and their count; false without a parent
	//   bool isEmpty(NodeT)  - the node has no children (:empty)
	//   bool isRoot(NodeT)   - the node owns the nearest style scope (:root)
	// The target ([0]) compound is verified too (bucketing only guarantees one token).
	template <typename NodeT, typename Access>
	bool matchComplex(const ComplexSelector &sel, NodeT target, const Access &access) const {
		return matchComplexFrom(sel, 0, target, access);
	}

protected:
	template <typename NodeT, typename Access>
	bool matchComplexFrom(const ComplexSelector &sel, size_t idx, NodeT node,
			const Access &access) const {
		if (!access.matchCompound(node, sel.compounds[idx])
				|| !matchStructural(sel.compounds[idx], node, access)) {
			return false;
		}
		if (idx + 1 == sel.compounds.size()) {
			return true;
		}
		const auto &next = sel.compounds[idx + 1];
		switch (next.combinator) {
		case SelectorCombinator::Child: {
			auto p = access.parent(node);
			return access.valid(p) && matchComplexFrom(sel, idx + 1, p, access);
		}
		case SelectorCombinator::Descendant: {
			auto p = access.parent(node);
			while (access.valid(p)) {
				if (matchComplexFrom(sel, idx + 1, p, access)) {
					return true;
				}
				p = access.parent(p);
			}
			return false;
		}
		case SelectorCombinator::AdjacentSibling: {
			auto s = access.prevSibling(node);
			return access.valid(s) && matchComplexFrom(sel, idx + 1, s, access);
		}
		case SelectorCombinator::GeneralSibling: {
			auto s = access.prevSibling(node);
			while (access.valid(s)) {
				if (matchComplexFrom(sel, idx + 1, s, access)) {
					return true;
				}
				s = access.prevSibling(s);
			}
			return false;
		}
		}
		return false;
	}

	// parse a combinator selector and add it to the complex index (dedupe/merge by text)
	void addComplexSelector(StringView normalizedSelector, const StyleList &style);

	void import(StringReader &);

	void readStyleParameters(const StringView &name, const StringView &value,
			const StyleCallback &);

	// route one parsed declaration into `target`: a `--name` custom property and a value
	// containing var() are stored as raw text, everything else is parsed immediately
	void readStyleDeclaration(StyleList &target, StringView name, StringView value,
			MediaQueryId mediaQuery, StyleRule rule);

	DocumentData *_document = nullptr;
	StyleType _type = StyleType::Css;
	// set while parsing when any rule uses a structural pseudo-class - see
	// hasStructuralSelectors()
	bool _hasStructuralSelectors = false;
	// set while parsing when any rule declares or references a custom property
	bool _hasCustomProperties = false;
	Map<String, SimpleRule> _styles;
	Map<String, Vector<FontFace>> _fonts;

	// monotonic source-order counter assigned to every rule (simple + complex) as parsed;
	// used as the CSS cascade tie-break when specificity is equal (later = higher priority)
	uint32_t _ruleOrderCounter = 0;

	// combinator selectors, keyed by full normalized text for dedupe/merge; the ordered
	// map keeps element addresses stable so `_complexStyles` can point into it
	Map<String, ComplexSelector> _complexSelectors;
	// target-compound index: most-specific token of the rightmost compound
	// ('#id' > '.firstClass' > 'tag' > '*') -> candidate selectors bucketed under it
	Map<String, Vector<ComplexSelector *>> _complexStyles;
};

} // namespace stappler::document

#endif /* CORE_DOCUMENT_SPDOSTYLECONTAINER_H_ */
