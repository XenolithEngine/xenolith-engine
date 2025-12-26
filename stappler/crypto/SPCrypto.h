/**
Copyright (c) 2021-2022 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023-2024 Stappler LLC <admin@stappler.dev>
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

#ifndef STAPPLER_CRYPTO_SPCRYPTO_H_
#define STAPPLER_CRYPTO_SPCRYPTO_H_

#include "SPIO.h"
#include "SPBytesView.h"
#include "SPCoreCrypto.h"

namespace STAPPLER_VERSIONIZED stappler::crypto {

// Гарантирует единственность шифротекста для разных бэкэндов в блочных шифрах
// ценой производетльности. Если выключено - шифротексты могут различаться
// (при этом сохраняется интероперабельность)
#if DEBUG
constexpr bool SAFE_BLOCK_ENCODING = true;
#else
constexpr bool SAFE_BLOCK_ENCODING = false;
#endif

constexpr size_t BlockKeySize256 = 32; // 256-bit

class PublicKey;

enum class Backend : uint32_t {
	Default,
	MbedTLS,
	OpenSSL,
	GnuTLS,
	Custom = 32,
	Embedded = maxOf<uint32_t>() - 1
};

enum class BackendFlags : uint32_t {
	None = 0,
	SecureLibrary = 1 << 0,
	SupportsPKCS1 = 1 << 1,
	SupportsPKCS8 = 1 << 2,
	SupportsECDSA = 1 << 3,
	SupportsAes = 1 << 4,
	SupportsGost3412_2015 = 1 << 5,
	SupportsGost3410_2012 = 1 << 6, // ЭЦП
};

SP_DEFINE_ENUM_AS_MASK(BackendFlags)

enum class BlockCipher : uint16_t {
	AES_CBC,
	AES_CFB8,
	Gost3412_2015_CTR_ACPKM
};

enum class KeyType : uint32_t {
	Unknown,
	RSA,
	DSA,
	ECDSA,
	GOST3410_2012_256,
	GOST3410_2012_512,
	EDDSA_ED448,
};

enum class HashFunction {
	SHA_2,
	GOST_3411,
};

enum class SignAlgorithm {
	RSA_SHA256,
	RSA_SHA512,
	ECDSA_SHA256,
	ECDSA_SHA512,
	GOST_256, // GOST R 34.11-2012 256 bit
	GOST_512, // GOST R 34.11-2012 512 bit
};

enum class KeyBits {
	_1024,
	_2048,
	_4096
};

enum class KeyFormat {
	PKCS1,
	PKCS8,
	RSA = PKCS1,
};

struct KeyContext {
	void *cryptoCtx = nullptr;
	void *keyCtx = nullptr;
	uint32_t padding = 0;
	KeyType type = KeyType::Unknown;
	void *backendCtx = nullptr;
};

struct SP_PUBLIC BlockKey256 {
	uint16_t version = 0; // keygen version
	BlockCipher cipher = BlockCipher::AES_CBC;
	std::array<uint8_t, BlockKeySize256> data = {0};

	bool operator==(const BlockKey256 &) const = default;
	bool operator!=(const BlockKey256 &) const = default;
};

struct BlockCryptoHeader {
	uint64_t size;
	uint16_t version;
	uint16_t cipher;
	uint32_t padding;
};

struct BlockInfo {
	uint64_t dataSize = 0;
	uint16_t version = 0; // keygen version
	BlockCipher cipher = BlockCipher::AES_CBC;
};

class SP_PUBLIC PrivateKey {
public:
	PrivateKey(Backend = Backend::Default);
	PrivateKey(Backend, BytesView, const CoderSource &passwd = CoderSource());
	PrivateKey(BytesView, const CoderSource &passwd = CoderSource());
	~PrivateKey();

	PrivateKey(const PrivateKey &) = delete;
	PrivateKey &operator=(const PrivateKey &) = delete;

	PrivateKey(PrivateKey &&);
	PrivateKey &operator=(PrivateKey &&);

	bool generate(KeyType type = KeyType::RSA);
	bool generate(KeyBits = KeyBits::_2048, KeyType type = KeyType::RSA);

	bool import(BytesView, const CoderSource &passwd = CoderSource());

	PublicKey exportPublic() const;

	Backend getBackend() const;
	KeyContext getKey() const { return _key; }
	KeyType getType() const { return _key.type; }

	explicit operator bool() const { return _valid && _loaded; }

	bool exportPem(const Callback<void(BytesView)> &, KeyFormat = KeyFormat::PKCS8,
			const CoderSource &passPhrase = StringView()) const;
	bool exportPem(const Callback<void(BytesView)> &, const CoderSource &passPhrase) const;

	bool exportDer(const Callback<void(BytesView)> &, KeyFormat = KeyFormat::PKCS8,
			const CoderSource &passPhrase = StringView()) const;
	bool exportDer(const Callback<void(BytesView)> &, const CoderSource &passPhrase) const;

	bool sign(const Callback<void(BytesView)> &, const CoderSource &,
			SignAlgorithm = SignAlgorithm::RSA_SHA512) const;
	bool verify(const CoderSource &data, BytesView signature, SignAlgorithm) const;

	bool fingerprint(const Callback<void(BytesView)> &, const CoderSource &) const;

	bool isGenerateSupported(KeyType) const;
	bool isSupported(KeyFormat) const;

	bool encrypt(const Callback<void(BytesView)> &, const CoderSource &);
	bool decrypt(const Callback<void(BytesView)> &, const CoderSource &);

protected:
	bool _loaded = false;
	bool _valid = false;
	KeyContext _key;
};

class SP_PUBLIC PublicKey {
public:
	PublicKey(Backend = Backend::Default);
	PublicKey(Backend, BytesView);
	PublicKey(BytesView);
	PublicKey(const PrivateKey &);
	~PublicKey();

	PublicKey(const PublicKey &) = delete;
	PublicKey &operator=(const PublicKey &) = delete;

	PublicKey(PublicKey &&);
	PublicKey &operator=(PublicKey &&);

	bool import(BytesView);
	bool importOpenSSH(StringView);

	Backend getBackend() const;
	KeyContext getKey() const { return _key; }
	KeyType getType() const { return _key.type; }

	explicit operator bool() const { return _valid && _loaded; }

	bool exportPem(const Callback<void(BytesView)> &) const; // only pkcs8
	bool exportDer(const Callback<void(BytesView)> &) const; // only pkcs8

	bool verify(const CoderSource &data, BytesView signature, SignAlgorithm) const;

	bool encrypt(const Callback<void(BytesView)> &, const CoderSource &);

protected:
	bool _loaded = false;
	bool _valid = false;
	KeyContext _key;
};

inline constexpr size_t getBlockSize(BlockCipher c) {
	switch (c) {
	case BlockCipher::AES_CBC:
	case BlockCipher::AES_CFB8: return 16; break;
	case BlockCipher::Gost3412_2015_CTR_ACPKM: return 16; break;
	}
	return 16;
}

SP_PUBLIC void listBackends(const Callback<void(Backend, StringView, BackendFlags)> &);

SP_PUBLIC bool isPemKey(BytesView data);

SP_PUBLIC bool encryptBlock(const BlockKey256 &, BytesView, const Callback<void(BytesView)> &);
SP_PUBLIC bool encryptBlock(Backend b, const BlockKey256 &, BytesView,
		const Callback<void(BytesView)> &);

SP_PUBLIC bool decryptBlock(const BlockKey256 &, BytesView, const Callback<void(BytesView)> &);
SP_PUBLIC bool decryptBlock(Backend b, const BlockKey256 &, BytesView,
		const Callback<void(BytesView)> &);

SP_PUBLIC BlockKey256 makeBlockKey(Backend, BytesView pkey, BytesView hash,
		BlockCipher = BlockCipher::AES_CBC, uint32_t version = 2);
SP_PUBLIC BlockKey256 makeBlockKey(BytesView pkey, BytesView hash,
		BlockCipher = BlockCipher::AES_CBC, uint32_t version = 2);
SP_PUBLIC BlockKey256 makeBlockKey(const PrivateKey &pkey, BytesView hash, uint32_t version = 2);
SP_PUBLIC BlockKey256 makeBlockKey(const PrivateKey &pkey, BytesView hash, BlockCipher,
		uint32_t version = 2);

// get keygen version from encrypted block
SP_PUBLIC BlockInfo getBlockInfo(BytesView);

using HashCoderCallback = const Callback<bool(const CoderSource &)>;

SP_PUBLIC Sha256::Buf hash256(Backend, const Callback<void(const HashCoderCallback &upd)> &,
		HashFunction func = HashFunction::SHA_2);
SP_PUBLIC Sha256::Buf hash256(const Callback<void(const HashCoderCallback &upd)> &,
		HashFunction func = HashFunction::SHA_2);
SP_PUBLIC Sha256::Buf hash256(Backend, const CoderSource &,
		HashFunction func = HashFunction::SHA_2);
SP_PUBLIC Sha256::Buf hash256(const CoderSource &, HashFunction func = HashFunction::SHA_2);

SP_PUBLIC Sha512::Buf hash512(Backend, const Callback<void(const HashCoderCallback &upd)> &,
		HashFunction func = HashFunction::SHA_2);
SP_PUBLIC Sha512::Buf hash512(const Callback<void(const HashCoderCallback &upd)> &,
		HashFunction func = HashFunction::SHA_2);
SP_PUBLIC Sha512::Buf hash512(Backend, const CoderSource &,
		HashFunction func = HashFunction::SHA_2);
SP_PUBLIC Sha512::Buf hash512(const CoderSource &, HashFunction func = HashFunction::SHA_2);

} // namespace stappler::crypto

#endif /* STAPPLER_CRYPTO_SPCRYPTO_H_ */
