/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

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

#include <sprt/runtime/platform.h>
#include <sprt/runtime/unicode.h>
#include <sprt/runtime/utils/dso.h>
#include <sprt/c/__sprt_locale.h>

static constexpr sprt::uint32_t U_COMPARE_CODE_POINT_ORDER = 0x8000;
static constexpr int U_ZERO_ERROR = 0;
static constexpr int U_BUFFER_OVERFLOW_ERROR = 15;

using UErrorCode = int;
using UBreakIterator = void;

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

struct icu_iface {
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
	}

	explicit operator bool() const {
		return tolower_fn && toupper_fn && totitle_fn && u_strToLower_fn && u_strToUpper_fn
				&& u_strToTitle_fn && u_strCompare_fn && u_strCaseCompare_fn && u_errorName_fn;
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
	}
};

struct i18n {
	static i18n *getInstance() {
		static i18n s_instance;
		return &s_instance;
	}

	i18n() {
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
			__sprt_perror(icu.u_errorName_fn(status));
			return false;
		}

		status = U_ZERO_ERROR;

		auto capacity = len + 1;
		auto targetBuf = __sprt_typed_malloca(char16_t, capacity);
		len = icuFn(targetBuf, capacity, data.data(), data.size(), nullptr, &status);
		if (status == U_ZERO_ERROR && len >= 0 && len <= capacity) {
			cb(WideStringView(targetBuf, len));
			ret = true;
		}
		__sprt_freea(targetBuf);
		return ret;
	}

	bool applyUnistringFunction(const callback<void(StringView)> &cb, StringView data,
			unistring_iface::u8_case_fn ustrFn) {
		bool ret = false;
		size_t targetSize = data.size() + 1;
		auto targetBuf = __sprt_typed_malloca(char, data.size() + 1);

		auto buf = ustrFn((const uint8_t *)data.data(), data.size(), unistring.uc_locale_language(),
				nullptr, (uint8_t *)targetBuf, &targetSize);
		cb(StringView((const char *)buf, targetSize));
		ret = true;
		if (buf != (uint8_t *)targetBuf) {
			::__sprt_free(buf);
		}
		__sprt_freea(targetBuf);
		return ret;
	}

	bool applyUnistringFunction(const callback<void(WideStringView)> &cb, WideStringView data,
			unistring_iface::u16_case_fn ustrFn) {
		bool ret = false;
		size_t targetSize = data.size() + 1;
		auto targetBuf = __sprt_typed_malloca(char16_t, data.size() + 1);

		auto buf = ustrFn((const uint16_t *)data.data(), data.size(),
				unistring.uc_locale_language(), nullptr, (uint16_t *)targetBuf, &targetSize);
		cb(WideStringView((const char16_t *)buf, targetSize));
		ret = true;
		if (buf != (uint16_t *)targetBuf) {
			::__sprt_free(buf);
		}
		__sprt_freea(targetBuf);
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
				__sprt_perror(icu.u_errorName_fn(status));
				return false;
			}

			auto capacity = len + 1;
			auto targetBuf = __sprt_typed_malloca(char16_t, capacity);
			status = U_ZERO_ERROR;

			len = icu.u_strToTitle_fn(targetBuf, capacity, data.data(), data.size(), nullptr,
					nullptr, &status);
			if (status == U_ZERO_ERROR && len >= 0 && len <= capacity) {
				cb(WideStringView(targetBuf, len));
				ret = true;
			}
			__sprt_freea(targetBuf);
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
			*result = unistring.u8_cmp2((const uint8_t *)l.data(), l.size(),
					(const uint8_t *)r.data(), r.size());
			return true;
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
			// Lengths are unit counts, not NUL-terminated buffer sizes: a StringView
			// is not NUL-terminated, so size() + 1 would both read past the end and
			// feed the stray byte into the comparison.
			auto err = unistring.u8_casecoll((const uint8_t *)l.data(), l.size(),
					(const uint8_t *)r.data(), r.size(), lang, nullptr, &ret);
			if (err == 0) {
				*result = ret;
				return true;
			} else {
				auto st = status::errnoToStatus(__sprt_errno);
				status::getStatusDescription(st, [](StringView str) {
					__sprt_fwrite(str.data(), str.size(), 1, __sprt_stdout_impl());
				});
				__sprt_fwrite("\n", 1, 1, __sprt_stdout_impl());
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
					__sprt_fwrite(str.data(), str.size(), 1, __sprt_stdout_impl());
				});
				__sprt_fwrite("\n", 1, 1, __sprt_stdout_impl());
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

	Dso _handle;
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

} // namespace sprt::unicode
