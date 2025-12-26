/**
Copyright (c) 2016-2022 Roman Katuntsev <sbkarr@stappler.org>
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

/*
 * Sha2 source code in public domain
 * */

#include "SPCoreCrypto.h"

namespace STAPPLER_VERSIONIZED stappler::crypto {

Sha1::Buf Sha1::make(const CoderSource &source, const StringView &salt) {
	return Sha1().update(salt.empty() ? StringView(SP_SECURE_KEY) : salt).update(source).final();
}

Sha1::Buf Sha1::hmac(const CoderSource &data, const CoderSource &key) {
	Buf ret;
	std::array<uint8_t, Length * 2> keyData = {0};

	Sha1 shaCtx;
	if (key.size() > Length * 2) {
		shaCtx.update(key).final(keyData.data());
	} else {
		memcpy(keyData.data(), key.data(), key.size());
	}

	for (auto &it : keyData) { it ^= HMAC_I_PAD; }

	shaCtx.init().update(keyData).update(data).final(ret.data());

	for (auto &it : keyData) { it ^= HMAC_I_PAD ^ HMAC_O_PAD; }

	shaCtx.init().update(keyData).update(ret).final(ret.data());
	return ret;
}

Sha1::Sha1() { sprt::sha1::sha_init(ctx); }
Sha1 &Sha1::init() {
	sprt::sha1::sha_init(ctx);
	return *this;
}

Sha1 &Sha1::update(const uint8_t *ptr, size_t len) {
	if (len > 0) {
		sprt::sha1::sha_process(ctx, ptr, (uint32_t)len);
	}
	return *this;
}

Sha1 &Sha1::update(const CoderSource &source) { return update(source.data(), source.size()); }

Sha1::Buf Sha1::final() {
	Sha1::Buf ret;
	sprt::sha1::sha_done(ctx, ret.data());
	return ret;
}
void Sha1::final(uint8_t *buf) { sprt::sha1::sha_done(ctx, buf); }

Sha512::Buf Sha512::make(const CoderSource &source, const StringView &salt) {
	return Sha512().update(salt.empty() ? StringView(SP_SECURE_KEY) : salt).update(source).final();
}

Sha512::Buf Sha512::hmac(const CoderSource &data, const CoderSource &key) {
	Buf ret;
	std::array<uint8_t, Length * 2> keyData = {0};

	Sha512 shaCtx;
	if (key.size() > Length * 2) {
		shaCtx.update(key).final(keyData.data());
	} else {
		memcpy(keyData.data(), key.data(), key.size());
	}

	for (auto &it : keyData) { it ^= HMAC_I_PAD; }

	shaCtx.init().update(keyData).update(data).final(ret.data());

	for (auto &it : keyData) { it ^= HMAC_I_PAD ^ HMAC_O_PAD; }

	shaCtx.init().update(keyData).update(ret).final(ret.data());
	return ret;
}

Sha512::Sha512() { sprt::sha512::sha_init(ctx); }
Sha512 &Sha512::init() {
	sprt::sha512::sha_init(ctx);
	return *this;
}

Sha512 &Sha512::update(const uint8_t *ptr, size_t len) {
	if (len > 0) {
		sprt::sha512::sha_process(ctx, ptr, (uint32_t)len);
	}
	return *this;
}

Sha512 &Sha512::update(const CoderSource &source) { return update(source.data(), source.size()); }

Sha512::Buf Sha512::final() {
	Sha512::Buf ret;
	sprt::sha512::sha_done(ctx, ret.data());
	return ret;
}
void Sha512::final(uint8_t *buf) { sprt::sha512::sha_done(ctx, buf); }


Sha256::Buf Sha256::make(const CoderSource &source, const StringView &salt) {
	return Sha256().update(salt.empty() ? StringView(SP_SECURE_KEY) : salt).update(source).final();
}

Sha256::Buf Sha256::hmac(const CoderSource &data, const CoderSource &key) {
	Buf ret;
	std::array<uint8_t, Length * 2> keyData = {0};
	memset(keyData.data(), 0, keyData.size());

	Sha256 shaCtx;
	if (key.size() > Length * 2) {
		shaCtx.init().update(key).final(keyData.data());
	} else {
		memcpy(keyData.data(), key.data(), key.size());
	}

	for (auto &it : keyData) { it ^= HMAC_I_PAD; }

	shaCtx.init().update(keyData).update(data).final(ret.data());

	for (auto &it : keyData) { it ^= HMAC_I_PAD ^ HMAC_O_PAD; }

	shaCtx.init().update(keyData).update(ret).final(ret.data());
	return ret;
}

Sha256::Sha256() { sprt::sha256::sha_init(ctx); }
Sha256 &Sha256::init() {
	sprt::sha256::sha_init(ctx);
	return *this;
}

Sha256 &Sha256::update(const uint8_t *ptr, size_t len) {
	if (len) {
		sprt::sha256::sha_process(ctx, ptr, (uint32_t)len);
	}
	return *this;
}

Sha256 &Sha256::update(const CoderSource &source) { return update(source.data(), source.size()); }

Sha256::Buf Sha256::final() {
	Sha256::Buf ret;
	sprt::sha256::sha_done(ctx, ret.data());
	return ret;
}
void Sha256::final(uint8_t *buf) { sprt::sha256::sha_done(ctx, buf); }

} // namespace stappler::crypto
