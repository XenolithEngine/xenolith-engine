/**
 Copyright (c) 2025 Stappler Team <admin@stappler.org>
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

// The libidn2 C ABI on top of sprt::idn, for cURL and anything else that expects
// it. ONE implementation for every platform - this used to be two near-identical
// copies, in runtime/libc_wrapper/platform/{linux,android}/unicode.cc, each
// delegating to whatever IDN library that platform happened to have, and absent
// entirely on Windows, Darwin and wasm.
//
// Behaviour differences from those copies, all of them fixes:
//
//  * idn2_to_unicode_8z8z() performs ToUnicode. Both old copies called the
//    ToASCII entry point of the library they had loaded, so a caller asking for
//    the Unicode form got the Punycode form back.
//  * The error code says what happened. The old ones returned `-1000 - UErrorCode`
//    or a raw ICU error bitmask, neither of which is an idn2_rc, so cURL could only
//    report "some IDN error".
//  * The result is allocated exactly, instead of a fixed 2 KiB buffer per call.

#include <sprt/runtime/utils/idn.h>
#include <sprt/compat/idn2.h>
#include <sprt/c/__sprt_stdlib.h>
#include <sprt/c/__sprt_string.h>

namespace sprt::idn {

// idn2 flags -> UTS-46 options.
//
// The default profile is IDNA2008, which is what libidn2 implements and what cURL
// asks for. IDN2_NO_TR46 means "no UTS-46 mapping at all"; the closest this engine
// offers is running with every optional check off, so that is what it does.
static Options optionsFromIdn2Flags(int flags, bool toUnicode) {
	if (flags & IDN2_NO_TR46) {
		return Options::None;
	}

	auto options = Options::CheckBidi | Options::CheckContextJ | Options::CheckContextO;

	if (flags & IDN2_USE_STD3_ASCII_RULES) {
		options |= Options::UseStd3Rules;
	}

	// libidn2 defaults to nontransitional and only folds when asked; IDN2_TRANSITIONAL
	// therefore clears the flag rather than setting one.
	if ((flags & IDN2_TRANSITIONAL) == 0) {
		options |= toUnicode ? Options::NonTransitionalToUnicode : Options::NonTransitionalToAscii;
	}

	return options;
}

static int idn2ErrorFromStatus(Status status) {
	switch (status) {
	case Status::Ok: return IDN2_OK;

	case Status::ErrorIdnPunycode: return IDN2_PUNYCODE_BAD_INPUT;
	case Status::ErrorIdnInvalidAceLabel: return IDN2_INVALID_ALABEL;
	case Status::ErrorIdnLabelHasDot: return IDN2_DOT_IN_LABEL;
	// idn2 has no code for "empty label" (a name like "a..b"). The old shims folded
	// it into IDN2_OK, because ICU raises it for a trailing dot in some
	// configurations and cURL must accept "example.com."; this engine does not raise
	// it there, so a genuine empty label can be reported as the error it is.
	case Status::ErrorIdnEmptyLabel: return IDN2_ENCODING_ERROR;
	case Status::ErrorIdnDisallowed: return IDN2_DISALLOWED;
	case Status::ErrorIdnBidi: return IDN2_BIDI;
	case Status::ErrorIdnContextJ: return IDN2_CONTEXTJ;
	case Status::ErrorIdnContextOPunctuation: return IDN2_CONTEXTO;
	case Status::ErrorIdnContextODigits: return IDN2_CONTEXTO;
	case Status::ErrorIdnLeadingCombiningMark: return IDN2_LEADING_COMBINING;
	case Status::ErrorIdnLeadingHyphen: return IDN2_HYPHEN_STARTEND;
	case Status::ErrorIdnTrailingHyphen: return IDN2_HYPHEN_STARTEND;
	case Status::ErrorIdnHyphen34: return IDN2_2HYPHEN;
	case Status::ErrorIdnLabelTooLong: return IDN2_TOO_BIG_LABEL;
	case Status::ErrorIdnDomainNameTooLong: return IDN2_TOO_BIG_DOMAIN;

	case Status::ErrorOutOfHostMemory: return IDN2_MALLOC;
	default: return IDN2_ENCODING_ERROR;
	}
}

// Runs one conversion and hands the result back the way libidn2 does: a
// NUL-terminated buffer the caller owns and releases with idn2_free().
static int convert(const char *src, char **out, int flags, bool toUnicode) {
	if (!src) {
		if (out) {
			*out = nullptr;
		}
		return IDN2_OK;
	}

	StringView source(src, ::__sprt_strlen(src));
	if (source.empty()) {
		// libidn2 accepts an empty name and gives an empty one back; sprt::idn calls
		// it a bad argument. Keep the C ABI's behaviour, since cURL depends on it.
		if (out) {
			auto empty = reinterpret_cast<char *>(::__sprt_malloc(1));
			if (!empty) {
				return IDN2_MALLOC;
			}
			empty[0] = 0;
			*out = empty;
		}
		return IDN2_OK;
	}

	auto options = optionsFromIdn2Flags(flags, toUnicode);

	char *result = nullptr;
	auto sink = [&](StringView str) {
		result = reinterpret_cast<char *>(::__sprt_malloc(str.size() + 1));
		if (result) {
			::__sprt_memcpy(result, str.data(), str.size());
			result[str.size()] = 0;
		}
	};

	auto status = toUnicode ? to_unicode(sink, source, options) : to_ascii(sink, source, options);

	if (status != Status::Ok) {
		return idn2ErrorFromStatus(status);
	}
	if (!result) {
		return IDN2_MALLOC;
	}

	if (out) {
		*out = result;
	} else {
		::__sprt_free(result);
	}
	return IDN2_OK;
}

extern "C" SPRT_GLOBAL int idn2_lookup_u8(const uint8_t *src, uint8_t **lookupname, int flags) {
	return convert(reinterpret_cast<const char *>(src), reinterpret_cast<char **>(lookupname),
			flags, false);
}

extern "C" SPRT_GLOBAL int idn2_lookup_ul(const char *src, char **lookupname, int flags) {
	// libidn2 decodes from the locale encoding here. The runtime is UTF-8
	// throughout, so this is idn2_lookup_u8() by another name.
	return convert(src, lookupname, flags, false);
}

extern "C" SPRT_GLOBAL int idn2_to_unicode_8z8z(const char *src, char **lookupname, int flags) {
	return convert(src, lookupname, flags, true);
}

extern "C" SPRT_GLOBAL const char *idn2_strerror(int rc) {
	switch (rc) {
	case IDN2_OK: return "success";
	case IDN2_MALLOC: return "cannot allocate memory";
	case IDN2_ENCODING_ERROR: return "character encoding error";
	case IDN2_NFC: return "normalization failed";
	case IDN2_PUNYCODE_BAD_INPUT: return "Punycode invalid input";
	case IDN2_TOO_BIG_DOMAIN: return "domain name too long";
	case IDN2_TOO_BIG_LABEL: return "domain label too long";
	case IDN2_INVALID_ALABEL: return "domain name has invalid A-label";
	case IDN2_INVALID_FLAGS: return "invalid combination of flags";
	case IDN2_NOT_NFC: return "string is not in Unicode NFC format";
	case IDN2_2HYPHEN: return "string has forbidden two hyphens";
	case IDN2_HYPHEN_STARTEND: return "string has forbidden starting/ending hyphen";
	case IDN2_LEADING_COMBINING: return "string has forbidden leading combining character";
	case IDN2_DISALLOWED: return "string has disallowed character";
	case IDN2_CONTEXTJ: return "string has forbidden context-j character";
	case IDN2_CONTEXTO: return "string has forbidden context-o character";
	case IDN2_BIDI: return "string has forbidden bi-directional properties";
	case IDN2_DOT_IN_LABEL: return "label has forbidden dot (TR46)";
	default: return "processing error";
	}
}

extern "C" SPRT_GLOBAL const char *idn2_strerror_name(int rc) {
	switch (rc) {
	case IDN2_OK: return "IDN2_OK";
	case IDN2_MALLOC: return "IDN2_MALLOC";
	case IDN2_ENCODING_ERROR: return "IDN2_ENCODING_ERROR";
	case IDN2_NFC: return "IDN2_NFC";
	case IDN2_PUNYCODE_BAD_INPUT: return "IDN2_PUNYCODE_BAD_INPUT";
	case IDN2_TOO_BIG_DOMAIN: return "IDN2_TOO_BIG_DOMAIN";
	case IDN2_TOO_BIG_LABEL: return "IDN2_TOO_BIG_LABEL";
	case IDN2_INVALID_ALABEL: return "IDN2_INVALID_ALABEL";
	case IDN2_INVALID_FLAGS: return "IDN2_INVALID_FLAGS";
	case IDN2_NOT_NFC: return "IDN2_NOT_NFC";
	case IDN2_2HYPHEN: return "IDN2_2HYPHEN";
	case IDN2_HYPHEN_STARTEND: return "IDN2_HYPHEN_STARTEND";
	case IDN2_LEADING_COMBINING: return "IDN2_LEADING_COMBINING";
	case IDN2_DISALLOWED: return "IDN2_DISALLOWED";
	case IDN2_CONTEXTJ: return "IDN2_CONTEXTJ";
	case IDN2_CONTEXTO: return "IDN2_CONTEXTO";
	case IDN2_BIDI: return "IDN2_BIDI";
	case IDN2_DOT_IN_LABEL: return "IDN2_DOT_IN_LABEL";
	default: return "IDN2_ENCODING_ERROR";
	}
}

extern "C" SPRT_GLOBAL void idn2_free(void *ptr) {
	if (ptr) {
		::__sprt_free(ptr);
	}
}

extern "C" SPRT_GLOBAL const char *idn2_check_version(const char *req_version) {
	if (!req_version || ::__sprt_strverscmp(req_version, IDN2_VERSION) <= 0) {
		return IDN2_VERSION;
	}
	return nullptr;
}

} // namespace sprt::idn
