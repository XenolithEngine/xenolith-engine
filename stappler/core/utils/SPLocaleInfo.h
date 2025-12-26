/**
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

#include "SPString.h"

#ifndef STAPPLER_CORE_UTILS_SPLOCALEINFO_H_
#define STAPPLER_CORE_UTILS_SPLOCALEINFO_H_

namespace STAPPLER_VERSIONIZED stappler {

// Do not assume that StringViews in locale desription is null-terminated

struct LanguageInfo {
	static LanguageInfo get(StringView); // by code

	StringView name;
	StringView nameLocal;
	StringView code;
	StringView iso639_1;
	StringView iso639_2;
	StringView iso639_3;
	StringView countries; // ';' as separator
};

struct CountryInfo {
	static CountryInfo get(StringView); // by lowercased code

	StringView name;
	StringView nameLocal;
	StringView code; // without lowercasing
	StringView continent;
	StringView region;
	StringView capital;
	StringView currency;
	StringView currencyLocal;
	StringView currencyCode;
	StringView currencySymbol;
	StringView currencySubunit;
	StringView languages; // code with ';' as separator
	StringView flagSymbol;
	StringView timezones; // ';' as separator
	StringView borders; // ';' as separator
	StringView postalCodeFormat;
	StringView iso3166_1alpha2;
	StringView iso3166_1alpha3;
	StringView tld;
	StringView vehicleCode;
	StringView un_locode;
	uint32_t iso3166_1numeric = 0;
};

struct LocaleIdentifier {
	std::array<char, 16> data;
	StringView language;
	StringView country;
	StringView codeset;
	StringView id;
	bool valid = false;

	template <typename Interface>
	auto getPosixName() -> typename Interface::StringType;

	LocaleIdentifier() noexcept = default;
	LocaleIdentifier(StringView) noexcept;

	LocaleIdentifier(const LocaleIdentifier &) noexcept;
	LocaleIdentifier &operator=(const LocaleIdentifier &) noexcept;

	explicit operator bool() const noexcept { return valid; }

	bool operator==(const LocaleIdentifier &) const noexcept = default;
	bool operator!=(const LocaleIdentifier &) const noexcept = default;
};

struct LocaleInfo {
	// by POSIX or XML id
	static LocaleInfo get(StringView);

	static LocaleInfo get(LocaleIdentifier);

	LocaleIdentifier id;
	LanguageInfo language;
	CountryInfo country;
};

template <typename Interface>
auto LocaleIdentifier::getPosixName() -> typename Interface::StringType {
	if (codeset.empty()) {
		return string::toString<Interface>(language, "_",
				string::toupper<memory::StandartInterface>(country));
	} else {
		return string::toString<Interface>(language, "_",
				string::toupper<memory::StandartInterface>(country), ".", codeset);
	}
}

} // namespace STAPPLER_VERSIONIZED stappler

#endif // STAPPLER_CORE_UTILS_SPLOCALEINFO_H_
