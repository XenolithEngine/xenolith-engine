/**
Copyright (c) 2021 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>

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

#include "SPString.h"
#include "SPLog.h"
#include "SPValid.h"
#include "SPCrypto.h"

#if __CDT_PARSER__
#define MODULE_STAPPLER_CRYPTO_GNUTLS 1
#endif

#if MODULE_STAPPLER_CRYPTO_GNUTLS

#include "SPCryptoAsn1.h"

#include <gnutls/abstract.h>
#include <gnutls/crypto.h>
#include <gnutls/gnutls.h>
#include <nettle/bignum.h>
#include <nettle/gostdsa.h>
#include <nettle/ecc.h>
#include <nettle/ecc-curve.h>
#include <gmp.h>

namespace STAPPLER_VERSIONIZED stappler::crypto {

struct Hook_GnuTLS_RandomData {
	const Gost3411_512::Buf *buf;
	size_t offset;
};

static void hook_gnutls_random_func(void *ctx, size_t length, uint8_t *dst) {
	Hook_GnuTLS_RandomData *data = static_cast<Hook_GnuTLS_RandomData *>(ctx);
	memcpy(dst, data->buf->data() + data->offset, length);
	data->offset += length;
}

static void hook_gnutls_mpi_print(const mpz_t *p, void *buffer, size_t *nbytes) {
	unsigned int size = nettle_mpz_sizeinbase_256_u(*p);
	if (buffer) {
		nettle_mpz_get_str_256(size, static_cast<uint8_t *>(buffer), *p);
	}
	*nbytes = size;
}

static void hook_gnutls_mpi_bprint_size(const mpz_t *a, uint8_t *buf, size_t size) {
	size_t bytes = 0;
	hook_gnutls_mpi_print(a, NULL, &bytes);

	if (bytes <= size) {
		unsigned i;
		size_t diff = size - bytes;

		for (i = 0; i < diff; i++) { buf[i] = 0; }
		hook_gnutls_mpi_print(a, &buf[diff], &bytes);
	} else {
		hook_gnutls_mpi_print(a, buf, &bytes);
	}
}

static bool hook_gnutls_sign_gost(gnutls_privkey_t key, BytesView hash,
		const Callback<void(BytesView)> &cb) {
	gnutls_ecc_curve_t c;
	gnutls_digest_algorithm_t digest;
	gnutls_gost_paramset_t paramset;
	gnutls_datum_t k;
	gnutls_privkey_export_gost_raw2(key, &c, &digest, &paramset, nullptr, nullptr, &k, 0);

	struct ecc_scalar priv;
	struct dsa_signature sig;
	const struct ecc_curve *curve = nullptr;

	switch (c) {
	case GNUTLS_ECC_CURVE_GOST256CPA:
	case GNUTLS_ECC_CURVE_GOST256CPXA:
	case GNUTLS_ECC_CURVE_GOST256B: curve = nettle_get_gost_gc256b(); break;
	case GNUTLS_ECC_CURVE_GOST512A: curve = nettle_get_gost_gc512a(); break;
	default: break;
	}

	if (!curve) {
		gnutls_free(k.data);
		return false;
	}

	mpz_t privKeyK;
	mpz_init(privKeyK);
	mpz_import(privKeyK, k.size, -1, 1, 0, 0, k.data);

	ecc_scalar_init(&priv, curve);
	if (ecc_scalar_set(&priv, privKeyK) == 0) {
		ecc_scalar_clear(&priv);
	}

	dsa_signature_init(&sig);

	auto randSeed = Gost3411_512::hmac(BytesView(k.data, k.size), hash);

	Hook_GnuTLS_RandomData randomData{&randSeed, 0};

	gostdsa_sign(&priv, &randomData, hook_gnutls_random_func, hash.size(), hash.data(), &sig);

	auto intsize = (ecc_bit_size(curve) + 7) / 8;
	uint8_t data[intsize * 2];

	hook_gnutls_mpi_bprint_size(&sig.s, data, intsize);
	hook_gnutls_mpi_bprint_size(&sig.r, data + intsize, intsize);

	cb(BytesView(data, intsize * 2));

	dsa_signature_clear(&sig);
	ecc_scalar_clear(&priv);
	mpz_clear(privKeyK);

	gnutls_free(k.data);

	return true;
}

static gnutls_sign_algorithm_t getGnuTLSAlgo(SignAlgorithm a) {
	switch (a) {
	case SignAlgorithm::RSA_SHA256: return GNUTLS_SIGN_RSA_SHA256; break;
	case SignAlgorithm::RSA_SHA512: return GNUTLS_SIGN_RSA_SHA512; break;
	case SignAlgorithm::ECDSA_SHA256: return GNUTLS_SIGN_ECDSA_SHA256; break;
	case SignAlgorithm::ECDSA_SHA512: return GNUTLS_SIGN_ECDSA_SHA512; break;
	case SignAlgorithm::GOST_256: return GNUTLS_SIGN_GOST_256; break;
	case SignAlgorithm::GOST_512: return GNUTLS_SIGN_GOST_512; break;
	}
	return GNUTLS_SIGN_UNKNOWN;
}

static gnutls_cipher_algorithm_t getGnuTLSAlgo(BlockCipher b) {
	switch (b) {
	case BlockCipher::AES_CBC: return GNUTLS_CIPHER_AES_256_CBC; break;
	case BlockCipher::AES_CFB8: return GNUTLS_CIPHER_AES_256_CFB8; break;
	case BlockCipher::Gost3412_2015_CTR_ACPKM: return GNUTLS_CIPHER_KUZNYECHIK_CTR_ACPKM; break;
	}
	return GNUTLS_CIPHER_AES_256_CBC;
}

static KeyType getGnuTLSKeyType(int a) {
	switch (a) {
	case GNUTLS_PK_RSA: return KeyType::RSA; break;
	case GNUTLS_PK_DSA: return KeyType::DSA; break;
	case GNUTLS_PK_DH: break;
	case GNUTLS_PK_ECDSA: return KeyType::ECDSA; break;
	case GNUTLS_PK_RSA_PSS: break;
	case GNUTLS_PK_EDDSA_ED25519: break;
	case GNUTLS_PK_GOST_01: break;
	case GNUTLS_PK_GOST_12_256: return KeyType::GOST3410_2012_256; break;
	case GNUTLS_PK_GOST_12_512: return KeyType::GOST3410_2012_512; break;
	case GNUTLS_PK_EDDSA_ED448: return KeyType::EDDSA_ED448; break;
	default: break;
	}
	return KeyType::Unknown;
}

struct Pkcs1RsaPubKeyReader {
	using Decoder = Asn1Decoder<memory::StandartInterface, Pkcs1RsaPubKeyReader>;

	enum State {
		Init,
		Seq,
		Exp,
		Mod,
		Fin,
		Invalid
	};

	Pkcs1RsaPubKeyReader(const BytesViewNetwork &source) {
		Decoder dec;
		dec.decode(*this, source);
	}

	void onBeginSequence(Decoder &) {
		if (state == Init) {
			state = Seq;
		} else {
			state = Invalid;
		}
	}
	void onEndSequence(Decoder &) {
		if (state == Exp) {
			state = Fin;
		} else {
			state = Invalid;
		}
	}

	void onBigInteger(Decoder &, const BytesViewNetwork &val) {
		switch (state) {
		case Seq:
			mod = val;
			state = Mod;
			break;
		case Mod:
			exp = val;
			state = Exp;
			break;
		default: state = Invalid; break;
		}
	}

	SP_COVERAGE_TRIVIAL
	void onCustom(Decoder &, uint8_t, const BytesViewNetwork &val) { state = Invalid; }

	State state = State::Init;
	BytesViewNetwork exp;
	BytesViewNetwork mod;
};

static BackendCtx s_gnuTLSCtx = {
	.name = Backend::GnuTLS,
	.title = StringView("GnuTLS"),
	.flags = BackendFlags::SupportsPKCS1 | BackendFlags::SupportsPKCS8 | BackendFlags::SupportsAes
			| BackendFlags::SecureLibrary | BackendFlags::SupportsGost3410_2012
			| BackendFlags::SupportsGost3412_2015,
	.initialize =
			[](BackendCtx &) {
	log::source().verbose("Crypto", "GnuTLS backend loaded: ", gnutls_check_version("3.0.0"));
	/*gnutls_global_set_log_level(9);
		gnutls_global_set_log_function([] (int l, const char *data) {
			log::source().verbose("GnuTLS", l, ": ", data);
		})*/
	;
	gnutls_global_init();
},
	.finalize = [](BackendCtx &) { gnutls_global_deinit(); },
	.encryptBlock = [](const BlockKey256 &key, BytesView d,
							const Callback<void(BytesView)> &cb) -> bool {
	auto cipherBlockSize = getBlockSize(key.cipher);
	auto algo = getGnuTLSAlgo(key.cipher);

	uint64_t dataSize = d.size();
	auto blockSize = math::align<size_t>(dataSize, cipherBlockSize)
			+ cipherBlockSize; // allocate space for possible padding

	uint8_t output[blockSize + sizeof(BlockCryptoHeader)];

	uint8_t iv[16] = {0};
	gnutls_datum_t ivData = {.data = static_cast<unsigned char *>(iv),
		.size = static_cast<unsigned int>(16)};

	gnutls_datum_t keyData = {
		.data = const_cast<unsigned char *>(static_cast<const unsigned char *>(key.data.data())),
		.size = static_cast<unsigned int>(key.data.size())};

	gnutls_cipher_hd_t aes;
	auto err = gnutls_cipher_init(&aes, algo, &keyData, &ivData);
	if (err != 0) {
		log::source().error("Crypto", "gnutls_cipher_init() = [", err, "] ", gnutls_strerror(err));
		return false;
	}

	size_t outSize = blockSize - sizeof(BlockCryptoHeader);
	fillCryptoBlockHeader(output, key, d);

	if constexpr (SAFE_BLOCK_ENCODING) {
		memcpy(output + sizeof(BlockCryptoHeader), d.data(), d.size());
		memset(output + sizeof(BlockCryptoHeader) + d.size(), 0, blockSize - d.size());
		err = gnutls_cipher_encrypt(aes, output + sizeof(BlockCryptoHeader), outSize);
	} else {
		err = gnutls_cipher_encrypt2(aes, d.data(), math::align<size_t>(dataSize, cipherBlockSize),
				output + sizeof(BlockCryptoHeader), outSize);
	}

	if (err != 0) {
		gnutls_cipher_deinit(aes);
		log::source().error("Crypto", "gnutls_cipher_encrypt() = [", err, "] ",
				gnutls_strerror(err));
		return false;
	}

	gnutls_cipher_deinit(aes);
	cb(BytesView(output, blockSize + sizeof(BlockCryptoHeader) - cipherBlockSize));
	return true;
},
	.decryptBlock = [](const BlockKey256 &key, BytesView b,
							const Callback<void(BytesView)> &cb) -> bool {
	auto info = getBlockInfo(b);
	auto cipherBlockSize = getBlockSize(info.cipher);
	auto algo = getGnuTLSAlgo(info.cipher);

	auto blockSize = math::align<size_t>(info.dataSize, cipherBlockSize) + cipherBlockSize;
	b.offset(sizeof(BlockCryptoHeader));

	uint8_t output[blockSize];

	uint8_t iv[16] = {0};
	gnutls_datum_t ivData = {.data = static_cast<unsigned char *>(iv),
		.size = static_cast<unsigned int>(16)};

	gnutls_datum_t keyData = {
		.data = const_cast<unsigned char *>(static_cast<const unsigned char *>(key.data.data())),
		.size = static_cast<unsigned int>(key.data.size())};

	gnutls_cipher_hd_t aes;
	auto err = gnutls_cipher_init(&aes, algo, &keyData, &ivData);
	if (err != 0) {
		log::source().error("Crypto", "gnutls_cipher_init() = [", err, "] ", gnutls_strerror(err));
		return false;
	}

	err = gnutls_cipher_decrypt2(aes, b.data(), b.size(), output, blockSize);
	if (err != 0) {
		gnutls_cipher_deinit(aes);
		log::source().error("Crypto", "gnutls_cipher_decrypt2() = [", err, "] ",
				gnutls_strerror(err));
		return false;
	}

	gnutls_cipher_deinit(aes);
	cb(BytesView(output, info.dataSize));
	return true;
},
	.hash256 = [](Sha256::Buf &buf, const Callback<void(const HashCoderCallback &upd)> &cb,
					   HashFunction func) -> bool {
	gnutls_hash_hd_t hash;
	switch (func) {
	case HashFunction::SHA_2:
		if (gnutls_hash_init(&hash, GNUTLS_DIG_SHA256) != GNUTLS_E_SUCCESS) {
			return false;
		}
		break;
	case HashFunction::GOST_3411:
		if (gnutls_hash_init(&hash, GNUTLS_DIG_STREEBOG_256) != GNUTLS_E_SUCCESS) {
			return false;
		}
		break;
	}

	bool success = true;
	cb([&](const CoderSource &data) {
		if (success && gnutls_hash(hash, data.data(), data.size()) != GNUTLS_E_SUCCESS) {
			success = false;
			return false;
		}
		return true;
	});
	gnutls_hash_deinit(hash, buf.data());
	return success;
},
	.hash512 = [](Sha512::Buf &buf, const Callback<void(const HashCoderCallback &upd)> &cb,
					   HashFunction func) -> bool {
	gnutls_hash_hd_t hash;
	switch (func) {
	case HashFunction::SHA_2:
		if (gnutls_hash_init(&hash, GNUTLS_DIG_SHA512) != GNUTLS_E_SUCCESS) {
			return false;
		}
		break;
	case HashFunction::GOST_3411:
		if (gnutls_hash_init(&hash, GNUTLS_DIG_STREEBOG_512) != GNUTLS_E_SUCCESS) {
			return false;
		}
		break;
	}

	bool success = true;
	cb([&](const CoderSource &data) {
		if (success && gnutls_hash(hash, data.data(), data.size()) != GNUTLS_E_SUCCESS) {
			success = false;
			return false;
		}
		return true;
	});
	gnutls_hash_deinit(hash, buf.data());
	return success;
},
	.privInit = [](KeyContext &ctx) -> bool {
	auto err = gnutls_privkey_init(reinterpret_cast<gnutls_privkey_t *>(&ctx.keyCtx));
	if (err == GNUTLS_E_SUCCESS) {
		return true;
	}
	return false;
},
	.privFree =
			[](KeyContext &ctx) {
	gnutls_privkey_deinit(static_cast<gnutls_privkey_t>(ctx.keyCtx));
},
	.privGen = [](KeyContext &ctx, KeyBits bits, KeyType type) -> bool {
	auto key = static_cast<gnutls_privkey_t>(ctx.keyCtx);

	int err = 0;

	switch (type) {
	case KeyType::Unknown:
	case KeyType::DSA:
	case KeyType::ECDSA:
	case KeyType::EDDSA_ED448:
		log::source().error("Crypto-gnutls", "Unsupported key type for keygen");
		return false;
		break;
	case KeyType::GOST3410_2012_256:
		err = gnutls_privkey_generate(key, GNUTLS_PK_GOST_12_256, 512, 0);
		break;
	case KeyType::GOST3410_2012_512:
		err = gnutls_privkey_generate(key, GNUTLS_PK_GOST_12_512, 512, 0);
		break;
	case KeyType::RSA:
		switch (bits) {
		case KeyBits::_1024: err = gnutls_privkey_generate(key, GNUTLS_PK_RSA, 1'024, 0); break;
		case KeyBits::_2048: err = gnutls_privkey_generate(key, GNUTLS_PK_RSA, 2'048, 0); break;
		case KeyBits::_4096: err = gnutls_privkey_generate(key, GNUTLS_PK_RSA, 4'096, 0); break;
		}
		break;
	}

	if (err == GNUTLS_E_SUCCESS) {
		ctx.type = getGnuTLSKeyType(gnutls_privkey_get_pk_algorithm(key, nullptr));
		return true;
	}

	return false;
},
	.privImport =
			[](KeyContext &ctx, BytesView data, const CoderSource &passwd) {
	gnutls_datum_t keyData;
	keyData.data = const_cast<unsigned char *>(static_cast<const unsigned char *>(data.data()));
	keyData.size = static_cast<unsigned int>(data.size());

	auto key = static_cast<gnutls_privkey_t>(ctx.keyCtx);
	if (isPemKey(data)) {
		if (gnutls_privkey_import_x509_raw(key, &keyData, GNUTLS_X509_FMT_PEM,
					reinterpret_cast<const char *>(
							passwd._data.empty() ? nullptr : passwd._data.data()),
					0)
				== GNUTLS_E_SUCCESS) {
			ctx.type = getGnuTLSKeyType(gnutls_privkey_get_pk_algorithm(key, nullptr));
			return true;
		}
	} else {
		if (gnutls_privkey_import_x509_raw(key, &keyData, GNUTLS_X509_FMT_DER,
					reinterpret_cast<const char *>(
							passwd._data.empty() ? nullptr : passwd._data.data()),
					0)
				== GNUTLS_E_SUCCESS) {
			ctx.type = getGnuTLSKeyType(gnutls_privkey_get_pk_algorithm(key, nullptr));
			return true;
		}
	}
	return false;
},
	.privExportPem =
			[](const KeyContext &ctx, const Callback<void(BytesView)> &cb, KeyFormat fmt,
					const CoderSource &passPhrase) {
	auto key = static_cast<gnutls_privkey_t>(ctx.keyCtx);
	bool success = false;
	gnutls_datum_t out;
	gnutls_x509_privkey_t pk;
	if (gnutls_privkey_export_x509(key, &pk) == GNUTLS_E_SUCCESS) {
		switch (fmt) {
		case KeyFormat::PKCS1:
			if (!passPhrase.empty()) {
				log::source().error("Crypto", "Password-encoding is not supported for PKCS1");
			}
			if (gnutls_x509_privkey_export2(pk, GNUTLS_X509_FMT_PEM, &out) == GNUTLS_E_SUCCESS) {
				cb(BytesView(out.data, out.size));
				gnutls_free(out.data);
				success = true;
			}
			break;
		case KeyFormat::PKCS8:
			if (!passPhrase.empty()) {
				char buf[passPhrase.size() + 1];
				memcpy(buf, passPhrase.data(), passPhrase.size());
				buf[passPhrase.size()] = 0;
				if (gnutls_x509_privkey_export2_pkcs8(pk, GNUTLS_X509_FMT_PEM, buf, 0, &out)
						== GNUTLS_E_SUCCESS) {
					cb(BytesView(out.data, out.size));
					gnutls_free(out.data);
					success = true;
				}
			} else {
				if (gnutls_x509_privkey_export2_pkcs8(pk, GNUTLS_X509_FMT_PEM, nullptr, 0, &out)
						== GNUTLS_E_SUCCESS) {
					cb(BytesView(out.data, out.size));
					gnutls_free(out.data);
					success = true;
				}
			}
			break;
		}
		gnutls_x509_privkey_deinit(pk);
	}
	return success;
},
	.privExportDer =
			[](const KeyContext &ctx, const Callback<void(BytesView)> &cb, KeyFormat fmt,
					const CoderSource &passPhrase) {
	auto key = static_cast<gnutls_privkey_t>(ctx.keyCtx);
	bool success = false;
	gnutls_datum_t out;
	gnutls_x509_privkey_t pk;
	if (gnutls_privkey_export_x509(key, &pk) == GNUTLS_E_SUCCESS) {
		switch (fmt) {
		case KeyFormat::PKCS1:
			if (!passPhrase.empty()) {
				log::source().error("Crypto", "Password-encoding is not supported for PKCS1");
			}
			if (gnutls_x509_privkey_export2(pk, GNUTLS_X509_FMT_DER, &out) == GNUTLS_E_SUCCESS) {
				cb(BytesView(out.data, out.size));
				gnutls_free(out.data);
				success = true;
			}
			break;
		case KeyFormat::PKCS8:
			if (!passPhrase.empty()) {
				char buf[passPhrase.size() + 1];
				memcpy(buf, passPhrase.data(), passPhrase.size());
				buf[passPhrase.size()] = 0;
				if (gnutls_x509_privkey_export2_pkcs8(pk, GNUTLS_X509_FMT_DER, buf, 0, &out)
						== GNUTLS_E_SUCCESS) {
					cb(BytesView(out.data, out.size));
					gnutls_free(out.data);
					success = true;
				}
			} else {
				if (gnutls_x509_privkey_export2_pkcs8(pk, GNUTLS_X509_FMT_DER, nullptr, 0, &out)
						== GNUTLS_E_SUCCESS) {
					cb(BytesView(out.data, out.size));
					gnutls_free(out.data);
					success = true;
				}
			}
			break;
		}
		gnutls_x509_privkey_deinit(pk);
	}
	return success;
},
	.privExportPublic =
			[](KeyContext &target, const KeyContext &privKey) {
	auto err = gnutls_pubkey_init(reinterpret_cast<gnutls_pubkey_t *>(&target.keyCtx));
	if (err == GNUTLS_E_SUCCESS) {
		if (gnutls_pubkey_import_privkey(static_cast<gnutls_pubkey_t>(target.keyCtx),
					static_cast<gnutls_privkey_t>(privKey.keyCtx), 0, 0)
				!= GNUTLS_E_SUCCESS) {
			gnutls_pubkey_deinit(static_cast<gnutls_pubkey_t>(target.keyCtx));
		} else {
			target.type = getGnuTLSKeyType(gnutls_pubkey_get_pk_algorithm(
					static_cast<gnutls_pubkey_t>(target.keyCtx), nullptr));
			return true;
		}
	} else {
		log::source().error("Crypto", "gnutls_pubkey_init() = [", err, "] ", gnutls_strerror(err));
	}
	return false;
},
	.privSign =
			[](const KeyContext &ctx, const Callback<void(BytesView)> &cb, const CoderSource &data,
					SignAlgorithm algo) {
	auto key = static_cast<gnutls_privkey_t>(ctx.keyCtx);

	gnutls_datum_t dataToSign;
	dataToSign.data = const_cast<unsigned char *>(static_cast<const unsigned char *>(data.data()));
	dataToSign.size = static_cast<unsigned int>(data.size());

	gnutls_datum_t signature;

	if (gnutls_privkey_sign_data2(key, getGnuTLSAlgo(algo), 0, &dataToSign, &signature)
			== GNUTLS_E_SUCCESS) {
		cb(BytesView(signature.data, signature.size));
		gnutls_free(signature.data);
		return true;
	}
	return false;
},
	.privVerify =
			[](const KeyContext &ctx, const CoderSource &data, BytesView signature,
					SignAlgorithm algo) {
	auto key = static_cast<gnutls_privkey_t>(ctx.keyCtx);
	bool success = false;

	gnutls_pubkey_t pubkey;
	auto err = gnutls_pubkey_init(&pubkey);
	if (err == GNUTLS_E_SUCCESS) {
		if (gnutls_pubkey_import_privkey(pubkey, key, 0, 0) == GNUTLS_E_SUCCESS) {
			gnutls_datum_t inputData;
			inputData.data =
					const_cast<unsigned char *>(static_cast<const unsigned char *>(data.data()));
			inputData.size = static_cast<unsigned int>(data.size());

			gnutls_datum_t signatureData;
			signatureData.data = const_cast<unsigned char *>(
					static_cast<const unsigned char *>(signature.data()));
			signatureData.size = static_cast<unsigned int>(signature.size());

			success = gnutls_pubkey_verify_data2(pubkey, getGnuTLSAlgo(algo), 0, &inputData,
							  &signatureData)
					>= 0;
		}
		gnutls_pubkey_deinit(pubkey);
	} else {
		log::source().error("Crypto", "gnutls_pubkey_init() = [", err, "] ", gnutls_strerror(err));
	}
	return success;
},
	.privEncrypt =
			[](const KeyContext &ctx, const Callback<void(BytesView)> &cb,
					const CoderSource &data) {
	auto key = static_cast<gnutls_privkey_t>(ctx.keyCtx);
	bool success = false;

	gnutls_pubkey_t pubkey;
	auto err = gnutls_pubkey_init(&pubkey);
	if (err == GNUTLS_E_SUCCESS) {
		auto err = gnutls_pubkey_import_privkey(pubkey, key, 0, 0);
		if (err == GNUTLS_E_SUCCESS) {
			gnutls_datum_t dataToEncrypt;
			dataToEncrypt.data =
					const_cast<unsigned char *>(static_cast<const unsigned char *>(data.data()));
			dataToEncrypt.size = static_cast<unsigned int>(data.size());

			gnutls_datum_t output;

			err = gnutls_pubkey_encrypt_data(pubkey, 0, &dataToEncrypt, &output);
			if (err == GNUTLS_E_SUCCESS) {
				cb(BytesView(output.data, output.size));
				gnutls_free(output.data);
				success = true;
			}
		}
		gnutls_pubkey_deinit(pubkey);
	} else {
		log::source().error("Crypto", "gnutls_pubkey_init() = [", err, "] ", gnutls_strerror(err));
	}
	return success;
},
	.privDecrypt =
			[](const KeyContext &ctx, const Callback<void(BytesView)> &cb,
					const CoderSource &data) {
	auto key = static_cast<gnutls_privkey_t>(ctx.keyCtx);

	gnutls_datum_t dataToDecrypt;
	dataToDecrypt.data =
			const_cast<unsigned char *>(static_cast<const unsigned char *>(data.data()));
	dataToDecrypt.size = static_cast<unsigned int>(data.size());

	gnutls_datum_t output;

	auto err = gnutls_privkey_decrypt_data(key, 0, &dataToDecrypt, &output);
	if (err == GNUTLS_E_SUCCESS) {
		cb(BytesView(output.data, output.size));
		gnutls_free(output.data);
		return true;
	}
	return false;
},
	.privFingerprint =
			[](const KeyContext &ctx, const Callback<void(BytesView)> &cb,
					const CoderSource &data) {
	switch (ctx.type) {
	case KeyType::RSA:
	case KeyType::DSA: return s_gnuTLSCtx.privSign(ctx, cb, data, SignAlgorithm::RSA_SHA512); break;
	case KeyType::ECDSA:
	case KeyType::EDDSA_ED448:
		return s_gnuTLSCtx.privSign(ctx, cb, data, SignAlgorithm::ECDSA_SHA512);
		break;
	case KeyType::GOST3410_2012_256:
		return hook_gnutls_sign_gost(static_cast<gnutls_privkey_t>(ctx.keyCtx),
				Gost3411_256().update(data).final(), cb);
		break;
	case KeyType::GOST3410_2012_512:
		return hook_gnutls_sign_gost(static_cast<gnutls_privkey_t>(ctx.keyCtx),
				Gost3411_512().update(data).final(), cb);
		break;
	default: break;
	}
	return false;
},
	.pubInit = [](KeyContext &ctx) -> bool {
	return gnutls_pubkey_init(reinterpret_cast<gnutls_pubkey_t *>(&ctx.keyCtx)) == GNUTLS_E_SUCCESS;
},
	.pubFree =
			[](KeyContext &ctx) { gnutls_pubkey_deinit(static_cast<gnutls_pubkey_t>(ctx.keyCtx)); },
	.pubImport =
			[](KeyContext &ctx, BytesView data) {
	auto key = static_cast<gnutls_pubkey_t>(ctx.keyCtx);

	gnutls_datum_t keyData;
	keyData.data = const_cast<unsigned char *>(static_cast<const unsigned char *>(data.data()));
	keyData.size = static_cast<unsigned int>(data.size());

	int err = 0;
	if (isPemKey(data)) {
		err = gnutls_pubkey_import(key, &keyData, GNUTLS_X509_FMT_PEM);
		if (err == GNUTLS_E_SUCCESS) {
			ctx.type = getGnuTLSKeyType(gnutls_pubkey_get_pk_algorithm(key, nullptr));
			return true;
		}

		StringView str(reinterpret_cast<const char *>(data.data()), data.size());
		str.skipUntilString("-----");
		if (str.is("-----BEGIN RSA PUBLIC KEY-----\n")) {
			str += "-----BEGIN RSA PUBLIC KEY-----\n"_len;

			auto data = str.readUntilString("\n-----END RSA PUBLIC KEY-----");
			auto tmp = data;
			tmp.skipChars<StringView::Base64, StringView::WhiteSpace>();
			if (tmp.empty()) {
				size_t len = 0;
				uint8_t buf[base64::decodeSize(data.size())];
				base64::decode([&](uint8_t b) { buf[len++] = b; }, data);

				BytesViewNetwork asn1(buf, len);
				Pkcs1RsaPubKeyReader r(asn1);
				if (r.state == Pkcs1RsaPubKeyReader::Fin) {
					gnutls_datum_t mData;
					mData.data = const_cast<unsigned char *>(r.mod.data());
					mData.size = static_cast<unsigned int>(r.mod.size());

					gnutls_datum_t eData;
					eData.data = const_cast<unsigned char *>(r.exp.data());
					eData.size = static_cast<unsigned int>(r.exp.size());

					auto key = static_cast<gnutls_pubkey_t>(ctx.keyCtx);
					auto err = gnutls_pubkey_import_rsa_raw(key, &mData, &eData);
					if (err == GNUTLS_E_SUCCESS) {
						ctx.type = getGnuTLSKeyType(gnutls_pubkey_get_pk_algorithm(key, nullptr));
						return true;
					}
				}
			}
		}
	} else {
		err = gnutls_pubkey_import(key, &keyData, GNUTLS_X509_FMT_DER);
		if (err == GNUTLS_E_SUCCESS) {
			ctx.type = getGnuTLSKeyType(gnutls_pubkey_get_pk_algorithm(key, nullptr));
			return true;
		}
	}
	return false;
},
	.pubImportOpenSSH =
			[](KeyContext &ctx, StringView r) {
	auto origKeyType = r.readUntil<StringView::CharGroup<CharGroupId::WhiteSpace>>();
	r.skipChars<StringView::CharGroup<CharGroupId::WhiteSpace>>();
	auto dataBlock = r.readUntil<StringView::CharGroup<CharGroupId::WhiteSpace>>();
	auto valid = valid::validateBase64(dataBlock);
	if (valid) {
		uint8_t bytes[base64::decodeSize(dataBlock.size())];
		uint8_t *target = bytes;
		base64::decode([&](uint8_t c) { *target++ = c; }, dataBlock);

		BytesViewNetwork dataView(bytes, target - bytes);
		auto len = dataView.readUnsigned32();
		auto keyType = dataView.readString(len);

		if (origKeyType != keyType || keyType != "ssh-rsa") {
			return false;
		}

		auto elen = dataView.readUnsigned32();
		auto exp = dataView.readBytes(elen);

		auto mlen = dataView.readUnsigned32();
		auto modulus = dataView.readBytes(mlen);

		gnutls_datum_t mData;
		mData.data = const_cast<unsigned char *>(modulus.data());
		mData.size = static_cast<unsigned int>(mlen);

		gnutls_datum_t eData;
		eData.data = const_cast<unsigned char *>(exp.data());
		eData.size = static_cast<unsigned int>(elen);

		auto key = static_cast<gnutls_pubkey_t>(ctx.keyCtx);
		auto err = gnutls_pubkey_import_rsa_raw(key, &mData, &eData);
		if (err == GNUTLS_E_SUCCESS) {
			ctx.type = getGnuTLSKeyType(gnutls_pubkey_get_pk_algorithm(key, nullptr));
			return true;
		}
	}
	return false;
},
	.pubExportPem =
			[](const KeyContext &ctx, const Callback<void(BytesView)> &cb) {
	auto key = static_cast<gnutls_pubkey_t>(ctx.keyCtx);

	gnutls_datum_t out;
	if (gnutls_pubkey_export2(key, GNUTLS_X509_FMT_PEM, &out) == GNUTLS_E_SUCCESS) {
		cb(BytesView(out.data, out.size));
		gnutls_free(out.data);
		return true;
	}
	return false;
},
	.pubExportDer =
			[](const KeyContext &ctx, const Callback<void(BytesView)> &cb) {
	auto key = static_cast<gnutls_pubkey_t>(ctx.keyCtx);

	gnutls_datum_t out;
	if (gnutls_pubkey_export2(key, GNUTLS_X509_FMT_DER, &out) == GNUTLS_E_SUCCESS) {
		cb(BytesView(out.data, out.size));
		gnutls_free(out.data);
		return true;
	}
	return false;
},
	.pubVerify =
			[](const KeyContext &ctx, const CoderSource &data, BytesView signature,
					SignAlgorithm algo) {
	auto key = static_cast<gnutls_pubkey_t>(ctx.keyCtx);
	gnutls_datum_t inputData;
	inputData.data = const_cast<unsigned char *>(static_cast<const unsigned char *>(data.data()));
	inputData.size = static_cast<unsigned int>(data.size());

	gnutls_datum_t signatureData;
	signatureData.data =
			const_cast<unsigned char *>(static_cast<const unsigned char *>(signature.data()));
	signatureData.size = static_cast<unsigned int>(signature.size());

	return gnutls_pubkey_verify_data2(key, getGnuTLSAlgo(algo), 0, &inputData, &signatureData) >= 0;
},
	.pubEncrypt =
			[](const KeyContext &ctx, const Callback<void(BytesView)> &cb,
					const CoderSource &data) {
	auto key = static_cast<gnutls_pubkey_t>(ctx.keyCtx);

	gnutls_datum_t dataToEncrypt;
	dataToEncrypt.data =
			const_cast<unsigned char *>(static_cast<const unsigned char *>(data.data()));
	dataToEncrypt.size = static_cast<unsigned int>(data.size());

	gnutls_datum_t output;

	auto err = gnutls_pubkey_encrypt_data(key, 0, &dataToEncrypt, &output);
	if (err == GNUTLS_E_SUCCESS) {
		cb(BytesView(output.data, output.size));
		gnutls_free(output.data);
		return true;
	}
	return false;
},
};

BackendCtxRef s_gnuTlsRef(&s_gnuTLSCtx);

} // namespace stappler::crypto

#endif
