/**
Copyright (c) 2016-2022 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

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

#include "SPHtmlParser.h"

namespace STAPPLER_VERSIONIZED stappler::html {

using HtmlIdentifier16 = sprt::chars::Compose<char16_t, sprt::chars::Range<char16_t, u'0', u'9'>,
		sprt::chars::Range<char16_t, u'A', u'Z'>, sprt::chars::Range<char16_t, u'a', u'z'>,
		sprt::chars::Chars<char16_t, u'_', u'-', u'!', u'/', u':'> >;

using HtmlIdentifier32 = sprt::chars::Compose<char32_t, sprt::chars::Range<char32_t, u'0', u'9'>,
		sprt::chars::Range<char32_t, u'A', u'Z'>, sprt::chars::Range<char32_t, u'a', u'z'>,
		sprt::chars::Chars<char32_t, u'_', u'-', u'!', u'/', u':'> >;

using HtmlIdentifier8 = sprt::chars::Compose<char, sprt::chars::Range<char, u'0', u'9'>,
		sprt::chars::Range<char, u'A', u'Z'>, sprt::chars::Range<char, u'a', u'z'>,
		sprt::chars::Chars<char, u'_', u'-', u'!', u'/', u':'> >;


template <>
SP_PUBLIC StringViewUtf8 Tag_readName<StringViewUtf8>(StringViewUtf8 &is) {
	StringViewUtf8 s = is;
	s.skipUntil<HtmlIdentifier32, StringViewUtf8::MatchChars<'>', '?'>>();
	if (s.is("!--")) {
		auto ret = StringViewUtf8(s, "!--"_len);
		s += "!--"_len;
		is = s;
		return ret;
	}
	StringViewUtf8 name(s.readChars<HtmlIdentifier32, StringViewUtf8::MatchChars<'?'>>());
	if (name.size() > 1 && name.back() == '/') {
		name.set(name.data(), name.size() - 1);
		is += (is.size() - s.size() - 1);
	} else {
		s.skipUntil<HtmlIdentifier32, StringViewUtf8::MatchChars<'>'>>();
		is = s;
	}
	return name;
}

template <>
SP_PUBLIC StringViewUtf8 Tag_readAttrName<StringViewUtf8>(StringViewUtf8 &s) {
	s.skipUntil<HtmlIdentifier32>();
	StringViewUtf8 name(s.readChars<HtmlIdentifier32>());
	return name;
}

template <>
SP_PUBLIC StringViewUtf8 Tag_readAttrValue<StringViewUtf8>(StringViewUtf8 &s) {
	s.skipChars<StringViewUtf8::WhiteSpace>();
	if (!s.is('=')) {
		s.skipUntil<HtmlIdentifier32>();
		return StringViewUtf8();
	}

	s++;
	char quoted = 0;
	if (s.is('"') || s.is('\'')) {
		quoted = s[0];
		s++;
		StringViewUtf8 tmp = s;
		while (!s.empty() && !s.is(quoted)) {
			if (quoted == '"') {
				s.skipUntil<StringViewUtf8::MatchChars<u'\\', u'"'>>();
			} else {
				s.skipUntil<StringViewUtf8::MatchChars<u'\\', u'\''>>();
			}
			if (s.is('\\')) {
				s += 2;
			}
		}

		StringViewUtf8 ret(tmp.data(), tmp.size() - s.size());
		if (s.is(quoted)) {
			s++;
		}
		s.skipUntil<HtmlIdentifier32, StringViewUtf8::MatchChars<'>'>>();
		return ret;
	}

	return s.readChars<HtmlIdentifier32>();
}


template <>
SP_PUBLIC StringView Tag_readName<StringView>(StringView &is) {
	StringView s = is;
	s.skipUntil<HtmlIdentifier8, StringView::MatchChars<'>', '?'>>();
	if (s.is("!--")) {
		auto ret = StringView(s, "!--"_len);
		s += "!--"_len;
		is = s;
		return ret;
	}
	StringView name(s.readChars<HtmlIdentifier8, StringView::MatchChars<'?'>>());
	if (name.size() > 1 && name.back() == '/') {
		name.set(name.data(), name.size() - 1);
		is += (is.size() - s.size() - 1);
	} else {
		s.skipUntil<HtmlIdentifier8, StringView::MatchChars<'>'>>();
		is = s;
	}
	return name;
}

template <>
SP_PUBLIC StringView Tag_readAttrName<StringView>(StringView &s) {
	s.skipUntil<HtmlIdentifier8>();
	StringView name(s.readChars<HtmlIdentifier8>());
	return name;
}

template <>
SP_PUBLIC StringView Tag_readAttrValue<StringView>(StringView &s) {
	s.skipChars<StringView::WhiteSpace>();
	if (!s.is('=')) {
		s.skipUntil<HtmlIdentifier8>();
		return StringView();
	}

	s++;
	char quoted = 0;
	if (s.is('"') || s.is('\'')) {
		quoted = s[0];
		s++;
		StringView tmp = s;
		while (!s.empty() && !s.is(quoted)) {
			if (quoted == '"') {
				s.skipUntil<StringView::MatchChars<'\\', '"'>>();
			} else {
				s.skipUntil<StringView::MatchChars<'\\', '\''>>();
			}
			if (s.is('\\')) {
				s += 2;
			}
		}

		StringView ret(tmp.data(), tmp.size() - s.size());
		if (s.is(quoted)) {
			s++;
		}
		s.skipUntil<HtmlIdentifier8, StringView::MatchChars<'>'>>();
		return ret;
	}

	return s.readChars<HtmlIdentifier8>();
}


template <>
SP_PUBLIC WideStringView Tag_readName<WideStringView>(WideStringView &is) {
	WideStringView s = is;
	s.skipUntil<HtmlIdentifier16, WideStringView::MatchChars<u'>', u'?'>>();
	if (s.is(u"!--")) {
		auto ret = WideStringView(s, u"!--"_len);
		s += u"!--"_len;
		is = s;
		return ret;
	}
	WideStringView name(s.readChars<HtmlIdentifier16, WideStringView::MatchChars<u'?'>>());
	if (name.size() > 1 && name.back() == '/') {
		name.set(name.data(), name.size() - 1);
		is += (is.size() - s.size() - 1);
	} else {
		s.skipUntil<HtmlIdentifier16, WideStringView::MatchChars<u'>'>>();
		is = s;
	}
	return name;
}

template <>
SP_PUBLIC WideStringView Tag_readAttrName<WideStringView>(WideStringView &s) {
	s.skipUntil<HtmlIdentifier16>();
	WideStringView name(s.readChars<HtmlIdentifier16>());
	return name;
}

template <>
SP_PUBLIC WideStringView Tag_readAttrValue<WideStringView>(WideStringView &s) {
	s.skipChars<WideStringView::WhiteSpace>();
	if (!s.is('=')) {
		s.skipUntil<HtmlIdentifier16>();
		return WideStringView();
	}

	s++;
	char16_t quoted = 0;
	if (s.is(u'"') || s.is(u'\'')) {
		quoted = s[0];
		s++;
		WideStringView tmp = s;
		while (!s.empty() && !s.is(quoted)) {
			if (quoted == '"') {
				s.skipUntil<WideStringView::MatchChars<u'\\', u'"'>>();
			} else {
				s.skipUntil<WideStringView::MatchChars<u'\\', u'\''>>();
			}
			if (s.is('\\')) {
				s += 2;
			}
		}

		WideStringView ret(tmp.data(), tmp.size() - s.size());
		if (s.is(quoted)) {
			s++;
		}
		s.skipUntil<HtmlIdentifier16, WideStringView::MatchChars<u'>'>>();
		return ret;
	}

	return s.readChars<HtmlIdentifier16>();
}

} // namespace stappler::html
