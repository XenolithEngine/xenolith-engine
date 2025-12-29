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

#include "SPString.h" // IWYU pragma: keep
#include <sprt/runtime/unicode.h>

namespace STAPPLER_VERSIONIZED stappler::platform {

char32_t tolower(char32_t c) { return sprt::unicode::tolower(c); }

char32_t toupper(char32_t c) { return sprt::unicode::toupper(c); }

char32_t totitle(char32_t c) { return sprt::unicode::totitle(c); }

template <>
auto tolower<memory::PoolInterface>(StringView data) -> memory::PoolInterface::StringType {
	memory::PoolInterface::StringType ret;
	sprt::unicode::tolower([&](StringView str) { ret = str.str<memory::PoolInterface>(); }, data);
	return ret;
}

template <>
auto tolower<memory::StandartInterface>(StringView data) -> memory::StandartInterface::StringType {
	memory::StandartInterface::StringType ret;
	sprt::unicode::tolower([&](StringView str) { ret = str.str<memory::StandartInterface>(); },
			data);
	return ret;
}

template <>
auto toupper<memory::PoolInterface>(StringView data) -> memory::PoolInterface::StringType {
	memory::PoolInterface::StringType ret;
	sprt::unicode::toupper([&](StringView str) { ret = str.str<memory::PoolInterface>(); }, data);
	return ret;
}

template <>
auto toupper<memory::StandartInterface>(StringView data) -> memory::StandartInterface::StringType {
	memory::StandartInterface::StringType ret;
	sprt::unicode::toupper([&](StringView str) { ret = str.str<memory::StandartInterface>(); },
			data);
	return ret;
}

template <>
auto totitle<memory::PoolInterface>(StringView data) -> memory::PoolInterface::StringType {
	memory::PoolInterface::StringType ret;
	sprt::unicode::totitle([&](StringView str) { ret = str.str<memory::PoolInterface>(); }, data);
	return ret;
}

template <>
auto totitle<memory::StandartInterface>(StringView data) -> memory::StandartInterface::StringType {
	memory::StandartInterface::StringType ret;
	sprt::unicode::totitle([&](StringView str) { ret = str.str<memory::StandartInterface>(); },
			data);
	return ret;
}

template <>
auto tolower<memory::PoolInterface>(WideStringView data) -> memory::PoolInterface::WideStringType {
	memory::PoolInterface::WideStringType ret;
	sprt::unicode::tolower([&](WideStringView str) { ret = str.str<memory::PoolInterface>(); },
			data);
	return ret;
}

template <>
auto tolower<memory::StandartInterface>(WideStringView data)
		-> memory::StandartInterface::WideStringType {
	memory::StandartInterface::WideStringType ret;
	sprt::unicode::tolower([&](WideStringView str) { ret = str.str<memory::StandartInterface>(); },
			data);
	return ret;
}

template <>
auto toupper<memory::PoolInterface>(WideStringView data) -> memory::PoolInterface::WideStringType {
	memory::PoolInterface::WideStringType ret;
	sprt::unicode::toupper([&](WideStringView str) { ret = str.str<memory::PoolInterface>(); },
			data);
	return ret;
}

template <>
auto toupper<memory::StandartInterface>(WideStringView data)
		-> memory::StandartInterface::WideStringType {
	memory::StandartInterface::WideStringType ret;
	sprt::unicode::toupper([&](WideStringView str) { ret = str.str<memory::StandartInterface>(); },
			data);
	return ret;
}

template <>
auto totitle<memory::PoolInterface>(WideStringView data) -> memory::PoolInterface::WideStringType {
	memory::PoolInterface::WideStringType ret;
	sprt::unicode::totitle([&](WideStringView str) { ret = str.str<memory::PoolInterface>(); },
			data);
	return ret;
}

template <>
auto totitle<memory::StandartInterface>(WideStringView data)
		-> memory::StandartInterface::WideStringType {
	memory::StandartInterface::WideStringType ret;
	sprt::unicode::totitle([&](WideStringView str) { ret = str.str<memory::StandartInterface>(); },
			data);
	return ret;
}

int compare_u(StringView l, StringView r) {
	int result = 0;
	if (sprt::unicode::compare(l, r, &result)) {
		return result;
	}
	return sprt::detail::compare_c(l, r);
}

int compare_u(WideStringView l, WideStringView r) {
	int result = 0;
	if (sprt::unicode::compare(l, r, &result)) {
		return result;
	}
	return sprt::detail::compare_c(l, r);
}

int caseCompare_u(StringView l, StringView r) {
	int result = 0;
	if (sprt::unicode::caseCompare(l, r, &result)) {
		return result;
	}
	return sprt::detail::caseCompare_c(l, r);
}

int caseCompare_u(WideStringView l, WideStringView r) {
	int result = 0;
	if (sprt::unicode::caseCompare(l, r, &result)) {
		return result;
	}
	return sprt::detail::caseCompare_c(l, r);
}

} // namespace stappler::platform
