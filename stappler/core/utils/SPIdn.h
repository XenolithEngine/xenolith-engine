/**
Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
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

#ifndef STAPPLER_CORE_UTILS_SPIDN_H_
#define STAPPLER_CORE_UTILS_SPIDN_H_

#include "SPStringView.h" // IWYU pragma: keep

namespace STAPPLER_VERSIONIZED stappler::idn {

template <typename Interface>
SP_PUBLIC auto toAscii(StringView, bool validate = true) -> typename Interface::StringType;

template <typename Interface>
SP_PUBLIC auto toUnicode(StringView, bool validate = false) -> typename Interface::StringType;

template <typename Interface>
SP_PUBLIC auto encodePunycode(StringView) -> typename Interface::StringType;

template <typename Interface>
SP_PUBLIC auto decodePunycode(StringView) -> typename Interface::StringType;

SP_PUBLIC bool isKnownTld(StringView);

} // namespace stappler::idn

#endif /* STAPPLER_CORE_UTILS_SPIDN_H_ */
