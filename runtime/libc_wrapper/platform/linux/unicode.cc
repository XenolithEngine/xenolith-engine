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

using UErrorCode = int;

namespace sprt::unicode {

// Only collation is asked of either library now. Every case mapping - lower,
// upper and title, per code point and per string, in both encodings - comes from
// the compiled-in Unicode tables (runtime/src/unicode), which is why the case
// entry points are neither loaded nor required any more. That has loosened what
// counts as a usable library three times over the course of the port; a
// libunistring or ICU that exports only the comparison functions is now enough.
struct unistring_iface {
	const char *(*uc_locale_language)() = nullptr;

	int (*u8_cmp2)(const uint8_t *s1, size_t n1, const uint8_t *s2, size_t n2) = nullptr;
	int (*u8_casecoll)(const uint8_t *s1, size_t n1, const uint8_t *s2, size_t n2,
			const char *iso639_language, void *nf, int *resultp) = nullptr;

	int (*u16_cmp2)(const uint16_t *s1, size_t n1, const uint16_t *s2, size_t n2) = nullptr;
	int (*u16_casecoll)(const uint16_t *s1, size_t n1, const uint16_t *s2, size_t n2,
			const char *iso639_language, void *nf, int *resultp) = nullptr;

	void load(Dso &handle) {
		uc_locale_language = handle.sym<decltype(uc_locale_language)>("uc_locale_language");

		u8_cmp2 = handle.sym<decltype(u8_cmp2)>("u8_cmp2");
		u8_casecoll = handle.sym<decltype(u8_casecoll)>("u8_casecoll");

		u16_cmp2 = handle.sym<decltype(u16_cmp2)>("u16_cmp2");
		u16_casecoll = handle.sym<decltype(u16_casecoll)>("u16_casecoll");
	}

	explicit operator bool() const {
		return uc_locale_language && u8_cmp2 && u8_casecoll && u16_cmp2 && u16_casecoll;
	}

	void clear() {
		uc_locale_language = nullptr;

		u8_cmp2 = nullptr;
		u8_casecoll = nullptr;

		u16_cmp2 = nullptr;
		u16_casecoll = nullptr;
	}
};

struct icu_iface {
	using cmp_fn = int32_t (*)(const char16_t *s1, int32_t length1, const char16_t *s2,
			int32_t length2, int8_t codePointOrder);
	using case_cmp_fn = int32_t (*)(const char16_t *s1, int32_t length1, const char16_t *s2,
			int32_t length2, uint32_t options, UErrorCode *pErrorCode);

	cmp_fn u_strCompare_fn = nullptr;
	case_cmp_fn u_strCaseCompare_fn = nullptr;

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
		u_strCompare_fn = reinterpret_cast<decltype(u_strCompare_fn)>(
				loadIcu(handle, "u_strCompare", verSuffix));
		u_strCaseCompare_fn = reinterpret_cast<decltype(u_strCaseCompare_fn)>(
				loadIcu(handle, "u_strCaseCompare", verSuffix));
	}

	explicit operator bool() const {
		return u_strCompare_fn && u_strCaseCompare_fn;
	}

	void clear() {
		u_strCompare_fn = nullptr;
		u_strCaseCompare_fn = nullptr;
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

// No case mapping is left here, for code points or for strings: it all comes from
// the compiled-in Unicode tables (runtime/src/unicode), which need no library to
// be installed and answer the same on every target. What this file still does is
// collation, which is language-dependent ordering and genuinely belongs to the
// system.

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
