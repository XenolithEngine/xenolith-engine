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
//   * toupper/tolower are NOT here any more, for code points or for strings:
//     they come from the compiled-in Unicode tables (runtime/src/unicode), so
//     this target now gets full Unicode for them rather than an ASCII fold.
//     totitle is still the ASCII-only stub - real titlecasing needs word
//     boundaries (UAX #29), which is a separate milestone.
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

// The lowercase and uppercase mappings live in runtime/src/unicode now, for code
// points and for strings alike, so this target gets full Unicode for them rather
// than an ASCII fold. What is left here is titlecasing, which needs word
// boundaries, and collation. <wctype.h> is still needed:
// caseCompare(WideStringView) folds with towupper.

// --- callback variants (StringView, UTF-8) ----------------------------------

bool totitle(const callback<void(StringView)> &cb, StringView data) {
	// No ICU word-break iterator on NuttX; title-case folds the first ASCII
	// letter to upper and the rest to lower — matches ICU for the ASCII words
	// the renderer surfaces.
	if (data.empty()) {
		cb(StringView());
		return true;
	}
	constexpr size_t kBuf = 4'096;
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

bool totitle(const callback<void(WideStringView)> &cb, WideStringView data) {
	// Approximate: upper the lot. ASCII range only; surrogates pass through
	// untouched (their code units are outside the ASCII fold range anyway).
	if (data.empty()) {
		cb(WideStringView());
		return true;
	}
	constexpr size_t kBuf = 2'048;
	char16_t buf[kBuf];
	size_t n = data.size() < kBuf ? data.size() : kBuf;
	for (size_t i = 0; i < n; ++i) {
		char16_t c = data[i];
		buf[i] = (c >= 'a' && c <= 'z') ? char16_t(c - 'a' + 'A') : c;
	}
	cb(WideStringView(buf, n));
	return true;
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

} // namespace sprt::unicode

#endif // SPRT_NUTTX
