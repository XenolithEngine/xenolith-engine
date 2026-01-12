/**
Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>
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

#include "SPCrypto.h"
#include "SPCore.h"
#include "SPString.h"
#include "SPValid.h"
#include "SPMemory.h"

namespace STAPPLER_VERSIONIZED stappler::crypto {

struct BackendCtx {
	static BackendCtx *get(Backend);

	Backend name;
	StringView title;
	BackendFlags flags;

	void (*initialize)(BackendCtx &) = nullptr;
	void (*finalize)(BackendCtx &) = nullptr;

	bool (*encryptBlock)(const BlockKey256 &key, BytesView d,
			const Callback<void(BytesView)> &cb) = nullptr;
	bool (*decryptBlock)(const BlockKey256 &key, BytesView d,
			const Callback<void(BytesView)> &cb) = nullptr;

	bool (*hash256)(Sha256::Buf &, const Callback<void(const HashCoderCallback &upd)> &cb,
			HashFunction func);
	bool (*hash512)(Sha512::Buf &, const Callback<void(const HashCoderCallback &upd)> &cb,
			HashFunction func);

	bool (*privInit)(KeyContext &ctx) = nullptr;
	void (*privFree)(KeyContext &ctx) = nullptr;

	bool (*privGen)(KeyContext &ctx, KeyBits, KeyType) = nullptr;
	bool (*privImport)(KeyContext &ctx, BytesView data, const CoderSource &passwd) = nullptr;
	bool (*privExportPem)(const KeyContext &ctx, const Callback<void(BytesView)> &cb, KeyFormat fmt,
			const CoderSource &passPhrase) = nullptr;
	bool (*privExportDer)(const KeyContext &ctx, const Callback<void(BytesView)> &cb, KeyFormat fmt,
			const CoderSource &passPhrase) = nullptr;
	bool (*privExportPublic)(KeyContext &target, const KeyContext &privKey) = nullptr;
	bool (*privSign)(const KeyContext &ctx, const Callback<void(BytesView)> &cb,
			const CoderSource &data, SignAlgorithm algo) = nullptr;
	bool (*privVerify)(const KeyContext &ctx, const CoderSource &data, BytesView signature,
			SignAlgorithm algo) = nullptr;
	bool (*privEncrypt)(const KeyContext &ctx, const Callback<void(BytesView)> &cb,
			const CoderSource &data) = nullptr;
	bool (*privDecrypt)(const KeyContext &ctx, const Callback<void(BytesView)> &cb,
			const CoderSource &data) = nullptr;
	bool (*privFingerprint)(const KeyContext &ctx, const Callback<void(BytesView)> &cb,
			const CoderSource &data) = nullptr;

	bool (*pubInit)(KeyContext &ctx) = nullptr;
	void (*pubFree)(KeyContext &ctx) = nullptr;

	bool (*pubImport)(KeyContext &ctx, BytesView data) = nullptr;
	bool (*pubImportOpenSSH)(KeyContext &ctx, StringView data) = nullptr;
	bool (*pubExportPem)(const KeyContext &ctx, const Callback<void(BytesView)> &cb) = nullptr;
	bool (*pubExportDer)(const KeyContext &ctx, const Callback<void(BytesView)> &cb) = nullptr;
	bool (*pubVerify)(const KeyContext &ctx, const CoderSource &data, BytesView signature,
			SignAlgorithm algo) = nullptr;
	bool (*pubEncrypt)(const KeyContext &ctx, const Callback<void(BytesView)> &cb,
			const CoderSource &data) = nullptr;
};

struct BackendCtxRef {
	BackendCtxRef(BackendCtx *);
	~BackendCtxRef();

	BackendCtx *backend;
};

struct BackendInterface {
	static void initialize(void *ptr) { reinterpret_cast<BackendInterface *>(ptr)->init(); }
	static void terminate(void *ptr) { reinterpret_cast<BackendInterface *>(ptr)->term(); }

	BackendInterface() { addInitializer(this, initialize, terminate); }

	void init() {
		for (auto &it : backends) {
			if (it.second->initialize) {
				it.second->initialize(*it.second);
			}
		}
	}
	void term() {
		for (auto &it : backends) {
			if (it.second->finalize) {
				it.second->finalize(*it.second);
			}
		}
	}

	mem_std::HashMap<Backend, BackendCtx *> backends;
};

static BackendInterface s_interface;

BackendCtx *BackendCtx::get(Backend b) {
	auto it = s_interface.backends.find(b);
	if (it != s_interface.backends.end()) {
		return it->second;
	}

	if (b == Backend::Default) {
		BackendCtx *ret = nullptr;
#if MODULE_STAPPLER_CRYPTO_GNUTLS
		if (!ret) {
			ret = get(Backend::GnuTLS);
		}
#endif
#if MODULE_STAPPLER_CRYPTO_OPENSSL
		if (!ret) {
			ret = get(Backend::OpenSSL);
		}
#endif
#if MODULE_STAPPLER_CRYPTO_MBEDTLS
		if (!ret) {
			ret = get(Backend::MbedTLS);
		}
#endif
		if (!ret) {
			ret = get(Backend::Embedded);
		}
		return ret;
	}

	return nullptr;
}

BackendCtxRef::BackendCtxRef(BackendCtx *ctx) {
	s_interface.backends.emplace(ctx->name, ctx);
	backend = ctx;
}

BackendCtxRef::~BackendCtxRef() { s_interface.backends.erase(backend->name); }

void listBackends(const Callback<void(Backend, StringView, BackendFlags)> &cb) {
	for (auto &it : s_interface.backends) { cb(it.first, it.second->title, it.second->flags); }
}

bool isPemKey(BytesView data) {
	StringView str(reinterpret_cast<const char *>(data.data()), data.size());
	str.skipUntilString("-----");
	if (str.is("-----")) {
		return true;
	}
	return false;
}

static bool isBackendValidForBlock(BackendCtx *b, BlockCipher c) {
	switch (c) {
	case BlockCipher::AES_CBC:
	case BlockCipher::AES_CFB8:
		if ((b->flags & BackendFlags::SupportsAes) != BackendFlags::None && b->encryptBlock
				&& b->decryptBlock) {
			return true;
		}
		break;
	case BlockCipher::Gost3412_2015_CTR_ACPKM:
		if ((b->flags & BackendFlags::SupportsGost3412_2015) != BackendFlags::None
				&& b->encryptBlock && b->decryptBlock) {
			return true;
		}
		break;
	}
	return false;
}

static BackendCtx *findBackendForBlock(BlockCipher c) {
	// check default

	auto check = [&](BackendCtx *b, bool secure) -> BackendCtx * {
		if (b
				&& (b->flags & BackendFlags::SecureLibrary)
						== (secure ? BackendFlags::SecureLibrary : BackendFlags::None)) {
			if (isBackendValidForBlock(b, c)) {
				return b;
			}
		}
		return nullptr;
	};

	auto b = check(BackendCtx::get(Backend::Default), true);
	if (b) {
		return b;
	}

	// check secure libs first
	for (auto &it : s_interface.backends) {
		if (check(it.second, true)) {
			return it.second;
		}
	}

	for (auto &it : s_interface.backends) {
		if (check(it.second, false)) {
			return it.second;
		}
	}
	return nullptr;
}

static void fillCryptoBlockHeader(uint8_t *buf, const BlockKey256 &key, BytesView d) {
	uint64_t dataSize = d.size();

	BlockCryptoHeader header;
	header.size = sprt::byteorder::HostToLittle(dataSize);
	header.version = sprt::byteorder::HostToLittle(key.version);
	header.cipher = sprt::byteorder::HostToLittle(toInt(key.cipher));
	header.padding = 0;

	memcpy(buf, &header, sizeof(BlockCryptoHeader));
}

static SignAlgorithm getSignForBlockCipher(const PrivateKey &key) {
	switch (key.getType()) {
	case KeyType::GOST3410_2012_256: return SignAlgorithm::GOST_256; break;
	case KeyType::GOST3410_2012_512: return SignAlgorithm::GOST_512; break;
	default: break;
	}
	return SignAlgorithm::RSA_SHA512;
}

bool encryptBlock(const BlockKey256 &key, BytesView data, const Callback<void(BytesView)> &cb) {
	auto b = findBackendForBlock(key.cipher);
	return b->encryptBlock(key, data, cb);
}

bool encryptBlock(Backend b, const BlockKey256 &key, BytesView data,
		const Callback<void(BytesView)> &cb) {
	auto backend = BackendCtx::get(b);
	if (!backend || !backend->encryptBlock) {
		return false;
	}
	return backend->encryptBlock(key, data, cb);
}

bool decryptBlock(const BlockKey256 &key, BytesView data, const Callback<void(BytesView)> &cb) {
	auto b = findBackendForBlock(key.cipher);
	return b->decryptBlock(key, data, cb);
}

bool decryptBlock(Backend b, const BlockKey256 &key, BytesView data,
		const Callback<void(BytesView)> &cb) {
	auto backend = BackendCtx::get(b);
	if (!backend || !backend->decryptBlock) {
		return false;
	}
	return backend->decryptBlock(key, data, cb);
}

BlockKey256 makeBlockKey(Backend b, BytesView pkey, BytesView hash, BlockCipher c,
		uint32_t version) {
	crypto::PrivateKey pk(b, pkey);
	if (pk && version > 0) {
		auto ret = makeBlockKey(pk, hash, c, version);
		if (ret.version == 0) {
			ret.data = string::Sha256().update(hash).update(pkey).final();
		}
		ret.cipher = c;
		return ret;
	} else {
		return BlockKey256{0, c, string::Sha256().update(hash).update(pkey).final()};
	}
}

BlockKey256 makeBlockKey(BytesView pkey, BytesView hash, BlockCipher b, uint32_t version) {
	return makeBlockKey(Backend::Default, pkey, hash, b, version);
}

BlockKey256 makeBlockKey(const PrivateKey &pkey, BytesView hash, uint32_t version) {
	switch (pkey.getType()) {
	case KeyType::GOST3410_2012_256:
	case KeyType::GOST3410_2012_512:
		return makeBlockKey(pkey, hash, BlockCipher::Gost3412_2015_CTR_ACPKM, version);
		break;
	default: break;
	}
	return makeBlockKey(pkey, hash, BlockCipher::AES_CBC, version);
}

BlockKey256 makeBlockKey(const PrivateKey &pkey, BytesView hash, BlockCipher b, uint32_t version) {
	BlockKey256 ret;
	ret.cipher = b;
	if (version == 2) {
		ret.version = 0;
		switch (b) {
		case BlockCipher::AES_CBC:
		case BlockCipher::AES_CFB8:
			pkey.sign([&](BytesView data) {
				ret.version = version;
				ret.data = hash256(pkey.getBackend(), CoderSource(data), HashFunction::SHA_2);
			}, hash, getSignForBlockCipher(pkey));
			break;
		case BlockCipher::Gost3412_2015_CTR_ACPKM:
			pkey.fingerprint([&](BytesView data) {
				ret.version = version;
				ret.data = Gost3411_256::hmac(hash, data);
			}, hash);
			break;
		}
	} else if (version == 1) {
		if (!pkey.sign([&](BytesView data) {
			auto s = std::min(data.size(), size_t(256));
			ret.data = hash256(pkey.getBackend(), CoderSource(BytesView(data, s)),
					HashFunction::SHA_2);
			ret.version = version;
		}, hash, getSignForBlockCipher(pkey))) {
			ret.version = 0;
		}
	} else {
		ret.version = 0;
	}
	return ret;
}

BlockInfo getBlockInfo(BytesView val) {
	BlockInfo ret;
	BytesViewTemplate<sprt::endian::little> b(val);
	ret.dataSize = b.readUnsigned64();
	ret.version = b.readUnsigned16();
	ret.cipher = BlockCipher(b.readUnsigned16());
	return ret;
}

Sha256::Buf hash256(Backend b, const Callback<void(const HashCoderCallback &upd)> &cb,
		HashFunction func) {
	auto embedded = [&] {
		switch (func) {
		case HashFunction::SHA_2: {
			Sha256 ctx;
			cb([&](const CoderSource &data) {
				ctx.update(data);
				return true;
			});
			return ctx.final();
			break;
		}
		case HashFunction::GOST_3411:
			Gost3411_256 ctx;
			cb([&](const CoderSource &data) {
				ctx.update(data);
				return true;
			});
			return ctx.final();
			break;
		}
		return Sha256::Buf();
	};

	auto bctx = BackendCtx::get(b);
	if (!bctx) {
		return embedded();
	} else {
		Sha256::Buf ctx;
		if (!bctx->hash256 || !bctx->hash256(ctx, cb, func)) {
			return embedded();
		}
		return ctx;
	}
}

Sha256::Buf hash256(Backend b, const CoderSource &data, HashFunction func) {
	return hash256(b,
			Callback<void(const HashCoderCallback &upd)>(
					[&](const HashCoderCallback &upd) { upd(data); }),
			func);
}

Sha512::Buf hash512(Backend b, const Callback<void(const HashCoderCallback &upd)> &cb,
		HashFunction func) {
	auto embedded = [&] {
		switch (func) {
		case HashFunction::SHA_2: {
			Sha512 ctx;
			cb([&](const CoderSource &data) {
				ctx.update(data);
				return true;
			});
			return ctx.final();
			break;
		}
		case HashFunction::GOST_3411:
			Gost3411_512 ctx;
			cb([&](const CoderSource &data) {
				ctx.update(data);
				return true;
			});
			return ctx.final();
			break;
		}
		return Sha512::Buf();
	};

	auto bctx = BackendCtx::get(b);
	if (!bctx) {
		return embedded();
	} else {
		Sha512::Buf ctx;
		if (!bctx->hash512 || !bctx->hash512(ctx, cb, func)) {
			return embedded();
		}
		return ctx;
	}
}

Sha512::Buf hash512(Backend b, const CoderSource &data, HashFunction func) {
	return hash512(b,
			Callback<void(const HashCoderCallback &upd)>(
					[&](const HashCoderCallback &upd) { upd(data); }),
			func);
}

Sha256::Buf hash256(const Callback<void(const Callback<bool(const CoderSource &)> &upd)> &cb,
		HashFunction func) {
	return hash256(Backend::Default, cb, func);
}
Sha256::Buf hash256(const CoderSource &data, HashFunction func) {
	return hash256(Backend::Default, data, func);
}

Sha512::Buf hash512(const Callback<void(const Callback<bool(const CoderSource &)> &upd)> &cb,
		HashFunction func) {
	return hash512(Backend::Default, cb, func);
}
Sha512::Buf hash512(const CoderSource &data, HashFunction func) {
	return hash512(Backend::Default, data, func);
}

PrivateKey::PrivateKey(Backend b) : _valid(true), _key() {
	auto backend = BackendCtx::get(b);
	if (!backend) {
		_valid = false;
		return;
	}

	if (!backend->privInit || !backend->privInit(_key)) {
		_valid = false;
	}

	_key.backendCtx = backend;
}

PrivateKey::PrivateKey(Backend b, BytesView data, const CoderSource &str) : PrivateKey(b) {
	this->import(data, str);
}

PrivateKey::PrivateKey(BytesView data, const CoderSource &str) : PrivateKey(Backend::Default) {
	this->import(data, str);
}

PrivateKey::~PrivateKey() {
	if (_valid) {
		auto backend = static_cast<BackendCtx *>(_key.backendCtx);
		if (backend && backend->privFree) {
			backend->privFree(_key);
			_valid = false;
		}
	}
}

PrivateKey::PrivateKey(PrivateKey &&other) {
	_key = other._key;
	_valid = other._valid;
	_loaded = other._loaded;

	other._valid = false;
	other._loaded = false;
	other._key.cryptoCtx = nullptr;
	other._key.keyCtx = nullptr;
	other._key.backendCtx = nullptr;
}

PrivateKey &PrivateKey::operator=(PrivateKey &&other) {
	if (_valid) {
		auto backend = static_cast<BackendCtx *>(_key.backendCtx);
		if (backend && backend->privFree) {
			backend->privFree(_key);
			_valid = false;
		}
	}

	_key = other._key;
	_valid = other._valid;
	_loaded = other._loaded;

	other._valid = false;
	other._loaded = false;
	other._key.cryptoCtx = nullptr;
	other._key.keyCtx = nullptr;
	other._key.backendCtx = nullptr;
	return *this;
}

bool PrivateKey::generate(KeyType type) { return generate(KeyBits::_4096, type); }

bool PrivateKey::generate(KeyBits bits, KeyType type) {
	if (!_valid) {
		return false;
	}

	auto backend = static_cast<BackendCtx *>(_key.backendCtx);
	if (backend && backend->privGen && backend->privGen(_key, bits, type)) {
		_loaded = true;
		return true;
	}

	return false;
}

bool PrivateKey::import(BytesView data, const CoderSource &passwd) {
	if (_loaded || !_valid || data.empty()) {
		return false;
	}

	auto backend = static_cast<BackendCtx *>(_key.backendCtx);
	if (backend && backend->privImport && backend->privImport(_key, data, passwd)) {
		_loaded = true;
		return true;
	}

	return false;
}

PublicKey PrivateKey::exportPublic() const { return PublicKey(*this); }

Backend PrivateKey::getBackend() const {
	auto backend = static_cast<BackendCtx *>(_key.backendCtx);
	return backend->name;
}

bool PrivateKey::exportPem(const Callback<void(BytesView)> &cb, KeyFormat fmt,
		const CoderSource &passPhrase) const {
	if (!_loaded || !_valid) {
		return false;
	}

	if (fmt == KeyFormat::RSA && getType() != KeyType::RSA) {
		log::source().error("Crypto", "Unable to export non-RSA key in PKCS#1 format");
		return false;
	}

	auto backend = static_cast<BackendCtx *>(_key.backendCtx);
	if (backend && backend->privExportPem && backend->privExportPem(_key, cb, fmt, passPhrase)) {
		return true;
	}

	return false;
}

bool PrivateKey::exportPem(const Callback<void(BytesView)> &cb,
		const CoderSource &passPhrase) const {
	return exportPem(cb, KeyFormat::PKCS8, passPhrase);
}

bool PrivateKey::exportDer(const Callback<void(BytesView)> &cb, KeyFormat fmt,
		const CoderSource &passPhrase) const {
	if (!_loaded || !_valid) {
		return false;
	}

	if (fmt == KeyFormat::RSA && getType() != KeyType::RSA) {
		log::source().error("Crypto", "Unable to export non-RSA key in PKCS#1 format");
		return false;
	}

	auto backend = static_cast<BackendCtx *>(_key.backendCtx);
	if (backend && backend->privExportDer && backend->privExportDer(_key, cb, fmt, passPhrase)) {
		return true;
	}

	return false;
}

bool PrivateKey::exportDer(const Callback<void(BytesView)> &cb,
		const CoderSource &passPhrase) const {
	return exportDer(cb, KeyFormat::PKCS8, passPhrase);
}

bool PrivateKey::sign(const Callback<void(BytesView)> &cb, const CoderSource &data,
		SignAlgorithm algo) const {
	if (!_loaded || !_valid) {
		return false;
	}

	auto backend = static_cast<BackendCtx *>(_key.backendCtx);
	if (backend && backend->privSign && backend->privSign(_key, cb, data, algo)) {
		return true;
	}

	return false;
}

bool PrivateKey::verify(const CoderSource &data, BytesView signature, SignAlgorithm algo) const {
	if (!_loaded || !_valid) {
		return false;
	}

	auto backend = static_cast<BackendCtx *>(_key.backendCtx);
	if (backend && backend->privVerify && backend->privVerify(_key, data, signature, algo)) {
		return true;
	}

	return false;
}

bool PrivateKey::fingerprint(const Callback<void(BytesView)> &cb, const CoderSource &data) const {
	if (!_loaded || !_valid) {
		return false;
	}

	auto backend = static_cast<BackendCtx *>(_key.backendCtx);
	if (backend && backend->privFingerprint && backend->privFingerprint(_key, cb, data)) {
		return true;
	}

	return false;
}

bool PrivateKey::isGenerateSupported(KeyType type) const {
	auto backend = static_cast<BackendCtx *>(_key.backendCtx);
	switch (type) {
	case KeyType::RSA:
		return (backend->flags & BackendFlags::SecureLibrary) != BackendFlags::None;
		break;
	case KeyType::GOST3410_2012_256:
	case KeyType::GOST3410_2012_512:
		return (backend->flags & BackendFlags::SupportsGost3410_2012) != BackendFlags::None;
		break;

	case KeyType::Unknown:
	case KeyType::DSA:
	case KeyType::ECDSA:
	case KeyType::EDDSA_ED448: return false; break;
	}
	return false;
}

bool PrivateKey::isSupported(KeyFormat fmt) const {
	auto backend = static_cast<BackendCtx *>(_key.backendCtx);
	switch (fmt) {
	case KeyFormat::PKCS1:
		return (backend->flags & BackendFlags::SupportsPKCS1) != BackendFlags::None;
		break;
	case KeyFormat::PKCS8:
		return (backend->flags & BackendFlags::SupportsPKCS8) != BackendFlags::None;
		break;
	}
	return false;
}

bool PrivateKey::encrypt(const Callback<void(BytesView)> &cb, const CoderSource &data) {
	if (!_loaded || !_valid) {
		return false;
	}

	auto backend = static_cast<BackendCtx *>(_key.backendCtx);
	if (backend && backend->privEncrypt) {
		return backend->privEncrypt(_key, cb, data);
	}

	return false;
}

bool PrivateKey::decrypt(const Callback<void(BytesView)> &cb, const CoderSource &data) {
	if (!_loaded || !_valid) {
		return false;
	}

	auto backend = static_cast<BackendCtx *>(_key.backendCtx);
	if (backend && backend->privDecrypt) {
		return backend->privDecrypt(_key, cb, data);
	}

	return false;
}

PublicKey::PublicKey(Backend b) : _valid(true), _key() {
	auto backend = BackendCtx::get(b);
	if (!backend) {
		_valid = false;
		return;
	}

	if (!backend->pubInit || !backend->pubInit(_key)) {
		_valid = false;
	}
	_key.backendCtx = backend;
}

PublicKey::PublicKey(Backend b, BytesView data) : PublicKey(b) {
	if (data.starts_with(reinterpret_cast<const uint8_t *>("ssh-rsa"), "ssh-rsa"_len)) {
		importOpenSSH(StringView(reinterpret_cast<const char *>(data.data()), data.size()));
	} else {
		this->import(data);
	}
}

PublicKey::PublicKey(BytesView data) : PublicKey(Backend::Default) {
	if (data.starts_with(reinterpret_cast<const uint8_t *>("ssh-rsa"), "ssh-rsa"_len)) {
		importOpenSSH(StringView(reinterpret_cast<const char *>(data.data()), data.size()));
	} else {
		this->import(data);
	}
}

PublicKey::PublicKey(const PrivateKey &priv) : _valid(false) {
	auto backend = static_cast<BackendCtx *>(priv.getKey().backendCtx);
	if (backend && backend->privExportPublic && backend->privExportPublic(_key, priv.getKey())) {
		_valid = true;
		_loaded = true;
		_key.backendCtx = priv.getKey().backendCtx;
	}
}

PublicKey::~PublicKey() {
	if (_valid) {
		auto backend = static_cast<BackendCtx *>(_key.backendCtx);
		if (backend && backend->pubFree) {
			backend->pubFree(_key);
			_valid = false;
		}
	}
}

PublicKey::PublicKey(PublicKey &&other) {
	_key = other._key;
	_valid = other._valid;
	_loaded = other._loaded;

	other._valid = false;
	other._loaded = false;
	other._key.cryptoCtx = nullptr;
	other._key.keyCtx = nullptr;
	other._key.backendCtx = nullptr;
}

PublicKey &PublicKey::operator=(PublicKey &&other) {
	if (_valid) {
		auto backend = static_cast<BackendCtx *>(_key.backendCtx);
		if (backend && backend->pubFree) {
			backend->pubFree(_key);
			_valid = false;
		}
	}

	_key = other._key;
	_valid = other._valid;
	_loaded = other._loaded;

	other._valid = false;
	other._loaded = false;
	other._key.cryptoCtx = nullptr;
	other._key.keyCtx = nullptr;
	other._key.backendCtx = nullptr;
	return *this;
}

bool PublicKey::import(BytesView data) {
	if (_loaded || !_valid || data.empty()) {
		return false;
	}

	auto backend = static_cast<BackendCtx *>(_key.backendCtx);
	if (backend && backend->pubImport && backend->pubImport(_key, data)) {
		_loaded = true;
		return true;
	}

	return false;
}

bool PublicKey::importOpenSSH(StringView r) {
	if (!_valid || _loaded) {
		return false;
	}

	auto backend = static_cast<BackendCtx *>(_key.backendCtx);
	if (backend && backend->pubImportOpenSSH && backend->pubImportOpenSSH(_key, r)) {
		_loaded = true;
		return true;
	}

	return false;
}

Backend PublicKey::getBackend() const {
	auto backend = static_cast<BackendCtx *>(_key.backendCtx);
	return backend->name;
}

bool PublicKey::exportPem(const Callback<void(BytesView)> &cb) const {
	if (!_loaded || !_valid) {
		return false;
	}

	auto backend = static_cast<BackendCtx *>(_key.backendCtx);
	if (backend && backend->pubExportPem && backend->pubExportPem(_key, cb)) {
		return true;
	}

	return false;
}

bool PublicKey::exportDer(const Callback<void(BytesView)> &cb) const {
	if (!_loaded || !_valid) {
		return false;
	}

	auto backend = static_cast<BackendCtx *>(_key.backendCtx);
	if (backend && backend->pubExportDer && backend->pubExportDer(_key, cb)) {
		return true;
	}

	return false;
}

bool PublicKey::verify(const CoderSource &data, BytesView signature, SignAlgorithm algo) const {
	if (!_loaded || !_valid) {
		return false;
	}

	auto backend = static_cast<BackendCtx *>(_key.backendCtx);
	if (backend && backend->pubVerify && backend->pubVerify(_key, data, signature, algo)) {
		return true;
	}

	return false;
}

bool PublicKey::encrypt(const Callback<void(BytesView)> &cb, const CoderSource &data) {
	if (!_loaded || !_valid) {
		return false;
	}

	auto backend = static_cast<BackendCtx *>(_key.backendCtx);
	if (backend && backend->pubEncrypt) {
		return backend->pubEncrypt(_key, cb, data);
	}

	return false;
}

static uint8_t *writeRSAKey(uint8_t *buf, BytesViewNetwork mod, BytesViewNetwork exp) {
	size_t modSize = 1;
	size_t expSize = 1;

	auto readSize = [&](size_t s) {
		if (s < 128) {
			return 1;
		} else if (s < 256) {
			return 2;
		} else {
			return 3;
		}
	};

	auto writeSize = [&](size_t s) {
		if (s < 128) {
			*buf = uint8_t(s);
			++buf;
		} else if (s < 256) {
			*buf = uint8_t(0x81);
			++buf;
			*buf = uint8_t(s);
			++buf;
		} else {
			*buf = uint8_t(0x82);
			++buf;
			*buf = uint8_t((s >> 8) & 0xFF);
			++buf;
			*buf = uint8_t(s & 0xFF);
			++buf;
		}
	};

	modSize += readSize(mod.size()) + mod.size();
	expSize += readSize(exp.size()) + exp.size();

	*buf = uint8_t(0x30);
	++buf;
	writeSize(modSize + expSize);

	*buf = uint8_t(0x02);
	++buf;
	writeSize(mod.size());
	for (size_t i = 0; i < mod.size(); ++i) {
		*buf = mod.at(i);
		++buf;
	}

	*buf = uint8_t(0x02);
	++buf;
	writeSize(exp.size());
	for (size_t i = 0; i < exp.size(); ++i) {
		*buf = exp.at(i);
		++buf;
	}

	return buf;
}

} // namespace stappler::crypto

#ifdef __LCC__
#pragma diag_suppress 2'464
#pragma diag_suppress 1'444
#endif

#include "SPCrypto-gost.cc"
#include "SPCrypto-openssl.cc"
#include "SPCrypto-mbedtls.cc"
#include "SPCrypto-gnutls.cc"

#ifdef __LCC__
#pragma diag_default 2'464
#pragma diag_default 1'444
#endif
