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

#include <sprt/runtime/init.h>

#if SPRT_LINUX

#include <sprt/runtime/filesystem/lookup.h>
#include <sprt/runtime/unicode.h>
#include <sprt/runtime/platform.h>
#include <sprt/runtime/mutex.h>
#include <sprt/c/__sprt_errno.h>
#include <sprt/c/__sprt_stdio.h>
#include <sprt/c/__sprt_locale.h>

#include "private/SPRTDso.h"

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/random.h>
#include <fcntl.h>
#include <unistd.h>

#if __SPRT_RUNTIME_CONFIG_HAVE_ICU

#define U_SHOW_CPLUSPLUS_API 0

#include <unicode/uchar.h>
#include <unicode/ustring.h>
#include <unicode/uidna.h>

#else

constexpr sprt::uint32_t U_COMPARE_CODE_POINT_ORDER = 0x8000;
constexpr int U_ZERO_ERROR = 0;
constexpr int U_BUFFER_OVERFLOW_ERROR = 15;
using UErrorCode = int;
using UBreakIterator = void;
using UIDNA = void;

struct UIDNAInfo {
	sprt::int8_t isTransitionalDifferent;
	sprt::uint32_t errors;
};

#endif

namespace sprt::unicode {

struct unistring_iface {
	using u8_case_fn = uint8_t *(*)(const uint8_t *s, size_t n, const char *iso639_language,
			void *nf, uint8_t *resultbuf, size_t *lengthp);
	using u16_case_fn = uint16_t *(*)(const uint16_t *s, size_t n, const char *iso639_language,
			void *nf, uint16_t *resultbuf, size_t *lengthp);

	int32_t (*tolower_fn)(int32_t) = nullptr;
	int32_t (*toupper_fn)(int32_t) = nullptr;
	int32_t (*totitle_fn)(int32_t) = nullptr;

	const char *(*uc_locale_language)() = nullptr;

	u8_case_fn u8_toupper = nullptr;
	u8_case_fn u8_tolower = nullptr;
	u8_case_fn u8_totitle = nullptr;

	int (*u8_cmp2)(const uint8_t *s1, size_t n1, const uint8_t *s2, size_t n2) = nullptr;
	int (*u8_casecoll)(const uint8_t *s1, size_t n1, const uint8_t *s2, size_t n2,
			const char *iso639_language, void *nf, int *resultp) = nullptr;

	u16_case_fn u16_toupper = nullptr;
	u16_case_fn u16_tolower = nullptr;
	u16_case_fn u16_totitle = nullptr;

	int (*u16_cmp2)(const uint16_t *s1, size_t n1, const uint16_t *s2, size_t n2) = nullptr;
	int (*u16_casecoll)(const uint16_t *s1, size_t n1, const uint16_t *s2, size_t n2,
			const char *iso639_language, void *nf, int *resultp) = nullptr;

	void load(Dso &handle) {
		tolower_fn = handle.sym<decltype(tolower_fn)>("uc_tolower");
		toupper_fn = handle.sym<decltype(toupper_fn)>("uc_toupper");
		totitle_fn = handle.sym<decltype(totitle_fn)>("uc_totitle");

		uc_locale_language = handle.sym<decltype(uc_locale_language)>("uc_locale_language");

		u8_toupper = handle.sym<decltype(u8_toupper)>("u8_toupper");
		u8_tolower = handle.sym<decltype(u8_tolower)>("u8_tolower");
		u8_totitle = handle.sym<decltype(u8_totitle)>("u8_totitle");

		u8_cmp2 = handle.sym<decltype(u8_cmp2)>("u8_cmp2");
		u8_casecoll = handle.sym<decltype(u8_casecoll)>("u8_casecoll");

		u16_toupper = handle.sym<decltype(u16_toupper)>("u16_toupper");
		u16_tolower = handle.sym<decltype(u16_tolower)>("u16_tolower");
		u16_totitle = handle.sym<decltype(u16_totitle)>("u16_totitle");

		u16_cmp2 = handle.sym<decltype(u16_cmp2)>("u16_cmp2");
		u16_casecoll = handle.sym<decltype(u16_casecoll)>("u16_casecoll");
	}

	explicit operator bool() const {
		return uc_locale_language && tolower_fn && toupper_fn && totitle_fn && u8_toupper
				&& u8_tolower && u8_totitle && u8_cmp2 && u8_casecoll && u16_toupper && u16_tolower
				&& u16_totitle && u16_cmp2 && u16_casecoll;
	}

	void clear() {
		tolower_fn = nullptr;
		toupper_fn = nullptr;
		totitle_fn = nullptr;

		uc_locale_language = nullptr;

		u8_toupper = nullptr;
		u8_tolower = nullptr;
		u8_totitle = nullptr;

		u8_cmp2 = nullptr;
		u8_casecoll = nullptr;

		u16_toupper = nullptr;
		u16_tolower = nullptr;
		u16_totitle = nullptr;

		u16_cmp2 = nullptr;
		u16_casecoll = nullptr;
	}
};

struct idn2_iface {
	enum flags {
		IDN2_NFC_INPUT = 1,
		IDN2_ALABEL_ROUNDTRIP = 2,
		IDN2_TRANSITIONAL = 4,
		IDN2_NONTRANSITIONAL = 8,
		IDN2_ALLOW_UNASSIGNED = 16,
		IDN2_USE_STD3_ASCII_RULES = 32,
		IDN2_NO_TR46 = 64,
		IDN2_NO_ALABEL_ROUNDTRIP = 128
	};

	int (*lookup_u8)(const uint8_t *src, uint8_t **lookupname, int flags) = nullptr;
	int (*lookup_ul)(const char *src, char **lookupname, int flags) = nullptr;
	int (*to_unicode_8z8z)(const char *src, char **lookupname, int flags) = nullptr;

	void load(Dso &handle) {
		lookup_u8 = handle.sym<decltype(lookup_u8)>("idn2_lookup_u8");
		lookup_ul = handle.sym<decltype(lookup_ul)>("idn2_lookup_ul");
		to_unicode_8z8z = handle.sym<decltype(to_unicode_8z8z)>("idn2_to_unicode_8z8z");
	}

	explicit operator bool() const { return lookup_u8 && lookup_ul && to_unicode_8z8z; }

	void clear() {
		lookup_u8 = nullptr;
		lookup_ul = nullptr;
		to_unicode_8z8z = nullptr;
	}
};

struct icu_iface {
	enum {
		UIDNA_DEFAULT = 0x30,
		UIDNA_USE_STD3_RULES = 2,
		UIDNA_CHECK_BIDI = 4,
		UIDNA_CHECK_CONTEXTJ = 8,
		UIDNA_NONTRANSITIONAL_TO_ASCII = 0x10,
		UIDNA_NONTRANSITIONAL_TO_UNICODE = 0x20,
		UIDNA_CHECK_CONTEXTO = 0x40
	};

	using case_fn = int32_t (*)(char16_t *dest, int32_t destCapacity, const char16_t *src,
			int32_t srcLength, const char *locale, UErrorCode *pErrorCode);
	using case_iter_fn = int32_t (*)(char16_t *dest, int32_t destCapacity, const char16_t *src,
			int32_t srcLength, UBreakIterator *iter, const char *locale, UErrorCode *pErrorCode);

	using cmp_fn = int32_t (*)(const char16_t *s1, int32_t length1, const char16_t *s2,
			int32_t length2, int8_t codePointOrder);
	using case_cmp_fn = int32_t (*)(const char16_t *s1, int32_t length1, const char16_t *s2,
			int32_t length2, uint32_t options, UErrorCode *pErrorCode);

	int32_t (*tolower_fn)(int32_t) = nullptr;
	int32_t (*toupper_fn)(int32_t) = nullptr;
	int32_t (*totitle_fn)(int32_t) = nullptr;

	case_fn u_strToLower_fn = nullptr;
	case_fn u_strToUpper_fn = nullptr;
	case_iter_fn u_strToTitle_fn = nullptr;

	cmp_fn u_strCompare_fn = nullptr;
	case_cmp_fn u_strCaseCompare_fn = nullptr;

	const char *(*u_errorName_fn)(UErrorCode code) = nullptr;

	UIDNA *(*uidna_openUTS46_fn)(uint32_t options, UErrorCode *pErrorCode) = nullptr;
	void (*uidna_close_fn)(UIDNA *idna) = nullptr;

	int32_t (*uidna_labelToASCII_UTF8_fn)(const UIDNA *idna, const char *label, int32_t length,
			char *dest, int32_t capacity, UIDNAInfo *pInfo, UErrorCode *pErrorCode) = nullptr;

	int32_t (*uidna_labelToUnicodeUTF8_fn)(const UIDNA *idna, const char *label, int32_t length,
			char *dest, int32_t capacity, UIDNAInfo *pInfo, UErrorCode *pErrorCode) = nullptr;

	int32_t (*uidna_nameToASCII_UTF8_fn)(const UIDNA *idna, const char *name, int32_t length,
			char *dest, int32_t capacity, UIDNAInfo *pInfo, UErrorCode *pErrorCode) = nullptr;

	int32_t (*uidna_nameToUnicodeUTF8_fn)(const UIDNA *idna, const char *name, int32_t length,
			char *dest, int32_t capacity, UIDNAInfo *pInfo, UErrorCode *pErrorCode) = nullptr;

	static void *loadIcu(Dso &h, const char *name, StringView ver) {
		char buf[256] = {0};
		auto ret = h.sym<void *>(name);
		if (!ret && !ver.empty()) {
			__sprt_strcpy(buf, name);
			__sprt_strcat(buf, "_");
			__sprt_strncat(buf, ver.data(), ver.size());

			ret = h.sym<void *>(buf);
		}
		return ret;
	}

	void load(Dso &handle, StringView verSuffix) {
#if __SPRT_RUNTIME_CONFIG_HAVE_ICU
		tolower_fn = &::u_tolower;
		toupper_fn = &::u_toupper;
		totitle_fn = &::u_totitle;

		u_strToLower_fn = &::u_strToLower;
		u_strToUpper_fn = &::u_strToUpper;
		u_strToTitle_fn = &::u_strToTitle;
		u_strCompare_fn = &::u_strCompare;
		u_strCaseCompare_fn = &::u_strCaseCompare;

		u_errorName_fn = &::u_errorName;
		uidna_openUTS46_fn = &::uidna_openUTS46;
		uidna_close_fn = &::uidna_close;

		uidna_labelToASCII_UTF8_fn = &::uidna_labelToASCII_UTF8;
		uidna_labelToUnicodeUTF8_fn = &::uidna_labelToUnicodeUTF8;
		uidna_nameToASCII_UTF8_fn = &::uidna_nameToASCII_UTF8;
		uidna_nameToUnicodeUTF8_fn = &::uidna_nameToUnicodeUTF8;
#else
		tolower_fn =
				reinterpret_cast<decltype(tolower_fn)>(loadIcu(handle, "u_tolower", verSuffix));
		toupper_fn =
				reinterpret_cast<decltype(toupper_fn)>(loadIcu(handle, "u_toupper", verSuffix));
		totitle_fn =
				reinterpret_cast<decltype(totitle_fn)>(loadIcu(handle, "u_totitle", verSuffix));
		u_strToLower_fn = reinterpret_cast<decltype(u_strToLower_fn)>(
				loadIcu(handle, "u_strToLower", verSuffix));
		u_strToUpper_fn = reinterpret_cast<decltype(u_strToUpper_fn)>(
				loadIcu(handle, "u_strToUpper", verSuffix));
		u_strToTitle_fn = reinterpret_cast<decltype(u_strToTitle_fn)>(
				loadIcu(handle, "u_strToTitle", verSuffix));
		u_strCompare_fn = reinterpret_cast<decltype(u_strCompare_fn)>(
				loadIcu(handle, "u_strCompare", verSuffix));
		u_strCaseCompare_fn = reinterpret_cast<decltype(u_strCaseCompare_fn)>(
				loadIcu(handle, "u_strCaseCompare", verSuffix));

		u_errorName_fn = reinterpret_cast<decltype(u_errorName_fn)>(
				loadIcu(handle, "u_errorName", verSuffix));
		uidna_openUTS46_fn = reinterpret_cast<decltype(uidna_openUTS46_fn)>(
				loadIcu(handle, "uidna_openUTS46", verSuffix));
		uidna_close_fn = reinterpret_cast<decltype(uidna_close_fn)>(
				loadIcu(handle, "uidna_close", verSuffix));

		uidna_labelToASCII_UTF8_fn = reinterpret_cast<decltype(uidna_labelToASCII_UTF8_fn)>(
				loadIcu(handle, "uidna_labelToASCII_UTF8", verSuffix));
		uidna_labelToUnicodeUTF8_fn = reinterpret_cast<decltype(uidna_labelToUnicodeUTF8_fn)>(
				loadIcu(handle, "uidna_labelToUnicodeUTF8", verSuffix));
		uidna_nameToASCII_UTF8_fn = reinterpret_cast<decltype(uidna_nameToASCII_UTF8_fn)>(
				loadIcu(handle, "uidna_nameToASCII_UTF8", verSuffix));
		uidna_nameToUnicodeUTF8_fn = reinterpret_cast<decltype(uidna_nameToUnicodeUTF8_fn)>(
				loadIcu(handle, "uidna_nameToUnicodeUTF8", verSuffix));
#endif
	}

	explicit operator bool() const {
		return tolower_fn && toupper_fn && totitle_fn && u_strToLower_fn && u_strToUpper_fn
				&& u_strToTitle_fn && u_strCompare_fn && u_strCaseCompare_fn && u_errorName_fn
				&& uidna_openUTS46_fn && uidna_close_fn && uidna_labelToASCII_UTF8_fn
				&& uidna_labelToUnicodeUTF8_fn && uidna_nameToASCII_UTF8_fn
				&& uidna_nameToUnicodeUTF8_fn;
	}

	void clear() {
		tolower_fn = nullptr;
		toupper_fn = nullptr;
		totitle_fn = nullptr;
		u_strToLower_fn = nullptr;
		u_strToUpper_fn = nullptr;
		u_strToTitle_fn = nullptr;
		u_strCompare_fn = nullptr;
		u_strCaseCompare_fn = nullptr;
		u_errorName_fn = nullptr;
		uidna_openUTS46_fn = nullptr;
		uidna_close_fn = nullptr;
		uidna_labelToASCII_UTF8_fn = nullptr;
		uidna_labelToUnicodeUTF8_fn = nullptr;
		uidna_nameToASCII_UTF8_fn = nullptr;
		uidna_nameToUnicodeUTF8_fn = nullptr;
	}
};

struct i18n {
	static i18n *getInstance() {
		static i18n s_instance;
		return &s_instance;
	}

	i18n() {
#if __SPRT_RUNTIME_CONFIG_HAVE_ICU
		icu.load(_handle, StringView());
		if (!icu) {
			icu.clear();
			_handle.close();
		} else {
			return;
		}
#endif

		// try unistring
		// try version 0 or 1 if no general symlink
		_handle = Dso("libunistring.so");
		if (!_handle) {
			_handle = Dso("libunistring.so.1");
		}
		if (!_handle) {
			_handle = Dso("libunistring.so.0");
		}
		if (_handle) {
			unistring.load(_handle);
			if (unistring) {
				_idnHandle = Dso("libidn2.so");
				if (_idnHandle) {
					idn2.load(_idnHandle);
					if (!idn2) {
						idn2.clear();
						_idnHandle.close();
					}
				}

				// We have to set locale for unistring to work
				auto loc = sprt::platform::getOsLocale();
				__sprt_setlocale(__SPRT_LC_ALL, loc.data());
				return;
			} else {
				unistring.clear();
				_handle.close();
			}
		}

		// try ICU
		char buf[256] = {0};
		const char *paramName = nullptr;
		StringView verSuffix;

		auto dbg = Dso("libicutu.so");
		if (dbg) {
			auto getSystemParameterNameByIndex =
					dbg.sym<const char *(*)(int32_t)>("udbg_getSystemParameterNameByIndex");
			auto getSystemParameterValueByIndex =
					dbg.sym<int32_t (*)(int32_t i, char *, int32_t, int *)>(
							"udbg_getSystemParameterValueByIndex");

			if (getSystemParameterNameByIndex && getSystemParameterValueByIndex) {
				int status;
				for (int32_t i = 0; (paramName = getSystemParameterNameByIndex(i)) != nullptr;
						++i) {
					getSystemParameterValueByIndex(i, buf, 256, &status);
					if (StringView(paramName) == "version") {
						break;
					}
				}
			}
		}

		if (StringView(paramName) == "version") {
			verSuffix = StringView(buf).readUntil<StringView::Chars<'.'>>();
		}

		_handle = Dso("libicuuc.so");
		if (_handle) {
			icu.load(_handle, verSuffix);
			if (!icu) {
				icu.clear();
				_handle.close();
			}
		}

		if (!icu) {
			_idnHandle = Dso("libidn2.so");
			if (_idnHandle) {
				idn2.load(_idnHandle);
				if (!idn2) {
					idn2.clear();
					_idnHandle.close();
				}
			}
		}
	}

	~i18n() { }

	char32_t tolower(char32_t c) {
		return _handle
				? char32_t(icu ? icu.tolower_fn(int32_t(c)) : unistring.tolower_fn(int32_t(c)))
				: 0;
	}

	char32_t toupper(char32_t c) {
		return _handle
				? char32_t(icu ? icu.toupper_fn(int32_t(c)) : unistring.toupper_fn(int32_t(c)))
				: 0;
	}

	char32_t totitle(char32_t c) {
		return _handle
				? char32_t(icu ? icu.totitle_fn(int32_t(c)) : unistring.totitle_fn(int32_t(c)))
				: 0;
	}

	bool applyIcuFunction(const callback<void(WideStringView)> &cb, WideStringView data,
			icu_iface::case_fn icuFn) {
		bool ret = false;

		UErrorCode status = U_ZERO_ERROR;
		auto len = icuFn(nullptr, 0, data.data(), data.size(), nullptr, &status);
		if (status != U_ZERO_ERROR && status != U_BUFFER_OVERFLOW_ERROR) {
			perror(icu.u_errorName_fn(status));
			return false;
		}

		status = U_ZERO_ERROR;
		auto targetBuf = new char16_t[len + 1];
		len = icuFn(targetBuf, len + 1, data.data(), data.size(), nullptr, &status);
		if (len <= int32_t(len)) {
			cb(WideStringView(targetBuf, len));
			ret = true;
		}
		delete[] targetBuf;
		return ret;
	}

	bool applyUnistringFunction(const callback<void(StringView)> &cb, StringView data,
			unistring_iface::u8_case_fn ustrFn) {
		bool ret = false;
		size_t targetSize = data.size();
		auto targetBuf = new char[data.size() + 1];

		auto buf = ustrFn((const uint8_t *)data.data(), data.size(), unistring.uc_locale_language(),
				nullptr, (uint8_t *)targetBuf, &targetSize);
		if (targetSize > data.size() + 1) {
			cb(StringView((const char *)buf, targetSize));
			::free(buf);
			ret = true;
		} else {
			cb(StringView((const char *)buf, targetSize));
			ret = true;
		}
		delete[] targetBuf;
		return ret;
	}

	bool applyUnistringFunction(const callback<void(WideStringView)> &cb, WideStringView data,
			unistring_iface::u16_case_fn ustrFn) {
		bool ret = false;
		size_t targetSize = data.size();
		auto targetBuf = new char16_t[data.size() + 1];

		auto buf = ustrFn((const uint16_t *)data.data(), data.size(),
				unistring.uc_locale_language(), nullptr, (uint16_t *)targetBuf, &targetSize);
		if (targetSize > data.size() + 1) {
			cb(WideStringView((const char16_t *)buf, targetSize));
			::free(buf);
			ret = true;
		} else {
			cb(WideStringView((const char16_t *)buf, targetSize));
			ret = true;
		}
		delete[] targetBuf;
		return ret;
	}

	auto applyFunction(const callback<void(StringView)> &cb, StringView data,
			icu_iface::case_fn icuFn, unistring_iface::u8_case_fn ustrFn) {
		bool ret = false;
		if (icuFn) {
			unicode::toUtf16([&](WideStringView str) {
				applyIcuFunction([&](WideStringView result) {
					unicode::toUtf8([&](StringView out) {
						cb(out);
						ret = true;
					}, result);
				}, str, icuFn);
			}, data);
		} else if (ustrFn) {
			return applyUnistringFunction(cb, data, ustrFn);
		}
		return ret;
	}

	auto applyFunction(const callback<void(WideStringView)> &cb, WideStringView data,
			icu_iface::case_fn icuFn, unistring_iface::u16_case_fn ustrFn) {
		if (icuFn) {
			return applyIcuFunction(cb, data, icuFn);
		} else if (ustrFn) {
			return applyUnistringFunction(cb, data, ustrFn);
		}
		return false;
	}

	bool tolower(const callback<void(StringView)> &cb, StringView data) {
		return applyFunction(cb, data, icu.u_strToLower_fn, unistring.u8_tolower);
	}

	bool tolower(const callback<void(WideStringView)> &cb, WideStringView data) {
		return applyFunction(cb, data, icu.u_strToLower_fn, unistring.u16_tolower);
	}

	bool toupper(const callback<void(StringView)> &cb, StringView data) {
		return applyFunction(cb, data, icu.u_strToUpper_fn, unistring.u8_toupper);
	}

	bool toupper(const callback<void(WideStringView)> &cb, WideStringView data) {
		return applyFunction(cb, data, icu.u_strToUpper_fn, unistring.u16_toupper);
	}

	auto totitle(const callback<void(StringView)> &cb, StringView data) {
		bool ret = false;
		if (icu.u_strToTitle_fn) {
			unicode::toUtf16([&](WideStringView str) {
				totitle([&](WideStringView result) {
					unicode::toUtf8([&](StringView out) {
						cb(out);
						ret = true;
					}, result);
				}, str);
			}, data);
		} else if (unistring.u8_totitle) {
			return applyUnistringFunction(cb, data, unistring.u8_totitle);
		}
		return ret;
	}

	bool totitle(const callback<void(WideStringView)> &cb, WideStringView data) {
		if (icu.u_strToTitle_fn) {
			bool ret = false;
			UErrorCode status = U_ZERO_ERROR;
			auto len = icu.u_strToTitle_fn(nullptr, 0, data.data(), data.size(), nullptr, nullptr,
					&status);
			if (status != U_ZERO_ERROR && status != U_BUFFER_OVERFLOW_ERROR) {
				perror(icu.u_errorName_fn(status));
				return false;
			}

			auto targetBuf = new char16_t[len + 1];
			status = U_ZERO_ERROR;

			len = icu.u_strToTitle_fn(targetBuf, len + 1, data.data(), data.size(), nullptr,
					nullptr, &status);
			if (len <= int32_t(data.size())) {
				cb(WideStringView(targetBuf, len));
				ret = true;
			}
			delete[] targetBuf;
			return ret;
		} else if (unistring.u16_totitle) {
			return applyUnistringFunction(cb, data, unistring.u16_totitle);
		}

		return false;
	}

	bool compare(StringView l, StringView r, int *result) {
		if (!result) {
			return false;
		}

		if (unistring.u8_cmp2) {
			return unistring.u8_cmp2((const uint8_t *)l.data(), l.size(), (const uint8_t *)r.data(),
					r.size());
		} else if (icu.u_strCompare_fn) {
			bool ret = false;
			unicode::toUtf16([&](WideStringView lStr) {
				unicode::toUtf16([&](WideStringView rStr) {
					*result = icu.u_strCompare_fn(lStr.data(), lStr.size(), rStr.data(),
							rStr.size(), 1);
					ret = true;
				}, r);
			}, l);
			return ret;
		}
		return false;
	}

	bool compare(WideStringView l, WideStringView r, int *result) {
		if (!result) {
			return false;
		}

		if (unistring.u16_cmp2) {
			*result = unistring.u16_cmp2((const uint16_t *)l.data(), l.size(),
					(const uint16_t *)r.data(), r.size());
			return true;
		} else if (icu.u_strCompare_fn) {
			*result = icu.u_strCompare_fn(l.data(), l.size(), r.data(), r.size(), 1);
			return true;
		}
		return false;
	}
	bool caseCompare(StringView l, StringView r, int *result) {
		if (!result) {
			return false;
		}

		if (unistring.u8_casecoll) {
			int ret = 0;
			auto lang = unistring.uc_locale_language();
			auto err = unistring.u8_casecoll((const uint8_t *)l.data(), l.size() + 1,
					(const uint8_t *)r.data(), r.size() + 1, lang, nullptr, &ret);
			if (err == 0) {
				*result = ret;
				return true;
			} else {
				auto st = status::errnoToStatus(__sprt_errno);
				status::getStatusDescription(st, [](StringView str) {
					fwrite(str.data(), str.size(), 1, __sprt_stdout_impl());
				});
				fwrite("\n", 1, 1, __sprt_stdout_impl());
			}
		} else if (icu.u_strCaseCompare_fn) {
			bool ret = false;
			unicode::toUtf16([&](WideStringView lStr) {
				unicode::toUtf16([&](WideStringView rStr) {
					UErrorCode status = U_ZERO_ERROR;
					*result = icu.u_strCaseCompare_fn(lStr.data(), lStr.size(), rStr.data(),
							rStr.size(), U_COMPARE_CODE_POINT_ORDER, &status);
					ret = status == U_ZERO_ERROR;
				}, r);
			}, l);
			return ret;
		}
		return false;
	}

	bool caseCompare(WideStringView l, WideStringView r, int *result) {
		if (!result) {
			return false;
		}

		if (unistring.u16_casecoll) {
			int ret = 0;
			auto lang = unistring.uc_locale_language();
			auto err = unistring.u16_casecoll((const uint16_t *)l.data(), l.size(),
					(const uint16_t *)r.data(), r.size(), lang, nullptr, &ret);
			if (err == 0) {
				*result = ret;
				return true;
			} else {
				auto st = status::errnoToStatus(__sprt_errno);
				status::getStatusDescription(st, [](StringView str) {
					fwrite(str.data(), str.size(), 1, __sprt_stdout_impl());
				});
				fwrite("\n", 1, 1, __sprt_stdout_impl());
			}
		} else if (icu.u_strCaseCompare_fn) {
			UErrorCode status = U_ZERO_ERROR;
			*result = icu.u_strCaseCompare_fn(l.data(), l.size(), r.data(), r.size(),
					U_COMPARE_CODE_POINT_ORDER, &status);
			return status == U_ZERO_ERROR;
		}
		return false;
	}

	icu_iface icu;
	unistring_iface unistring;
	idn2_iface idn2;

	Dso _handle;
	Dso _idnHandle;
};

static i18n *s_instance = i18n::getInstance();

char32_t tolower(char32_t c) { return s_instance->tolower(c); }

char32_t toupper(char32_t c) { return s_instance->toupper(c); }

char32_t totitle(char32_t c) { return s_instance->totitle(c); }

bool toupper(const callback<void(StringView)> &cb, StringView data) {
	return s_instance->toupper(cb, data);
}
bool totitle(const callback<void(StringView)> &cb, StringView data) {
	return s_instance->totitle(cb, data);
}
bool tolower(const callback<void(StringView)> &cb, StringView data) {
	return s_instance->tolower(cb, data);
}

bool toupper(const callback<void(WideStringView)> &cb, WideStringView data) {
	return s_instance->toupper(cb, data);
}
bool totitle(const callback<void(WideStringView)> &cb, WideStringView data) {
	return s_instance->totitle(cb, data);
}
bool tolower(const callback<void(WideStringView)> &cb, WideStringView data) {
	return s_instance->tolower(cb, data);
}

bool compare(StringView l, StringView r, int *result) { return s_instance->compare(l, r, result); }

bool compare(WideStringView l, WideStringView r, int *result) {
	return s_instance->compare(l, r, result);
}

bool caseCompare(StringView l, StringView r, int *result) {
	return s_instance->caseCompare(l, r, result);
}

bool caseCompare(WideStringView l, WideStringView r, int *result) {
	return s_instance->caseCompare(l, r, result);
}

bool idnToAscii(const callback<void(StringView)> &cb, StringView source) {
	uint8_t *out = nullptr;
	if (s_instance->idn2) {
		int flags = idn2_iface::IDN2_NFC_INPUT | idn2_iface::IDN2_NONTRANSITIONAL;
		int rc = s_instance->idn2.lookup_u8((const uint8_t *)source.data(), &out, flags);
		if (rc != 0) {
			rc = s_instance->idn2.lookup_u8((const uint8_t *)source.data(), &out,
					idn2_iface::IDN2_TRANSITIONAL);
		}

		if (rc == 0) {
			cb(StringView((const char *)out, ::__sprt_strlen((const char *)out)));
			::free(out);
			return true;
		}
	} else if (s_instance->icu) {
		UErrorCode err = U_ZERO_ERROR;
		auto idna = s_instance->icu.uidna_openUTS46_fn(
				icu_iface::UIDNA_CHECK_BIDI | icu_iface::UIDNA_NONTRANSITIONAL_TO_ASCII, &err);
		if (err == U_ZERO_ERROR) {
			UIDNAInfo info = {0, 0};
			char buffer[1_KiB] = {0};
			auto retLen = s_instance->icu.uidna_nameToASCII_UTF8_fn(idna, source.data(),
					(int)source.size(), buffer, 1_KiB - 1, &info, &err);
			s_instance->icu.uidna_close_fn(idna);
			if (retLen > 0 && err == 0 && !info.errors) {
				cb(StringView(buffer, retLen));
				return true;
			}
		}
	}
	return false;
}

bool idnToUnicode(const callback<void(StringView)> &cb, StringView source) {
	if (s_instance->idn2) {
		char *out = nullptr;
		auto err = s_instance->idn2.to_unicode_8z8z(source.data(), &out, 0);
		if (err == 0) {
			cb(StringView((const char *)out, ::__sprt_strlen((const char *)out)));
			free(out);
			return true;
		}
	} else if (s_instance->icu) {
		UErrorCode err = U_ZERO_ERROR;
		auto idna = s_instance->icu.uidna_openUTS46_fn(
				icu_iface::UIDNA_CHECK_BIDI | icu_iface::UIDNA_NONTRANSITIONAL_TO_UNICODE, &err);
		if (err == U_ZERO_ERROR) {
			char buffer[1_KiB] = {0};
			UIDNAInfo info = {0, 0};
			auto retLen = s_instance->icu.uidna_nameToUnicodeUTF8_fn(idna, source.data(),
					(int)source.size(), buffer, 1_KiB - 1, &info, &err);
			s_instance->icu.uidna_close_fn(idna);
			if (retLen > 0 && err == 0 && !info.errors) {
				cb(StringView(buffer, retLen));
				return true;
			}
		}
	}
	return false;
}

} // namespace sprt::unicode

namespace sprt::platform {

struct GlobalConfig {
	qmutex s_infoMutex;
	StringView uniqueIdBuf;
	StringView execPathBuf;
	StringView homePathBuf;

	AppConfig config;

	filesystem::LocationInfo current;

	memory::pool_t *_pool = memory::pool::create(memory::self_contained_allocator);
};

static GlobalConfig s_globalConfig;

StringView getOsLocale() {
	auto locale = ::getenv("LC_ALL");
	if (!locale) {
		locale = ::getenv("LANG");
	}
	return StringView(locale, ::__sprt_strlen(locale));
}

StringView getUniqueDeviceId() {
	if (s_globalConfig.uniqueIdBuf.empty()) {
		// optimistic multithreaded lazy-init
		// it can allocate more-then needed memory but protected from general lock

		struct __SPRT_STAT_NAME st;
		if (stat("/etc/machine-id", &st) == 0) {
			auto buf = __sprt_malloca(st.st_size + 1);
			auto fd = open("/etc/machine-id", 0);
			if (fd > 0) {
				auto v = ::read(fd, buf, st.st_size);
				if (v == st.st_size) {
					auto id = StringView((const char *)buf, v);
					id.trimChars<StringView::WhiteSpace>();
					unique_lock lock(s_globalConfig.s_infoMutex);
					s_globalConfig.uniqueIdBuf = id.pdup(s_globalConfig._pool);
				}
			}
			__sprt_freea(buf);
		}
	}

	return s_globalConfig.uniqueIdBuf;
}

StringView getExecPath() {
	if (s_globalConfig.execPathBuf.empty()) {
		// optimistic multithreaded lazy-init
		// it can allocate more-then needed memory but protected from general lock

		auto buf = (char *)__sprt_malloca(PATH_MAX);
		auto v = ::readlink("/proc/self/exe", buf, PATH_MAX);
		if (v > 0) {
			unique_lock lock(s_globalConfig.s_infoMutex);
			s_globalConfig.execPathBuf = StringView(buf, v).pdup(s_globalConfig._pool);
		}
		__sprt_freea(buf);
	}

	return s_globalConfig.execPathBuf;
}

StringView getHomePath() {
	if (s_globalConfig.homePathBuf.empty()) {
		// optimistic multithreaded lazy-init
		// it can allocate more-then needed memory but protected from general lock

		auto path = StringView(getenv("HOME"));

		unique_lock lock(s_globalConfig.s_infoMutex);
		s_globalConfig.homePathBuf = path.pdup(s_globalConfig._pool);
	}
	return s_globalConfig.homePathBuf;
}

bool initialize(AppConfig &&cfg, int &resultCode) {
	s_globalConfig.config.bundleName = cfg.bundleName.pdup(s_globalConfig._pool);
	s_globalConfig.config.bundlePath = cfg.bundlePath.pdup(s_globalConfig._pool);
	s_globalConfig.config.pathScheme = cfg.pathScheme;

	s_globalConfig.current.lookupType = filesystem::LookupFlags::Public
			| filesystem::LookupFlags::Shared | filesystem::LookupFlags::Writable;
	s_globalConfig.current.locationFlags = filesystem::LocationFlags::Writable;
	s_globalConfig.current.interface = filesystem::getDefaultInterface();

	filesystem::getCurrentDir([&](StringView path) {
		s_globalConfig.current.path = path.pdup(s_globalConfig._pool);
	});

	return true;
}

void terminate() { }

memory::pool_t *getConfigPool() { return s_globalConfig._pool; }

} // namespace sprt::platform

namespace sprt {

const AppConfig &getAppConfig() { return platform::s_globalConfig.config; }

} // namespace sprt

namespace sprt::filesystem {

const LocationInfo &getCurrentLocation() { return platform::s_globalConfig.current; }

} // namespace sprt::filesystem

#endif
