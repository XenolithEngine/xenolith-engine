/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
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

#ifndef XENOLITH_FONT_XLFONTLOCALE_H_
#define XENOLITH_FONT_XLFONTLOCALE_H_

#include "XLEvent.h"
#include "XLFontConfig.h"
#include "SPString.h"
#include "SPLocaleInfo.h"
#include "SPMetastring.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-string-literal-operator-template"
#endif

// full localized string
template <typename CharType, CharType... Chars>
auto operator""_locale() {
	return metastring::merge("@Locale:"_meta, metastring::metastring<Chars...>());
}

#ifdef __clang__
#pragma clang diagnostic pop
#endif

// localized token
inline String operator""_token(const char *str, std::size_t len) {
	String ret;
	ret.reserve(len + 2);
	ret.append("%");
	ret.append(str, len);
	ret.append("%");
	return ret;
}

inline String localeIndex(size_t idx) {
	String ret;
	ret.reserve(20);
	ret.append("%=");
	ret.append(toString(idx));
	ret.push_back('%');
	return ret;
}

template <size_t Index>
inline constexpr auto localeIndex() {
	return metastring::merge("%="_meta, metastring::numeric<Index>(), "%"_meta);
}

} // namespace stappler::xenolith

namespace STAPPLER_VERSIONIZED stappler::xenolith::locale {

// Locale module implements system for the locale tags resolution
// Tag can be a string or an index
//
// String, prefixed with '@Locale:' interpreted as string-key accessor.
// Whole string after prefix is the key in localization table.
// So, string "@Locale:LOCALE_KEY" will be replaced with the string,
// that was defined for the key "LOCALE_KEY"
//
// Within string, locale tag defined as %LOCALE_TAG%
// This tag can contain latin symbols (a-z, A-Z), numbers (0-9)
// and some special symbols (in quotes): ':', '.', '-', '_', '[', ']', '+', '='
//
// Tag %=<number>% (like %=1234%) can be used to insert localized string by numeric index
//
// Tag $?<number>:<key>% works with special form of localization definition.
// You can define a list of words as one term like "WORD_ONE:WORD_TWO:WORD_THREE"
// When you define tag like $?<number>:<key>%, definition with this <key> assumed
// to be word-list definition, and <number> defines specific word within it.
// e.g. for definition NUMBER_LIST = "ONE:TWO:THREE:FOUR", after substitution
// "2 is %=2:NUMBER_LIST%" become "2 is TWO".
// It's useful when some term has variadic spelling on some condition
//

// Locale can be set in POSIX format (en_US.utf8, [language[_territory][.codeset]]) or
// LOWERCASE XML format (en-us, [language]-[subscript])
//
// For definitions (`define`) you should always use LOWERCASE XML format.


using LocaleInitList = std::initializer_list<Pair<StringView, StringView>>;
using LocaleIndexList = std::initializer_list<Pair<uint32_t, StringView>>;

enum class TimeTokens {
	Today = 0,
	Yesterday,
	Jan,
	Feb,
	Mar,
	Apr,
	Nay,
	Jun,
	jul,
	Aug,
	Sep,
	Oct,
	Nov,
	Dec,
	Max
};

//Event: Locale was changed
extern EventHeader onLocale;

// Defines key-value pairs for locale-based substitutuion, locale must be an lowercased XML land-territory pair
SP_PUBLIC void define(const StringView &locale, LocaleInitList &&);

// Defines index-value pairs for locale-based substitutuion, locale must be an lowercased XML land-territory pair
SP_PUBLIC void define(const StringView &locale, LocaleIndexList &&);
SP_PUBLIC void define(const StringView &locale,
		const std::array<StringView, toInt(TimeTokens::Max)> &);

// Set locale for the default lookup (if none were found for primary locale)
SP_PUBLIC void setDefault(StringView);

// Returls default locale as it's used by definition lookups
SP_PUBLIC StringView getDefault();

// Returns default locale in POSIX format [language[_territory][.codeset]]
SP_PUBLIC LocaleInfo getDefaultInfo();

// Set lookup locale
// Produces onLocale event if successful
SP_PUBLIC void setLocale(StringView);

// Returls locale as it's used by definition lookups
SP_PUBLIC StringView getLocale();

// Returns default locale in POSIX format [language[_territory][.codeset]]
SP_PUBLIC LocaleInfo getLocaleInfo();

SP_PUBLIC WideStringView string(const WideStringView &);
SP_PUBLIC WideStringView string(size_t);

template <char... Chars>
SP_PUBLIC WideStringView string(const metastring::metastring<Chars...> &str) {
	return string(WideStringView(str.to_std_ustring()));
}

SP_PUBLIC WideStringView numeric(const WideStringView &, uint32_t);

template <char... Chars>
SP_PUBLIC WideStringView numeric(const metastring::metastring<Chars...> &str, uint32_t n) {
	return numeric(WideStringView(str.to_std_ustring()), n);
}

SP_PUBLIC bool hasLocaleTagsFast(const WideStringView &);
SP_PUBLIC bool hasLocaleTags(const WideStringView &);
SP_PUBLIC WideString resolveLocaleTags(const WideStringView &);

SP_PUBLIC StringView timeToken(TimeTokens);

SP_PUBLIC const std::array<memory::string, toInt(TimeTokens::Max)> &timeTokenTable();

SP_PUBLIC String localDate(Time);
SP_PUBLIC String localDate(const std::array<StringView, toInt(TimeTokens::Max)> &, Time);

} // namespace stappler::xenolith::locale

#endif /* XENOLITH_FONT_XLFONTLOCALE_H_ */
