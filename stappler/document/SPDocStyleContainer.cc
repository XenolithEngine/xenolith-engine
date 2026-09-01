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

#include "SPDocStyleContainer.h"
#include "SPString.h"
#include "SPFilesystem.h"

namespace STAPPLER_VERSIONIZED stappler::document {

auto StyleContainer::StyleBuffers::getSelectorStream() {
	selector.clear();
	return [this](StyleContainer::StringReader s) { selector.put(s.data(), s.size()); };
}

auto StyleContainer::StyleBuffers::getNameStream() {
	name.clear();
	return [this](StyleContainer::StringReader s) { name.put(s.data(), s.size()); };
}

auto StyleContainer::StyleBuffers::getValueStream() {
	value.clear();
	return [this](StyleContainer::StringReader s) { value.put(s.data(), s.size()); };
}

void StyleContainer::StyleBuffers::nameToLower() {
	auto d = (char *)name.data();
	for (size_t i = 0; i < name.size(); ++i) {
		*d = sprt::tolower_c(*d);
		++d;
	}
}

void StyleContainer::StyleBuffers::valueToLower() {
	char quoted = 0;
	auto d = (char *)value.data();
	while (d < (char *)value.data() + value.size()) {
		if (quoted != 0) {
			if (*d == '\\') {
				d += 2;
			} else if (*d == quoted) {
				quoted = 0;
				++d;
			} else {
				++d;
			}
		} else if (*d == '\'') {
			quoted = '\'';
			++d;
		} else if (*d == '"') {
			quoted = '"';
			++d;
		} else {
			*d = sprt::tolower_c(*d);
		}
		++d;
	}
}

static void checkCssComments(StyleContainer::StringReader &s) {
	s.skipChars<StyleContainer::Group<CharGroupId::WhiteSpace>>();
	while (s.is("/*")) {
		s.skipUntilString("*/", false);
		s.skipChars<StyleContainer::Group<CharGroupId::WhiteSpace>>();
	}
}

template <char32_t Q>
void readNameQuoted(const Callback<void(StyleContainer::StringReader)> &out,
		StyleContainer::StringReader &s) {
	if (s.is<Q>()) {
		++s;
	}

	while (!s.empty() && !s.is<Q>()) {
		auto tmp = s.readUntil<StyleContainer::Chars<Q, '\\'>>();
		if (!tmp.empty()) {
			out << tmp;
		}
		if (s.is('\\')) {
			++s;
			out << s.letter();
			++s;
		}
	}

	if (s.is<Q>()) {
		++s;
	}
}

void readName(const Callback<void(StyleContainer::StringReader)> &out,
		StyleContainer::StringReader &s) {
	s.skipChars<StyleContainer::StringReader::WhiteSpace>();

	char32_t end = 0;
	StyleContainer::StringReader tmp;
	if (s.is('(')) {
		end = ')';
		++s;
	}

	while (!s.empty() && !s.is(end)) {
		tmp = s.readUntil<StyleContainer::Chars<'\'', '"', '(', ')'>>();
		tmp.trimChars<StyleContainer::StringReader::WhiteSpace>();
		if (!tmp.empty()) {
			out << tmp;
		}

		if (s.is('\'')) {
			readNameQuoted<'\''>(out, s);
		} else if (s.is('"')) {
			readNameQuoted<'"'>(out, s);
		}
	}

	if (end && s.is(end)) {
		++s;
	}
}

template <char32_t F>
StyleContainer::StringReader readQuotedBlock(StyleContainer::StringReader &s) {
	StyleContainer::StringReader ret(s, 0);

	if (s.is<F>()) {
		++s;
	}

	while (!s.empty() && !s.is<F>()) {
		s.skipUntil<StyleContainer::Chars<u'\\', F>>();
		if (s.is('\\')) {
			s += 2;
		}
	}

	if (s.is<F>()) {
		++s;
	}

	return StyleContainer::StringReader(ret.data(), s.data() - ret.data());
}

template <char32_t Start, char32_t End>
static bool readBracedBlock(const Callback<void(StyleContainer::StringReader)> &out,
		StyleContainer::StringReader &s, uint32_t depth = 0) {
	// bound recursion: deeply nested ()/[] in a selector would otherwise exhaust the
	// native stack. Past the cap, consume the opening bracket and bail so the caller
	// still makes progress (degraded parse, no crash).
	static constexpr uint32_t MaxBraceDepth = 64;
	if (depth >= MaxBraceDepth) {
		if (s.is<Start>()) {
			out << s.letter();
			++s;
		}
		return false;
	}

	if (s.is<Start>()) {
		out << s.letter();
		++s;
	}

	checkCssComments(s);

	StyleContainer::StringReader tmp;

	while (!s.is<End>() && !s.empty()) {
		tmp = s.readChars<StyleContainer::CssIdentifierExtended>();

		checkCssComments(s);

		if (!tmp.empty()) {
			out << tmp;
		}

		if (s.is('\'')) {
			out << readQuotedBlock<'\''>(s).str<StyleContainer::Interface>();
		} else if (s.is('"')) {
			out << readQuotedBlock<'"'>(s).str<StyleContainer::Interface>();
		} else if (s.is('(')) {
			readBracedBlock<'(', ')'>(out, s, depth + 1);
		} else if (s.is('[')) {
			readBracedBlock<'[', ']'>(out, s, depth + 1);
		} else if (s.is<End>()) {
			break;
		} else if (Start == '[' && s.is('=')) {
			out << "=";
			++s;
		} else if (!s.is<StyleContainer::CssIdentifierExtended>()) {
			return false;
		} else {
			out << " ";
		}
	}

	checkCssComments(s);

	if (s.is<End>()) {
		out << s.letter();
		++s;
	}

	checkCssComments(s);
	return true;
}

template <char32_t F>
static bool readCssSelector(const Callback<void(StyleContainer::StringReader)> &out,
		StyleContainer::StringReader &s) {
	checkCssComments(s);
	s.skipUntil<StyleContainer::CssSelectorStart, StyleContainer::Chars<F>>();
	if (s.is<F>() || s.empty()) {
		return false;
	}

	StyleContainer::StringReader tmp;
	if (s.is('@')) {
		out << s.readChars<StyleContainer::CssIdentifier>();
		;
		checkCssComments(s);
		return true;
	}

	while (!s.is<F>() && !s.empty()) {
		tmp = s.readChars<StyleContainer::CssIdentifier>();

		checkCssComments(s);

		if (!tmp.empty()) {
			out << tmp;
		}

		if (s.is('\'')) {
			out << readQuotedBlock<'\''>(s).str<StyleContainer::Interface>();
		} else if (s.is('"')) {
			out << readQuotedBlock<'"'>(s).str<StyleContainer::Interface>();
		} else if (s.is('(')) {
			readBracedBlock<'(', ')'>(out, s);
		} else if (s.is('[')) {
			readBracedBlock<'[', ']'>(out, s);
		} else if (s.is('{') || s.is(';')) {
			return true;
		} else if (s.is(':')) {
			out << ":";
			++s;
		} else if (!s.is<StyleContainer::CssIdentifier>()) {
			return false;
		} else {
			out << " ";
		}
	}

	checkCssComments(s);
	return true;
}

template <char32_t F>
static void readCssIdentifier(const Callback<void(StyleContainer::StringReader)> &out,
		StyleContainer::StringReader &s) {
	checkCssComments(s);
	s.skipUntil<StyleContainer::CssIdentifier, StyleContainer::Chars<'}', F>>();
	if (s.is<F>() || s.is('}')) {
		return;
	}

	auto tmp = s.readChars<StyleContainer::CssIdentifier>();
	tmp.trimChars<StyleContainer::StringReader::WhiteSpace>();

	out << tmp;

	checkCssComments(s);
}

template <char32_t F>
static bool readCssValue(const Callback<void(StyleContainer::StringReader)> &out,
		StyleContainer::StringReader &s) {
	if (!s.is(':')) {
		s.skipUntil<StyleContainer::CssIdentifier, StyleContainer::Chars<F, ';', '}'>>();
		if (s.is(';') || s.is('}')) {
			s++;
		}
		return false;
	}

	s++;
	checkCssComments(s);
	while (!s.empty() && !s.is(';') && !s.is('}') && !s.is<F>()) {
		out << s.readUntil<StyleContainer::StringReader::Chars<F, '\'', '"', '}', ';'>>();

		checkCssComments(s);
		if (s.is<F>()) {
			break;
		}
		if (s.is('\'')) {
			out << readQuotedBlock<'\''>(s);
		} else if (s.is('"')) {
			out << readQuotedBlock<'"'>(s);
		}
	}

	checkCssComments(s);
	return true;
}

template <char32_t F>
void readCssStyleRules(StyleContainer::StyleBuffers &buffers, StyleContainer::StringReader &s,
		const Callback<void(const StyleContainer::StringReader &name,
				const StyleContainer::StringReader &value, StyleRule rule)> &fn) {
	if (s.is('{')) {
		s++;
	}

	checkCssComments(s);

	StyleContainer::StringReader name, value;
	while (!s.empty() && !s.is('}') && !s.is<F>()) {
		name.clear();
		value.clear();

		readCssIdentifier<F>(buffers.getNameStream(), s);
		buffers.nameToLower();
		name = buffers.name.get<StyleContainer::StringReader>();
		name.trimChars<StyleContainer::StringReader::WhiteSpace>();

		// A custom property (`--name`) has no grammar of its own - its value is kept verbatim
		// and only interpreted where it is substituted, so it must not be case-folded. The
		// NAME is still folded: a `var(--Brand)` reference sits inside an ordinary value, which
		// is folded, so keeping declaration names case-sensitive could never match.
		const bool customProperty = name.starts_with("--");

		readCssValue<F>(buffers.getValueStream(), s);
		if (!customProperty) {
			buffers.valueToLower();
		}
		value = buffers.value.get<StyleContainer::StringReader>();
		value.trimChars<StyleContainer::StringReader::WhiteSpace>();

		if (value.ends_with("!important")) {
			value = value.sub(0, value.size() - "!important"_len);
			value.trimChars<StyleContainer::StringReader::WhiteSpace>();

			fn(name, value, StyleRule::Important);
		} else {
			fn(name, value, StyleRule::None);
		}

		checkCssComments(s);
		s.skipChars<StyleContainer::Group<CharGroupId::WhiteSpace>, StyleContainer::Chars<';'>>();
		checkCssComments(s);
	}

	checkCssComments(s);
	if (s.is('}')) {
		s++;
		checkCssComments(s);
	}
}

StringView StyleContainer::resolveCssString(const StringView &origStr) {
	StringView str(origStr);
	str.trimChars<StringView::CharGroup<CharGroupId::WhiteSpace>>();

	StringView tmp(str);
	tmp.trimUntil<StringView::Chars<'(', '"', '\'', ')'>>();
	if (tmp.size() > 2) {
		auto c = tmp.front();
		auto b = tmp.back();
		if ((c == '(' || c == '"' || c == '\'' || c == ')')
				&& (b == '(' || b == '"' || b == '\'' || b == ')')) {
			tmp.trimChars<StringView::Chars<'(', '"', '\'', ')'>,
					StringView::CharGroup<CharGroupId::WhiteSpace>>();
			return tmp;
		}
	}

	return str;
}

void StyleContainer::readQuotedString(StringReader &s, String &str, char quoted) {
	while (!s.empty() && !s.is(quoted)) {
		if (quoted == '"') {
			str += s.readUntil<Chars<u'\\'>, DoubleQuote>().str<Interface>();
		} else {
			str += s.readUntil<Chars<u'\\'>, SingleQuote>().str<Interface>();
		}
		if (s.is('\\')) {
			s++;
			str += s.letter().str<Interface>();
			s++;
		}
	}

	if (s.is(quoted)) {
		s++;
	}
}

StyleContainer::StyleContainer(DocumentData *doc, StyleType t) : _document(doc), _type(t) { }

bool StyleContainer::readStyle(StringReader &s) {
	struct BlockData {
		MediaQueryId media = MediaQueryIdNone;
		bool disabled = false;
	};

	StringReader selector;
	StyleList style;
	MediaQuery mediaQuery;
	Vector<BlockData> blockStack;
	StyleBuffers buffers;

	blockStack.emplace_back(BlockData());

	while (!s.empty() && !s.is('<')) {
		selector.clear();
		style.data.clear();
		style.custom.clear();
		style.pending.clear();

		// Comments first: `*` is a selector start (the universal selector), so the skipUntil below
		// would stop on the `*` of an opening `/*` and hand readCssSelector the comment body -
		// which fails and drops the WHOLE stylesheet. Every other position already checks comments
		// on the way out of a rule; this is the one that was missing, so a stylesheet simply could
		// not begin with a comment.
		checkCssComments(s);

		s.skipUntil<CssSelectorStart, Chars<'}'>>();
		if (s.empty()) {
			return true;
		}

		if (s.is('}')) {
			if (!blockStack.empty()) {
				blockStack.pop_back();
				s++;
				continue;
			} else {
				return false;
			}
		}

		auto tmp = s;
		if (!readCssSelector<char(0)>(buffers.getSelectorStream(), s)) {
			log::source().error("document::StyleContainer", "Ill-formed CSS: '", tmp, "'");
			return false;
		}

		selector = buffers.selector.get<StringReader>();

		if (selector == "@import") {
			s.skipUntil<CssIdentifier, Chars<'<', '}'>>();

			readCssSelector<';'>(buffers.getSelectorStream(), s);
			selector = buffers.selector.get<StringReader>();
			checkCssComments(s);
			if (s.is(';')) {
				++s;
			} else {
				log::source().error("document::StyleContainer", "Ill-formed CSS (@import)");
				return false;
			}

			if (!selector.empty()) {
				import(selector);
				selector.clear();
			}
		} else if (selector == "@font-face") {
			s.skipUntil<CssIdentifier, Chars<'<', '{'>>();
			if (s.is('{')) {
				auto face = readFontFace(buffers, s);
				if (!face.fontFamily.empty()) {
					auto it = _fonts.find(face.fontFamily);
					if (it == _fonts.end()) {
						it = _fonts.emplace(face.fontFamily, Vector<FontFace>()).first;
					}
					it->second.emplace_back(move(face));
				}
			}
			continue;
		} else if (selector == "@media") {
			;
			_document->queries.emplace_back();

			// explicit assignment: MediaQuery is an aggregate with a base class, so
			// MediaQuery{list} would initialize the AllocPool base, not `list`
			MediaQuery query;
			query.list = readMediaQueryList(buffers, s);
			blockStack.emplace_back(
					BlockData{_document->addQuery(sp::move(query)), blockStack.back().disabled});
			continue;
		} else if (selector.is('@')) {
			// skip at-rule
			log::source().warn("document::StyleContainer", "Unknown at-rule: ", selector);

			readCssSelector<';'>(buffers.getSelectorStream(), s);
			checkCssComments(s);
			if (s.is(';')) {
				++s;
				continue;
			} else if (s.is('{')) {
				++s;
				blockStack.emplace_back(BlockData{blockStack.back().media, true});
				continue;
			}
		} else if (selector.empty()) {
			log::source().error("document::StyleContainer", "Invalid Css format");
			return false;
		}

		if (!selector.empty() && s.is('{')) {
			readCssStyleRules<'}'>(buffers, s,
					[&](const StringReader &name, const StringReader &value, StyleRule rule) {
				if (!blockStack.back().disabled) {
					// log::source().verbose("document::StyleContainer", selector, " {", name, ": ", value, "}");
					readStyleDeclaration(style, name, value, blockStack.back().media, rule);
				}
			});
		}

		// a rule may carry only custom properties (`:root { --brand: … }`) or only deferred
		// var() declarations - it must still be indexed
		if (!selector.empty()
				&& (!style.data.empty() || !style.custom.empty() || !style.pending.empty())) {
			splitSelectorList(selector, [&](StringView r) {
				r.trimChars<StringView::WhiteSpace>();
				if (r.empty()) {
					return;
				}
				if (selectorNeedsStructured(r)) {
					// combinator or pseudo-class selector: parse into a structured, bucketed rule
					addComplexSelector(r, style);
					return;
				}
				auto it = _styles.find(r);
				if (it == _styles.end()) {
					SimpleRule rule;
					rule.style = style;
					rule.specificity = specificityOfSimpleKey(r);
					rule.order = _ruleOrderCounter++;
					_styles.emplace(r.str<Interface>(), sp::move(rule));
				} else {
					it->second.style.merge(style);
					it->second.order = _ruleOrderCounter++; // later occurrence wins the tie-break
				}
			});
		}

		if (s.is(';')) {
			s++;
			checkCssComments(s);
		}

		s.readUntil<CssSelectorStart, Chars<'<', '}'>>();
	}

	return true;
}

bool StyleContainer::readStyle(FileInfo path) {
	if (filesystem::exists(path)) {
		auto d = filesystem::readIntoMemory<mem_std::Interface>(path);
		StringReader r((const char *)d.data(), d.size());
		return readStyle(r);
	}
	return false;
}

bool StyleContainer::readStyle(StyleList &target, StringReader &r) {
	StyleBuffers buffers;
	readCssStyleRules<'}'>(buffers, r,
			[&](const StringReader &name, const StringReader &value, StyleRule rule) {
		readStyleDeclaration(target, name, value, MediaQueryIdNone, rule);
	});
	return r.empty();
}

FontFace StyleContainer::readFontFace(StyleBuffers &buffers, StringReader &s) {
	if (s.is('{')) {
		++s;
	}

	FontFace ret;
	StringReader name, value, data;

	while (!s.empty() && !s.is('}')) {
		readCssIdentifier<')'>(buffers.getNameStream(), s);
		buffers.nameToLower();
		name = buffers.name.get<StringReader>();

		if (name == "src") {
			if (s.is(':')) {
				++s;
			}

			checkCssComments(s);

			while (!s.is(';') && !s.empty()) {
				FontFace::FontFaceSource source;

				while (!s.is(',') && !s.is(';') && !s.empty()) {
					readCssIdentifier<')'>(buffers.getNameStream(), s);
					buffers.nameToLower();
					name = buffers.name.get<StringReader>();

					readName(buffers.getSelectorStream(), s);
					data = buffers.selector.get<StringReader>();

					if (name == "local") {
						source.url = data.str<Interface>();
						source.isLocal = true;
					} else if (name == "url") {
						source.url = data.str<Interface>();
					} else if (name == "format") {
						source.format = data.str<Interface>();
					} else if (name == "tech") {
						source.tech = data.str<Interface>();
					}

					checkCssComments(s);
				}

				if (s.is(',')) {
					++s;
					checkCssComments(s);
				}

				if (!source.url.empty()) {
					ret.src.emplace_back(move(source));
				}
			}
		} else if (name == "font-family") {
			readCssValue<')'>(buffers.getValueStream(), s);
			buffers.valueToLower();
			value = buffers.value.get<StringReader>();

			readName(buffers.getSelectorStream(), value);
			data = buffers.selector.get<StringReader>();

			ret.fontFamily = data.str<Interface>();
		} else if (name == "font-stretch") {
			readCssValue<')'>(buffers.getValueStream(), s);
			buffers.valueToLower();
			value = buffers.value.get<StringReader>();

			int i = 0;
			value.split<StringReader::WhiteSpace>([&](const StringReader &r) {
				readStyleParameters(name, r, [&](StyleParameter &&param) {
					if (i == 0) {
						ret.variations.axisMask |= FontVariableAxis::Stretch;
						ret.variations.stretch = param.value.fontStretch;
					} else {
						ret.variations.stretch.max = param.value.fontStretch;
					}
					++i;
					return true;
				});
			});
		} else if (name == "font-style") {
			readCssValue<')'>(buffers.getValueStream(), s);
			buffers.valueToLower();
			value = buffers.value.get<StringReader>();

			if (value == "normal") {
				ret.variations.axisMask |= FontVariableAxis::Slant | FontVariableAxis::Italic;
				ret.variations.slant = FontStyle::Normal;
				ret.variations.italic = 0;
			} else if (value == "italic") {
				ret.variations.axisMask |= FontVariableAxis::Slant | FontVariableAxis::Italic;
				ret.variations.slant = FontStyle::Italic;
				ret.variations.italic = 1;
			} else if (value == "oblique") {
				ret.variations.axisMask |= FontVariableAxis::Slant | FontVariableAxis::Italic;
				ret.variations.slant = FontStyle::Oblique;
				ret.variations.italic = 0;
			} else if (value.starts_with("oblique")) {
				StringView tmp(value);
				tmp += "oblique"_len;
				tmp.skipChars<StringView::WhiteSpace>();
				auto val = tmp.readFloat().get(nan());
				if (!sprt::isnan(val)) {
					tmp.skipChars<StringView::WhiteSpace>();
					if (tmp.is("deg") && val >= -90.0 && val <= 90.0) {
						ret.variations.axisMask |=
								FontVariableAxis::Slant | FontVariableAxis::Italic;
						ret.variations.italic = 0;
						ret.variations.slant = FontStyle(uint16_t(val * (1 << 6)));
						tmp += "deg"_len;
						tmp.skipChars<StringView::WhiteSpace>();
						if (!tmp.empty()) {
							val = tmp.readFloat().get(nan());
							if (!sprt::isnan(val)) {
								tmp.skipChars<StringView::WhiteSpace>();
								if (tmp.is("deg") && val >= -90.0 && val <= 90.0) {
									ret.variations.slant.max = FontStyle(uint16_t(val * (1 << 6)));
								}
							}
						}
					}
				}
			}
		} else if (name == "font-weight") {
			readCssValue<')'>(buffers.getValueStream(), s);
			buffers.valueToLower();
			value = buffers.value.get<StringReader>();

			int i = 0;
			value.split<StringReader::WhiteSpace>([&](const StringReader &r) {
				readStyleParameters(name, r, [&](StyleParameter &&param) {
					if (i == 0) {
						ret.variations.axisMask |= FontVariableAxis::Weight;
						ret.variations.weight = param.value.fontWeight;
					} else {
						ret.variations.weight.max = param.value.fontWeight;
					}
					++i;
					return true;
				});
			});
		} else {
			readCssValue<')'>(buffers.getValueStream(), s);
			buffers.valueToLower();
			value = buffers.value.get<StringReader>();

			readStyleParameters(name, value, [&](StyleParameter &&param) {
				ret.style.emplace_back(move(param));
				return true;
			});
		}

		checkCssComments(s);
		if (s.is(';')) {
			++s;
			checkCssComments(s);
		}
	}

	if (s.is('}')) {
		++s;
	}

	return ret;
}

MediaQuery::Query StyleContainer::readMediaQuery(StyleBuffers &buffers, StringReader &s) {
	MediaQuery::Query q;

	s.skipChars<Group<CharGroupId::WhiteSpace>>();

	if (!s.empty() && !s.is('(')) {
		if (s.is("not")) {
			q.negative = true;
			s.skipString("not");
			s.skipChars<Group<CharGroupId::WhiteSpace>>();
		} else if (s.is("only")) {
			s.skipString("only");
			s.skipChars<Group<CharGroupId::WhiteSpace>>();
		}

		readCssIdentifier<';'>(buffers.getSelectorStream(), s);
		auto id = buffers.selector.get<StringReader>();

		if (!q.setMediaType(id)) {
			return q;
		}

		if (!s.is("and")) {
			return q;
		} else {
			s.skipString("and");
			s.skipChars<Group<CharGroupId::WhiteSpace>>();
		}
	}

	StringReader identifier, value;

	while (s.is('(')) {
		s++;

		readCssIdentifier<')'>(buffers.getNameStream(), s);
		buffers.nameToLower();
		identifier = buffers.name.get<StringReader>();
		identifier.trimChars<StringReader::WhiteSpace>();

		readCssValue<')'>(buffers.getValueStream(), s);
		buffers.valueToLower();
		value = buffers.value.get<StringReader>();
		value.trimChars<StringReader::WhiteSpace>();

		if (s.is(')')) {
			++s;
		}

		s.skipChars<Group<CharGroupId::WhiteSpace>>();

		if (identifier.empty() && value.empty()) {
			return q;
		} else {
			// log::source().verbose("document::StyleContainer", "@media (", identifier, ": ", value, ")");
			readStyleParameters(identifier, value, [&](StyleParameter &&param) {
				q.params.emplace_back(move(param));
				return true;
			});
		}

		if (s != "and") {
			return q;
		} else {
			s.skipString("and");
			s.skipChars<Group<CharGroupId::WhiteSpace>>();
		}
	}

	return q;
}

MediaQuery::Vector<MediaQuery::Query> StyleContainer::readMediaQueryList(StyleBuffers &buffers,
		StringReader &s) {
	MediaQuery::Vector<MediaQuery::Query> query;
	while (!s.empty()) {
		MediaQuery::Query q = readMediaQuery(buffers, s);
		if (!q.params.empty()) {
			query.push_back(sp::move(q));
		}

		s.skipChars<StringView::CharGroup<CharGroupId::WhiteSpace>>();
		if (!s.is(',')) {
			break;
		}
	}
	return query;
}

void StyleContainer::resolveNodeStyle(StyleList &style, const Node &node,
		const SpanView<const Node *> &stack, const MediaParameters &media,
		const SpanView<bool> &resolved) const {
	BufferTemplate<Interface> stringBuffer;

	// collect every matching simple rule with its specificity + source order, then apply in
	// CSS cascade order (ascending specificity, then source order). No `*`-quirk: universal
	// is just a specificity-0 rule that anything more specific overrides.
	Vector<MatchedRule> matches;
	auto add = [&](StringView key) {
		auto it = _styles.find(key);
		if (it != _styles.end()) {
			matches.push_back(MatchedRule{&it->second.style, resolved, _document->strings,
				it->second.specificity, it->second.order});
		}
	};

	add(StringView("*"));
	add(node.getHtmlName());
	for (auto &cl : node.getClasses()) {
		add(stringBuffer.resetWithStrings('.', cl));
		add(stringBuffer.resetWithStrings(node.getHtmlName(), '.', cl));
	}
	if (!node.getHtmlId().empty()) {
		add(stringBuffer.resetWithStrings('#', node.getHtmlId()));
		stringBuffer.clear();
		add(stringBuffer.resetWithStrings(node.getHtmlName(), '#', node.getHtmlId()));
	}

	sortMatchedRules(matches);
	for (auto &m : matches) { style.merge(*m.style, m.media); }
}

namespace {

static bool isSelSpace(char c) {
	return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
}

static bool isSelIdent(char c) {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_'
			|| c == '-';
}

// map a bare interactive or structural pseudo-class name to its predicate; false = unsupported
static bool applyPseudoClass(StringView name, StyleContainer::CompoundSelector &out) {
	using P = InteractiveFlags;
	using NthTest = StyleContainer::NthTest;
	if (name == "hover") {
		out.pseudoRequire |= uint32_t(P::Hover);
	} else if (name == "focus") {
		out.pseudoRequire |= uint32_t(P::Focus);
	} else if (name == "active") {
		out.pseudoRequire |= uint32_t(P::Active);
	} else if (name == "checked") {
		out.pseudoRequire |= uint32_t(P::Checked);
	} else if (name == "enabled") {
		out.pseudoRequire |= uint32_t(P::Enabled);
	} else if (name == "disabled") {
		out.pseudoForbid |= uint32_t(P::Enabled);
	} else if (name == "invalid") {
		out.pseudoRequire |= uint32_t(P::Invalid);
	} else if (name == "valid") {
		out.pseudoForbid |= uint32_t(P::Invalid);
	} else if (name == "read-only") {
		out.pseudoRequire |= uint32_t(P::ReadOnly);
	} else if (name == "read-write") {
		out.pseudoForbid |= uint32_t(P::ReadOnly);
	} else if (name == "indeterminate") {
		out.pseudoRequire |= uint32_t(P::Indeterminate);
	} else if (name == "required") {
		out.pseudoRequire |= uint32_t(P::Required);
	} else if (name == "optional") {
		out.pseudoForbid |= uint32_t(P::Required);
	} else if (name == "default") {
		out.pseudoRequire |= uint32_t(P::Default);
	} else if (name == "focus-visible") {
		out.pseudoRequire |= uint32_t(P::FocusVisible);
	} else if (name == "focus-within") {
		out.pseudoRequire |= uint32_t(P::FocusWithin);
	} else if (name == "selected") {
		out.pseudoRequire |= uint32_t(P::Selected);
	} else if (name == "selection-within") {
		out.pseudoRequire |= uint32_t(P::SelectionWithin);
	} else if (name == "root") {
		out.requireRoot = true;
	} else if (name == "empty") {
		out.requireEmpty = true;
	} else if (name == "first-child") {
		out.nth.emplace_back(NthTest{0, 1, false, false});
	} else if (name == "last-child") {
		out.nth.emplace_back(NthTest{0, 1, true, false});
	} else if (name == "only-child") {
		out.nth.emplace_back(NthTest{0, 1, false, false});
		out.nth.emplace_back(NthTest{0, 1, true, false});
	} else if (name == "first-of-type") {
		out.nth.emplace_back(NthTest{0, 1, false, true});
	} else if (name == "last-of-type") {
		out.nth.emplace_back(NthTest{0, 1, true, true});
	} else if (name == "only-of-type") {
		out.nth.emplace_back(NthTest{0, 1, false, true});
		out.nth.emplace_back(NthTest{0, 1, true, true});
	} else {
		// pseudo-elements and everything not listed above are unsupported
		return false;
	}
	return true;
}

// CSS An+B: `odd`, `even`, `<int>`, `n`, `-n`, `An`, `An+B`, `An-B`, with optional spaces
// around the sign (the selector reader normalizes runs of whitespace to a single space)
static bool parseAnPlusB(StringView arg, int32_t &a, int32_t &b) {
	arg.trimChars<StringView::WhiteSpace>();
	if (arg.empty()) {
		return false;
	}
	if (arg == "odd") {
		a = 2;
		b = 1;
		return true;
	}
	if (arg == "even") {
		a = 2;
		b = 0;
		return true;
	}

	size_t i = 0;
	const size_t n = arg.size();
	auto skipSpace = [&] {
		while (i < n && isSelSpace(arg[i])) { ++i; }
	};
	// a signed integer; `hasDigits` reports whether any digit was actually present, `sign` is
	// reported separately because `-n` carries a sign with no digits at all
	int32_t sign = 1;
	auto readSigned = [&](int32_t &out, bool &hasDigits) {
		sign = 1;
		if (i < n && (arg[i] == '+' || arg[i] == '-')) {
			sign = (arg[i] == '-') ? -1 : 1;
			++i;
			skipSpace();
		}
		int32_t value = 0;
		hasDigits = false;
		while (i < n && arg[i] >= '0' && arg[i] <= '9') {
			value = value * 10 + (arg[i] - '0');
			hasDigits = true;
			++i;
		}
		out = sign * value;
	};

	bool digits = false;
	int32_t first = 0;
	readSigned(first, digits);
	const int32_t firstSign = sign;
	skipSpace();

	if (i >= n || (arg[i] != 'n' && arg[i] != 'N')) {
		// a plain integer: `:nth-child(3)` == 0n+3
		if (!digits || i != n) {
			return false;
		}
		a = 0;
		b = first;
		return true;
	}
	++i; // consume 'n'
	// no digits before 'n' means a coefficient of 1, signed as written (`n`, `-n`, `+n`)
	a = digits ? first : firstSign;
	skipSpace();
	if (i == n) {
		b = 0;
		return true;
	}
	if (arg[i] != '+' && arg[i] != '-') {
		return false;
	}
	bool tailDigits = false;
	readSigned(b, tailDigits);
	skipSpace();
	return tailDigits && i == n;
}

// a functional pseudo-class `name(arg)`; false = unsupported or ill-formed argument
/* Parse one argument of `:not()`/`:is()`/`:where()`: a compound and nothing else.

Refuses what the restriction excludes - a combinator (`:is(.a .b)`), a nested functional pseudo-class
(`:is(:not(.a))`), a structural one (`:is(:first-child)`) - and refusing is the whole answer: the
caller drops the selector, exactly as it does for a pseudo-element or an attribute selector. Half-
applying an argument nobody can express would be the one outcome worse than not supporting it. */
static bool parseSelectorArg(StringView t, StyleContainer::SelectorArg &out) {
	uint32_t idC = 0, clsC = 0, typeC = 0;
	bool hasTag = false;
	size_t j = 0;
	while (j < t.size()) {
		const char c = t[j];
		if (isSelSpace(c) || c == '>' || c == '+' || c == '~' || c == ',') {
			return false; // a combinator or a list: the argument is ONE compound
		} else if (c == '*') {
			out.universal = true;
			++j;
		} else if (c == '#' || c == '.') {
			++j;
			const size_t st = j;
			while (j < t.size() && isSelIdent(t[j])) { ++j; }
			if (j == st) {
				return false;
			}
			if (c == '#') {
				out.id.assign(t.data() + st, j - st);
				++idC;
			} else {
				out.classes.emplace_back(StyleContainer::String(t.data() + st, j - st));
				++clsC;
			}
		} else if (c == ':') {
			++j;
			if (j < t.size() && t[j] == ':') {
				return false; // pseudo-element
			}
			const size_t st = j;
			while (j < t.size() && isSelIdent(t[j])) { ++j; }
			if (j == st || (j < t.size() && t[j] == '(')) {
				return false; // nothing, or a functional pseudo-class: no nesting here
			}

			// Interactive states only. A structural one would have to reach the sibling pass, which
			// an argument has no way of doing.
			StyleContainer::CompoundSelector probe;
			if (!applyPseudoClass(StringView(t.data() + st, j - st), probe)) {
				return false;
			}
			if (probe.requireRoot || probe.requireEmpty || !probe.nth.empty()) {
				return false;
			}
			out.pseudoRequire |= probe.pseudoRequire;
			out.pseudoForbid |= probe.pseudoForbid;
			clsC += uint32_t(__builtin_popcount(probe.pseudoRequire))
					+ uint32_t(__builtin_popcount(probe.pseudoForbid));
		} else if (isSelIdent(c)) {
			const size_t st = j;
			while (j < t.size() && isSelIdent(t[j])) { ++j; }
			if (hasTag) {
				return false;
			}
			hasTag = true;
			out.tag.assign(t.data() + st, j - st);
			++typeC;
		} else {
			return false;
		}
	}

	if (j == 0) {
		return false; // empty argument: `:is()` matches nothing, and saying so is not the same as
		// matching everything
	}

	out.specificity = StyleContainer::packSpecificity(idC, clsC, typeC);
	return true;
}

// Does this argument test ONLY interactive bits? Such a `:not()` folds into pseudoForbid and needs
// no negation entry at all - which is what keeps `:not(:hover)` as cheap as `:disabled`.
static bool argIsPseudoOnly(const StyleContainer::SelectorArg &arg) {
	return arg.tag.empty() && arg.id.empty() && arg.classes.empty() && !arg.universal;
}

static bool applyFunctionalPseudoClass(StringView name, StringView arg,
		StyleContainer::CompoundSelector &out) {
	bool fromEnd = false;
	bool ofType = false;
	if (name == "nth-child") {
	} else if (name == "nth-last-child") {
		fromEnd = true;
	} else if (name == "nth-of-type") {
		ofType = true;
	} else if (name == "nth-last-of-type") {
		fromEnd = ofType = true;
	} else if (name == "not" || name == "is" || name == "where") {
		const bool isNot = (name == "not");
		const bool zeroSpec = (name == "where");

		StyleContainer::SelectorMatchAny anyOf;
		uint32_t bestId = 0, bestClass = 0, bestType = 0, bestPacked = 0;
		bool empty = true;

		bool ok = true;
		StyleContainer::splitSelectorList(arg, [&](StringView part) {
			if (!ok) {
				return;
			}
			part.trimChars<StyleContainer::Group<CharGroupId::WhiteSpace>>();

			StyleContainer::SelectorArg parsed;
			if (!parseSelectorArg(part, parsed)) {
				ok = false;
				return;
			}
			empty = false;

			// `:not()` takes its argument's specificity; `:is()` the LARGEST of its own. Both are
			// compared packed, so the triple that wins is the triple that is added.
			if (parsed.specificity > bestPacked) {
				bestPacked = parsed.specificity;
				bestId = (parsed.specificity >> 16) & 0xFFu;
				bestClass = (parsed.specificity >> 8) & 0xFFu;
				bestType = parsed.specificity & 0xFFu;
			}

			if (isNot) {
				if (argIsPseudoOnly(parsed)) {
					// `:not(:hover)` is the forbid mask the matcher already has
					out.pseudoForbid |= parsed.pseudoRequire;
					out.pseudoRequire |= parsed.pseudoForbid;
				} else {
					out.negations.emplace_back(sp::move(parsed));
				}
			} else {
				anyOf.options.emplace_back(sp::move(parsed));
			}
		});

		if (!ok || empty) {
			return false;
		}

		if (isNot) {
			// A comma-separated `:not(a, b)` is "neither a nor b", so every part is added and the
			// specificity is still the largest one.
		} else {
			anyOf.zeroSpecificity = zeroSpec;
			out.anyOf.emplace_back(sp::move(anyOf));
		}

		if (!zeroSpec) {
			out.extraId += bestId;
			out.extraClass += bestClass;
			out.extraType += bestType;
		}
		return true;
	} else {
		return false;
	}

	int32_t a = 0, b = 0;
	if (!parseAnPlusB(arg, a, b)) {
		return false;
	}
	out.nth.emplace_back(StyleContainer::NthTest{a, b, fromEnd, ofType});
	return true;
}

// parse a single compound run ('*', tag, '.class'..., '#id', ':pseudo'...) into its parts
static bool parseCompound(StringView t, StyleContainer::CompoundSelector &out) {
	bool hasTag = false;
	size_t j = 0;
	while (j < t.size()) {
		char c = t[j];
		if (c == '*') {
			out.universal = true;
			++j;
		} else if (c == '#') {
			++j;
			size_t s = j;
			while (j < t.size() && isSelIdent(t[j])) { ++j; }
			if (j == s || !out.id.empty()) {
				return false;
			}
			out.id.assign(t.data() + s, j - s);
		} else if (c == '.') {
			++j;
			size_t s = j;
			while (j < t.size() && isSelIdent(t[j])) { ++j; }
			if (j == s) {
				return false;
			}
			out.classes.emplace_back(StyleContainer::String(t.data() + s, j - s));
		} else if (c == ':') {
			++j;
			if (j < t.size() && t[j] == ':') {
				return false; // '::' pseudo-element - unsupported
			}
			size_t s = j;
			while (j < t.size() && isSelIdent(t[j])) { ++j; }
			if (j == s) {
				return false;
			}
			auto name = StringView(t.data() + s, j - s);
			if (j < t.size() && t[j] == '(') {
				// functional form `:name(arg)`; the argument runs to the matching paren
				size_t argStart = ++j;
				uint32_t depth = 1;
				while (j < t.size() && depth > 0) {
					if (t[j] == '(') {
						++depth;
					} else if (t[j] == ')') {
						--depth;
					}
					++j;
				}
				if (depth != 0) {
					return false; // unbalanced
				}
				auto arg = StringView(t.data() + argStart, (j - 1) - argStart);
				if (!applyFunctionalPseudoClass(name, arg, out)) {
					return false;
				}
			} else if (!applyPseudoClass(name, out)) {
				return false;
			}
		} else if (isSelIdent(c)) {
			size_t s = j;
			while (j < t.size() && isSelIdent(t[j])) { ++j; }
			if (hasTag) {
				return false;
			}
			hasTag = true;
			out.tag.assign(t.data() + s, j - s);
		} else {
			return false;
		}
	}
	return true;
}

} // namespace

uint64_t StyleContainer::selectorTokenBit(uint32_t kind, StringView token) {
	// FNV-1a over the token, seeded with `kind` so tag/class/id namespaces don't collide
	uint64_t h = 1'469'598'103'934'665'603ull;
	h ^= (kind + 1u);
	h *= 1'099'511'628'211ull;
	for (size_t i = 0; i < token.size(); ++i) {
		h ^= static_cast<unsigned char>(token[i]);
		h *= 1'099'511'628'211ull;
	}
	return uint64_t(1) << (h & 63u);
}

uint32_t StyleContainer::packSpecificity(uint32_t idCount, uint32_t classCount,
		uint32_t typeCount) {
	auto clamp8 = [](uint32_t v) -> uint32_t { return v > 0xFFu ? 0xFFu : v; };
	return (clamp8(idCount) << 16) | (clamp8(classCount) << 8) | clamp8(typeCount);
}

uint32_t StyleContainer::specificityOfSimpleKey(StringView key) {
	// simple keys are one compound: '*', 'tag', '.cls', 'tag.cls', '#id', 'tag#id', 'tag.a.b#id'
	if (key.empty() || key == "*") {
		return 0;
	}
	uint32_t id = 0, cls = 0, type = 0;
	for (size_t i = 0; i < key.size(); ++i) {
		char c = key[i];
		if (c == '#') {
			++id;
		} else if (c == '.') {
			++cls;
		}
	}
	char c0 = key[0];
	if ((c0 >= 'a' && c0 <= 'z') || (c0 >= 'A' && c0 <= 'Z') || c0 == '_') {
		type = 1; // a leading identifier char = a type/tag compound
	}
	return packSpecificity(id, cls, type);
}

void StyleContainer::splitSelectorList(StringView sel, const Callback<void(StringView)> &cb) {
	size_t start = 0;
	uint32_t paren = 0, bracket = 0;
	char quote = 0;
	for (size_t i = 0; i < sel.size(); ++i) {
		const char c = sel[i];
		if (quote != 0) {
			if (c == quote) {
				quote = 0;
			}
		} else if (c == '\'' || c == '"') {
			quote = c;
		} else if (c == '(') {
			++paren;
		} else if (c == ')') {
			if (paren > 0) {
				--paren;
			}
		} else if (c == '[') {
			++bracket;
		} else if (c == ']') {
			if (bracket > 0) {
				--bracket;
			}
		} else if (c == ',' && paren == 0 && bracket == 0) {
			cb(StringView(sel.data() + start, i - start));
			start = i + 1;
		}
	}
	cb(StringView(sel.data() + start, sel.size() - start));
}

bool StyleContainer::selectorNeedsStructured(StringView sel) {
	uint32_t tokens = 0; // '.' and '#' qualifiers past the (optional) leading tag
	for (size_t i = 0; i < sel.size(); ++i) {
		char c = sel[i];
		// combinator (space/>/+/~) or an interactive pseudo-class (':') - the simple string
		// keys can express neither, so route the whole selector to the structured path
		if (isSelSpace(c) || c == '>' || c == '+' || c == '~' || c == ':') {
			return true;
		}
		if (c == '.' || c == '#') {
			++tokens;
		}
	}
	// The simple-key lookup (StyleSheet::collectMatches) only ever builds "tag", ".class",
	// "tag.class", "#id" and "tag#id" - one qualifier at most. A compound carrying two or more
	// ('.a.b', 'tag.a.b', '#a.b') has a key nothing looks up, so it would silently never match.
	// Hand those to the structured path, which matches compounds token by token.
	return tokens > 1;
}

void StyleContainer::addComplexSelector(StringView sel, const StyleList &style) {
	struct SrcCompound {
		SelectorCombinator comb =
				SelectorCombinator::Descendant; // combinator preceding this compound
		StringView text;
	};

	Vector<SrcCompound> src;
	SelectorCombinator pending = SelectorCombinator::Descendant;
	bool havePending = false; // an explicit '>'/'+'/'~' was seen since the last compound
	bool sawSpace = false;

	const size_t n = sel.size();
	size_t i = 0;
	while (i < n) {
		char c = sel[i];
		if (isSelSpace(c)) {
			sawSpace = true;
			++i;
			continue;
		}
		if (c == '>' || c == '+' || c == '~') {
			pending = (c == '>') ? SelectorCombinator::Child
					: (c == '+') ? SelectorCombinator::AdjacentSibling
								 : SelectorCombinator::GeneralSibling;
			havePending = true;
			sawSpace = false;
			++i;
			continue;
		}

		// start of a compound run ('*', tag, '.class', '#id', ':pseudo', ':pseudo(arg)').
		// Inside a functional argument everything is consumed verbatim, spaces included, so
		// `:nth-child(2n + 1)` stays one compound.
		size_t start = i;
		uint32_t argDepth = 0;
		while (i < n) {
			char d = sel[i];
			if (d == '(') {
				++argDepth;
				++i;
			} else if (d == ')') {
				if (argDepth == 0) {
					break;
				}
				--argDepth;
				++i;
			} else if (argDepth > 0) {
				++i;
			} else if (d == '*' || d == '.' || d == '#' || d == ':' || isSelIdent(d)) {
				++i;
			} else {
				break;
			}
		}
		if (argDepth != 0) {
			log::source().verbose("document::StyleContainer",
					"Unbalanced parentheses in selector, skipped: '", sel, "'");
			return;
		}
		if (i == start) {
			// unsupported character ('[', '(', ...) - not representable, skip the rule
			log::source().verbose("document::StyleContainer", "Unsupported selector, skipped: '",
					sel, "'");
			return;
		}

		SrcCompound sc;
		sc.text = StringView(sel.data() + start, i - start);
		if (src.empty()) {
			sc.comb = SelectorCombinator::Descendant; // first compound: no leading combinator
		} else if (havePending) {
			sc.comb = pending;
		} else if (sawSpace) {
			sc.comb = SelectorCombinator::Descendant;
		} else {
			log::source().verbose("document::StyleContainer", "Ill-formed selector, skipped: '",
					sel, "'");
			return;
		}
		src.emplace_back(sc);
		havePending = false;
		sawSpace = false;
		pending = SelectorCombinator::Descendant;
	}

	if (havePending || src.empty()) {
		// trailing combinator, or nothing parsed - nothing to index here
		return;
	}

	// build compounds RIGHT-TO-LEFT: compounds[k] = src[cnt-1-k]
	ComplexSelector cs;
	const size_t cnt = src.size();
	cs.compounds.resize(cnt);
	for (size_t k = 0; k < cnt; ++k) {
		if (!parseCompound(src[cnt - 1 - k].text, cs.compounds[k])) {
			log::source().verbose("document::StyleContainer", "Ill-formed compound, skipped: '",
					sel, "'");
			return;
		}
		if (k >= 1) {
			// relation of compounds[k] to compounds[k-1] is the combinator preceding src[cnt-k]
			cs.compounds[k].combinator = src[cnt - k].comb;
		}
	}

	if (cnt == 1 && !cs.compounds[0].hasPredicates()
			&& cs.compounds[0].classes.size() + (cs.compounds[0].id.empty() ? 0u : 1u) <= 1u) {
		// a single plain compound with at most one qualifier ('tag', '.a', 'tag.a', '#a',
		// 'tag#a') belongs in the simple-key store, not here - shouldn't happen
		// (selectorNeedsStructured wouldn't route it), skip safely. Anything richer ('.a.b',
		// '#a.b') has no simple key the matcher ever builds, so it must stay here.
		return;
	}

	// CSS specificity: a = #id count, b = .class + :pseudo count, c = type/tag count
	{
		uint32_t idC = 0, clsC = 0, typeC = 0;
		for (auto &comp : cs.compounds) {
			if (!comp.id.empty()) {
				++idC;
			}
			clsC += uint32_t(comp.classes.size()) + comp.countPseudoSpecificity();
			if (!comp.universal && !comp.tag.empty()) {
				++typeC;
			}

			// `:not()` and `:is()` contribute a full triple rather than "one more class", and
			// `:where()` contributes nothing at all - which is what makes a base layer written with
			// it lose to every application rule.
			idC += comp.extraId;
			clsC += comp.extraClass;
			typeC += comp.extraType;
			if (!comp.nth.empty() || comp.requireEmpty || comp.requireRoot) {
				// consumers gate sibling-order invalidation on this
				_hasStructuralSelectors = true;
			}
		}
		cs.specificity = packSpecificity(idC, clsC, typeC);
	}

	// Bloom bits over every ancestor-axis (Descendant/Child) compound's required tokens
	for (size_t k = 1; k < cnt; ++k) {
		const auto &comp = cs.compounds[k];
		if (comp.combinator == SelectorCombinator::Descendant
				|| comp.combinator == SelectorCombinator::Child) {
			if (!comp.tag.empty()) {
				cs.ancestorFilterBits |= selectorTokenBit(0, comp.tag);
			}
			if (!comp.id.empty()) {
				cs.ancestorFilterBits |= selectorTokenBit(2, comp.id);
			}
			for (auto &cl : comp.classes) { cs.ancestorFilterBits |= selectorTokenBit(1, cl); }
		}
	}

	// bucket by the most specific token of the target (rightmost) compound
	const auto &target = cs.compounds[0];
	String bucketKey;
	if (!target.id.empty()) {
		bucketKey.assign("#");
		bucketKey.append(target.id.data(), target.id.size());
	} else if (!target.classes.empty()) {
		bucketKey.assign(".");
		bucketKey.append(target.classes.front().data(), target.classes.front().size());
	} else if (!target.tag.empty()) {
		bucketKey.assign(target.tag.data(), target.tag.size());
	} else {
		bucketKey.assign("*");
	}

	cs.style = style;

	String text = sel.str<Interface>();
	auto it = _complexSelectors.find(text);
	if (it != _complexSelectors.end()) {
		it->second.style.merge(style); // duplicate selector text: merge declarations
		it->second.order = _ruleOrderCounter++; // later occurrence wins the tie-break
		return;
	}

	cs.order = _ruleOrderCounter++;
	auto res = _complexSelectors.emplace(sp::move(text), sp::move(cs));

	auto bit = _complexStyles.find(bucketKey);
	if (bit == _complexStyles.end()) {
		bit = _complexStyles.emplace(sp::move(bucketKey), Vector<ComplexSelector *>()).first;
	}
	bit->second.emplace_back(&res.first->second);
}

void StyleContainer::import(StringReader &r) {
	log::source().debug("document::StyleContainer", "Import is not implemented: ", r);
}

void StyleContainer::readStyleParameters(const StringView &name, const StringView &value,
		const StyleCallback &cb) {
	switch (_type) {
	case StyleType::Css:
		readCssParameter(name, value, cb,
				[&](const StringView &str) -> StringId { return _document->addString(str); });
	}
}

void StyleContainer::readStyleDeclaration(StyleList &target, StringView name, StringView value,
		MediaQueryId mediaQuery, StyleRule rule) {
	if (name.starts_with("--")) {
		// a custom property: keep the raw text, there is nothing to parse until it is used
		_hasCustomProperties = true;
		target.custom.emplace_back(StyleList::CustomProperty{_document->addString(name),
			_document->addString(value), mediaQuery, rule});
		return;
	}

	if (value.find("var(") != maxOf<size_t>()) {
		// the value depends on custom properties, which are only known per element - defer the
		// whole declaration to resolve time (see StyleList::PendingParameter)
		_hasCustomProperties = true;
		target.pending.emplace_back(StyleList::PendingParameter{_document->addString(name),
			_document->addString(value), mediaQuery, rule});
		return;
	}

	readStyleParameters(name, value, [&](StyleParameter &&param) {
		param.rule = rule;
		param.mediaQuery = mediaQuery;
		target.data.emplace_back(move(param));
		return true;
	});
}

} // namespace stappler::document
