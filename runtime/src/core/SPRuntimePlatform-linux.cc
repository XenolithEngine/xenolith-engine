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

#include "SPRuntimeUnicode.h"
#include "private/SPRTDso.h"
#include <stdlib.h>

#if SPRT_LINUX

#if STAPPLER_STATIC_TOOLCHAIN

#include <unicode/uchar.h>
#include <unicode/ustring.h>
#include <unicode/uidna.h>

#else

constexpr sprt::uint32_t U_COMPARE_CODE_POINT_ORDER = 0x8000;
constexpr int U_ZERO_ERROR = 0;
using UErrorCode = int;
using UBreakIterator = void;
using UIDNA = void;

struct UIDNAInfo {
	sprt::int8_t isTransitionalDifferent;
	sprt::uint32_t errors;
};

#endif

#include <sys/random.h>

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
#if STAPPLER_STATIC_TOOLCHAIN
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
#if STAPPLER_STATIC_TOOLCHAIN
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
					if (__sprt_strcmp(paramName, "version") == 0) {
						break;
					}
				}
			}
		}

		if (::__sprt_strcmp(paramName, "version") == 0) {
			StringView tmp(buf, ::__sprt_strlen(buf));
			while (!tmp.empty() && !tmp.is('.')) { tmp.offset(1); }
			if (tmp.is('.')) {
				verSuffix = StringView(buf, tmp.ptr - buf);
			}
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
		auto targetBuf = new char16_t[data.size() + 1];

		UErrorCode status;
		auto len = icuFn(targetBuf, data.size(), data.data(), data.size(), nullptr, &status);
		if (len <= int32_t(data.size())) {
			cb(WideStringView(targetBuf, len));
			ret = true;
		} else {
			delete[] targetBuf;
			targetBuf = new char16_t[len + 1];
			len = icuFn(targetBuf, len, data.data(), data.size(), nullptr, &status);
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
			auto targetBuf = new char16_t[data.size() + 1];

			UErrorCode status;
			auto len = icu.u_strToTitle_fn(targetBuf, data.size(), data.data(), data.size(),
					nullptr, nullptr, &status);
			if (len <= int32_t(data.size())) {
				cb(WideStringView(targetBuf, len));
				ret = true;
			} else {
				delete[] targetBuf;
				targetBuf = new char16_t[len + 1];
				icu.u_strToTitle_fn(targetBuf, len, data.data(), data.size(), nullptr, nullptr,
						&status);
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
			int32_t ret = 0;
			if (unistring.u8_casecoll((const uint8_t *)l.data(), l.size(),
						(const uint8_t *)r.data(), r.size(), unistring.uc_locale_language(),
						nullptr, &ret)
					== 0) {
				return ret;
			}
		} else if (icu.u_strCaseCompare_fn) {
			bool ret = false;
			unicode::toUtf16([&](WideStringView lStr) {
				unicode::toUtf16([&](WideStringView rStr) {
					UErrorCode status;
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
			int32_t ret = 0;
			if (unistring.u16_casecoll((const uint16_t *)l.data(), l.size(),
						(const uint16_t *)r.data(), r.size(), unistring.uc_locale_language(),
						nullptr, &ret)
					== 0) {
				*result = ret;
				return true;
			}
		} else if (icu.u_strCaseCompare_fn) {
			UErrorCode status;
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
		if (err == 0) {
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
		UErrorCode err;
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

size_t makeRandomBytes(uint8_t *buf, size_t count) {
	size_t generated = 0;
	auto ret = ::getrandom(buf, count, GRND_RANDOM | GRND_NONBLOCK);
	if (ret < ssize_t(count)) {
		buf += ret;
		count -= ret;
		generated += ret;

		ret = ::getrandom(buf, count, GRND_NONBLOCK);
		if (ret >= 0) {
			generated += ret;
		}
	} else {
		generated += ret;
	}
	return generated;
}

StringView getOsLocale() {
	auto locale = ::getenv("LC_ALL");
	if (!locale) {
		locale = ::getenv("LANG");
	}
	return StringView(locale, ::__sprt_strlen(locale));
}

bool initialize(int &resultCode) { return true; }

void terminate() { }

} // namespace sprt::platform

#endif
