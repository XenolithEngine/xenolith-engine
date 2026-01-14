/**
 Copyright (c) 2026 Xenolith Team <admin@stappler.org>

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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_IDN_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_IDN_H_

#include <sprt/runtime/stringview.h>

namespace sprt::idn {

/**
	Encodes a string in UTF-8 in punicode using an internal temporary buffer. Returns the result in callback.
	If makeUrlPrefix is ​​true, prefixes the resulting string with "xn--".

	Return true on success.
*/
SPRT_API bool puny_encode(const callback<void(StringView)> &cb, StringView source,
		bool makeUrlPrefix);

/**
	Decodes a punicode string to UTF-8 using an internal temporary buffer. Returns the result in callback.
	If prefixed is true, checks for "xn--" prefix and remove it before decoding

	Return true on success.
*/
SPRT_API bool puny_decode(const callback<void(StringView)> &cb, StringView source, bool prefixed);

/**
	Checks whether the given top-level domain identifier is in the list of known domains.
	Accepts domain names encoded in both punicode and UTF-8 in the national language.

	A list of domains is available at https://data.iana.org/TLD/tlds-alpha-by-domain.txt
*/
SPRT_API bool is_known_tld(StringView);

/**
	Converts a hostname from a unicode representation in UTF-8 to an ASCII representation.

	If validate == true, first checks that only valid characters are used, otherwise returns false.

	Return true on success.
*/
SPRT_API bool to_ascii(const callback<void(StringView)> &, StringView source, bool validate);

/**
	Converts the hostname from ASCII representation to UTF-8 Unicode.

	If validate == true, first checks that only valid characters are used, otherwise returns false.

	Return true on success.
*/
SPRT_API bool to_unicode(const callback<void(StringView)> &, StringView source, bool validate);

} // namespace sprt::idn

#endif
