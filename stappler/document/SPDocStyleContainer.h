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

	using CssSelectorStart = chars::Compose<char32_t, CssIdentifier, chars::Chars<char32_t, u'['> >;

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

		// does `state` (InteractiveFlags bits) satisfy this compound's pseudo-class requirements?
		bool matchesPseudo(uint32_t state) const {
			return (state & pseudoRequire) == pseudoRequire && (state & pseudoForbid) == 0;
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
							|| (v[j - 1].specificity == x.specificity && v[j - 1].order > x.order))) {
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

	// Right-to-left combinator match. `Access` adapts an arbitrary node model, providing:
	//   bool matchCompound(NodeT, const CompoundSelector &)
	//   NodeT parent(NodeT)  / NodeT prevSibling(NodeT)  / bool valid(NodeT)
	// The target ([0]) compound is verified too (bucketing only guarantees one token).
	template <typename NodeT, typename Access>
	bool matchComplex(const ComplexSelector &sel, NodeT target, const Access &access) const {
		return matchComplexFrom(sel, 0, target, access);
	}

protected:
	template <typename NodeT, typename Access>
	bool matchComplexFrom(const ComplexSelector &sel, size_t idx, NodeT node,
			const Access &access) const {
		if (!access.matchCompound(node, sel.compounds[idx])) {
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

	DocumentData *_document = nullptr;
	StyleType _type = StyleType::Css;
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
