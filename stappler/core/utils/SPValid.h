/**
Copyright (c) 2019 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

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

#ifndef STAPPLER_CORE_UTILS_SPVALID_H_
#define STAPPLER_CORE_UTILS_SPVALID_H_

#include "SPStringView.h"
#include "SPStringView.h"

namespace STAPPLER_VERSIONIZED stappler::valid {

/** Identifier starts with [a-zA-Z_] and can contain [a-zA-Z0-9_\-.@] */
SP_PUBLIC bool validateIdentifier(StringView str);

/** Text can contain all characters above 0x1F and \t, \r, \n, \b, \f */
SP_PUBLIC bool validateText(StringView str);

SP_PUBLIC bool validateEmailWithoutNormalization(StringView str);
SP_PUBLIC bool validateEmail(memory::PoolInterface::StringType &str);
SP_PUBLIC bool validateEmail(memory::StandartInterface::StringType &str);

SP_PUBLIC bool validateUrl(memory::PoolInterface::StringType &str);
SP_PUBLIC bool validateUrl(memory::StandartInterface::StringType &str);

SP_PUBLIC bool validateNumber(const StringView &str);
SP_PUBLIC bool validateHexadecimial(const StringView &str);
SP_PUBLIC bool validateBase64(const StringView &str);

SP_PUBLIC void makeRandomBytes(uint8_t *, size_t);

template <typename Interface>
SP_PUBLIC auto makeRandomBytes(size_t) -> typename Interface::BytesType;

template <typename Interface>
SP_PUBLIC auto makePassword(const StringView &str, const StringView &key = StringView()) ->
		typename Interface::BytesType;

SP_PUBLIC bool validatePassord(const StringView &str, const BytesView &passwd,
		const StringView &key = StringView());

static constexpr size_t MIN_GENPASSWORD_LENGTH = 6;

// Minimal length is 6
template <typename Interface>
SP_PUBLIC auto generatePassword(size_t len) -> typename Interface::StringType;

SP_PUBLIC uint32_t readIp(StringView r);
SP_PUBLIC uint32_t readIp(StringView r, bool &err);
SP_PUBLIC Pair<uint32_t, uint32_t> readIpRange(StringView r);

} // namespace stappler::valid

#endif /* STAPPLER_CORE_UTILS_SPVALID_H_ */
