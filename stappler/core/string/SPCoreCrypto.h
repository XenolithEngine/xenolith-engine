/**
Copyright (c) 2021-2022 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023-2924 Stappler LLC <admin@stappler.dev>

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

#ifndef STAPPLER_CORE_STRING_SPCRYPTO_H_
#define STAPPLER_CORE_STRING_SPCRYPTO_H_

#include "SPIO.h"
#include "SPStringView.h"
#include <sprt/runtime/hash.h>

namespace STAPPLER_VERSIONIZED stappler {

struct SP_PUBLIC CoderSource {
	CoderSource(const uint8_t *d, size_t len);
	CoderSource(const char *d, size_t len);
	CoderSource(const char *d);
	CoderSource(const StringView &d);

	CoderSource(const typename memory::PoolInterface::BytesType &d);
	CoderSource(const typename memory::StandartInterface::BytesType &d);

	CoderSource(const typename memory::PoolInterface::StringType &d);
	CoderSource(const typename memory::StandartInterface::StringType &d);

	template <sprt::endian Order>
	CoderSource(const BytesViewTemplate<Order> &d);

	CoderSource(const BytesReader<uint8_t> &d);
	CoderSource(const BytesReader<char> &d);

	template <size_t Size>
	CoderSource(const std::array<uint8_t, Size> &d);

	CoderSource();

	CoderSource(const CoderSource &) = delete;
	CoderSource(CoderSource &&) = delete;

	CoderSource &operator=(const CoderSource &) = delete;
	CoderSource &operator=(CoderSource &&) = delete;

	BytesViewTemplate<sprt::endian::network> _data;
	size_t _offset = 0;

	const uint8_t *data() const;
	size_t size() const;
	bool empty() const;

	BytesViewTemplate<sprt::endian::network> view() const;

	uint8_t operator[](size_t s) const;

	size_t read(uint8_t *buf, size_t nbytes);
	size_t seek(int64_t offset, io::Seek s);
	size_t tell() const;
};

} // namespace STAPPLER_VERSIONIZED stappler


namespace STAPPLER_VERSIONIZED stappler::crypto {

constexpr uint8_t HMAC_I_PAD = 0x36;
constexpr uint8_t HMAC_O_PAD = 0x5C;

#ifndef SP_SECURE_KEY
constexpr auto SP_SECURE_KEY = "Nev3rseenany0nesoequalinth1sscale";
#endif

/* SHA-1 160-bit context
 * designed for chain use: Sha1().update(input).final() */
struct SP_PUBLIC Sha1 {
	using _Ctx = sprt::sha1::Ctx;

	constexpr static uint32_t Length = 20;
	using Buf = std::array<uint8_t, Length>;

	static Buf make(const CoderSource &, const StringView &salt = StringView());
	static Buf hmac(const CoderSource &data, const CoderSource &key);

	template <typename... Args>
	static Buf perform(Args &&...args);

	Sha1();
	Sha1 &init();

	Sha1 &update(const uint8_t *, size_t);
	Sha1 &update(const CoderSource &);

	template <typename T, typename... Args>
	void _update(T &&t, Args &&...args);

	template <typename T>
	void _update(T &&t);

	Buf final();
	void final(uint8_t *);

	_Ctx ctx;
};

/* SHA-2 512-bit context
 * designed for chain use: Sha512().update(input).final() */
struct SP_PUBLIC Sha512 {
	using _Ctx = sprt::sha512::Ctx;

	constexpr static uint32_t Length = 64;
	using Buf = std::array<uint8_t, Length>;

	static Buf make(const CoderSource &, const StringView &salt = StringView());
	static Buf hmac(const CoderSource &data, const CoderSource &key);

	template <typename... Args>
	static Buf perform(Args &&...args);

	Sha512();
	Sha512 &init();

	Sha512 &update(const uint8_t *, size_t);
	Sha512 &update(const CoderSource &);

	template <typename T, typename... Args>
	void _update(T &&t, Args &&...args);

	template <typename T>
	void _update(T &&t);

	Buf final();
	void final(uint8_t *);

	_Ctx ctx;
};

/* SHA-2 256-bit context
 * designed for chain use: Sha256().update(input).final() */
struct SP_PUBLIC Sha256 {
	using _Ctx = sprt::sha256::Ctx;

	constexpr static uint32_t Length = 32;
	using Buf = std::array<uint8_t, Length>;

	static Buf make(const CoderSource &, const StringView &salt = StringView());
	static Buf hmac(const CoderSource &data, const CoderSource &key);

	template <typename... Args>
	static Buf perform(Args &&...args);

	Sha256();
	Sha256 &init();

	Sha256 &update(const uint8_t *, size_t);
	Sha256 &update(const CoderSource &);

	template <typename T, typename... Args>
	void _update(T &&t, Args &&...args);

	template <typename T>
	void _update(T &&t);

	Buf final();
	void final(uint8_t *);

	_Ctx ctx;
};

union alignas(16) uint512_u {
	unsigned long long QWORD[8];
	unsigned char B[64];
};

/* GOST R 34.11-2012 hash context */
struct Gost3411_Ctx {
	uint512_u buffer;
	uint512_u h;
	uint512_u N;
	uint512_u Sigma;
	size_t bufsize;
	unsigned int digest_size;
};

struct SP_PUBLIC Gost3411_512 {
	using _Ctx = Gost3411_Ctx;

	constexpr static uint32_t Length = 64;
	using Buf = std::array<uint8_t, Length>;

	template <typename... Args>
	static Buf perform(Args &&...args);

	static Buf make(const CoderSource &, const StringView &salt = StringView());
	static Buf hmac(const CoderSource &data, const CoderSource &key);

	Gost3411_512();
	Gost3411_512 &init();

	Gost3411_512 &update(const uint8_t *, size_t);
	Gost3411_512 &update(const CoderSource &);

	template <typename T, typename... Args>
	void _update(T &&t, Args &&...args);

	template <typename T>
	void _update(T &&t);

	Buf final();
	void final(uint8_t *);

	_Ctx ctx;
};

struct SP_PUBLIC Gost3411_256 {
	using _Ctx = Gost3411_Ctx;

	constexpr static uint32_t Length = 32;
	using Buf = std::array<uint8_t, Length>;

	template <typename... Args>
	static Buf perform(Args &&...args);

	static Buf make(const CoderSource &, const StringView &salt = StringView());
	static Buf hmac(const CoderSource &data, const CoderSource &key);

	Gost3411_256();
	Gost3411_256 &init();

	Gost3411_256 &update(const uint8_t *, size_t);
	Gost3411_256 &update(const CoderSource &);

	template <typename T, typename... Args>
	void _update(T &&t, Args &&...args);

	template <typename T>
	void _update(T &&t);

	Buf final();
	void final(uint8_t *);

	_Ctx ctx;
};

} // namespace stappler::crypto


namespace STAPPLER_VERSIONIZED stappler {

inline CoderSource::CoderSource(const uint8_t *d, size_t len) : _data(d, len) { }

inline CoderSource::CoderSource(const char *d, size_t len) : _data((uint8_t *)d, len) { }

inline CoderSource::CoderSource(const char *d) : _data((uint8_t *)d, strlen(d)) { }

inline CoderSource::CoderSource(const StringView &d) : _data((uint8_t *)d.data(), d.size()) { }

inline CoderSource::CoderSource(const typename memory::PoolInterface::BytesType &d)
: _data(d.data(), d.size()) { }

inline CoderSource::CoderSource(const typename memory::StandartInterface::BytesType &d)
: _data(d.data(), d.size()) { }

inline CoderSource::CoderSource(const typename memory::PoolInterface::StringType &d)
: _data((const uint8_t *)d.data(), d.size()) { }

inline CoderSource::CoderSource(const typename memory::StandartInterface::StringType &d)
: _data((const uint8_t *)d.data(), d.size()) { }

template <sprt::endian Order>
inline CoderSource::CoderSource(const BytesViewTemplate<Order> &d) : _data(d.data(), d.size()) { }

inline CoderSource::CoderSource(const BytesReader<uint8_t> &d) : _data(d.data(), d.size()) { }

inline CoderSource::CoderSource(const BytesReader<char> &d)
: _data((const uint8_t *)d.data(), d.size()) { }

template <size_t Size>
inline CoderSource::CoderSource(const std::array<uint8_t, Size> &d)
: _data(d.data(), Size), _offset(0) { }

inline CoderSource::CoderSource() { }

inline const uint8_t *CoderSource::data() const { return _data.data() + _offset; }
inline size_t CoderSource::size() const { return _data.size() - _offset; }
inline bool CoderSource::empty() const { return _data.empty() || _offset == _data.size(); }

inline BytesViewTemplate<sprt::endian::network> CoderSource::view() const { return _data; }

inline uint8_t CoderSource::operator[](size_t s) const { return _data[s + _offset]; }

inline size_t CoderSource::read(uint8_t *buf, size_t nbytes) {
	const auto remains = _data.size() - _offset;
	if (nbytes > remains) {
		nbytes = remains;
	}
	memcpy(buf, _data.data() + _offset, nbytes);
	_offset += nbytes;
	return nbytes;
}

inline size_t CoderSource::seek(int64_t offset, io::Seek s) {
	switch (s) {
	case io::Seek::Current:
		if (offset + _offset > _data.size()) {
			_offset = _data.size();
		} else if (offset + int64_t(_offset) < 0) {
			_offset = 0;
		} else {
			_offset += offset;
		}
		break;
	case io::Seek::End:
		if (offset > 0) {
			_offset = _data.size();
		} else if (size_t(-offset) > _data.size()) {
			_offset = 0;
		} else {
			_offset = size_t(-offset);
		}
		break;
	case io::Seek::Set:
		if (offset < 0) {
			_offset = 0;
		} else if (size_t(offset) <= _data.size()) {
			_offset = size_t(offset);
		} else {
			_offset = _data.size();
		}
		break;
	}
	return _offset;
}

inline size_t CoderSource::tell() const { return _offset; }

} // namespace STAPPLER_VERSIONIZED stappler


namespace STAPPLER_VERSIONIZED stappler::crypto {

template <typename... Args>
inline Sha512::Buf Sha512::perform(Args &&...args) {
	Sha512 ctx;
	ctx._update(std::forward<Args>(args)...);
	return ctx.final();
}

template <typename T, typename... Args>
inline void Sha512::_update(T &&t, Args &&...args) {
	update(std::forward<T>(t));
	_update(std::forward<Args>(args)...);
}

template <typename T>
inline void Sha512::_update(T &&t) {
	update(std::forward<T>(t));
}

template <typename... Args>
inline Sha256::Buf Sha256::perform(Args &&...args) {
	Sha256 ctx;
	ctx._update(std::forward<Args>(args)...);
	return ctx.final();
}

template <typename T, typename... Args>
inline void Sha256::_update(T &&t, Args &&...args) {
	update(std::forward<T>(t));
	_update(std::forward<Args>(args)...);
}

template <typename T>
inline void Sha256::_update(T &&t) {
	update(std::forward<T>(t));
}

template <typename... Args>
inline Gost3411_512::Buf Gost3411_512::perform(Args &&...args) {
	Gost3411_512 c;
	c._update(std::forward<Args>(args)...);
	return c.final();
}

template <typename T, typename... Args>
inline void Gost3411_512::_update(T &&t, Args &&...args) {
	update(std::forward<T>(t));
	_update(std::forward<Args>(args)...);
}

template <typename T>
inline void Gost3411_512::_update(T &&t) {
	update(std::forward<T>(t));
}

template <typename... Args>
inline Gost3411_256::Buf Gost3411_256::perform(Args &&...args) {
	Gost3411_256 c;
	c._update(std::forward<Args>(args)...);
	return c.final();
}

template <typename T, typename... Args>
inline void Gost3411_256::_update(T &&t, Args &&...args) {
	update(std::forward<T>(t));
	_update(std::forward<Args>(args)...);
}

template <typename T>
inline void Gost3411_256::_update(T &&t) {
	update(std::forward<T>(t));
}

} // namespace stappler::crypto


namespace STAPPLER_VERSIONIZED stappler::io {

template <>
struct ProducerTraits<CoderSource> {
	using type = CoderSource;
	static size_t ReadFn(void *ptr, uint8_t *buf, size_t nbytes) {
		return ((type *)ptr)->read(buf, nbytes);
	}

	static size_t SeekFn(void *ptr, int64_t offset, Seek s) {
		return ((type *)ptr)->seek(offset, s);
	}
	static size_t TellFn(void *ptr) { return ((type *)ptr)->tell(); }
};

} // namespace stappler::io

#endif /* STAPPLER_CORE_STRING_SPCRYPTO_H_ */
