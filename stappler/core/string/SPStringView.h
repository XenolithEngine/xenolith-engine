/**
Copyright (c) 2016-2022 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>
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

#ifndef STAPPLER_CORE_STRING_SPSTRINGVIEW_H_
#define STAPPLER_CORE_STRING_SPSTRINGVIEW_H_

// Umbrella header for StringView implementation

#include "SPBytesReader.h"
#include "SPStringDetail.h" // IWYU pragma: keep

namespace STAPPLER_VERSIONIZED stappler {

// Fast reader for char string
// Matching function based on templates
//
// Usage:
//   using StringView::Chars;
//   using StringView::Range;
//
//   reader.readUntil<Chars<' ', '\n', '\r', '\t'>>();
//   reader.readChars<Chars<'-', '+', '.', 'e'>, Range<'0', '9'>>();
//

using CallbackStream = Callback<void(StringView)>;

SP_PUBLIC StringView getStatusName(Status);

// Returns status description (strerror), thread-safe
// Callback will be called exactly one time, do not store StringView from it directly!
SP_PUBLIC void getStatusDescription(Status, const Callback<void(StringView)> &cb);

} // namespace STAPPLER_VERSIONIZED stappler

namespace STAPPLER_VERSIONIZED stappler::memory {

template <typename Char>
inline auto makeCallback(std::basic_ostream<Char> &stream) {
	return makeCallback([&](StringViewBase<Char> str) { stream << str; });
}

} // namespace stappler::memory

#endif /* STAPPLER_CORE_STRING_SPSTRINGVIEW_H_ */
