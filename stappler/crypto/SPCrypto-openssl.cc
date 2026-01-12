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
#include "SPCrypto.h"
#include "SPValid.h"

#include <sprt/runtime/base64.h>

#if __CDT_PARSER__
#define MODULE_STAPPLER_CRYPTO_OPENSSL 1
#endif

#if MODULE_STAPPLER_CRYPTO_OPENSSL

#if XWIN
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnonportable-include-path"
#pragma clang diagnostic ignored "-Wmacro-redefined"
#pragma clang diagnostic ignored "-Wignored-pragma-intrinsic"
#pragma clang diagnostic ignored "-Wignored-attributes"
#pragma clang diagnostic ignored "-Wpragma-pack"
#pragma clang diagnostic ignored "-Wunknown-pragmas"
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#pragma clang diagnostic ignored "-Wunused-value"
#endif

#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/engine.h>

#if XWIN
#pragma clang diagnostic pop
#endif

#ifndef STAPPLER_SHARED
extern "C" {
#include <gost-engine.h>
}
#endif // STAPPLER_SHARED

#define EVP_PKEY_CTRL_GOST_PARAMSET (EVP_PKEY_ALG_CTRL+1)

namespace STAPPLER_VERSIONIZED stappler::crypto {

static uint8_t *writeRSAKey(uint8_t *buf, BytesViewNetwork mod, BytesViewNetwork exp);
static void fillCryptoBlockHeader(uint8_t *buf, const BlockKey256 &key, BytesView d);

static const char *ossl_engine_gost_id = "stappler-gost-hook";
static const char *ossl_engine_gost_name = "Hook for GOST engine sign functions";

static EVP_PKEY_METHOD *s_ossl_GostR3410_2012_256_resign;
static int (*s_ossl_GostR3410_2012_256_psign_init)(EVP_PKEY_CTX *ctx);
static int (*s_ossl_GostR3410_2012_256_psign)(EVP_PKEY_CTX *ctx, unsigned char *sig, size_t *siglen,
		const unsigned char *tbs, size_t tbslen);

static EVP_PKEY_METHOD *s_ossl_GostR3410_2012_512_resign;
static int (*s_ossl_GostR3410_2012_512_psign_init)(EVP_PKEY_CTX *ctx);
static int (*s_ossl_GostR3410_2012_512_psign)(EVP_PKEY_CTX *ctx, unsigned char *sig, size_t *siglen,
		const unsigned char *tbs, size_t tbslen);

static EVP_PKEY_METHOD *s_ossl_GostR3410_2012_meths[] = {nullptr, nullptr};

static int s_ossl_GostR3410_2012_nids_array[] = {NID_id_GostR3410_2012_256,
	NID_id_GostR3410_2012_512};

static ENGINE *s_ossl_Engine = nullptr;

typedef enum bnrand_flag_e {
	NORMAL,
	TESTING,
	PRIVATE
} BNRAND_FLAG;

static int hook_ossl_bnrand(BIGNUM *rnd, int bits, int top, int bottom, unsigned int strength,
		BytesView rndData) {
	unsigned char *buf = NULL;
	int ret = 0, bit, bytes, mask;

	if (bits == 0) {
		if (top != BN_RAND_TOP_ANY || bottom != BN_RAND_BOTTOM_ANY) {
			goto toosmall;
		}
		BN_zero(rnd);
		return 1;
	}
	if (bits < 0 || (bits == 1 && top > 0)) {
		goto toosmall;
	}

	bytes = (bits + 7) / 8;
	bit = (bits - 1) % 8;
	mask = 0xff << (bit + 1);

	buf = static_cast<unsigned char *>(OPENSSL_malloc(bytes));
	if (buf == NULL) {
		ERR_raise(ERR_LIB_BN, ERR_R_MALLOC_FAILURE);
		goto err;
	}

	/* make a random number and set the top and bottom bits */
	if (size_t(bytes) > rndData.size()) {
		goto err;
	}
	memcpy(buf, rndData.data(), bytes);

	if (top >= 0) {
		if (top) {
			if (bit == 0) {
				buf[0] = 1;
				buf[1] |= 0x80;
			} else {
				buf[0] |= (3 << (bit - 1));
			}
		} else {
			buf[0] |= (1 << bit);
		}
	}
	buf[0] &= ~mask;
	if (bottom) { /* set bottom bit if requested */
		buf[bytes - 1] |= 1;
	}
	if (!BN_bin2bn(buf, bytes, rnd)) {
		goto err;
	}
	ret = 1;
err:
	OPENSSL_clear_free(buf, bytes);
	return ret;

toosmall:
	ERR_raise(ERR_LIB_BN, BN_R_BITS_TOO_SMALL);
	return 0;
}

/* random number r:  0 <= r < range */
static int hook_ossl_bnrand_range(BIGNUM *r, const BIGNUM *range, unsigned int strength,
		BytesView rndData) {
	int n;
	int count = 100;

	if (r == NULL) {
		ERR_raise(ERR_LIB_BN, ERR_R_PASSED_NULL_PARAMETER);
		return 0;
	}

	if (BN_is_zero(range)) {
		ERR_raise(ERR_LIB_BN, BN_R_INVALID_RANGE);
		return 0;
	}

	n = BN_num_bits(range); /* n > 0 */

	/* BN_is_bit_set(range, n - 1) always holds */

	if (n == 1) {
		BN_zero(r);
	} else if (!BN_is_bit_set(range, n - 2) && !BN_is_bit_set(range, n - 3)) {
		/*
		 * range = 100..._2, so 3*range (= 11..._2) is exactly one bit longer
		 * than range
		 */
		do {
			if (!hook_ossl_bnrand(r, n + 1, BN_RAND_TOP_ANY, BN_RAND_BOTTOM_ANY, strength,
						rndData)) {
				return 0;
			}

			/*
			 * If r < 3*range, use r := r MOD range (which is either r, r -
			 * range, or r - 2*range). Otherwise, iterate once more. Since
			 * 3*range = 11..._2, each iteration succeeds with probability >=
			 * .75.
			 */
			if (BN_cmp(r, range) >= 0) {
				if (!BN_sub(r, r, range)) {
					return 0;
				}
				if (BN_cmp(r, range) >= 0) {
					if (!BN_sub(r, r, range)) {
						return 0;
					}
				}
			}

			if (!--count) {
				break;
			}

		} while (BN_cmp(r, range) >= 0);
	} else {
		do {
			/* range = 11..._2  or  range = 101..._2 */
			if (!hook_ossl_bnrand(r, n, BN_RAND_TOP_ANY, BN_RAND_BOTTOM_ANY, 0, rndData)) {
				return 0;
			}

			if (!--count) {
				ERR_raise(ERR_LIB_BN, BN_R_TOO_MANY_ITERATIONS);
				return 0;
			}
		} while (BN_cmp(r, range) >= 0);
	}

	return 1;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wwrite-strings"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

static ECDSA_SIG *hook_ossl_gost_ec_sign(const unsigned char *dgst, int dlen, EC_KEY *eckey,
		int nbytes) {
	ECDSA_SIG *newsig = NULL, *ret = NULL;
	BIGNUM *md = NULL;
	BIGNUM *order = NULL;
	const EC_GROUP *group;
	const BIGNUM *priv_key;
	BIGNUM *r = NULL, *s = NULL, *X = NULL, *tmp = NULL, *tmp2 = NULL, *k = NULL, *e = NULL;

	BIGNUM *new_r = NULL, *new_s = NULL;

	EC_POINT *C = NULL;
	BN_CTX *ctx;

	OPENSSL_assert(dgst != NULL && eckey != NULL);

	if (!(ctx = BN_CTX_secure_new())) {
		SPGOSTerr(SP_GOST_F_GOST_EC_SIGN, ERR_R_MALLOC_FAILURE);
		return NULL;
	}

	BN_CTX_start(ctx);
	OPENSSL_assert(dlen == 32 || dlen == 64);
	md = BN_lebin2bn(dgst, dlen, NULL);
	newsig = ECDSA_SIG_new();
	if (!newsig || !md) {
		SPGOSTerr(SP_GOST_F_GOST_EC_SIGN, ERR_R_MALLOC_FAILURE);
		goto err;
	}
	group = EC_KEY_get0_group(eckey);
	if (!group) {
		SPGOSTerr(SP_GOST_F_GOST_EC_SIGN, ERR_R_INTERNAL_ERROR);
		goto err;
	}
	order = BN_CTX_get(ctx);
	if (!order || !EC_GROUP_get_order(group, order, ctx)) {
		SPGOSTerr(SP_GOST_F_GOST_EC_SIGN, ERR_R_INTERNAL_ERROR);
		goto err;
	}
	priv_key = EC_KEY_get0_private_key(eckey);

	if (!priv_key) {
		SPGOSTerr(SP_GOST_F_GOST_EC_SIGN, ERR_R_INTERNAL_ERROR);
		goto err;
	}
	e = BN_CTX_get(ctx);
	if (!e || !BN_mod(e, md, order, ctx)) {
		SPGOSTerr(SP_GOST_F_GOST_EC_SIGN, ERR_R_INTERNAL_ERROR);
		goto err;
	}
#ifdef DEBUG_SIGN
	fprintf(stderr, "digest as bignum=");
	BN_print_fp(stderr, md);
	fprintf(stderr, "\ndigest mod q=");
	BN_print_fp(stderr, e);
	fprintf(stderr, "\n");
#endif
	if (BN_is_zero(e)) {
		BN_one(e);
	}
	k = BN_CTX_get(ctx);
	C = EC_POINT_new(group);
	if (!k || !C) {
		SPGOSTerr(SP_GOST_F_GOST_EC_SIGN, ERR_R_MALLOC_FAILURE);
		goto err;
	}

	do {
		do {
			auto hexPrivKey = BN_bn2hex(priv_key);
			auto privBytes = base16::decode<memory::StandartInterface>(StringView(hexPrivKey));
			std::reverse(privBytes.begin(), privBytes.end());
			OPENSSL_free(hexPrivKey);

			auto randSeed = Gost3411_512::hmac(privBytes, BytesView(dgst, dlen));

			if (!hook_ossl_bnrand_range(k, order, 0, randSeed)) {
				SPGOSTerr(SP_GOST_F_GOST_EC_SIGN, SP_GOST_R_RNG_ERROR);
				goto err;
			}

			if (!gost_ec_point_mul(group, C, k, NULL, NULL, ctx)) {
				SPGOSTerr(SP_GOST_F_GOST_EC_SIGN, ERR_R_EC_LIB);
				goto err;
			}
			if (!X) {
				X = BN_CTX_get(ctx);
			}
			if (!r) {
				r = BN_CTX_get(ctx);
			}
			if (!X || !r) {
				SPGOSTerr(SP_GOST_F_GOST_EC_SIGN, ERR_R_MALLOC_FAILURE);
				goto err;
			}
			if (!EC_POINT_get_affine_coordinates(group, C, X, NULL, ctx)) {
				SPGOSTerr(SP_GOST_F_GOST_EC_SIGN, ERR_R_EC_LIB);
				goto err;
			}

			if (!BN_nnmod(r, X, order, ctx)) {
				SPGOSTerr(SP_GOST_F_GOST_EC_SIGN, ERR_R_INTERNAL_ERROR);
				goto err;
			}
		} while (BN_is_zero(r));
		/* s =  (r*priv_key+k*e) mod order */
		if (!tmp) {
			tmp = BN_CTX_get(ctx);
		}
		if (!tmp2) {
			tmp2 = BN_CTX_get(ctx);
		}
		if (!s) {
			s = BN_CTX_get(ctx);
		}
		if (!tmp || !tmp2 || !s) {
			SPGOSTerr(SP_GOST_F_GOST_EC_SIGN, ERR_R_MALLOC_FAILURE);
			goto err;
		}

		if (!BN_mod_mul(tmp, priv_key, r, order, ctx) || !BN_mod_mul(tmp2, k, e, order, ctx)
				|| !BN_mod_add(s, tmp, tmp2, order, ctx)) {
			SPGOSTerr(SP_GOST_F_GOST_EC_SIGN, ERR_R_INTERNAL_ERROR);
			goto err;
		}
	} while (BN_is_zero(s));

	new_s = BN_dup(s);
	new_r = BN_dup(r);
	if (!new_s || !new_r) {
		SPGOSTerr(SP_GOST_F_GOST_EC_SIGN, ERR_R_MALLOC_FAILURE);
		goto err;
	}
	ECDSA_SIG_set0(newsig, new_r, new_s);

	ret = newsig;
err:
	BN_CTX_end(ctx);
	BN_CTX_free(ctx);
	if (C) {
		EC_POINT_free(C);
	}
	if (md) {
		BN_free(md);
	}
	if (!ret && newsig) {
		ECDSA_SIG_free(newsig);
	}
	return ret;
}

static int s_ossl_GostR3410_2012_256_psign_resign(EVP_PKEY_CTX *ctx, unsigned char *sig,
		size_t *siglen, const unsigned char *tbs, size_t tbs_len) {
	if (!sig) {
		return s_ossl_GostR3410_2012_256_psign(ctx, sig, siglen, tbs, tbs_len);
	} else {
		size_t order;
		s_ossl_GostR3410_2012_256_psign(ctx, nullptr, &order, tbs, tbs_len);

		EVP_PKEY *pkey = EVP_PKEY_CTX_get0_pkey(ctx);
		auto unpacked_sig =
				hook_ossl_gost_ec_sign(tbs, int(tbs_len), (EC_KEY *)EVP_PKEY_get0(pkey), 32);
		if (!unpacked_sig) {
			return 0;
		}
		return pack_sign_cp(unpacked_sig, int(order / 2), sig, siglen);
	}
}

static int s_ossl_GostR3410_2012_512_psign_resign(EVP_PKEY_CTX *ctx, unsigned char *sig,
		size_t *siglen, const unsigned char *tbs, size_t tbs_len) {
	if (!sig) {
		return s_ossl_GostR3410_2012_512_psign(ctx, sig, siglen, tbs, tbs_len);
	} else {
		size_t order;
		s_ossl_GostR3410_2012_512_psign(ctx, nullptr, &order, tbs, tbs_len);

		EVP_PKEY *pkey = EVP_PKEY_CTX_get0_pkey(ctx);
		auto unpacked_sig =
				hook_ossl_gost_ec_sign(tbs, int(tbs_len), (EC_KEY *)EVP_PKEY_get0(pkey), 64);
		if (!unpacked_sig) {
			return 0;
		}
		return pack_sign_cp(unpacked_sig, int(order / 2), sig, siglen);
	}
}

#pragma GCC diagnostic pop

static int ossl_gost_meth_nids(const int **nids) {
	if (nids) {
		*nids = s_ossl_GostR3410_2012_nids_array;
	}
	return 2;
}

static int ossl_gost_pkey_meths(ENGINE *e, EVP_PKEY_METHOD **pmeth, const int **nids, int nid) {
	if (!pmeth) {
		return ossl_gost_meth_nids(nids);
	}

	size_t i = 0;
	for (auto &it : s_ossl_GostR3410_2012_nids_array) {
		if (nid == it) {
			*pmeth = s_ossl_GostR3410_2012_meths[i];
			return 1;
		}
		++i;
	}

	*pmeth = NULL;
	return 0;
}

// For interoperability with GnuTLS
static constexpr int OPENSSL_PK_ENCRYPT_PADDING = RSA_PKCS1_PADDING;

SP_COVERAGE_TRIVIAL
static void logOpenSSLErrors() {
	BIO *bio = BIO_new(BIO_s_mem());
	ERR_print_errors(bio);
	char *buf;
	size_t len = BIO_get_mem_data(bio, &buf);
	log::source().error("OpenSSL", StringView(buf, len));
	BIO_free(bio);
	ERR_clear_error();
}

static KeyType getOpenSSLKeyType(int id) {
	if (id == EVP_PKEY_RSA) {
		return KeyType::RSA;
	} else if (id == EVP_PKEY_DSA) {
		return KeyType::DSA;
	} else if (id == EVP_PKEY_EC) {
		return KeyType::ECDSA;
	} else if (id == EVP_PKEY_ED448) {
		return KeyType::EDDSA_ED448;
	} else if (id == NID_id_GostR3410_2012_256) {
		return KeyType::GOST3410_2012_256;
	} else if (id == NID_id_GostR3410_2012_512) {
		return KeyType::GOST3410_2012_512;
	}
	return KeyType::Unknown;
}

static const EVP_CIPHER *getOpenSSLCipher(BlockCipher b) {
	switch (b) {
	case BlockCipher::AES_CBC: return EVP_aes_256_cbc(); break;
	case BlockCipher::AES_CFB8: return EVP_aes_256_cfb8(); break;
	case BlockCipher::Gost3412_2015_CTR_ACPKM:
		if (auto m = EVP_get_cipherbyname("kuznyechik-ctr-acpkm")) {
			return m;
		}
		break;
	}
	return EVP_aes_256_cbc();
}

static bool s_opensslHasGost = false;

static bool OpenSSL_initSPGost() {
	static bool init = false;
	if (init) {
		return s_opensslHasGost;
	}

	init = true;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#ifndef STAPPLER_SHARED
	ENGINE_load_gost();
#endif

	auto e = ENGINE_get_pkey_meth_engine(NID_id_GostR3410_2012_256);
	if (e) {
		if (auto meth = ENGINE_get_pkey_meth(e, NID_id_GostR3410_2012_256)) {
			s_ossl_GostR3410_2012_256_resign = EVP_PKEY_meth_new(NID_id_GostR3410_2012_256, 0);
			EVP_PKEY_meth_copy(s_ossl_GostR3410_2012_256_resign, meth);

			EVP_PKEY_meth_get_sign(s_ossl_GostR3410_2012_256_resign,
					&s_ossl_GostR3410_2012_256_psign_init, &s_ossl_GostR3410_2012_256_psign);
			EVP_PKEY_meth_set_sign(s_ossl_GostR3410_2012_256_resign,
					s_ossl_GostR3410_2012_256_psign_init, &s_ossl_GostR3410_2012_256_psign_resign);

			s_ossl_GostR3410_2012_meths[0] = s_ossl_GostR3410_2012_256_resign;
			EVP_PKEY_meth_add0(s_ossl_GostR3410_2012_256_resign);
		}
		if (auto meth = ENGINE_get_pkey_meth(e, NID_id_GostR3410_2012_512)) {
			s_ossl_GostR3410_2012_512_resign = EVP_PKEY_meth_new(NID_id_GostR3410_2012_512, 0);
			EVP_PKEY_meth_copy(s_ossl_GostR3410_2012_512_resign, meth);

			EVP_PKEY_meth_get_sign(s_ossl_GostR3410_2012_512_resign,
					&s_ossl_GostR3410_2012_512_psign_init, &s_ossl_GostR3410_2012_512_psign);
			EVP_PKEY_meth_set_sign(s_ossl_GostR3410_2012_512_resign,
					s_ossl_GostR3410_2012_512_psign_init, &s_ossl_GostR3410_2012_512_psign_resign);

			s_ossl_GostR3410_2012_meths[1] = s_ossl_GostR3410_2012_512_resign;
			EVP_PKEY_meth_add0(s_ossl_GostR3410_2012_512_resign);
		}
	}

	if (s_ossl_GostR3410_2012_meths[0] && s_ossl_GostR3410_2012_meths[1]) {
		if (auto meth = ENGINE_get_pkey_asn1_meth(e, NID_id_GostR3410_2012_256)) {
			EVP_PKEY_asn1_add0(meth);
		}
		if (auto meth = ENGINE_get_pkey_asn1_meth(e, NID_id_GostR3410_2012_512)) {
			EVP_PKEY_asn1_add0(meth);
		}

		s_ossl_Engine = ENGINE_new();
		ENGINE_set_id(s_ossl_Engine, ossl_engine_gost_id);
		ENGINE_set_name(s_ossl_Engine, ossl_engine_gost_name);
		ENGINE_set_pkey_meths(s_ossl_Engine, ossl_gost_pkey_meths);

		ENGINE_register_pkey_meths(s_ossl_Engine);

		ENGINE_register_all_complete();
		SP_ERR_load_GOST_strings();
		return true;
	}
#pragma GCC diagnostic pop
	return false;
}

static BackendCtx s_openSSLCtx = {
	.name = Backend::OpenSSL,
	.title = StringView("OpenSSL"),
	.flags = BackendFlags::SupportsPKCS1 | BackendFlags::SupportsPKCS8 | BackendFlags::SupportsAes | BackendFlags::SecureLibrary
			| BackendFlags::SupportsGost3410_2012 | BackendFlags::SupportsGost3412_2015,
	.initialize = [] (BackendCtx &ctx) {
		auto gostLoaded = OpenSSL_initSPGost();
		OPENSSL_init_ssl(OPENSSL_INIT_SSL_DEFAULT, NULL);
		if (gostLoaded) {
			s_opensslHasGost = true;
			log::source().verbose("Crypto", "OpenSSL+gost backend loaded");
		} else {
			ctx.flags &= ~(BackendFlags::SupportsGost3410_2012 | BackendFlags::SupportsGost3412_2015);
			log::source().warn("Crypto", "OpenSSL backend loaded without GOST support");
		}
	},
	.finalize = [] (BackendCtx &ctx) {
		if (s_opensslHasGost) {
			SP_ERR_unload_GOST_strings();
		}
	},
	.encryptBlock = [] (const BlockKey256 &key, BytesView d, const Callback<void(BytesView)> &cb) -> bool {
		auto cipherBlockSize = getBlockSize(key.cipher);
		auto cipher = getOpenSSLCipher(key.cipher);

		uint64_t dataSize = d.size();
		auto blockSize = math::align<size_t>(dataSize, cipherBlockSize)
				+ cipherBlockSize; // allocate space for possible padding

		uint8_t output[blockSize + sizeof(BlockCryptoHeader)];

		fillCryptoBlockHeader(output, key, d);

		uint8_t iv[16] = { 0 };
		EVP_CIPHER_CTX *en = nullptr;

		auto finalize = [&] (bool value) {
			if (en) {
				EVP_CIPHER_CTX_free(en);
				en = nullptr;
			}
			return value;
		};

		en = EVP_CIPHER_CTX_new();
		if (!en) {
			return finalize(false);
		}

		if (!EVP_EncryptInit_ex(en, cipher, NULL, key.data.data(), iv)) {
			return finalize(false);
		}

		auto perform = [] (EVP_CIPHER_CTX *en, const uint8_t *target, size_t targetSize, uint8_t *out) {
			int outSize = 0;
			while (targetSize > 0) {
				if (!EVP_EncryptUpdate(en, out, &outSize, target, int(targetSize))) {
					return false;
				}

				out += outSize;
				target += outSize;
				if (int(targetSize) >= outSize) {
					targetSize -= outSize;
				} else {
					targetSize = 0;
				}
			}

			if (!EVP_EncryptFinal(en, out, &outSize)) {
				return false;
			}

			return true;
		};

		fillCryptoBlockHeader(output, key, d);

		if constexpr (SAFE_BLOCK_ENCODING) {
			uint8_t tmp[blockSize];
			memset(tmp, 0, blockSize);
			memcpy(tmp, d.data(), d.size());

			if (!perform(en, tmp, blockSize - cipherBlockSize, output + sizeof(BlockCryptoHeader))) {
				return finalize(false);
			}
		} else {
			if (!perform(en, d.data(), d.size(), output + sizeof(BlockCryptoHeader))) {
				return finalize(false);
			}
		}

		cb(BytesView(output, blockSize + sizeof(BlockCryptoHeader) - cipherBlockSize));
		return finalize(true);
	},
	.decryptBlock = [] (const BlockKey256 &key, BytesView b, const Callback<void(BytesView)> &cb) -> bool {
		auto info = getBlockInfo(b);
		auto cipherBlockSize = getBlockSize(info.cipher);
		auto cipher = getOpenSSLCipher(info.cipher);

		auto blockSize = math::align<size_t>(info.dataSize, cipherBlockSize) + cipherBlockSize;
		b.offset(sizeof(BlockCryptoHeader));

		uint8_t output[blockSize];

		uint8_t iv[16] = { 0 };
		EVP_CIPHER_CTX *de = nullptr;

		auto finalize = [&] (bool value) {
			if (de) {
				EVP_CIPHER_CTX_free(de);
				de = nullptr;
			}
			return value;
		};

		de = EVP_CIPHER_CTX_new();
		if (!de) {
			return finalize(false);
		}

		if (!EVP_DecryptInit_ex(de, cipher, NULL, key.data.data(), iv)) {
			return finalize(false);
		}

		auto target = b.data();
		auto targetSize = b.size();
		auto out = output;

		int outSize = 0;
		while (targetSize > 0) {
			if (!EVP_DecryptUpdate(de, out, &outSize, target, int(targetSize))) {
				return finalize(false);
			}

			out += outSize;
			target += outSize;
			if (int(targetSize) > outSize) {
				targetSize -= outSize;
			} else {
				targetSize = 0;
			}
		}

		EVP_DecryptFinal(de, out, &outSize); // gives false-positive error

		cb(BytesView(output, info.dataSize));
		return finalize(true);
	},
	.hash256 = [] (Sha256::Buf &buf, const Callback<void( const HashCoderCallback &upd )> &cb, HashFunction func) -> bool {
		bool success = true;
		EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
		EVP_MD_CTX_init(mdctx);

		switch (func) {
		case HashFunction::SHA_2:
			if (EVP_DigestInit(mdctx, EVP_sha256()) == 0) {
				success = false;
			}
			break;
		case HashFunction::GOST_3411:
			if (!s_opensslHasGost) {
				log::source().warn("Crypto", "OpenSSL backend loaded without GOST support");
				success = false;
			} else if (auto md = EVP_get_digestbyname("md_gost12_256")) {
				if (EVP_DigestInit(mdctx, md) == 0) {
					success = false;
				}
			} else {
				success = false;
			}
			break;
		}

		if (success) {
			cb([&] (const CoderSource &data) {
				if (success && EVP_DigestUpdate(mdctx, data.data(), data.size()) == 0) {
					success = false;
					return false;
				}
				return true;
			});

			if (success && EVP_DigestFinal(mdctx, buf.data(), nullptr) == 0) {
				success = false;
			}
		}

		EVP_MD_CTX_free(mdctx);
		return success;
	},
	.hash512 = [] (Sha512::Buf &buf, const Callback<void( const HashCoderCallback &upd )> &cb, HashFunction func) -> bool {
		bool success = true;
		EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
		EVP_MD_CTX_init(mdctx);

		switch (func) {
		case HashFunction::SHA_2:
			if (EVP_DigestInit(mdctx, EVP_sha512()) == 0) {
				success = false;
			}
			break;
		case HashFunction::GOST_3411:
			if (!s_opensslHasGost) {
				log::source().warn("Crypto", "OpenSSL backend loaded without GOST support");
				success = false;
			} else if (auto md = EVP_get_digestbyname("md_gost12_512")) {
				if (EVP_DigestInit(mdctx, md) == 0) {
					success = false;
				}
			} else {
				success = false;
			}
			break;
		}

		if (success) {
			cb([&] (const CoderSource &data) {
				if (success && EVP_DigestUpdate(mdctx, data.data(), data.size()) == 0) {
					success = false;
					return false;
				}
				return true;
			});

			if (success && EVP_DigestFinal(mdctx, buf.data(), nullptr) == 0) {
				success = false;
			}
		}

		EVP_MD_CTX_free(mdctx);
		return success;
	},
	.privInit = [] (KeyContext &ctx) -> bool {
		ctx.keyCtx = nullptr;
		return true;
	},
	.privFree = [] (KeyContext &ctx) {
		if (ctx.keyCtx) {
			EVP_PKEY_free( static_cast<EVP_PKEY *>(ctx.keyCtx) );
			ctx.keyCtx = nullptr;
		}
	},
	.privGen = [] (KeyContext &ctx, KeyBits bits, KeyType type) -> bool {
		EVP_PKEY_CTX *kctx = nullptr;
		auto finalize = [&] (bool value) {
			if (kctx) {
				EVP_PKEY_CTX_free(kctx);
				kctx = nullptr;
			}
			if (!value) {
				ctx.keyCtx = nullptr;
				logOpenSSLErrors();
			}
			return value;
		};

		if (type == KeyType::RSA) {
			kctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
			if (!kctx) { return finalize(false); }

			if (!EVP_PKEY_keygen_init(kctx)) { return finalize(false); }

			switch (bits) {
			case KeyBits::_1024: if (!EVP_PKEY_CTX_set_rsa_keygen_bits(kctx, 1'024)) { return finalize(false); } break;
			case KeyBits::_2048: if (!EVP_PKEY_CTX_set_rsa_keygen_bits(kctx, 2'048)) { return finalize(false); } break;
			case KeyBits::_4096: if (!EVP_PKEY_CTX_set_rsa_keygen_bits(kctx, 4'096)) { return finalize(false); } break;
			}
		} else if (type == KeyType::GOST3410_2012_256) {
			if (!s_opensslHasGost) {
				log::source().warn("Crypto", "OpenSSL backend loaded without GOST support");
				return false;
			}

			kctx = EVP_PKEY_CTX_new_id(NID_id_GostR3410_2012_256, NULL);
			if (!kctx) { return finalize(false); }

			EVP_PKEY_paramgen_init(kctx);

			EVP_PKEY_CTX_ctrl(kctx, NID_id_GostR3410_2012_256, EVP_PKEY_OP_PARAMGEN, EVP_PKEY_CTRL_GOST_PARAMSET,
					NID_id_tc26_gost_3410_2012_256_paramSetA, NULL);

			if (!EVP_PKEY_keygen_init(kctx)) { return finalize(false); }

		} else if (type == KeyType::GOST3410_2012_512) {
			if (!s_opensslHasGost) {
				log::source().warn("Crypto", "OpenSSL backend loaded without GOST support");
				return false;
			}

			kctx = EVP_PKEY_CTX_new_id(NID_id_GostR3410_2012_512, NULL);
			if (!kctx) { return finalize(false); }

			EVP_PKEY_paramgen_init(kctx);

			EVP_PKEY_CTX_ctrl(kctx, NID_id_GostR3410_2012_512, EVP_PKEY_OP_PARAMGEN, EVP_PKEY_CTRL_GOST_PARAMSET,
					NID_id_tc26_gost_3410_2012_512_paramSetA, NULL);

			if (!EVP_PKEY_keygen_init(kctx)) { return finalize(false); }
		} else if (type == KeyType::ECDSA) {
			kctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, NULL);
			if (!kctx) { return finalize(false); }

			if (!EVP_PKEY_keygen_init(kctx)) { return finalize(false); }

			if (1 != EVP_PKEY_CTX_set_ec_paramgen_curve_nid(kctx, NID_X9_62_prime256v1)) { return finalize(false); }
		} else if (type == KeyType::EDDSA_ED448) {
			kctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED448, NULL);
			if (!kctx) { return finalize(false); }

			if (!EVP_PKEY_keygen_init(kctx)) { return finalize(false); }
		} else {
			log::source().error("Crypto-openssl", "Unsupported key type for keygen");
			return finalize(false);
		}

		EVP_PKEY *ret = nullptr;
		if (EVP_PKEY_keygen(kctx, &ret)) {
			ctx.keyCtx = ret;
			ctx.type = getOpenSSLKeyType(EVP_PKEY_get_id(ret));
			return finalize(true);
		}
		return finalize(false);
	},
	.privImport = [] (KeyContext &ctx, BytesView data, const CoderSource &passwd) {
		BIO *bioData = nullptr;

		auto finalize = [&] (bool value) {
			if (bioData) {
				BIO_free(bioData);
				bioData = nullptr;
			}
			if (!value) {
				ctx.keyCtx = nullptr;
				logOpenSSLErrors();
			} else {
				ctx.type = getOpenSSLKeyType(EVP_PKEY_get_id(static_cast<EVP_PKEY *>(ctx.keyCtx)));
			}
			return value;
		};

		bioData = BIO_new_mem_buf(data.data(), int(data.size()));
		if (!bioData) {
			return finalize(false);
		}

		if (isPemKey(data)) {
			ctx.keyCtx = PEM_read_bio_PrivateKey(bioData, NULL, [] (char *buf, int size, int rwflag, void *userdata) -> int {
				auto passwd = static_cast<const CoderSource *>(userdata);
				int i = int(passwd->size());
				i = (i > size) ? size : i;
				memcpy(buf, passwd->data(), i);
				return i;
			}, const_cast<CoderSource *>(&passwd));

			if (ctx.keyCtx) {
				return finalize(true);
			}
		} else {
			if (!passwd.empty()) {
				ctx.keyCtx = d2i_PKCS8PrivateKey_bio(bioData, NULL, [] (char *buf, int size, int rwflag, void *userdata) -> int {
					auto passwd = static_cast<const CoderSource *>(userdata);
					int i = int(passwd->size());
					i = (i > size) ? size : i;
					memcpy(buf, passwd->data(), i);
					return i;
				}, const_cast<CoderSource *>(&passwd));
			} else {
				ctx.keyCtx = d2i_PrivateKey_bio(bioData, NULL);
			}

			if (ctx.keyCtx) {
				return finalize(true);
			}
		}
		return finalize(false);
	},
	.privExportPem = [] (const KeyContext &ctx, const Callback<void(BytesView)> &cb, KeyFormat fmt, const CoderSource &passPhrase) {
		BIO *bp = nullptr;
		auto key = static_cast<const EVP_PKEY *>(ctx.keyCtx);

		auto finalize = [&] (bool value) {
			if (bp) {
				BIO_free(bp);
				bp = nullptr;
			}
			return value;
		};

		if (!key) {
			return finalize(false);
		}

		bp = BIO_new(BIO_s_mem());
		if (!bp) {
			return finalize(false);
		}

		switch (fmt) {
		case KeyFormat::RSA:
			if (passPhrase.empty()) {
				if (!PEM_write_bio_PrivateKey_traditional(bp, key, NULL, nullptr, 0, 0, NULL)) {
					return finalize(false);
				}
			} else {
				if (!PEM_write_bio_PrivateKey_traditional(bp, key, EVP_des_ede3_cbc(), const_cast<unsigned char *>(passPhrase.data()), int(passPhrase.size()), 0, NULL)) {
					return finalize(false);
				}
			}
			break;
		case KeyFormat::PKCS8:
			if (passPhrase.empty()) {
				if (!PEM_write_bio_PKCS8PrivateKey(bp, key, NULL, NULL, 0, 0, NULL)) {
					return finalize(false);
				}
			} else {
				if (!PEM_write_bio_PKCS8PrivateKey(bp, key, EVP_des_ede3_cbc(), reinterpret_cast<const char *>(passPhrase.data()), int(passPhrase.size()), 0, NULL)) {
					return finalize(false);
				}
			}
			break;
		}

		char *buf = nullptr;
		size_t len = BIO_get_mem_data(bp, &buf);
		if (len > 0) {
			cb(BytesView(reinterpret_cast<uint8_t *>(buf), len));
			return finalize(true);
		}

		return finalize(false);
	},
	.privExportDer = [] (const KeyContext &ctx, const Callback<void(BytesView)> &cb, KeyFormat fmt, const CoderSource &passPhrase) {
		BIO *bp = nullptr;
		auto key = static_cast<const EVP_PKEY *>(ctx.keyCtx);

		auto finalize = [&] (bool value) {
			if (bp) {
				BIO_free(bp);
				bp = nullptr;
			}
			return value;
		};

		if (!key) {
			return finalize(false);
		}

		bp = BIO_new(BIO_s_mem());
		if (!bp) {
			return finalize(false);
		}

		switch (fmt) {
		case KeyFormat::RSA:
			if (passPhrase.empty()) {
				if (!i2d_PrivateKey_bio(bp, key)) {
					return finalize(false);
				}
			} else {
				log::source().error("PrivateKey", "exportDer: passPhrase is not supported for KeyFormat::RSA");
				return finalize(false);
			}
			break;
		case KeyFormat::PKCS8:
			if (passPhrase.empty()) {
				if (!i2d_PKCS8PrivateKey_bio(bp, key, NULL, NULL, 0, 0, NULL)) {
					return finalize(false);
				}
			} else {
				if (!i2d_PKCS8PrivateKey_bio(bp, key, EVP_des_ede3_cbc(), reinterpret_cast<const char *>(passPhrase.data()), int(passPhrase.size()), 0, NULL)) {
					return finalize(false);
				}
			}
			break;
		}

		char *buf = nullptr;
		size_t len = BIO_get_mem_data(bp, &buf);
		if (len > 0) {
			cb(BytesView(reinterpret_cast<uint8_t *>(buf), len));
			return finalize(true);
		}

		return finalize(false);
	},
	.privExportPublic = [] (KeyContext &target, const KeyContext &privKey) {
		if (!privKey.keyCtx) {
			return false;
		}

		auto bp = BIO_new(BIO_s_mem());
		if (!bp) {
			return false;
		}

		auto size = i2d_PUBKEY_bio(bp, static_cast<const EVP_PKEY *>(privKey.keyCtx));
		if (size > 0) {
			target.keyCtx = d2i_PUBKEY_bio(bp, NULL);
			target.type = getOpenSSLKeyType(EVP_PKEY_get_id(static_cast<EVP_PKEY *>(target.keyCtx)));
		}

		BIO_free(bp);
		return target.keyCtx != nullptr;
	},
	.privSign = [] (const KeyContext &ctx, const Callback<void(BytesView)> &cb, const CoderSource &data, SignAlgorithm algo) {
		auto key = static_cast<EVP_PKEY *>(ctx.keyCtx);
		EVP_MD_CTX *mdctx =  EVP_MD_CTX_create();
		EVP_PKEY_CTX *pctx = nullptr;
		unsigned char *sigdata = nullptr;
		size_t siglen = 0;

		auto cleanup = [&] (bool value) {
			if (mdctx) {
				EVP_MD_CTX_destroy(mdctx);
				mdctx = nullptr;
			}
			if (sigdata) {
				OPENSSL_free(sigdata);
				sigdata = nullptr;
			}
			return value;
		};

		if (!mdctx) {
			return cleanup(false);
		}

		switch (algo) {
		case SignAlgorithm::RSA_SHA256:
		case SignAlgorithm::ECDSA_SHA256:
			if (1 != EVP_DigestSignInit(mdctx, &pctx, (ctx.type == KeyType::EDDSA_ED448) ? nullptr : EVP_sha256(), NULL, key)) {
				return cleanup(false);
			}
			break;
		case SignAlgorithm::RSA_SHA512:
		case SignAlgorithm::ECDSA_SHA512:
			if (1 != EVP_DigestSignInit(mdctx, &pctx, (ctx.type == KeyType::EDDSA_ED448) ? nullptr : EVP_sha512(), NULL, key)) {
				return cleanup(false);
			}
			break;
		case SignAlgorithm::GOST_256:
			if (!s_opensslHasGost) {
				log::source().warn("Crypto", "OpenSSL backend loaded without GOST support");
				return cleanup(false);
			}

			if (auto md = EVP_get_digestbyname("md_gost12_256")) {
				if (1 != EVP_DigestSignInit(mdctx, &pctx, md, NULL, key)) {
					return cleanup(false);
				}
			} else {
				return cleanup(false);
			}
			break;
		case SignAlgorithm::GOST_512:
			if (!s_opensslHasGost) {
				log::source().warn("Crypto", "OpenSSL backend loaded without GOST support");
				return cleanup(false);
			}

			if (auto md = EVP_get_digestbyname("md_gost12_512")) {
				if (1 != EVP_DigestSignInit(mdctx, &pctx, md, NULL, key)) {
					return cleanup(false);
				}
			} else {
				return cleanup(false);
			}
			break;
		}
		if (algo == SignAlgorithm::GOST_256 || algo == SignAlgorithm::GOST_512) {
			if (1 != EVP_DigestSignUpdate(mdctx, data.data(), data.size())) {
				return cleanup(false);
			}

			if (1 == EVP_DigestSignFinal(mdctx, NULL, &siglen)) {
				sigdata = static_cast<unsigned char *>(OPENSSL_zalloc(sizeof(unsigned char) * siglen));
				if (sigdata) {
					if (1 == EVP_DigestSignFinal(mdctx, sigdata, &siglen)) {
						cb(BytesView(sigdata, siglen));
						return cleanup(true);
					} else {
						return cleanup(false);
					}
				} else {
					return cleanup(false);
				}
			} else {
				return cleanup(false);
			}
		} else {
			if (1 != EVP_DigestSign(mdctx, NULL, &siglen, data.data(), data.size())) {
				return cleanup(false);
			}

			sigdata = static_cast<unsigned char *>(OPENSSL_malloc(sizeof(unsigned char) * siglen));
			if (1 == EVP_DigestSign(mdctx, sigdata, &siglen, data.data(), data.size())) {
				cb(BytesView(sigdata, siglen));
				return cleanup(true);
			}
		}
		return cleanup(false);
	},
	.privVerify = [] (const KeyContext &ctx, const CoderSource &data, BytesView signature, SignAlgorithm algo) {
		auto key = static_cast<EVP_PKEY *>(ctx.keyCtx);
		EVP_MD_CTX *mdctx =  EVP_MD_CTX_create();

		auto cleanup = [&] (bool value) {
			if (mdctx) {
				EVP_MD_CTX_destroy(mdctx);
				mdctx = nullptr;
			}
			return value;
		};

		if (!mdctx) {
			return cleanup(false);
		}

		switch (algo) {
		case SignAlgorithm::RSA_SHA256:
		case SignAlgorithm::ECDSA_SHA256:
			if (!EVP_DigestVerifyInit(mdctx, NULL, (ctx.type == KeyType::EDDSA_ED448) ? nullptr : EVP_sha256(), NULL, key)) {
				return cleanup(false);
			}
			break;
		case SignAlgorithm::RSA_SHA512:
		case SignAlgorithm::ECDSA_SHA512:
			if (!EVP_DigestVerifyInit(mdctx, NULL, (ctx.type == KeyType::EDDSA_ED448) ? nullptr : EVP_sha512(), NULL, key)) {
				return cleanup(false);
			}
			break;
		case SignAlgorithm::GOST_256:
			if (!s_opensslHasGost) {
				log::source().warn("Crypto", "OpenSSL backend loaded without GOST support");
				return cleanup(false);
			}

			if (auto md = EVP_get_digestbyname("md_gost12_256")) {
				if (1 != EVP_DigestVerifyInit(mdctx, NULL, md, NULL, key)) {
					return cleanup(false);
				}
			} else {
				return cleanup(false);
			}
			break;
		case SignAlgorithm::GOST_512:
			if (!s_opensslHasGost) {
				log::source().warn("Crypto", "OpenSSL backend loaded without GOST support");
				return cleanup(false);
			}

			if (auto md = EVP_get_digestbyname("md_gost12_512")) {
				if (1 != EVP_DigestVerifyInit(mdctx, NULL, md, NULL, key)) {
					return cleanup(false);
				}
			} else {
				return cleanup(false);
			}
			break;
		}

		/* Initialize `key` with a public key */
		if (!EVP_DigestVerifyUpdate(mdctx, data.data(), data.size())) {
			return cleanup(false);
		}

		if (EVP_DigestVerifyFinal(mdctx, signature.data(), signature.size())) {
			return cleanup(true);
		}

		return cleanup(false);
	},
	.privEncrypt = [] (const KeyContext &ctx, const Callback<void(BytesView)> &cb, const CoderSource &data) {
		auto key = static_cast<EVP_PKEY *>(ctx.keyCtx);
		auto pctx = EVP_PKEY_CTX_new(key, nullptr);

		void *out = nullptr;

		auto cleanup = [&] (bool value) {
			if (out) {
				OPENSSL_free(out);
				out = nullptr;
			}
			if (pctx) {
				EVP_PKEY_CTX_free(pctx);
				pctx = nullptr;
			}
			return value;
		};

		if (EVP_PKEY_encrypt_init(pctx) <= 0) {
			return cleanup(false);
		}

		switch (ctx.type) {
		case KeyType::RSA:
			if (EVP_PKEY_CTX_set_rsa_padding(pctx, OPENSSL_PK_ENCRYPT_PADDING) <= 0) {
				return cleanup(false);
			}
			break;
		default:
			break;
		}

		/* Determine buffer length */
		size_t outlen;
		if (EVP_PKEY_encrypt(pctx, NULL, &outlen, data.data(), data.size()) <= 0) {
			return cleanup(false);
		}

		out = OPENSSL_malloc(outlen);

		if (!out) {
			return cleanup(false);
		}

		if (EVP_PKEY_encrypt(pctx, static_cast<uint8_t *>(out), &outlen, data.data(), data.size()) <= 0) {
			return cleanup(false);
		}

		cb(BytesView(static_cast<uint8_t *>(out), outlen));

		return cleanup(true);
	},
	.privDecrypt = [] (const KeyContext &ctx, const Callback<void(BytesView)> &cb, const CoderSource &data) {
		auto key = static_cast<EVP_PKEY *>(ctx.keyCtx);
		auto pctx = EVP_PKEY_CTX_new(key, nullptr);

		void *out = nullptr;

		auto cleanup = [&] (bool value) {
			if (out) {
				OPENSSL_free(out);
				out = nullptr;
			}
			if (pctx) {
				EVP_PKEY_CTX_free(pctx);
				pctx = nullptr;
			}
			return value;
		};

		if (EVP_PKEY_decrypt_init(pctx) <= 0) {
			return cleanup(false);
		}

		switch (ctx.type) {
		case KeyType::RSA:
			if (EVP_PKEY_CTX_set_rsa_padding(pctx, OPENSSL_PK_ENCRYPT_PADDING) <= 0) {
				return cleanup(false);
			}
			break;
		default:
			break;
		}

		/* Determine buffer length */
		size_t outlen;
		if (EVP_PKEY_decrypt(pctx, NULL, &outlen, data.data(), data.size()) <= 0) {
			return cleanup(false);
		}

		out = OPENSSL_malloc(outlen);

		if (!out) {
			return cleanup(false);
		}

		if (EVP_PKEY_decrypt(pctx, static_cast<uint8_t *>(out), &outlen, data.data(), data.size()) <= 0) {
			return cleanup(false);
		}

		cb(BytesView(static_cast<uint8_t *>(out), outlen));

		return cleanup(true);
	},
	.privFingerprint = [] (const KeyContext &ctx, const Callback<void(BytesView)> &cb, const CoderSource &data) {
		auto key = static_cast<EVP_PKEY *>(ctx.keyCtx);
		bool success = false;
		switch (ctx.type) {
		case KeyType::RSA:
		case KeyType::DSA:
			return s_openSSLCtx.privSign(ctx, cb, data, SignAlgorithm::RSA_SHA512);
			break;
		case KeyType::ECDSA:
		case KeyType::EDDSA_ED448:
			return s_openSSLCtx.privSign(ctx, cb, data, SignAlgorithm::RSA_SHA512);
			break;
		case KeyType::GOST3410_2012_256:
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
			if (!s_opensslHasGost) {
				log::source().warn("Crypto", "OpenSSL backend loaded without GOST support");
				return false;
			}

			EVP_PKEY_set1_engine(key, s_ossl_Engine);
			success = s_openSSLCtx.privSign(ctx, cb, data, SignAlgorithm::GOST_256);
			EVP_PKEY_set1_engine(key, nullptr);
			break;
		case KeyType::GOST3410_2012_512:
			if (!s_opensslHasGost) {
				log::source().warn("Crypto", "OpenSSL backend loaded without GOST support");
				return false;
			}

			EVP_PKEY_set1_engine(key, s_ossl_Engine);
			success = s_openSSLCtx.privSign(ctx, cb, data, SignAlgorithm::GOST_512);
			EVP_PKEY_set1_engine(key, nullptr);
#pragma GCC diagnostic pop
			break;
		default:
			break;
		}
		return success;
	},
	.pubInit = [] (KeyContext &ctx) -> bool {
		ctx.keyCtx = nullptr;
		return true;
	},
	.pubFree = [] (KeyContext &ctx) {
		if (ctx.keyCtx) {
			EVP_PKEY_free( static_cast<EVP_PKEY *>(ctx.keyCtx) );
			ctx.keyCtx = nullptr;
		}
	},
	.pubImport = [] (KeyContext &ctx, BytesView data) {
		BIO *bioData = nullptr;

		auto finalize = [&] (bool value) {
			if (bioData) {
				BIO_free(bioData);
				bioData = nullptr;
			}
			if (!value) {
				ctx.keyCtx = nullptr;
			} else {
				ctx.type = getOpenSSLKeyType(EVP_PKEY_get_id(static_cast<EVP_PKEY *>(ctx.keyCtx)));
			}
			return value;
		};

		bioData = BIO_new_mem_buf((void*)data.data(), int(data.size()));
		if (isPemKey(data)) {
			ctx.keyCtx = PEM_read_bio_PUBKEY(bioData, NULL, NULL, NULL);
			if (ctx.keyCtx) {
				return finalize(true);
			}
		} else {
			ctx.keyCtx = d2i_PUBKEY_bio(bioData, NULL);
			if (ctx.keyCtx) {
				return finalize(true);
			}
		}
		return finalize(false);
	},
	.pubImportOpenSSH = [] (KeyContext &ctx, StringView r) {
		auto origKeyType = r.readUntil<StringView::CharGroup<CharGroupId::WhiteSpace>>();
		r.skipChars<StringView::CharGroup<CharGroupId::WhiteSpace>>();
		auto dataBlock = r.readUntil<StringView::CharGroup<CharGroupId::WhiteSpace>>();
		dataBlock = dataBlock.readChars<StringView::CharGroup<CharGroupId::Base64>>();
		if (valid::validateBase64(dataBlock)) {
			auto bytesSize = base64::decodeSize(dataBlock.size());
			uint8_t bytes[base64::decodeSize(dataBlock.size())];

			BytesViewNetwork dataView(bytes,  sprt::base64::decode(dataBlock.data(), dataBlock.size(), bytes, bytesSize));
			auto len = dataView.readUnsigned32();
			auto keyType = dataView.readString(len);

			if (origKeyType != keyType || keyType != "ssh-rsa") {
				return false;
			}

			auto elen = dataView.readUnsigned32();
			auto exp = dataView.readBytes(elen);

			auto mlen = dataView.readUnsigned32();
			auto modulus = dataView.readBytes(mlen);

			uint8_t out[12_KiB];
			uint8_t *buf = out;
			buf = writeRSAKey(buf, modulus, exp);

			std::stringstream stream;
			stream << "-----BEGIN RSA PUBLIC KEY-----\n";
			sprt::base64::encode(out, buf - out, [&] (const char *str, size_t len) {
				while (len > 80) {
					stream.write(str, 80);
					len -= 80;
					str += 80;
					stream << '\n';
				}
				if (len > 0) {
					stream.write(str, len);
				}
			});

			stream << "\n-----END RSA PUBLIC KEY-----\n";

			BIO *bioData = nullptr;

			auto finalize = [&] (bool value) {
				if (bioData) {
					BIO_free(bioData);
					bioData = nullptr;
				}
				if (!value) {
					ctx.keyCtx = nullptr;
				} else {
					ctx.type = getOpenSSLKeyType(EVP_PKEY_get_id(static_cast<EVP_PKEY *>(ctx.keyCtx)));
				}
				return value;
			};

			std::string tmp = stream.str();

			bioData = BIO_new_mem_buf(tmp.data(), int(tmp.size()));
			ctx.keyCtx = PEM_read_bio_PUBKEY(bioData, NULL, NULL, NULL);
			if (ctx.keyCtx) {
				return finalize(true);
			}
			return finalize(false);
		}
		return false;
	},
	.pubExportPem = [] (const KeyContext &ctx, const Callback<void(BytesView)> &cb) {
		auto key = static_cast<EVP_PKEY *>(ctx.keyCtx);
		BIO *bp = nullptr;

		auto finalize = [&] (bool value) {
			if (bp) {
				BIO_free(bp);
				bp = nullptr;
			}
			return value;
		};

		if (!key) {
			return finalize(false);
		}

		bp = BIO_new(BIO_s_mem());
		if (!bp) {
			return finalize(false);
		}

		if (!PEM_write_bio_PUBKEY(bp, key)) {
			return finalize(false);
		}

		auto len = BIO_pending(bp);
		if (len > 0) {
			uint8_t buf[len];

			if (BIO_read(bp, buf, len)) {
				cb(BytesView(buf, len));
				return finalize(true);
			}
		}
		return finalize(false);
	},
	.pubExportDer = [] (const KeyContext &ctx, const Callback<void(BytesView)> &cb) {
		auto key = static_cast<EVP_PKEY *>(ctx.keyCtx);
		BIO *bp = nullptr;

		auto finalize = [&] (bool value) {
			if (bp) {
				BIO_free(bp);
				bp = nullptr;
			}
			return value;
		};

		if (!key) {
			return finalize(false);
		}

		bp = BIO_new(BIO_s_mem());
		if (!bp) {
			return finalize(false);
		}

		if (!i2d_PUBKEY_bio(bp, key)) {
			return finalize(false);
		}

		auto len = BIO_pending(bp);
		if (len > 0) {
			uint8_t buf[len];

			if (BIO_read(bp, buf, len)) {
				cb(BytesView(buf, len));
				return finalize(true);
			}
		}
		return finalize(false);
	},
	.pubVerify = [] (const KeyContext &ctx, const CoderSource &data, BytesView signature, SignAlgorithm algo) {
		auto key = static_cast<EVP_PKEY *>(ctx.keyCtx);
		EVP_MD_CTX *mdctx =  EVP_MD_CTX_create();

		auto cleanup = [&] (bool value) {
			if (mdctx) {
				EVP_MD_CTX_destroy(mdctx);
				mdctx = nullptr;
			}
			return value;
		};

		if (!mdctx) {
			return cleanup(false);
		}

		switch (algo) {
		case SignAlgorithm::RSA_SHA256:
		case SignAlgorithm::ECDSA_SHA256:
			if (!EVP_DigestVerifyInit(mdctx, NULL, (ctx.type == KeyType::EDDSA_ED448) ? nullptr : EVP_sha256(), NULL, key)) {
				return cleanup(false);
			}
			break;
		case SignAlgorithm::RSA_SHA512:
		case SignAlgorithm::ECDSA_SHA512:
			if (!EVP_DigestVerifyInit(mdctx, NULL, (ctx.type == KeyType::EDDSA_ED448) ? nullptr : EVP_sha512(), NULL, key)) {
				return cleanup(false);
			}
			break;
		case SignAlgorithm::GOST_256:
			if (!s_opensslHasGost) {
				log::source().warn("Crypto", "OpenSSL backend loaded without GOST support");
				return cleanup(false);
			}

			if (auto md = EVP_get_digestbyname("md_gost12_256")) {
				if (1 != EVP_DigestVerifyInit(mdctx, NULL, md, NULL, key)) {
					return cleanup(false);
				}
			} else {
				return cleanup(false);
			}
			break;
		case SignAlgorithm::GOST_512:
			if (!s_opensslHasGost) {
				log::source().warn("Crypto", "OpenSSL backend loaded without GOST support");
				return cleanup(false);
			}

			if (auto md = EVP_get_digestbyname("md_gost12_512")) {
				if (1 != EVP_DigestVerifyInit(mdctx, NULL, md, NULL, key)) {
					return cleanup(false);
				}
			} else {
				return cleanup(false);
			}
			break;
		}

		if (EVP_DigestVerify(mdctx, signature.data(), signature.size(), data.data(), data.size()) == 1) {
			return cleanup(true);
		}

		return cleanup(false);
	},
	.pubEncrypt = [] (const KeyContext &ctx, const Callback<void(BytesView)> &cb, const CoderSource &data) {
		auto key = static_cast<EVP_PKEY *>(ctx.keyCtx);
		auto pctx = EVP_PKEY_CTX_new(key, nullptr);

		void *out = nullptr;

		auto cleanup = [&] (bool value) {
			if (out) {
				OPENSSL_free(out);
				out = nullptr;
			}
			if (pctx) {
				EVP_PKEY_CTX_free(pctx);
				pctx = nullptr;
			}
			return value;
		};

		if (EVP_PKEY_encrypt_init(pctx) <= 0) {
			return cleanup(false);
		}

		switch (ctx.type) {
		case KeyType::RSA:
			if (EVP_PKEY_CTX_set_rsa_padding(pctx, OPENSSL_PK_ENCRYPT_PADDING) <= 0) {
				return cleanup(false);
			}
			break;
		default:
			break;
		}

		/* Determine buffer length */
		size_t outlen;
		if (EVP_PKEY_encrypt(pctx, NULL, &outlen, data.data(), data.size()) <= 0) {
			return cleanup(false);
		}

		out = OPENSSL_malloc(outlen);

		if (!out) {
			return cleanup(false);
		}

		if (EVP_PKEY_encrypt(pctx, static_cast<uint8_t *>(out), &outlen, data.data(), data.size()) <= 0) {
			return cleanup(false);
		}

		cb(BytesView(static_cast<uint8_t *>(out), outlen));

		return cleanup(true);
	}
};

BackendCtxRef s_openSSLRef(&s_openSSLCtx);

} // namespace stappler::crypto

#endif
