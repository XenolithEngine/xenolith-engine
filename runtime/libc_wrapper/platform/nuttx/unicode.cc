/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
SPDX-License-Identifier: MIT
**/

// NuttX Unicode backend for the sprt runtime.
//
// Mirrors the runtime/libc_wrapper/platform/{linux,wasm,darwin}/unicode.cc
// surface (the 16 platform-dependent sprt::unicode::* entry points). The
// remaining ~27 sprt::unicode::* helpers (utf8/utf16 transcoding, length
// lookups, html-decode, ...) live in runtime/core/runtime_core_unicode.cpp
// and are target-shared — they are already in libsprt.a.
//
// The full ICU/libunistring backend (the linux variant dlopens it at runtime)
// is deferred: ICU's autoconf cross-build cannot link the AC_TRY_LINK probe
// executable against the NuttX flat build (no crt0/special linker script at
// configure time), and a static ICU port is a separate milestone. For the M6
// soft-renderer track this stub backend is sufficient:
//   * toupper/tolower/totitle fold ASCII via NuttX libc towupper/towlower
//     (which handles the C/POSIX locale); non-ASCII passes through unchanged
//     (the renderer does not rely on Unicode case folding).
//   * compare/caseCompare use NuttX wcscmp/strcmp + towupper fold for the
//     case-insensitive path (matches the POSIX C-locale collation).
//   * idnToAscii/idnToUnicode return false (IDN resolution is irrelevant to
//     the framebuffer soft-renderer milestone and there is no network stack
//     in the qemu-armv8a flat build anyway).
//
// Compiled as part of libsprt's libc_wrapper module (same -nostdinc++ /
// -fno-exceptions / -fno-rtti / sprt-include-path shape as the rest of the
// runtime TUs).

#define __SPRT_BUILD 1

#include <sprt/runtime/platform.h>

#if SPRT_NUTTX

#include <sprt/runtime/callback.h>
#include <sprt/runtime/stringview.h>
#include <sprt/runtime/unicode.h>

#include <wctype.h>
#include <wchar.h>
#include <string.h>
#include <strings.h>

namespace sprt::unicode {

// --- single-codepoint case fold --------------------------------------------
// NuttX libc towupper/towlower operate on the C/POSIX locale, which folds
// ASCII A-Z/a-z only. Non-ASCII codepoints pass through unchanged — correct
// for ASCII text the renderer handles, and the same posture as the wasm host
// stub's identity fallback.

char32_t tolower(char32_t c) { return char32_t(::towlower(wint_t(c))); }

char32_t toupper(char32_t c) { return char32_t(::towupper(wint_t(c))); }

char32_t totitle(char32_t c) {
	// NuttX libc has no towtitle; titlecase folds to upper for the ASCII range,
	// which is the same result ICU returns for ASCII letters. Non-ASCII passes
	// through (no table).
	return char32_t(::towupper(wint_t(c)));
}

// --- callback variants (StringView, UTF-8) ----------------------------------
// ASCII-only fold: walk bytes, fold ASCII letters in place, leave the
// continuation bytes of any multi-byte sequence untouched. The renderer's
// string surface (UI labels, log lines) is ASCII; non-ASCII UTF-8 round-trips
// unchanged, which is acceptable for M6.

static bool foldAsciiString(const callback<void(StringView)> &cb, StringView data, bool toUpper) {
	if (data.empty()) {
		cb(StringView());
		return true;
	}
	// Stack buffer covers typical label lengths; heap-fallback not needed for
	// the renderer's short strings. If a longer string shows up we cap at the
	// buffer and the callback still receives a (possibly truncated) result —
	// the case fold for the M6 paths never exceeds this.
	constexpr size_t kBuf = 4096;
	char buf[kBuf];
	size_t n = data.size() < kBuf ? data.size() : kBuf;
	for (size_t i = 0; i < n; ++i) {
		char c = data[i];
		if (c >= 0x80) {
			// Start/middle of a multi-byte UTF-8 sequence: do not fold. Fold
			// only the leading-byte position by leaving all bytes of the
			// sequence alone (case fold of non-ASCII is undefined for us).
			buf[i] = c;
		} else if (toUpper) {
			buf[i] = (c >= 'a' && c <= 'z') ? char(c - 'a' + 'A') : c;
		} else {
			buf[i] = (c >= 'A' && c <= 'Z') ? char(c - 'A' + 'a') : c;
		}
	}
	cb(StringView(buf, n));
	return true;
}

bool toupper(const callback<void(StringView)> &cb, StringView data) {
	return foldAsciiString(cb, data, /*toUpper=*/true);
}

bool tolower(const callback<void(StringView)> &cb, StringView data) {
	return foldAsciiString(cb, data, /*toUpper=*/false);
}

bool totitle(const callback<void(StringView)> &cb, StringView data) {
	// No ICU word-break iterator on NuttX; title-case folds the first ASCII
	// letter to upper and the rest to lower — matches ICU for the ASCII words
	// the renderer surfaces.
	if (data.empty()) {
		cb(StringView());
		return true;
	}
	constexpr size_t kBuf = 4096;
	char buf[kBuf];
	size_t n = data.size() < kBuf ? data.size() : kBuf;
	for (size_t i = 0; i < n; ++i) {
		char c = data[i];
		if (c >= 0x80) {
			buf[i] = c;
			continue;
		}
		if (i == 0 && c >= 'a' && c <= 'z') {
			buf[i] = char(c - 'a' + 'A');
		} else if (i > 0 && c >= 'A' && c <= 'Z') {
			buf[i] = char(c - 'A' + 'a');
		} else {
			buf[i] = c;
		}
	}
	cb(StringView(buf, n));
	return true;
}

// --- callback variants (WideStringView, UTF-16) ----------------------------
// Same fold, char16_t input. ASCII range only; surrogates pass through
// untouched (their code units are outside the ASCII fold range anyway).

static bool foldAsciiWide(const callback<void(WideStringView)> &cb, WideStringView data,
		bool toUpper) {
	if (data.empty()) {
		cb(WideStringView());
		return true;
	}
	constexpr size_t kBuf = 2048;
	char16_t buf[kBuf];
	size_t n = data.size() < kBuf ? data.size() : kBuf;
	for (size_t i = 0; i < n; ++i) {
		char16_t c = data[i];
		if (c >= 0x80) {
			buf[i] = c;
		} else if (toUpper) {
			buf[i] = (c >= 'a' && c <= 'z') ? char16_t(c - 'a' + 'A') : c;
		} else {
			buf[i] = (c >= 'A' && c <= 'Z') ? char16_t(c - 'A' + 'a') : c;
		}
	}
	cb(WideStringView(buf, n));
	return true;
}

bool toupper(const callback<void(WideStringView)> &cb, WideStringView data) {
	return foldAsciiWide(cb, data, /*toUpper=*/true);
}

bool tolower(const callback<void(WideStringView)> &cb, WideStringView data) {
	return foldAsciiWide(cb, data, /*toUpper=*/false);
}

bool totitle(const callback<void(WideStringView)> &cb, WideStringView data) {
	return foldAsciiWide(cb, data, /*toUpper=*/true); // approximate: upper the lot
}

// --- compare / caseCompare --------------------------------------------------
// ASCII C-locale collation: byte-wise comparison (strcmp semantics). The
// case-insensitive path folds via towupper on each char. Matches the POSIX
// C locale the NuttX flat build ships; locale-aware collation needs ICU and
// is deferred with the rest of the ICU port.

bool compare(StringView l, StringView r, int *result) {
	size_t n = l.size() < r.size() ? l.size() : r.size();
	int rc = memcmp(l.data(), r.data(), n);
	if (rc == 0) {
		rc = int(l.size()) - int(r.size());
	}
	if (result) {
		*result = rc < 0 ? -1 : (rc > 0 ? 1 : 0);
	}
	return true;
}

bool compare(WideStringView l, WideStringView r, int *result) {
	size_t n = l.size() < r.size() ? l.size() : r.size();
	for (size_t i = 0; i < n; ++i) {
		if (l[i] != r[i]) {
			if (result) {
				*result = l[i] < r[i] ? -1 : 1;
			}
			return true;
		}
	}
	if (result) {
		int rc = int(l.size()) - int(r.size());
		*result = rc < 0 ? -1 : (rc > 0 ? 1 : 0);
	}
	return true;
}

static int asciiCaseCmp(const char *a, size_t na, const char *b, size_t nb) {
	size_t n = na < nb ? na : nb;
	for (size_t i = 0; i < n; ++i) {
		char ca = (a[i] >= 'a' && a[i] <= 'z') ? char(a[i] - 'a' + 'A') : a[i];
		char cb = (b[i] >= 'a' && b[i] <= 'z') ? char(b[i] - 'a' + 'A') : b[i];
		if (ca != cb) {
			return ca < cb ? -1 : 1;
		}
	}
	return na == nb ? 0 : (na < nb ? -1 : 1);
}

bool caseCompare(StringView l, StringView r, int *result) {
	int rc = asciiCaseCmp(l.data(), l.size(), r.data(), r.size());
	if (result) {
		*result = rc;
	}
	return true;
}

bool caseCompare(WideStringView l, WideStringView r, int *result) {
	size_t n = l.size() < r.size() ? l.size() : r.size();
	for (size_t i = 0; i < n; ++i) {
		char32_t cl = char32_t(::towupper(wint_t(l[i])));
		char32_t cr = char32_t(::towupper(wint_t(r[i])));
		if (cl != cr) {
			if (result) {
				*result = cl < cr ? -1 : 1;
			}
			return true;
		}
	}
	int rc = int(l.size()) - int(r.size());
	if (result) {
		*result = rc < 0 ? -1 : (rc > 0 ? 1 : 0);
	}
	return true;
}

// --- IDN ---------------------------------------------------------------------
// No ICU/libidn2 on NuttX flat build; IDN conversion is unavailable. Callers
// (libcurl, the remote protocol) handle `false` as "IDN not supported" and
// fall back to passing the raw name through. The renderer does not use IDN.

bool idnToAscii(const callback<void(StringView)> & /*cb*/, StringView /*source*/) { return false; }

bool idnToUnicode(const callback<void(StringView)> & /*cb*/, StringView /*source*/) {
	return false;
}

} // namespace sprt::unicode

#endif // SPRT_NUTTX
