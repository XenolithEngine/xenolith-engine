/**
Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023-2024 Stappler LLC <admin@stappler.dev>

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
#define MODULE_STAPPLER_CRYPTO_MBEDTLS 1
#endif

#if MODULE_STAPPLER_CRYPTO_MBEDTLS

#include "mbedtls/mbedtls_config.h"
#include "mbedtls/pk.h"
#include "mbedtls/md.h"
#include "mbedtls/error.h"
#include "mbedtls/pem.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"

namespace STAPPLER_VERSIONIZED stappler::crypto {

static constexpr size_t MBEDTLS_KEY_BUFFER_SIZE =
		12_KiB; // Должен вмещать ключ в формате DER и PEM максимального размера

static constexpr auto PERSONALIZATION_STRING = "SP_PERSONALIZATION_STRING";
static constexpr int PUBLIC_EXPONENT = 65'537;

static KeyType getMbedTLSKeyType(mbedtls_pk_type_t a) {
	switch (a) {
	case MBEDTLS_PK_RSA: return KeyType::RSA; break;
	case MBEDTLS_PK_ECDSA: return KeyType::ECDSA; break;
	default: break;
	}
	return KeyType::Unknown;
}

struct EntropyContext {
	bool valid = false;
	mbedtls_entropy_context entropy;
	mbedtls_ctr_drbg_context ctr_drbg;

	EntropyContext() {
		mbedtls_ctr_drbg_init(&ctr_drbg);
		mbedtls_entropy_init(&entropy);

		if (mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
					reinterpret_cast<const unsigned char *>(PERSONALIZATION_STRING),
					strlen(PERSONALIZATION_STRING))
				== 0) {
			valid = true;
		}
	}

	~EntropyContext() {
		mbedtls_ctr_drbg_free(&ctr_drbg);
		mbedtls_entropy_free(&entropy);
	}

	explicit operator bool() const { return valid; }
};

static BackendCtx s_mbedTLSCtx = {
	.name = Backend::MbedTLS,
	.title = StringView("MbedTLS"),
	.flags = BackendFlags::SupportsPKCS1 | BackendFlags::SupportsAes | BackendFlags::SecureLibrary,
	.initialize = [] (BackendCtx &) {
		log::source().verbose("Crypto", "MbedTLS backend loaded");
	},
	.finalize = [] (BackendCtx &) { },
	.encryptBlock = [] (const BlockKey256 &key, BytesView d, const Callback<void(BytesView)> &cb) -> bool {
		auto cipherBlockSize = getBlockSize(key.cipher);

		uint64_t dataSize = d.size();
		auto blockSize = math::align<size_t>(dataSize, cipherBlockSize)
				+ cipherBlockSize; // allocate space for possible padding

		uint8_t output[blockSize + sizeof(BlockCryptoHeader)];

		fillCryptoBlockHeader(output, key, d);

		auto perform = [&key] (const uint8_t *source, size_t size, uint8_t *out) {
			bool success = false;
			mbedtls_aes_context aes;
			unsigned char iv[16] = { 0 };

			mbedtls_aes_init( &aes );
			if (mbedtls_aes_setkey_enc( &aes, key.data.data(), 256 ) == 0) {
				switch (key.cipher) {
				case BlockCipher::AES_CBC:
					if (mbedtls_aes_crypt_cbc( &aes, MBEDTLS_AES_ENCRYPT, size, iv, source, out ) == 0) {
						success = true;
					}
					break;
				case BlockCipher::AES_CFB8:
					if (mbedtls_aes_crypt_cfb8( &aes, MBEDTLS_AES_ENCRYPT, size, iv, source, out ) == 0) {
						success = true;
					}
					break;
				default:
					break;
				}
			}
			mbedtls_aes_free( &aes );
			return success;
		};

		if constexpr (SAFE_BLOCK_ENCODING) {
			uint8_t tmp[blockSize];
			memset(tmp, 0, blockSize);
			memcpy(tmp, d.data(), d.size());

			if (!perform(tmp, blockSize - cipherBlockSize, output + sizeof(BlockCryptoHeader))) {
				return false;
			}
		} else {
			if (!perform(d.data(), blockSize - cipherBlockSize, output + sizeof(BlockCryptoHeader))) {
				return false;
			}
		}

		cb(BytesView(output, blockSize + sizeof(BlockCryptoHeader) - cipherBlockSize));
		return true;
	},
	.decryptBlock = [] (const BlockKey256 &key, BytesView b, const Callback<void(BytesView)> &cb) -> bool {
		bool success = false;
		auto info = getBlockInfo(b);
		auto cipherBlockSize = getBlockSize(info.cipher);

		auto blockSize = math::align<size_t>(info.dataSize, cipherBlockSize) + cipherBlockSize;
		b.offset(sizeof(BlockCryptoHeader));

		uint8_t output[blockSize];

		mbedtls_aes_context aes;
		unsigned char iv[16] = { 0 };

		mbedtls_aes_init( &aes );
		if (mbedtls_aes_setkey_dec( &aes, key.data.data(), 256 ) == 0) {
			switch (info.cipher) {
			case BlockCipher::AES_CBC:
				if (mbedtls_aes_crypt_cbc( &aes, MBEDTLS_AES_DECRYPT, blockSize, iv, b.data(), output ) == 0) {
					success = true;
				}
				break;
			case BlockCipher::AES_CFB8:
				if (mbedtls_aes_crypt_cfb8( &aes, MBEDTLS_AES_DECRYPT, blockSize, iv, b.data(), output ) == 0) {
					success = true;
				}
				break;
			default:
				break;
			}
		}
		mbedtls_aes_free( &aes );
		if (success) {
			cb(BytesView(output, info.dataSize));
		}
		return success;
	},
	.hash256 = [] (Sha256::Buf &buf, const Callback<void( const HashCoderCallback &upd )> &cb, HashFunction func) -> bool {
		bool success = true;
		mbedtls_md_context_t ctx;
		switch (func) {
		case HashFunction::SHA_2:
			mbedtls_md_init(&ctx);
			if (mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(mbedtls_md_type_t(MBEDTLS_MD_SHA256)), 0) == 0
					&& mbedtls_md_starts(&ctx) == 0) {
				cb([&] (const CoderSource &data) {
					if (success && mbedtls_md_update(&ctx, data.data(), data.size()) != 0) {
						success = false;
						return false;
					}
					return true;
				});
				if (success && mbedtls_md_finish(&ctx, buf.data()) != 0) {
					success = false;
				}
			} else {
				success = false;
			}
			mbedtls_md_free(&ctx);
			break;
		case HashFunction::GOST_3411:
			return false;
			break;
		}
		return success;
	},
	.hash512 = [] (Sha512::Buf &buf, const Callback<void( const HashCoderCallback &upd )> &cb, HashFunction func) -> bool {
		bool success = true;
		mbedtls_md_context_t ctx;
		switch (func) {
		case HashFunction::SHA_2:
			mbedtls_md_init(&ctx);
			if (mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(mbedtls_md_type_t(MBEDTLS_MD_SHA512)), 0) == 0
					&& mbedtls_md_starts(&ctx) == 0) {
				cb([&] (const CoderSource &data) {
					if (success && mbedtls_md_update(&ctx, data.data(), data.size()) != 0) {
						success = false;
						return false;
					}
					return true;
				});
				if (success && mbedtls_md_finish(&ctx, buf.data()) != 0) {
					success = false;
				}
			} else {
				success = false;
			}
			mbedtls_md_free(&ctx);
			break;
		case HashFunction::GOST_3411:
			return false;
			break;
		}
		return success;
	},
	.privInit = [] (KeyContext &ctx) -> bool {
		mbedtls_pk_init(reinterpret_cast<mbedtls_pk_context *>(&ctx));
		return true;
	},
	.privFree = [] (KeyContext &ctx) {
		mbedtls_pk_free(reinterpret_cast<mbedtls_pk_context *>(&ctx));
	},
	.privGen = [] (KeyContext &ctx, KeyBits bits, KeyType type) -> bool {
		if (type != KeyType::RSA) {
			log::source().error("Crypto-mbedtls", "Unsupported key type for keygen");
			return false;
		}

		int nbits = 0;

		switch (bits) {
		case KeyBits::_1024: nbits = 1'024; break;
		case KeyBits::_2048: nbits = 2'048; break;
		case KeyBits::_4096: nbits = 4'096; break;
		}

		if (nbits == 0) {
			return false;
		}

		EntropyContext entropy;
		if (!entropy) {
			return false;
		}

		auto finalize = [&] (bool value) {
			if (!value) {
				mbedtls_pk_free( reinterpret_cast<mbedtls_pk_context *>(&ctx) );
			}
			return value;
		};

		auto key = reinterpret_cast<mbedtls_pk_context *>(&ctx);

		int ret = -1;
		if ((ret = mbedtls_pk_setup(key, mbedtls_pk_info_from_type(mbedtls_pk_type_t(MBEDTLS_PK_RSA)))) != 0) {
			return finalize(false);
		}

		if ((ret = mbedtls_rsa_gen_key(mbedtls_pk_rsa(*key), mbedtls_ctr_drbg_random, &entropy.ctr_drbg, nbits, PUBLIC_EXPONENT)) != 0) {
			return finalize(false);
		}

		if (mbedtls_rsa_check_pubkey(mbedtls_pk_rsa(*key)) != 0) {
			return finalize(false);
		}

		if (mbedtls_rsa_check_privkey(mbedtls_pk_rsa(*key)) != 0) {
			return finalize(false);
		}

		ctx.type = getMbedTLSKeyType(mbedtls_pk_get_type(key));
		return finalize(true);
	},
	.privImport = [] (KeyContext &ctx, BytesView data, const CoderSource &passwd) {
		EntropyContext entropy;

		auto loadKey = [&] (BytesView input) {
			auto key = reinterpret_cast<mbedtls_pk_context *>(&ctx);
			auto err = mbedtls_pk_parse_key(key, input.data(), input.size(),
					passwd.data(), passwd.size(), mbedtls_ctr_drbg_random, &entropy.ctr_drbg);
			if (err != 0) {
				mbedtls_pk_free( key );
				return false;
			}

			ctx.type = getMbedTLSKeyType(mbedtls_pk_get_type(key));
			return true;
		};

		if (!isPemKey(data) || data.data()[data.size() - 1] == 0) {
			return loadKey(data);
		} else {
			if (data.data()[data.size()] == 0) {
				return loadKey(BytesView(data.data(), data.size() + 1));
			} else {
				uint8_t buf[data.size() + 1];
				memcpy(buf, data.data(), data.size());
				buf[data.size()] = 0;

				return loadKey(BytesView(buf, data.size() + 1));
			}
		}
		return false;
	},
	.privExportPem = [] (const KeyContext &ctx, const Callback<void(BytesView)> &cb, KeyFormat fmt, const CoderSource &passPhrase) {
		uint8_t buf[MBEDTLS_KEY_BUFFER_SIZE];
		int ret = -1;

		auto key = reinterpret_cast<const mbedtls_pk_context *>(&ctx);

		if (fmt == KeyFormat::PKCS8) { log::source().error("Crypto", "KeyFormat::PKCS8 is not supported by mbedtls backend, Fallback to PKCS1"); }

		if (!passPhrase.empty()) {
			log::source().error("Crypto", "Password-encoding is not supported for PKCS1");
		}
		ret = mbedtls_pk_write_key_pem(key, buf, MBEDTLS_KEY_BUFFER_SIZE);
		if (ret > 0) {
			cb(BytesView(buf + MBEDTLS_KEY_BUFFER_SIZE - ret, ret));
			return true;
		} else if (ret == 0) {
			auto len = strlen(reinterpret_cast<char *>(buf));
			if (len < MBEDTLS_KEY_BUFFER_SIZE) {
				cb(BytesView(buf, len));
				return true;
			}
		}

		return false;
	},
	.privExportDer = [] (const KeyContext &ctx, const Callback<void(BytesView)> &cb, KeyFormat fmt, const CoderSource &passPhrase) {
		uint8_t buf[MBEDTLS_KEY_BUFFER_SIZE];
		int ret = -1;

		auto key = reinterpret_cast<const mbedtls_pk_context *>(&ctx);

		if (fmt == KeyFormat::PKCS8) { log::source().error("Crypto", "KeyFormat::PKCS8 is not supported by mbedtls backend, Fallback to PKCS1"); }

		if (!passPhrase.empty()) {
			log::source().error("Crypto", "Password-encoding is not supported for PKCS1");
		}
		ret = mbedtls_pk_write_key_der(key, buf, MBEDTLS_KEY_BUFFER_SIZE);
		if (ret > 0) {
			cb(BytesView(buf + MBEDTLS_KEY_BUFFER_SIZE - ret, ret));
			return true;
		}

		return false;
	},
	.privExportPublic = [] (KeyContext &target, const KeyContext &privKey) {
		uint8_t buf[MBEDTLS_KEY_BUFFER_SIZE];
		int ret = -1;

		auto sourceKey = reinterpret_cast<const mbedtls_pk_context *>(&privKey);
		auto targetKey = reinterpret_cast<mbedtls_pk_context *>(&target);

		ret = mbedtls_pk_write_pubkey_der(sourceKey, buf, MBEDTLS_KEY_BUFFER_SIZE);
		if (ret > 0) {
			auto data = BytesView(buf + MBEDTLS_KEY_BUFFER_SIZE - ret, ret);

			mbedtls_pk_init(targetKey);

			auto err = mbedtls_pk_parse_public_key(targetKey, data.data(), data.size());
			if (err != 0) {
				mbedtls_pk_free(targetKey);
				return false;
			}
			target.type = getMbedTLSKeyType(mbedtls_pk_get_type(targetKey));
			return true;
		}
		return false;
	},
	.privSign = [] (const KeyContext &ctx, const Callback<void(BytesView)> &cb, const CoderSource &data, SignAlgorithm algo) {
		EntropyContext entropy;
		if (!entropy) {
			return false;
		}

		size_t writeSize = 0;
		uint8_t buf[MBEDTLS_PK_SIGNATURE_MAX_SIZE];

		auto key = const_cast<mbedtls_pk_context *>(reinterpret_cast<const mbedtls_pk_context *>(&ctx));

		switch (algo) {
		case SignAlgorithm::ECDSA_SHA256:
		case SignAlgorithm::RSA_SHA256: {
			auto hash = Sha256().update(data).final();

			if (mbedtls_pk_sign(key, mbedtls_md_type_t(MBEDTLS_MD_SHA256), hash.data(), hash.size(),
					buf, MBEDTLS_PK_SIGNATURE_MAX_SIZE, &writeSize, mbedtls_ctr_drbg_random, &entropy.ctr_drbg) == 0) {
				cb(BytesView(buf, writeSize));
				return true;
			}

			break;
		}
		case SignAlgorithm::ECDSA_SHA512:
		case SignAlgorithm::RSA_SHA512: {
			auto hash = Sha512().update(data).final();

			if (mbedtls_pk_sign(key, mbedtls_md_type_t(MBEDTLS_MD_SHA512), hash.data(), hash.size(),
					buf, MBEDTLS_PK_SIGNATURE_MAX_SIZE, &writeSize, mbedtls_ctr_drbg_random, &entropy.ctr_drbg) == 0) {
				cb(BytesView(buf, writeSize));
				return true;
			}

			break;
		}
		case SignAlgorithm::GOST_256:
		case SignAlgorithm::GOST_512:
			return false;
		}

		return false;
	},
	.privVerify = [] (const KeyContext &ctx, const CoderSource &data, BytesView signature, SignAlgorithm algo) {
		auto key = const_cast<mbedtls_pk_context *>(reinterpret_cast<const mbedtls_pk_context *>(&ctx));
		switch (algo) {
		case SignAlgorithm::ECDSA_SHA256:
		case SignAlgorithm::RSA_SHA256: {
			auto hash = string::Sha256().update(data).final();
			if (mbedtls_pk_verify(key, mbedtls_md_type_t(MBEDTLS_MD_SHA256), hash.data(), hash.size(),
					signature.data(), signature.size()) == 0) {
				return true;
			}
			break;
		}
		case SignAlgorithm::ECDSA_SHA512:
		case SignAlgorithm::RSA_SHA512: {
			auto hash = string::Sha512().update(data).final();
			if (mbedtls_pk_verify(key, mbedtls_md_type_t(MBEDTLS_MD_SHA512), hash.data(), hash.size(),
					signature.data(), signature.size()) == 0) {
				return true;
			}
			break;
		}
		case SignAlgorithm::GOST_256:
		case SignAlgorithm::GOST_512:
			return false;
		}
		return false;
	},
	.privEncrypt = [] (const KeyContext &ctx, const Callback<void(BytesView)> &cb, const CoderSource &data) {
		EntropyContext entropy;
		if (!entropy) {
			return false;
		}

		size_t bufSize = math::align<size_t>(data.size(), 1_KiB) + MBEDTLS_PK_SIGNATURE_MAX_SIZE;
		size_t writeSize = 0;
		uint8_t buf[bufSize];

		auto key = const_cast<mbedtls_pk_context *>(reinterpret_cast<const mbedtls_pk_context *>(&ctx));

		if (mbedtls_pk_encrypt(key, data.data(), data.size(), buf, &writeSize, bufSize, mbedtls_ctr_drbg_random, &entropy.ctr_drbg) == 0) {
			cb(BytesView(buf, writeSize));
			return true;
		}

		return false;
	},
	.privDecrypt = [] (const KeyContext &ctx, const Callback<void(BytesView)> &cb, const CoderSource &data) {
		EntropyContext entropy;
		if (!entropy) {
			return false;
		}

		size_t bufSize = math::align<size_t>(data.size(), 1_KiB) + MBEDTLS_PK_SIGNATURE_MAX_SIZE;
		size_t writeSize = 0;
		uint8_t buf[bufSize];

		auto key = const_cast<mbedtls_pk_context *>(reinterpret_cast<const mbedtls_pk_context *>(&ctx));

		if (mbedtls_pk_decrypt(key, data.data(), data.size(), buf, &writeSize, bufSize, mbedtls_ctr_drbg_random, &entropy.ctr_drbg) == 0) {
			cb(BytesView(buf, writeSize));
			return true;
		}

		return false;
	},
	.privFingerprint = [] (const KeyContext &ctx, const Callback<void(BytesView)> &cb, const CoderSource &data) {
		switch (ctx.type) {
		case KeyType::RSA:
		case KeyType::DSA:
			return s_mbedTLSCtx.privSign(ctx, cb, data, SignAlgorithm::RSA_SHA512);
			break;
		case KeyType::ECDSA:
		case KeyType::EDDSA_ED448:
			return s_mbedTLSCtx.privSign(ctx, cb, data, SignAlgorithm::ECDSA_SHA512);
			break;
		default:
			break;
		}
		return false;
	},
	.pubInit = [] (KeyContext &ctx) -> bool {
		mbedtls_pk_init(reinterpret_cast<mbedtls_pk_context *>(&ctx));
		return true;
	},
	.pubFree = [] (KeyContext &ctx) {
		mbedtls_pk_free(reinterpret_cast<mbedtls_pk_context *>(&ctx));
	},
	.pubImport = [] (KeyContext &ctx, BytesView data) {
		auto loadKey = [&] (BytesView input) {
			auto key = reinterpret_cast<mbedtls_pk_context *>(&ctx);
			auto err = mbedtls_pk_parse_public_key(key, input.data(), input.size());
			if (err != 0) {
				mbedtls_pk_free( key );
				return false;
			}

			ctx.type = getMbedTLSKeyType(mbedtls_pk_get_type(key));
			return true;
		};

		if (!isPemKey(data) || data.data()[data.size() - 1] == 0) {
			return loadKey(data);
		} else {
			if (data.data()[data.size()] == 0) {
				return loadKey(BytesView(data.data(), data.size() + 1));
			} else {
				uint8_t buf[data.size() + 1];
				memcpy(buf, data.data(), data.size());
				buf[data.size()] = 0;

				return loadKey(BytesView(buf, data.size() + 1));
			}
		}
		return false;
	},
	.pubImportOpenSSH = [] (KeyContext &ctx, StringView r) {
		uint8_t out[MBEDTLS_KEY_BUFFER_SIZE];
		uint8_t *buf = out;

		auto origKeyType = r.readUntil<StringView::CharGroup<CharGroupId::WhiteSpace>>();
		r.skipChars<StringView::CharGroup<CharGroupId::WhiteSpace>>();
		auto dataBlock = r.readUntil<StringView::CharGroup<CharGroupId::WhiteSpace>>();
		if (valid::validateBase64(dataBlock)) {
			uint8_t bytes[base64::decodeSize(dataBlock.size())];
			uint8_t *target = bytes;
			base64::decode([&] (uint8_t c) {
				*target++ = c;
			}, dataBlock);

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

			buf = writeRSAKey(buf, modulus, exp);

			auto key = reinterpret_cast<mbedtls_pk_context *>(&ctx);
			if (mbedtls_pk_parse_public_key(key, out, buf - out) != 0) {
				return false;
			}

			ctx.type = getMbedTLSKeyType(mbedtls_pk_get_type(key));
			return true;
		}

		return false;
	},
	.pubExportPem = [] (const KeyContext &ctx, const Callback<void(BytesView)> &cb) {
		auto key = const_cast<mbedtls_pk_context *>(reinterpret_cast<const mbedtls_pk_context *>(&ctx));

		uint8_t buf[MBEDTLS_KEY_BUFFER_SIZE];
		int ret = -1;

		ret = mbedtls_pk_write_pubkey_pem(key, buf, MBEDTLS_KEY_BUFFER_SIZE);
		if (ret > 0) {
			cb(BytesView(buf + MBEDTLS_KEY_BUFFER_SIZE - ret, ret));
			return true;
		} else if (ret == 0) {
			auto len = strlen(reinterpret_cast<char *>(buf));
			if (len < MBEDTLS_KEY_BUFFER_SIZE) {
				cb(BytesView(buf, len));
				return true;
			}
		}

		return false;
	},
	.pubExportDer = [] (const KeyContext &ctx, const Callback<void(BytesView)> &cb) {
		auto key = reinterpret_cast<const mbedtls_pk_context *>(&ctx);

		uint8_t buf[MBEDTLS_KEY_BUFFER_SIZE];
		int ret = -1;

		ret = mbedtls_pk_write_pubkey_der(key, buf, MBEDTLS_KEY_BUFFER_SIZE);
		if (ret > 0) {
			cb(BytesView(buf + MBEDTLS_KEY_BUFFER_SIZE - ret, ret));
			return true;
		}

		return false;
	},
	.pubVerify = [] (const KeyContext &ctx, const CoderSource &data, BytesView signature, SignAlgorithm algo) {
		auto key = const_cast<mbedtls_pk_context *>(reinterpret_cast<const mbedtls_pk_context *>(&ctx));

		switch (algo) {
		case SignAlgorithm::ECDSA_SHA256:
		case SignAlgorithm::RSA_SHA256: {
			auto hash = string::Sha256().update(data).final();
			if (mbedtls_pk_verify(key, mbedtls_md_type_t(MBEDTLS_MD_SHA256), hash.data(), hash.size(),
					signature.data(), signature.size()) == 0) {
				return true;
			}
			break;
		}
		case SignAlgorithm::ECDSA_SHA512:
		case SignAlgorithm::RSA_SHA512: {
			auto hash = string::Sha512().update(data).final();
			if (mbedtls_pk_verify(key, mbedtls_md_type_t(MBEDTLS_MD_SHA512), hash.data(), hash.size(),
					signature.data(), signature.size()) == 0) {
				return true;
			}
			break;
		}
		case SignAlgorithm::GOST_256:
		case SignAlgorithm::GOST_512:
			return false;
		}
		return false;
	},
	.pubEncrypt = [] (const KeyContext &ctx, const Callback<void(BytesView)> &cb, const CoderSource &data) {
		EntropyContext entropy;
		if (!entropy) {
			return false;
		}

		size_t bufSize = math::align<size_t>(data.size(), 1_KiB) + MBEDTLS_PK_SIGNATURE_MAX_SIZE;
		size_t writeSize = 0;
		uint8_t buf[bufSize];

		auto key = const_cast<mbedtls_pk_context *>(reinterpret_cast<const mbedtls_pk_context *>(&ctx));

		if (mbedtls_pk_encrypt(key, data.data(), data.size(), buf, &writeSize, bufSize, mbedtls_ctr_drbg_random, &entropy.ctr_drbg) == 0) {
			cb(BytesView(buf, writeSize));
			return true;
		}

		return false;
	}
};

BackendCtxRef s_mbedTlsRef(&s_mbedTLSCtx);

} // namespace stappler::crypto

#endif
