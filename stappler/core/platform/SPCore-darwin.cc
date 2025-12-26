/**
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

#include "SPStringView.h"
#include "SPString.h"

#if MACOS

#include <CoreFoundation/CFString.h>
#include <sys/random.h>

namespace STAPPLER_VERSIONIZED stappler::platform {

static char32_t convertChar(char32_t c, const Callback<void(CFMutableStringRef, CFLocaleRef)> &cb) {
	auto locale = CFLocaleCopyCurrent();
	UniChar buf[4] = {0};
	char32_t buf2[2];
	auto num = unicode::utf16EncodeBuf((char16_t *)buf, c);
	auto str = CFStringCreateMutable(nullptr, 2);
	CFStringAppendCharacters(str, buf, num);
	cb(str, locale);
	CFStringLowercase(str, locale);

	auto bytes = CFStringGetCStringPtr(str, kCFStringEncodingUTF32);

	if (bytes == NULL) {
		if (CFStringGetCString(str, (char *)buf2, 2 * sizeof(char32_t), kCFStringEncodingUTF32)) {
			c = buf2[0];
		}
	} else {
		c = ((char32_t *)bytes)[0];
	}

	CFRelease(str);
	CFRelease(locale);
	return c;
}

static CFMutableStringRef makeString(WideStringView str) {
	auto ret = CFStringCreateMutable(nullptr, str.size());
	CFStringAppendCharacters(ret, (UniChar *)str.data(), str.size());
	return ret;
}

static CFMutableStringRef makeString(StringViewUtf8 str) {
	auto ret = CFStringCreateMutable(nullptr, str.code_size());
	UniChar buf[4] = {0};
	str.foreach ([&](char32_t c) {
		auto num = unicode::utf16EncodeBuf((char16_t *)buf, c);
		CFStringAppendCharacters(ret, buf, num);
	});
	return ret;
}

static memory::PoolInterface::StringType toPoolString(CFMutableStringRef str) {
	auto len = CFStringGetLength(str);
	auto bytes = (const char16_t *)CFStringGetCStringPtr(str, kCFStringEncodingUTF16);

	if (bytes == NULL) {
		char16_t buf[len + 1];
		if (CFStringGetCString(str, (char *)buf, len * sizeof(char32_t), kCFStringEncodingUTF16)) {
			return string::toUtf8<memory::PoolInterface>(WideStringView(buf, len));
		}
	} else {
		return string::toUtf8<memory::PoolInterface>(WideStringView(bytes, len));
	}
	return memory::PoolInterface::StringType();
}

static memory::PoolInterface::WideStringType toPoolWideString(CFMutableStringRef str) {
	auto len = CFStringGetLength(str);
	auto bytes = (const char16_t *)CFStringGetCStringPtr(str, kCFStringEncodingUTF16);

	if (bytes == NULL) {
		char16_t buf[len + 1];
		if (CFStringGetCString(str, (char *)buf, len * sizeof(char32_t), kCFStringEncodingUTF16)) {
			return memory::PoolInterface::WideStringType(buf, len);
		}
	} else {
		return memory::PoolInterface::WideStringType(bytes, len);
	}
	return memory::PoolInterface::WideStringType();
}

static memory::StandartInterface::StringType toStdString(CFMutableStringRef str) {
	auto len = CFStringGetLength(str);
	auto bytes = (const char16_t *)CFStringGetCStringPtr(str, kCFStringEncodingUTF16);

	if (bytes == NULL) {
		char16_t buf[len + 1];
		if (CFStringGetCString(str, (char *)buf, len * sizeof(char32_t), kCFStringEncodingUTF16)) {
			return string::toUtf8<memory::StandartInterface>(WideStringView(buf, len));
		}
	} else {
		return string::toUtf8<memory::StandartInterface>(WideStringView(bytes, len));
	}
	return memory::StandartInterface::StringType();
}

static memory::StandartInterface::WideStringType toStdWideString(CFMutableStringRef str) {
	auto len = CFStringGetLength(str);
	auto bytes = (const char16_t *)CFStringGetCStringPtr(str, kCFStringEncodingUTF16);

	if (bytes == NULL) {
		char16_t buf[len + 1];
		if (CFStringGetCString(str, (char *)buf, len * sizeof(char32_t), kCFStringEncodingUTF16)) {
			return memory::StandartInterface::WideStringType(buf, len);
		}
	} else {
		return memory::StandartInterface::WideStringType(bytes, len);
	}
	return memory::StandartInterface::WideStringType();
}

char32_t tolower(char32_t c) {
	return convertChar(c,
			[](CFMutableStringRef str, CFLocaleRef locale) { CFStringLowercase(str, locale); });
}

char32_t toupper(char32_t c) {
	return convertChar(c,
			[](CFMutableStringRef str, CFLocaleRef locale) { CFStringUppercase(str, locale); });
}

char32_t totitle(char32_t c) {
	return convertChar(c,
			[](CFMutableStringRef str, CFLocaleRef locale) { CFStringCapitalize(str, locale); });
}

template <>
auto tolower<memory::PoolInterface>(StringView data) -> memory::PoolInterface::StringType {
	auto locale = CFLocaleCopyCurrent();
	auto str = makeString(data);

	CFStringLowercase(str, locale);

	auto ret = toPoolString(str);

	CFRelease(str);
	CFRelease(locale);
	return ret;
}

template <>
auto tolower<memory::StandartInterface>(StringView data) -> memory::StandartInterface::StringType {
	auto locale = CFLocaleCopyCurrent();
	auto str = makeString(data);

	CFStringLowercase(str, locale);

	auto ret = toStdString(str);

	CFRelease(str);
	CFRelease(locale);
	return ret;
}

template <>
auto toupper<memory::PoolInterface>(StringView data) -> memory::PoolInterface::StringType {
	auto locale = CFLocaleCopyCurrent();
	auto str = makeString(data);

	CFStringUppercase(str, locale);

	auto ret = toPoolString(str);

	CFRelease(str);
	CFRelease(locale);
	return ret;
}

template <>
auto toupper<memory::StandartInterface>(StringView data) -> memory::StandartInterface::StringType {
	auto locale = CFLocaleCopyCurrent();
	auto str = makeString(data);

	CFStringUppercase(str, locale);

	auto ret = toStdString(str);

	CFRelease(str);
	CFRelease(locale);
	return ret;
}

template <>
auto totitle<memory::PoolInterface>(StringView data) -> memory::PoolInterface::StringType {
	auto locale = CFLocaleCopyCurrent();
	auto str = makeString(data);

	CFStringCapitalize(str, locale);

	auto ret = toPoolString(str);

	CFRelease(str);
	CFRelease(locale);
	return ret;
}

template <>
auto totitle<memory::StandartInterface>(StringView data) -> memory::StandartInterface::StringType {
	auto locale = CFLocaleCopyCurrent();
	auto str = makeString(data);

	CFStringCapitalize(str, locale);

	auto ret = toStdString(str);

	CFRelease(str);
	CFRelease(locale);
	return ret;
}

template <>
auto tolower<memory::PoolInterface>(WideStringView data) -> memory::PoolInterface::WideStringType {
	auto locale = CFLocaleCopyCurrent();
	auto str = makeString(data);

	CFStringLowercase(str, locale);

	auto ret = toPoolWideString(str);

	CFRelease(str);
	CFRelease(locale);
	return ret;
}

template <>
auto tolower<memory::StandartInterface>(WideStringView data)
		-> memory::StandartInterface::WideStringType {
	auto locale = CFLocaleCopyCurrent();
	auto str = makeString(data);

	CFStringLowercase(str, locale);

	auto ret = toStdWideString(str);

	CFRelease(str);
	CFRelease(locale);
	return ret;
}

template <>
auto toupper<memory::PoolInterface>(WideStringView data) -> memory::PoolInterface::WideStringType {
	auto locale = CFLocaleCopyCurrent();
	auto str = makeString(data);

	CFStringUppercase(str, locale);

	auto ret = toPoolWideString(str);

	CFRelease(str);
	CFRelease(locale);
	return ret;
}

template <>
auto toupper<memory::StandartInterface>(WideStringView data)
		-> memory::StandartInterface::WideStringType {
	auto locale = CFLocaleCopyCurrent();
	auto str = makeString(data);

	CFStringUppercase(str, locale);

	auto ret = toStdWideString(str);

	CFRelease(str);
	CFRelease(locale);
	return ret;
}

template <>
auto totitle<memory::PoolInterface>(WideStringView data) -> memory::PoolInterface::WideStringType {
	auto locale = CFLocaleCopyCurrent();
	auto str = makeString(data);

	CFStringCapitalize(str, locale);

	auto ret = toPoolWideString(str);

	CFRelease(str);
	CFRelease(locale);
	return ret;
}

template <>
auto totitle<memory::StandartInterface>(WideStringView data)
		-> memory::StandartInterface::WideStringType {
	auto locale = CFLocaleCopyCurrent();
	auto str = makeString(data);

	CFStringCapitalize(str, locale);

	auto ret = toStdWideString(str);

	CFRelease(str);
	CFRelease(locale);
	return ret;
}

int compare_u(StringView l, StringView r) {
	auto locale = CFLocaleCopyCurrent();
	auto lstr = makeString(l);
	auto rstr = makeString(r);

	auto res = CFStringCompareWithOptionsAndLocale(lstr, rstr,
			CFRangeMake(0, CFStringGetLength(lstr)), kCFCompareLocalized, locale);

	CFRelease(rstr);
	CFRelease(lstr);
	CFRelease(locale);
	return int(res);
}

int compare_u(WideStringView l, WideStringView r) {
	auto locale = CFLocaleCopyCurrent();
	auto lstr = makeString(l);
	auto rstr = makeString(r);

	auto res = CFStringCompareWithOptionsAndLocale(lstr, rstr,
			CFRangeMake(0, CFStringGetLength(lstr)), kCFCompareLocalized, locale);

	CFRelease(rstr);
	CFRelease(lstr);
	CFRelease(locale);
	return int(res);
}

int caseCompare_u(StringView l, StringView r) {
	auto locale = CFLocaleCopyCurrent();
	auto lstr = makeString(l);
	auto rstr = makeString(r);

	auto res =
			CFStringCompareWithOptionsAndLocale(lstr, rstr, CFRangeMake(0, CFStringGetLength(lstr)),
					kCFCompareLocalized | kCFCompareCaseInsensitive, locale);

	CFRelease(rstr);
	CFRelease(lstr);
	CFRelease(locale);
	return int(res);
}

int caseCompare_u(WideStringView l, WideStringView r) {
	auto locale = CFLocaleCopyCurrent();
	auto lstr = makeString(l);
	auto rstr = makeString(r);

	auto res =
			CFStringCompareWithOptionsAndLocale(lstr, rstr, CFRangeMake(0, CFStringGetLength(lstr)),
					kCFCompareLocalized | kCFCompareCaseInsensitive, locale);

	CFRelease(rstr);
	CFRelease(lstr);
	CFRelease(locale);
	return int(res);
}

size_t makeRandomBytes(uint8_t *buf, size_t count) {
	::getentropy(buf, count);
	return count;
}

bool initialize(int &resultCode) { return true; }

void terminate() { }

thread_local char tl_localeBuf[64] = {0};

StringView getOsLocale() {
	CFLocaleRef cflocale = CFLocaleCopyCurrent();
	auto value = (CFStringRef)CFLocaleGetIdentifier(cflocale);
	CFStringGetCString(value, tl_localeBuf, 64, kCFStringEncodingUTF8);
	CFRelease(cflocale);
	return StringView(tl_localeBuf);
}

} // namespace stappler::platform

#endif
