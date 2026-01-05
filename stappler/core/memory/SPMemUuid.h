/**
Copyright (c) 2019-2022 Roman Katuntsev <sbkarr@stappler.org>
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

#ifndef STAPPLER_CORE_MEMORY_SPMEMUUID_H_
#define STAPPLER_CORE_MEMORY_SPMEMUUID_H_

#include "SPCore.h"
#include <sprt/runtime/mem/string.h>
#include <sprt/runtime/mem/vector.h>
#include "SPStringView.h" // IWYU pragma: keep

namespace STAPPLER_VERSIONIZED stappler::memory {

struct SP_PUBLIC uuid : AllocPool {
	static constexpr size_t FormattedLength = 36;

	using uuid_t = std::array<uint8_t, 16>;

	static bool parse(uuid_t &, StringView str);
	static void format(char *, const uuid_t &);
	static uuid generate();

	uuid() noexcept { memset(_uuid.data(), 0, 16); }

	uuid(StringView str) noexcept { parse(_uuid, str); }

	uuid(BytesView b) noexcept {
		if (b.size() == 16) {
			memcpy(_uuid.data(), b.data(), 16);
		}
	}

	uuid(const uuid_t &u) noexcept : _uuid(u) { }
	uuid(const uuid &u) noexcept : _uuid(u._uuid) { }

	uuid &operator=(const uuid &u) noexcept {
		_uuid = u._uuid;
		return *this;
	}

	uuid &operator=(const sprt::memory::string &str) noexcept {
		parse(_uuid, str.data());
		return *this;
	}

	uuid &operator=(const std::string &str) noexcept {
		parse(_uuid, str.data());
		return *this;
	}

	uuid &operator=(const sprt::memory::vector<uint8_t> &b) noexcept {
		if (b.size() == 16) {
			memcpy(_uuid.data(), b.data(), 16);
		}
		return *this;
	}

	uuid &operator=(const std::vector<uint8_t> &b) noexcept {
		if (b.size() == 16) {
			memcpy(_uuid.data(), b.data(), 16);
		}
		return *this;
	}

	template <typename Str = sprt::memory::string>
	auto str() const -> Str {
		char buf[FormattedLength] = {0};
		format(buf, _uuid);
		return Str(buf, FormattedLength);
	}

	template <typename B = sprt::memory::vector<uint8_t>>
	auto bytes() const -> B {
		return B(_uuid.data(), _uuid.data() + 16);
	}

	uuid_t array() const { return _uuid; }

	BytesView view() const { return BytesView(_uuid); }

	const uint8_t *data() const { return _uuid.data(); }
	size_t size() const { return 16; }

	bool operator==(const uuid &other) const {
		return ::memcmp(_uuid.data(), other._uuid.data(), _uuid.size()) == 0;
	}

	bool operator!=(const uuid &other) const {
		return ::memcmp(_uuid.data(), other._uuid.data(), _uuid.size()) != 0;
	}

	bool operator<(const uuid &other) const {
		return ::memcmp(_uuid.data(), other._uuid.data(), _uuid.size()) < 0;
	}

	bool operator<=(const uuid &other) const {
		return ::memcmp(_uuid.data(), other._uuid.data(), _uuid.size()) <= 0;
	}

	bool operator>(const uuid &other) const {
		return ::memcmp(_uuid.data(), other._uuid.data(), _uuid.size()) > 0;
	}

	bool operator>=(const uuid &other) const {
		return ::memcmp(_uuid.data(), other._uuid.data(), _uuid.size()) >= 0;
	}

	uuid_t _uuid;
};

} // namespace stappler::memory

#endif /* STAPPLER_CORE_MEMORY_SPMEMUUID_H_ */
