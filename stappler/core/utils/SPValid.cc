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

#include "SPValid.h"
#include "SPUrl.h"
#include "SPString.h"
#include <sprt/runtime/platform.h>
#include <sprt/runtime/idn.h>

#include <sys/random.h>

namespace STAPPLER_VERSIONIZED stappler::valid {

namespace chars = sprt::chars;

inline auto Config_getInternalPasswordKey() { return StringView("Serenity Password Salt"); }

/** Identifier starts with [a-zA-Z_] and can contain [a-zA-Z0-9_\-.@] */
bool validateIdentifier(StringView str) {
	if (str.empty()) {
		return false;
	}

	StringView r(str);
	if (!r.is<chars::Compose<char, chars::CharGroup<char, CharGroupId::Alphanumeric>,
					chars::Chars<char, '_'>>>()) {
		return false;
	}

	r.skipChars<chars::CharGroup<char, CharGroupId::Alphanumeric>,
			chars::Chars<char, '_', '-', '.', '@'>>();
	if (!r.empty()) {
		return false;
	}

	return true;
}

/** Text can contain all characters above 0x1F and \t, \r, \n, \b, \f */
bool validateText(StringView str) {
	if (str.empty()) {
		return false;
	}

	// 8 9 10 12 13 - allowed
	StringView r(str);
	r.skipUntil<chars::Range<char, 0, 7>, chars::Range<char, 14, 31>, chars::Chars<char, 11>>();
	if (!r.empty()) {
		return false;
	}

	return true;
}

template <typename Interface>
static bool _validateEmailQuotation(StringView &r, typename Interface::StringType *target) {
	++r;
	if (target) {
		target->push_back('"');
	}
	while (!r.empty() && !r.is('"')) {
		auto pos = r.readUntil<StringView::Chars<'"', '\\'>>();
		if (!pos.empty()) {
			if (target) {
				target->append(pos.data(), pos.size());
			}
		}

		if (r.is('\\')) {
			if (target) {
				target->push_back(r[0]);
			}
			++r;
			if (!r.empty()) {
				if (target) {
					target->push_back(r[0]);
				}
				++r;
			}
		}
	}
	if (r.empty()) {
		return false;
	} else {
		if (target) {
			target->push_back('"');
		}
		++r;
		return true;
	}
}

template <typename Interface>
static bool _validateEmailData(StringView r, typename Interface::StringType *target) {
	using namespace chars;
	using LocalChars = StringView::Compose<StringView::CharGroup<CharGroupId::Alphanumeric>,
			StringView::Chars<'_', '-', '+', '#', '!', '$', '%', '&', '\'', '*', '/', '=', '?', '^',
					'`', '{', '}', '|', '~' >,
			StringView::Range<char(128), char(255)>>;

	using Whitespace = CharGroup<char, CharGroupId::WhiteSpace>;

	r.trimChars<Whitespace>();

	if (r.is('(')) {
		r.skipUntil<StringView::Chars<')'>>();
		if (!r.is(')')) {
			return false;
		}
		r++;
		r.skipChars<Whitespace>();
	}
	if (r.is('"')) {
		if (!_validateEmailQuotation<Interface>(r, target)) {
			return false;
		}
	}

	while (!r.empty() && !r.is('@')) {
		auto pos = r.readChars<LocalChars>();
		if (!pos.empty()) {
			if (target) {
				target->append(pos.data(), pos.size());
			}
		}

		if (r.is('.')) {
			if (target) {
				target->push_back('.');
			}
			++r;
			if (r.is('"')) {
				if (!_validateEmailQuotation<Interface>(r, target)) {
					return false;
				}
				if (!r.is('.') && !r.is('@')) {
					return false;
				} else if (r.is('.')) {
					if (target) {
						target->push_back('.');
					}
					++r;
				}
			} else if (!r.is<LocalChars>()) {
				return false;
			}
		}
		if (r.is('(')) {
			r.skipUntil<StringView::Chars<')'>>();
			if (!r.is(')')) {
				return false;
			}
			r++;
			r.skipChars<Whitespace>();
			break;
		}
		if (!r.is('@') && !r.is<LocalChars>()) {
			return false;
		}
	}

	if (r.empty() || !r.is('@')) {
		return false;
	}

	if (target) {
		target->push_back('@');
	}
	++r;
	if (r.is('(')) {
		r.skipUntil<StringView::Chars<')'>>();
		if (!r.is(')')) {
			return false;
		}
		r++;
		r.skipChars<Whitespace>();
	}

	if (r.is('[')) {
		if (target) {
			target->push_back('[');
		}
		auto pos = r.readUntil<StringView::Chars<']'>>();
		if (r.is(']')) {
			r++;
			if (r.empty()) {
				if (target) {
					target->append(pos.data(), pos.size());
				}
				if (target) {
					target->push_back(']');
				}
			}
		}
	} else {
		if (!UrlView::validateHost(r)) {
			return false;
		}

		bool hasHost = false;
		sprt::idn::to_ascii([&](StringView host) {
			hasHost = true;

			if (target) {
				target->append(host.data(), host.size());
			}
		}, r, false);

		if (!hasHost) {
			return false;
		}
	}

	return true;
}

static StringView _trimValidatingEmail(StringView istr) {
	StringView str(istr);
	str.trimChars<StringView::WhiteSpace>();
	if (str.empty()) {
		return StringView();
	}

	if (str.back() == ')') {
		auto pos = str.rfind('(');
		if (pos != maxOf<size_t>()) {
			str = str.sub(0, pos);
		} else {
			return StringView();
		}
	}

	return str;
}

template <typename Interface>
static bool _validateEmail(typename Interface::StringType &istr) {
	auto str = _trimValidatingEmail(istr);
	if (str.empty()) {
		return false;
	}

	typename Interface::StringType ret;
	ret.reserve(str.size());
	if (_validateEmailData<Interface>(str, &ret)) {
		istr = sp::move(ret);
		return true;
	}
	return false;
}

bool validateEmailWithoutNormalization(StringView istr) {
	auto str = _trimValidatingEmail(istr);
	if (str.empty()) {
		return false;
	}

	return _validateEmailData<memory::PoolInterface>(str, nullptr);
}

bool validateEmail(memory::PoolInterface::StringType &str) {
	return _validateEmail<memory::PoolInterface>(str);
}

bool validateEmail(memory::StandartInterface::StringType &str) {
	return _validateEmail<memory::StandartInterface>(str);
}

template <typename Interface>
static bool _validateUrl(typename Interface::StringType &str) {
	typename Interface::StringType newHost;
	UrlView url;
	if (!url.parse(str)) {
		return false;
	}

	auto oldHost = url.host;
	if (url.host.empty() && url.path.size() < 2) {
		return false;
	}

	if (!oldHost.empty()) {
		if (!sprt::idn::to_ascii([&](StringView host) {
			newHost = host.str<decltype(newHost)>(); //
		}, oldHost, true)) {
			return false;
		}

		if (newHost.empty()) {
			return false;
		}
		url.host = newHost;
	}

	url.get([&](StringView u) {
		str = u.str<typename Interface::StringType>(); //
	});
	return true;
}

bool validateUrl(memory::PoolInterface::StringType &str) {
	return _validateUrl<memory::PoolInterface>(str);
}

bool validateUrl(memory::StandartInterface::StringType &str) {
	return _validateUrl<memory::StandartInterface>(str);
}

bool validateNumber(const StringView &str) {
	if (str.empty()) {
		return false;
	}

	StringView r(str);
	if (r.is('-')) {
		++r;
	}
	r.skipChars<chars::Range<char, '0', '9'>>();
	if (!r.empty()) {
		return false;
	}

	return true;
}

bool validateHexadecimial(const StringView &str) {
	if (str.empty()) {
		return false;
	}

	StringView r(str);
	r.skipChars<chars::CharGroup<char, CharGroupId::Hexadecimial>>();
	if (!r.empty()) {
		return false;
	}

	return true;
}

bool validateBase64(const StringView &str) {
	if (str.empty()) {
		return false;
	}

	StringView r(str);
	r.skipChars<chars::CharGroup<char, CharGroupId::Base64>>();
	if (!r.empty()) {
		return false;
	}

	return true;
}

size_t makeRandomBytes(uint8_t *buf, size_t count) {
	size_t generated = 0;
	auto ret = ::getrandom(buf, count, GRND_RANDOM | GRND_NONBLOCK);
	if (ret < ssize_t(count)) {
		buf += ret;
		count -= ret;
		generated += ret;

		ret = ::getrandom(buf, count, GRND_NONBLOCK | GRND_INSECURE);
		if (ret >= 0) {
			generated += ret;
		}
	} else {
		generated += ret;
	}
	return generated;
}

template <>
auto makeRandomBytes<memory::PoolInterface>(size_t count) -> memory::PoolInterface::BytesType {
	memory::PoolInterface::BytesType ret;
	ret.resize(count);
	makeRandomBytes(ret.data(), count);
	return ret;
}

template <>
auto makeRandomBytes<memory::StandartInterface>(size_t count)
		-> memory::StandartInterface::BytesType {
	memory::StandartInterface::BytesType ret;
	ret.resize(count);
	makeRandomBytes(ret.data(), count);
	return ret;
}

static void makePassword_buf(uint8_t *passwdKey, const StringView &str, const StringView &key) {
	string::Sha512::Buf source = string::Sha512::make(str, Config_getInternalPasswordKey());

	passwdKey[0] = 0;
	passwdKey[1] = 1; // version code
	makeRandomBytes(passwdKey + 2, 14);

	string::Sha512 hash_ctx;
	hash_ctx.update(passwdKey, 16);
	if (!key.empty()) {
		hash_ctx.update(key);
	}
	hash_ctx.update(source);
	hash_ctx.final(passwdKey + 16);
}

template <>
auto makePassword<memory::PoolInterface>(const StringView &str, const StringView &key)
		-> memory::PoolInterface::BytesType {
	if (str.empty() || key.empty()) {
		return memory::PoolInterface::BytesType();
	}

	memory::PoolInterface::BytesType passwdKey;
	passwdKey.resize(16 + string::Sha512::Length);
	makePassword_buf(passwdKey.data(), str, key);
	return passwdKey;
}

template <>
auto makePassword<memory::StandartInterface>(const StringView &str, const StringView &key)
		-> memory::StandartInterface::BytesType {
	if (str.empty() || key.empty()) {
		return memory::StandartInterface::BytesType();
	}

	memory::StandartInterface::BytesType passwdKey;
	passwdKey.resize(16 + string::Sha512::Length);
	makePassword_buf(passwdKey.data(), str, key);
	return passwdKey;
}

bool validatePassord(const StringView &str, const BytesView &passwd, const StringView &key) {
	if (passwd.size() < 8 + string::Sha256::Length) {
		return false; // not a password
	}

	// Serenity/2 protocol
	if (passwd.size() != 16 + string::Sha512::Length) {
		return false;
	}

	string::Sha512::Buf source = string::Sha512::make(str, Config_getInternalPasswordKey());
	uint8_t controlKey[16 + string::Sha512::Length] = {0};
	memcpy(controlKey, passwd.data(), 16);

	string::Sha512 hash_ctx;
	hash_ctx.update(passwd.data(), 16);
	if (!key.empty()) {
		hash_ctx.update(key);
	}
	hash_ctx.update(source);
	hash_ctx.final(controlKey + 16);

	if (memcmp(passwd.data() + 16, controlKey + 16, string::Sha512::Length) == 0) {
		return true;
	} else {
		return false;
	}
}

#define PSWD_NUMBERS "12345679"
#define PSWD_LOWER "abcdefghijkmnopqrstuvwxyz"
#define PSWD_UPPER "ABCDEFGHJKLMNPQRSTUVWXYZ"

static const char *const pswd_numbers = PSWD_NUMBERS;
static const char *const pswd_lower = PSWD_LOWER;
static const char *const pswd_upper = PSWD_UPPER;
static const char *const pswd_all = PSWD_NUMBERS PSWD_LOWER PSWD_UPPER;

static uint8_t pswd_numbersCount = uint8_t(strlen(PSWD_NUMBERS));
static uint8_t pswd_lowerCount = uint8_t(strlen(PSWD_LOWER));
static uint8_t pswd_upperCount = uint8_t(strlen(PSWD_UPPER));
static uint8_t pswd_allCount = uint8_t(strlen(PSWD_NUMBERS PSWD_LOWER PSWD_UPPER));


template <typename Callback>
static void generatePassword_buf(size_t len, const uint8_t *bytes, const Callback &cb) {
	uint16_t meta = 0;
	memcpy(&meta, bytes, sizeof(uint16_t));

	bool extraChars[3] = {false, false, false};
	for (size_t i = 0; i < len - 3; ++i) {
		cb(pswd_all[bytes[i + 5] % pswd_allCount]);
		if (!extraChars[0] && i == bytes[2] % (len - 3)) {
			cb(pswd_numbers[meta % pswd_numbersCount]);
			meta /= pswd_numbersCount;
			extraChars[0] = true;
		}
		if (!extraChars[1] && i == bytes[3] % (len - 3)) {
			cb(pswd_lower[meta % pswd_lowerCount]);
			meta /= pswd_lowerCount;
			extraChars[1] = true;
		}
		if (!extraChars[2] && i == bytes[4] % (len - 3)) {
			cb(pswd_upper[meta % pswd_upperCount]);
			meta /= pswd_upperCount;
			extraChars[2] = true;
		}
	}
}

template <>
auto generatePassword<memory::PoolInterface>(size_t len) -> memory::PoolInterface::StringType {
	if (len < MIN_GENPASSWORD_LENGTH) {
		return memory::PoolInterface::StringType();
	}

	auto bytes = makeRandomBytes<memory::PoolInterface>(len + 2);
	memory::PoolInterface::StringType ret;
	ret.reserve(len);
	generatePassword_buf(len, bytes.data(), [&](char c) { ret.push_back(c); });
	return ret;
}

template <>
auto generatePassword<memory::StandartInterface>(size_t len)
		-> memory::StandartInterface::StringType {
	if (len < MIN_GENPASSWORD_LENGTH) {
		return memory::StandartInterface::StringType();
	}

	auto bytes = makeRandomBytes<memory::StandartInterface>(len + 2);
	memory::StandartInterface::StringType ret;
	ret.reserve(len);
	generatePassword_buf(len, bytes.data(), [&](char c) { ret.push_back(c); });
	return ret;
}

uint32_t readIp(StringView r) {
	bool err = false;
	return readIp(r, err);
}

uint32_t readIp(StringView r, bool &err) {
	uint32_t octets = 0;
	uint32_t ret = 0;
	while (!r.empty() && octets < 4) {
		auto n = r.readChars<StringView::CharGroup<CharGroupId::Numbers>>();
		if (!n.empty()) {
			auto num = n.readInteger(10).get(256);
			if (num < 256) {
				ret = (ret << 8) | uint32_t(num);
			} else {
				err = true;
				return 0;
			}
		}
		if (r.is('.') && octets < 3) {
			++r;
			++octets;
		} else if (octets == 3 && r.empty()) {
			return ret;
		} else {
			err = true;
			return 0;
		}
	}
	err = true;
	return 0;
}

Pair<uint32_t, uint32_t> readIpRange(StringView r) {
	uint32_t start = 0;
	uint32_t end = 0;
	uint32_t mask = 0;

	auto fnReadIp = [](StringView &r, bool &err) -> uint32_t {
		uint32_t octets = 0;
		uint32_t ret = 0;
		while (!r.empty() && octets < 4) {
			auto n = r.readChars<StringView::CharGroup<CharGroupId::Numbers>>();
			if (!n.empty()) {
				auto num = n.readInteger(10).get(256);
				if (num < 256) {
					ret = (ret << 8) | uint32_t(num);
				} else {
					err = true;
					return 0;
				}
			}
			if (r.is('.') && octets < 3) {
				++r;
				++octets;
			} else if (octets == 3) {
				if (r.empty()) {
					return ret;
				} else if (r.is('/') || r.is('-')) {
					return ret;
				}
			} else {
				err = true;
				return 0;
			}
		}
		err = true;
		return 0;
	};

	bool err = false;
	start = fnReadIp(r, err);
	if (err) {
		return pair(0, 0);
	}
	if (r.empty()) {
		return pair(start, start);
	} else if (r.is('-')) {
		++r;
		end = fnReadIp(r, err);
		if (err || !r.empty()) {
			return pair(0, 0);
		} else {
			return pair(start, end);
		}
	} else if (r.is('/')) {
		++r;
		auto tmp = r;
		auto n = tmp.readChars<StringView::CharGroup<CharGroupId::Numbers>>();
		auto num = n.readInteger(10).get(256);
		if (tmp.is('.') && num < 256) {
			mask = fnReadIp(r, err);
			if (err || !r.empty()) {
				return pair(0, 0);
			}

			uint32_t i = 0;
			while ((mask & (1 << i)) == 0) { ++i; }

			while ((mask & (1 << i)) != 0 && i < 32) { ++i; }

			if (i != 32) {
				return pair(0, 0);
			}
		} else if (num < 32 && tmp.empty()) {
			mask = maxOf<uint32_t>() << (32 - num);
			r = tmp;
		} else {
			return pair(0, 0);
		}

		if (!r.empty()) {
			return pair(0, 0);
		}

		uint32_t netstart = (start & mask); // first ip in subnet
		uint32_t netend = (netstart | ~mask); // last ip in subnet

		return pair(netstart, netend);
	}
	return pair(0, 0);
}

} // namespace stappler::valid
