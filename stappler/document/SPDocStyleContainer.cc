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
		*d = std::tolower(*d);
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
			*d = std::tolower(*d);
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

	while (!s.is<Q>()) {
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
		StyleContainer::StringReader &s) {
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
			readBracedBlock<'(', ')'>(out, s);
		} else if (s.is('[')) {
			readBracedBlock<'[', ']'>(out, s);
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

		readCssValue<F>(buffers.getValueStream(), s);
		buffers.valueToLower();
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

			blockStack.emplace_back(
					BlockData{_document->addQuery(MediaQuery{readMediaQueryList(buffers, s)}),
						blockStack.back().disabled});
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
					readStyleParameters(name, value, [&](StyleParameter &&param) {
						param.rule = rule;
						param.mediaQuery = blockStack.back().media;
						style.data.emplace_back(move(param));
						return true;
					});
				}
			});
		}

		if (!selector.empty() && !style.data.empty()) {
			string::split(selector, ",", [&](StringView r) {
				r.trimChars<StringView::WhiteSpace>();
				auto it = _styles.find(r);
				if (it == _styles.end()) {
					_styles.emplace(r.str<Interface>(), style);
				} else {
					it->second.merge(style);
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
		auto d = filesystem::readIntoMemory<memory::StandartInterface>(path);
		StringReader r((const char *)d.data(), d.size());
		return readStyle(r);
	}
	return false;
}

bool StyleContainer::readStyle(StyleList &target, StringReader &r) {
	StyleBuffers buffers;
	readCssStyleRules<'}'>(buffers, r,
			[&](const StringReader &name, const StringReader &value, StyleRule rule) {
		readStyleParameters(name, value, [&](StyleParameter &&param) {
			param.rule = rule;
			param.mediaQuery = MediaQueryIdNone;
			target.data.emplace_back(move(param));
			return true;
		});
	});
	return r.empty();
}

FontFace StyleContainer::readFontFace(StyleBuffers &buffers, StringReader &s) {
	if (s.is('{')) {
		++s;
	}

	FontFace ret;
	StringReader name, value, data;

	while (!s.is('}')) {
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
				if (!std::isnan(val)) {
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
							if (!std::isnan(val)) {
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

		readCssValue<')'>(buffers.getValueStream(), s);
		buffers.valueToLower();
		value = buffers.value.get<StringReader>();

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

	auto it = _styles.find("*");
	if (it != _styles.end()) {
		style.merge(it->second, resolved, true);
	}
	it = _styles.find(node.getHtmlName());
	if (it != _styles.end()) {
		style.merge(it->second, resolved);
	}

	for (auto &cl : node.getClasses()) {
		auto it = _styles.find(stringBuffer.resetWithStrings('.', cl));
		if (it != _styles.end()) {
			style.merge(it->second, resolved);
		}

		it = _styles.find(stringBuffer.resetWithStrings(node.getHtmlName(), '.', cl));
		if (it != _styles.end()) {
			style.merge(it->second, resolved);
		}
	}

	if (!node.getHtmlId().empty()) {
		it = _styles.find(stringBuffer.resetWithStrings('#', node.getHtmlId()));
		if (it != _styles.end()) {
			style.merge(it->second, resolved);
		}

		stringBuffer.clear();
		it = _styles.find(stringBuffer.resetWithStrings(node.getHtmlName(), '#', node.getHtmlId()));
		if (it != _styles.end()) {
			style.merge(it->second, resolved);
		}
	}
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

} // namespace stappler::document
