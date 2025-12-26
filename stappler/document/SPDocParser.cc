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

#include "SPDocParser.h"

namespace STAPPLER_VERSIONIZED stappler::document::parser {

using HtmlIdentifier = chars::Compose<char32_t, chars::Range<char32_t, u'0', u'9'>,
		chars::Range<char32_t, u'A', u'Z'>, chars::Range<char32_t, u'a', u'z'>,
		chars::Chars<char32_t, u'_', u'-', u'!', u'/', u':'> >;

template <char32_t First, char32_t Second>
using Range = chars::Range<char32_t, First, Second>;

template <char16_t... Args>
using Chars = chars::Chars<char32_t, Args...>;

template <CharGroupId G>
using Group = chars::CharGroup<char32_t, G>;

using SingleQuote = chars::Chars<char32_t, u'\''>;
using DoubleQuote = chars::Chars<char32_t, u'"'>;

void readQuotedString(StringReader &s, String &str, char quoted) {
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

String readHtmlTagName(StringReader &is) {
	auto s = is;
	s.skipUntil<HtmlIdentifier, Chars<'>'>>();
	String name(s.readChars<HtmlIdentifier, StringReader::Chars<'?'>>().str<Interface>());
	if (name.size() > 1 && name.back() == '/') {
		name.pop_back();
		is += (is.size() - s.size() - 1);
	} else {
		s.skipUntil<HtmlIdentifier, Chars<'>'>>();
		is = s;
	}
	string::apply_tolower_c(name);
	return name;
}

String readHtmlTagParamName(StringReader &s) {
	s.skipUntil<HtmlIdentifier>();
	String name(s.readChars<HtmlIdentifier>().str<Interface>());
	string::apply_tolower_c(name);
	return name;
}

String readHtmlTagParamValue(StringReader &s) {
	if (!s.is('=')) {
		s.skipUntil<HtmlIdentifier>();
		return "";
	}

	s++;
	char quoted = 0;
	if (s.is('"') || s.is('\'')) {
		quoted = s[0];
		s++;
	}

	String name;
	if (quoted) {
		readQuotedString(s, name, quoted);
	} else {
		name = s.readChars<HtmlIdentifier>().str<Interface>();
	}

	s.skipUntil<HtmlIdentifier, Chars<u'>'>>();
	return name;
}


bool readStyleMargin(const StringView &r, Metric &top, Metric &right, Metric &bottom,
		Metric &left) {
	StringReader ir(r);
	return readStyleMargin(ir, top, right, bottom, left);
}

bool readStyleMargin(StringReader &str, Metric &top, Metric &right, Metric &bottom, Metric &left) {
	Metric values[4];
	int count = 0;

	while (!str.empty()) {
		if (str.empty()) {
			return false;
		}
		if (!readStyleMetric(str, values[count])) {
			return false;
		}
		str.skipChars<StringView::CharGroup<CharGroupId::WhiteSpace>>();
		count++;
		if (count == 4) {
			break;
		}
	}

	if (count == 0) {
		return false;
	} else if (count == 1) {
		top = right = bottom = left = values[0];
	} else if (count == 2) {
		top = bottom = values[0];
		right = left = values[1];
	} else if (count == 3) {
		top = values[0];
		right = left = values[1];
		bottom = values[2];
	} else {
		top = values[0];
		right = values[1];
		bottom = values[2];
		left = values[3];
	}

	return true;
}

bool readStyleMetric(const StringView &r, Metric &value, bool resolutionMetric,
		bool allowEmptyMetric) {
	StringReader ir(r);
	return readStyleMetric(ir, value, resolutionMetric, allowEmptyMetric);
}

bool readStyleMetric(StringReader &r, Metric &value, bool resolutionMetric, bool allowEmptyMetric) {
	r.skipChars<StringView::CharGroup<CharGroupId::WhiteSpace>>();
	if (!resolutionMetric && r.starts_with("auto")) {
		r += 4;
		value.metric = Metric::Units::Auto;
		value.value = 0.0f;
		return true;
	}

	auto fRes = r.readFloat();
	if (!fRes.valid()) {
		return false;
	}

	auto fvalue = fRes.get();
	if (fvalue == 0.0f) {
		value.value = fvalue;
		value.metric = Metric::Units::Px;
		return true;
	}

	r.skipChars<StringView::CharGroup<CharGroupId::WhiteSpace>>();

	auto str = r.readUntil<StringView::CharGroup<CharGroupId::WhiteSpace>>();

	if (!resolutionMetric) {
		if (str.is('%')) {
			++str;
			value.value = fvalue / 100.0f;
			value.metric = Metric::Units::Percent;
			return true;
		} else if (str == "em") {
			str += 2;
			value.value = fvalue;
			value.metric = Metric::Units::Em;
			return true;
		} else if (str == "rem") {
			str += 3;
			value.value = fvalue;
			value.metric = Metric::Units::Rem;
			return true;
		} else if (str == "px") {
			str += 2;
			value.value = fvalue;
			value.metric = Metric::Units::Px;
			return true;
		} else if (str == "pt") {
			str += 2;
			value.value = fvalue * 4.0f / 3.0f;
			value.metric = Metric::Units::Px;
			return true;
		} else if (str == "pc") {
			str += 2;
			value.value = fvalue * 15.0f;
			value.metric = Metric::Units::Px;
			return true;
		} else if (str == "mm") {
			str += 2;
			value.value = fvalue * 3.543307f;
			value.metric = Metric::Units::Px;
			return true;
		} else if (str == "cm") {
			str += 2;
			value.value = fvalue * 35.43307f;
			value.metric = Metric::Units::Px;
			return true;
		} else if (str == "in") {
			str += 2;
			value.value = fvalue * 90.0f;
			value.metric = Metric::Units::Px;
			return true;
		} else if (str == "vw") {
			str += 2;
			value.value = fvalue;
			value.metric = Metric::Units::Vw;
			return true;
		} else if (str == "vh") {
			str += 2;
			value.value = fvalue;
			value.metric = Metric::Units::Vh;
			return true;
		} else if (str == "vmin") {
			str += 4;
			value.value = fvalue;
			value.metric = Metric::Units::VMin;
			return true;
		} else if (str == "vmax") {
			str += 4;
			value.value = fvalue;
			value.metric = Metric::Units::VMax;
			return true;
		}
	} else {
		if (str == "dpi") {
			str += 3;
			value.value = fvalue;
			value.metric = Metric::Units::Dpi;
			return true;
		} else if (str == "dpcm") {
			str += 4;
			value.value = fvalue / 2.54f;
			value.metric = Metric::Units::Dpi;
			return true;
		} else if (str == "dppx") {
			str += 4;
			value.value = fvalue;
			value.metric = Metric::Units::Dppx;
			return true;
		}
	}

	if (allowEmptyMetric) {
		value.value = fvalue;
		return true;
	}

	return false;
}

void readBracedString(StringReader &r, String &target) {
	r.skipUntil<StringReader::Chars<'('>>();
	if (r.is('(')) {
		++r;
		r.skipChars<Group<CharGroupId::WhiteSpace>>();
		if (r.is('\'') || r.is('"')) {
			auto q = r[0];
			++r;
			readQuotedString(r, target, q);
			r.skipUntil<StringReader::Chars<')'>>();
		} else {
			target = r.readUntil<StringReader::Chars<')'>>().str<Interface>();
		}
		if (r.is(')')) {
			++r;
		}
	}
}

/*void readFontFaceSrc(const String &src, Vector<FontFace::FontFaceSource> &vec) {
	StringReader r(src);
	String url, format, tech;
	while (!r.empty()) {
		url.clear();
		format.clear();
		checkCssComments(r);
		if (r.is("url") || r.is("local")) {
			bool local = (r.is("local"));
			readBracedString(r, url);

			auto tmp = StringView(url);
			tmp.trimChars<StringView::WhiteSpace>();

			if (local) {
				url = "local://" + tmp.str<Interface>();
			} else {
				if (tmp.size() != url.size()) {
					url = tmp.str<Interface>();
				}
			}
		}

		checkCssComments(r);
		if (r.is("format(")) {
			readBracedString(r, format);
			checkCssComments(r);
		}

		if (r.is("tech(")) {
			readBracedString(r, tech);
			checkCssComments(r);
		}

		if (!url.empty()) {
			vec.emplace_back(FontFace::FontFaceSource{move(url), move(format), move(tech)});
		}

		if (r.is(',')) {
			++ r;
		}
	}
}

void readFontFace(StringReader &s, ContentPage::FontMap &fonts) {
	String fontFamily;
	String src;
	String selector;
	StyleVec style;
	while (!s.empty() && !s.is('}') && !s.is('<')) {
		parser::readCssStyleValue<'<'>(s, '<', [&] (const String &name, const String &value) {
			SP_RTREADER_LOG("font-face tag: '%s' : '%s'", name.c_str(), value.c_str());
			if (name == "font-family") {
				fontFamily = value;
			} else if (name == "src") {
				if (src.empty()) {
					src = value;
				} else {
					src.append(", ").append(value);
				}
			} else {
				style.push_back(pair(name, value));
			}
		});
	}

	if (fontFamily.size() >= 2) {
		if ((fontFamily.front() == '\'' && fontFamily.back() == '\'') || (fontFamily.front() == '"' && fontFamily.back() == '"')) {
			fontFamily = fontFamily.substr(1, fontFamily.size() - 2);
		}
	}

	if (!fontFamily.empty() && !src.empty()) {
		style::FontFace face;
		readFontFaceSrc(src, face.src);

		auto it = fonts.find(fontFamily);
		if (it == fonts.end()) {
			it = fonts.emplace(fontFamily, Vector<style::FontFace>{}).first;
		}

		style::ParameterList list;
		list.read(style);
		for (const style::Parameter & val : list.data) {
			switch (val.name) {
			case style::ParameterName::FontWeight: face.fontWeight = val.value.fontWeight; break;
			case style::ParameterName::FontStyle: face.fontStyle = val.value.fontStyle; break;
			case style::ParameterName::FontStretch: face.fontStretch = val.value.fontStretch; break;
			default: break;
			}
		}
		it->second.push_back(sp::move(face));
	}
}*/


/*RefParser::RefParser(const String &content, const Callback &cb)
: ptr(content.c_str()), func(cb) {
	bool isStart = true;
	while (ptr[idx] != 0) {
		if (isStart && strncasecmp(ptr + idx, "www.", 4) == 0) {
			readRef("www.", 4);
		} else if (isStart && strncasecmp(ptr + idx, "http://", 7) == 0 ) {
			readRef("http://", 7);
		} else if (isStart && strncasecmp(ptr + idx, "https://", 8) == 0 ) {
			readRef("https://", 8);
		} else if (isStart && strncasecmp(ptr + idx, "mailto:", 7) == 0 ) {
			readRef("mailto:", 7);
		} else {
			isStart = string::isspace(ptr + idx);
			idx += unicode::utf8_length_data[((const uint8_t *)ptr)[idx]];
		}
	}

	if (func) {
		if (idx != 0) {
			func(String(ptr, idx), "");
		} else {
			func("", "");
		}
	}
}

void RefParser::readRef(const char *ref, size_t offset) {
	while(ptr[idx + offset] && !string::isspace(&ptr[idx + offset])) {
		offset ++;
	}
	if (func) {
		func(String(ptr, idx), String(ptr + idx, offset));
	}

	ptr += (idx + offset);
	idx = 0;
}*/

} // namespace stappler::document::parser
