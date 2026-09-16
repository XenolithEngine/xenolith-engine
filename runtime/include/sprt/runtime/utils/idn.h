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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_UTILS_IDN_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_UTILS_IDN_H_

#include <sprt/runtime/stringview.h>
#include <sprt/runtime/enum.h>

namespace sprt::idn {

/** UTS-46 processing options.

	The bit values are ICU's UIDNA_* so that the idn2 shim and anything else
	speaking that vocabulary maps across without a translation table.
*/
enum class Options : uint32_t {
	None = 0,

	/// Restrict ASCII to letters, digits and hyphen (STD3). Off by default:
	/// underscores and the like are common in practice.
	UseStd3Rules = 0x0002,

	/// Enforce the IDNA Bidi Rule (RFC 5893) on right-to-left names.
	CheckBidi = 0x0004,

	/// Enforce CONTEXTJ (RFC 5892 A.1, A.2): ZWJ/ZWNJ only where the script needs them.
	CheckContextJ = 0x0008,

	/// Nontransitional ToASCII: keep sharp s, final sigma and the joiners rather
	/// than folding them to ss / sigma / nothing.
	NonTransitionalToAscii = 0x0010,

	/// Nontransitional ToUnicode. See NonTransitionalToAscii.
	NonTransitionalToUnicode = 0x0020,

	/// Enforce CONTEXTO (RFC 5892 A.3-A.9): middle dot, Greek keraia, Hebrew
	/// geresh, Katakana middle dot, and no mixing of the two Arabic-Indic digit sets.
	CheckContextO = 0x0040,

	/// What the WHATWG URL Standard prescribes, and what a browser does.
	Default = CheckBidi | CheckContextJ | NonTransitionalToAscii | NonTransitionalToUnicode,

	/// The full IDNA2008 profile, as a registry would apply it.
	Idna2008 = Default | CheckContextO,
};

SPRT_DEFINE_ENUM_AS_MASK(Options)

/** Converts a domain name from Unicode (UTF-8) to its ASCII (Punycode) form.

	On success returns Status::Ok and invokes the callback exactly ONCE with the
	result. The StringView handed to the callback is only valid for the duration of
	the call - copy what you need.

	On failure the callback is NOT invoked and the Status names the cause:
	  - one of the Status::ErrorIdn* codes, when UTS-46 rejected the name. A name
	    can break several rules at once; the code reported is the most specific one,
	    by the priority order documented next to the enumerators in status.h.
	  - Status::ErrorInvalidArguemnt for empty input.
	  - Status::ErrorOutOfHostMemory when a working buffer could not be allocated.

	transitionalDifferent, when not null, is set to whether transitional and
	nontransitional processing would disagree about this name - true does not mean
	failure, and it is filled in on the success path too.
*/
SPRT_API Status to_ascii(const callback<void(StringView)> &, StringView source,
		Options = Options::Default, bool *transitionalDifferent = nullptr);

/** Converts a domain name from its ASCII (Punycode) form back to Unicode (UTF-8).

	Same contract as to_ascii().
*/
SPRT_API Status to_unicode(const callback<void(StringView)> &, StringView source,
		Options = Options::Default, bool *transitionalDifferent = nullptr);

/** Single-label variants: the input is one label, and a dot in it is an error
	rather than a separator.
*/
SPRT_API Status label_to_ascii(const callback<void(StringView)> &, StringView label,
		Options = Options::Default, bool *transitionalDifferent = nullptr);
SPRT_API Status label_to_unicode(const callback<void(StringView)> &, StringView label,
		Options = Options::Default, bool *transitionalDifferent = nullptr);

/** Encodes a UTF-8 string as Punycode (RFC 3492) using an internal temporary
	buffer, and returns the result through the callback. With makeUrlPrefix, the
	result is prefixed with "xn--".

	This is the raw bootstring codec, NOT IDNA: it performs no mapping, no
	normalization and no validation. Use to_ascii() for domain names.
*/
SPRT_API Status puny_encode(const callback<void(StringView)> &cb, StringView source,
		bool makeUrlPrefix);

/** Decodes a Punycode string to UTF-8 using an internal temporary buffer, and
	returns the result through the callback. With prefixed, an "xn--" prefix is
	expected and stripped first.

	See puny_encode() on why this is not the same thing as to_unicode().
*/
SPRT_API Status puny_decode(const callback<void(StringView)> &cb, StringView source,
		bool prefixed);

/** Is this top-level domain in the IANA list?

	Accepts both Punycode and native UTF-8 spellings. The list is baked in, from
	https://data.iana.org/TLD/tlds-alpha-by-domain.txt
*/
SPRT_API bool is_known_tld(StringView);

} // namespace sprt::idn

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_UTILS_IDN_H_
