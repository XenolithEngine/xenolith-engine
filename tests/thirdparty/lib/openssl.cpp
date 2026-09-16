/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

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

// Functional tests for the shipped OpenSSL (libcrypto + libssl), used through
// its own public headers only. Where a published test vector exists (FIPS-197,
// RFC 2202/4231/4648/5869/6070, GCM spec) it is checked byte for byte, so a
// miscompiled or misconfigured build shows up as a wrong value rather than as a
// missing symbol.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/core_names.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/kdf.h>
#include <openssl/opensslv.h>
#include <openssl/params.h>
#include <openssl/pem.h>
#include <openssl/provider.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>

#include "../tests.h"
#include "certgen.h"

namespace thirdparty::test {

//
// libcrypto: build identity
//

void performOpensslVersionTest() {
	auto text = OpenSSL_version(OPENSSL_VERSION);
	printf("       runtime: %s\n", text ? text : "(null)");
	printf("       headers: %s\n", OPENSSL_VERSION_TEXT);

	check(text != nullptr && strlen(text) > 0, "OpenSSL_version(OPENSSL_VERSION) is non-empty");

	// The linked libcrypto must be the one the headers describe: compare the
	// major.minor.patch triple (the low bits carry pre-release state).
	auto runtimeVer = OpenSSL_version_num();
	checkInt("OpenSSL_version_num matches OPENSSL_VERSION_NUMBER",
			(long long)(runtimeVer >> 12), (long long)(OPENSSL_VERSION_NUMBER >> 12));

	// 3.x is what the whole test file assumes (EVP_KDF, EVP_PKEY_Q_keygen, ...).
	checkInt("OpenSSL major version", (long long)OPENSSL_VERSION_MAJOR, 3);

	printf("       providers: default=%d legacy=%d\n",
			OSSL_PROVIDER_available(nullptr, "default") ? 1 : 0,
			OSSL_PROVIDER_available(nullptr, "legacy") ? 1 : 0);
	check(OSSL_PROVIDER_available(nullptr, "default") == 1, "default provider is available");
}

//
// libcrypto: message digests
//

static bool digestOneShot(const EVP_MD *md, const void *data, size_t len, unsigned char *out,
		unsigned int *outLen) {
	auto ctx = EVP_MD_CTX_new();
	if (!ctx) {
		return false;
	}
	bool ok = EVP_DigestInit_ex(ctx, md, nullptr) == 1 && EVP_DigestUpdate(ctx, data, len) == 1
			&& EVP_DigestFinal_ex(ctx, out, outLen) == 1;
	EVP_MD_CTX_free(ctx);
	return ok;
}

void performOpensslDigestTest() {
	static const char *abc = "abc";
	unsigned char out[EVP_MAX_MD_SIZE];
	unsigned int outLen = 0;

	struct {
		const char *name;
		const EVP_MD *(*md)();
		const char *expect;
	} vectors[] = {
		{"MD5(\"abc\")", &EVP_md5, "900150983cd24fb0d6963f7d28e17f72"},
		{"SHA1(\"abc\")", &EVP_sha1, "a9993e364706816aba3e25717850c26c9cd0d89d"},
		{"SHA224(\"abc\")", &EVP_sha224,
			"23097d223405d8228642a477bda255b32aadbce4bda0b3f7e36c9da7"},
		{"SHA256(\"abc\")", &EVP_sha256,
			"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"},
		{"SHA384(\"abc\")", &EVP_sha384,
			"cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed8086072ba1e7cc235"
			"8baeca134c825a7"},
		{"SHA512(\"abc\")", &EVP_sha512,
			"ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a83"
			"6ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f"},
		{"SHA3-256(\"abc\")", &EVP_sha3_256,
			"3a985da74fe225b2045c172d6bd390bd855f086e3e9d525b46bfe24511431532"},
		{"SHA3-512(\"abc\")", &EVP_sha3_512,
			"b751850b1a57168a5693cd924b6b096e08f621827444f70d884f5d0240d2712e10e116e9192af3c91"
			"a7ec57647e3934057340b4cf408d5a56592f8274eec53f0"},
	};

	for (auto &v : vectors) {
		if (!digestOneShot(v.md(), abc, 3, out, &outLen)) {
			fail(v.name, "EVP_Digest* failed");
			continue;
		}
		checkHex(v.name, out, outLen, v.expect);
	}

	// The empty message is its own edge case (no Update call at all).
	auto ctx = EVP_MD_CTX_new();
	if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) == 1
			&& EVP_DigestFinal_ex(ctx, out, &outLen) == 1) {
		checkHex("SHA256(\"\")", out, outLen,
				"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
	} else {
		fail("SHA256(\"\")", "EVP_Digest* failed");
	}
	EVP_MD_CTX_free(ctx);

	// Chunked feeding must equal the one-shot result.
	ctx = EVP_MD_CTX_new();
	bool ok = EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) == 1;
	for (int i = 0; ok && i < 3; ++i) { ok = EVP_DigestUpdate(ctx, abc + i, 1) == 1; }
	ok = ok && EVP_DigestFinal_ex(ctx, out, &outLen) == 1;
	EVP_MD_CTX_free(ctx);
	if (ok) {
		checkHex("SHA256 chunked equals one-shot", out, outLen,
				"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
	} else {
		fail("SHA256 chunked equals one-shot", "EVP_Digest* failed");
	}

	// Fetch by name goes through the provider machinery rather than the static
	// table, so it proves the default provider actually resolves algorithms.
	auto fetched = EVP_MD_fetch(nullptr, "SHA2-256", nullptr);
	check(fetched != nullptr, "EVP_MD_fetch(\"SHA2-256\")");
	if (fetched) {
		checkInt("EVP_MD_get_size(SHA2-256)", EVP_MD_get_size(fetched), 32);
		checkInt("EVP_MD_get_block_size(SHA2-256)", EVP_MD_get_block_size(fetched), 64);
		EVP_MD_free(fetched);
	}
}

//
// libcrypto: HMAC (RFC 2202 / RFC 4231)
//

void performOpensslHmacTest() {
	unsigned char out[EVP_MAX_MD_SIZE];
	unsigned int outLen = 0;

	// RFC 4231, test case 1: key = 20 x 0x0b, data = "Hi There"
	unsigned char key1[20];
	memset(key1, 0x0b, sizeof(key1));
	if (HMAC(EVP_sha256(), key1, sizeof(key1), (const unsigned char *)"Hi There", 8, out,
				&outLen)) {
		checkHex("HMAC-SHA256 RFC4231 case 1", out, outLen,
				"b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7");
	} else {
		fail("HMAC-SHA256 RFC4231 case 1", "HMAC() returned null");
	}

	// RFC 4231, test case 2: key = "Jefe"
	static const char *data2 = "what do ya want for nothing?";
	if (HMAC(EVP_sha256(), "Jefe", 4, (const unsigned char *)data2, strlen(data2), out, &outLen)) {
		checkHex("HMAC-SHA256 RFC4231 case 2", out, outLen,
				"5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");
	} else {
		fail("HMAC-SHA256 RFC4231 case 2", "HMAC() returned null");
	}
	if (HMAC(EVP_sha512(), "Jefe", 4, (const unsigned char *)data2, strlen(data2), out, &outLen)) {
		checkHex("HMAC-SHA512 RFC4231 case 2", out, outLen,
				"164b7a7bfcf819e2e395fbe73b56e0a387bd64222e831fd610270cd7ea2505549758bf75c05a99"
				"4a6d034f65f8f0e6fdcaeab1a34d4a6b4b636e070a38bce737");
	} else {
		fail("HMAC-SHA512 RFC4231 case 2", "HMAC() returned null");
	}

	// RFC 2202, test case 2 (SHA-1)
	if (HMAC(EVP_sha1(), "Jefe", 4, (const unsigned char *)data2, strlen(data2), out, &outLen)) {
		checkHex("HMAC-SHA1 RFC2202 case 2", out, outLen,
				"effcdf6ae5eb2fa2d27416d5f184df9c259a7c79");
	} else {
		fail("HMAC-SHA1 RFC2202 case 2", "HMAC() returned null");
	}

	// The same, byte by byte, through the 3.x EVP_MAC interface (the non-deprecated
	// one, and the one that goes through the provider).
	auto mac = EVP_MAC_fetch(nullptr, "HMAC", nullptr);
	check(mac != nullptr, "EVP_MAC_fetch(\"HMAC\")");
	if (mac) {
		auto mctx = EVP_MAC_CTX_new(mac);
		OSSL_PARAM params[2];
		params[0] = OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_DIGEST, (char *)"SHA256", 0);
		params[1] = OSSL_PARAM_construct_end();

		size_t macLen = 0;
		bool ok = mctx && EVP_MAC_init(mctx, (const unsigned char *)"Jefe", 4, params) == 1;
		for (size_t i = 0; ok && data2[i]; ++i) {
			ok = EVP_MAC_update(mctx, (const unsigned char *)data2 + i, 1) == 1;
		}
		ok = ok && EVP_MAC_final(mctx, out, &macLen, sizeof(out)) == 1;
		EVP_MAC_CTX_free(mctx);
		EVP_MAC_free(mac);

		if (ok) {
			checkHex("EVP_MAC chunked equals HMAC one-shot", out, macLen,
					"5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");
		} else {
			fail("EVP_MAC chunked equals HMAC one-shot", "EVP_MAC API failed");
		}
	}

	(void)outLen;
}

//
// libcrypto: symmetric ciphers
//

void performOpensslCipherTest() {
	unsigned char buf[128];
	int len = 0, total = 0;

	// FIPS-197 appendix C.1: AES-128 over a single ECB block.
	{
		unsigned char key[16], input[16];
		fromHex("000102030405060708090a0b0c0d0e0f", key, sizeof(key));
		fromHex("00112233445566778899aabbccddeeff", input, sizeof(input));

		auto ctx = EVP_CIPHER_CTX_new();
		bool ok = ctx && EVP_EncryptInit_ex(ctx, EVP_aes_128_ecb(), nullptr, key, nullptr) == 1;
		ok = ok && EVP_CIPHER_CTX_set_padding(ctx, 0) == 1;
		ok = ok && EVP_EncryptUpdate(ctx, buf, &len, input, sizeof(input)) == 1;
		total = len;
		ok = ok && EVP_EncryptFinal_ex(ctx, buf + total, &len) == 1;
		total += len;
		EVP_CIPHER_CTX_free(ctx);
		if (ok) {
			checkHex("AES-128-ECB FIPS-197 C.1", buf, size_t(total),
					"69c4e0d86a7b0430d8cdb78070b4c55a");
		} else {
			fail("AES-128-ECB FIPS-197 C.1", "EVP_Encrypt* failed");
		}
	}

	// GCM spec test case 3 (McGrew & Viega): ciphertext AND tag must match.
	{
		unsigned char key[16], iv[12], plain[64], expected[64], tag[16];
		fromHex("feffe9928665731c6d6a8f9467308308", key, sizeof(key));
		fromHex("cafebabefacedbaddecaf888", iv, sizeof(iv));
		fromHex("d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c95956809"
				"532fcf0e2449a6b525b16aedf5aa0de657ba637b391aafd255",
				plain, sizeof(plain));
		fromHex("42831ec2217774244b7221b784d0d49ce3aa212f2c02a4e035c17e2329aca12e21d514b2546693"
				"1c7d8f6a5aac84aa051ba30b396a0aac973d58e091473f5985",
				expected, sizeof(expected));

		auto ctx = EVP_CIPHER_CTX_new();
		bool ok = ctx && EVP_EncryptInit_ex(ctx, EVP_aes_128_gcm(), nullptr, nullptr, nullptr) == 1;
		ok = ok
				&& EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, int(sizeof(iv)), nullptr) == 1;
		ok = ok && EVP_EncryptInit_ex(ctx, nullptr, nullptr, key, iv) == 1;
		ok = ok && EVP_EncryptUpdate(ctx, buf, &len, plain, sizeof(plain)) == 1;
		total = len;
		ok = ok && EVP_EncryptFinal_ex(ctx, buf + total, &len) == 1;
		total += len;
		ok = ok
				&& EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, int(sizeof(tag)), tag) == 1;
		EVP_CIPHER_CTX_free(ctx);

		if (ok) {
			checkInt("AES-128-GCM ciphertext length", total, int(sizeof(plain)));
			check(total == int(sizeof(expected)) && memcmp(buf, expected, sizeof(expected)) == 0,
					"AES-128-GCM ciphertext (spec case 3)");
			checkHex("AES-128-GCM tag (spec case 3)", tag, sizeof(tag),
					"4d5c2af327cd64a62cf35abd2ba6fab4");
		} else {
			fail("AES-128-GCM (spec case 3)", "EVP_Encrypt* failed");
		}

		// ... and the tag must be rejected once a ciphertext byte is flipped.
		unsigned char corrupted[64];
		memcpy(corrupted, expected, sizeof(corrupted));
		corrupted[0] ^= 0x01;

		ctx = EVP_CIPHER_CTX_new();
		bool init = ctx && EVP_DecryptInit_ex(ctx, EVP_aes_128_gcm(), nullptr, nullptr, nullptr) == 1
				&& EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, int(sizeof(iv)), nullptr) == 1
				&& EVP_DecryptInit_ex(ctx, nullptr, nullptr, key, iv) == 1
				&& EVP_DecryptUpdate(ctx, buf, &len, corrupted, sizeof(corrupted)) == 1
				&& EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, int(sizeof(tag)), tag) == 1;
		int finalRet = init ? EVP_DecryptFinal_ex(ctx, buf + len, &len) : -1;
		EVP_CIPHER_CTX_free(ctx);
		check(init && finalRet <= 0, "AES-128-GCM rejects a corrupted ciphertext");
	}

	// AES-256-CBC round trip with padding (the everyday path).
	{
		unsigned char key[32], iv[16];
		check(RAND_bytes(key, sizeof(key)) == 1, "RAND_bytes for AES-256-CBC key");
		check(RAND_bytes(iv, sizeof(iv)) == 1, "RAND_bytes for AES-256-CBC iv");

		static const char *plain = "The quick brown fox jumps over the lazy dog";
		auto plainLen = int(strlen(plain));

		auto ctx = EVP_CIPHER_CTX_new();
		bool ok = ctx && EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key, iv) == 1
				&& EVP_EncryptUpdate(ctx, buf, &len, (const unsigned char *)plain, plainLen) == 1;
		total = len;
		ok = ok && EVP_EncryptFinal_ex(ctx, buf + total, &len) == 1;
		total += len;
		EVP_CIPHER_CTX_free(ctx);

		// PKCS#7 padding always adds a full block when the input is block-aligned.
		checkInt("AES-256-CBC padded length", total, ((plainLen / 16) + 1) * 16);

		unsigned char plainOut[128];
		int outTotal = 0;
		ctx = EVP_CIPHER_CTX_new();
		ok = ok && ctx && EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key, iv) == 1
				&& EVP_DecryptUpdate(ctx, plainOut, &len, buf, total) == 1;
		outTotal = len;
		ok = ok && EVP_DecryptFinal_ex(ctx, plainOut + outTotal, &len) == 1;
		outTotal += len;
		EVP_CIPHER_CTX_free(ctx);

		if (ok) {
			plainOut[outTotal] = 0;
			checkStr("AES-256-CBC round trip", (const char *)plainOut, plain);
		} else {
			fail("AES-256-CBC round trip", "EVP_En/DecryptUpdate failed");
		}
	}

	// ChaCha20-Poly1305 with AAD - the other AEAD curl may negotiate.
	{
		unsigned char key[32], nonce[12], tag[16];
		memset(key, 0x42, sizeof(key));
		memset(nonce, 0x24, sizeof(nonce));
		static const char *aad = "thirdparty-test";
		static const char *plain = "chacha20-poly1305 payload";
		auto plainLen = int(strlen(plain));

		auto ctx = EVP_CIPHER_CTX_new();
		bool ok = ctx
				&& EVP_EncryptInit_ex(ctx, EVP_chacha20_poly1305(), nullptr, key, nonce) == 1
				&& EVP_EncryptUpdate(ctx, nullptr, &len, (const unsigned char *)aad,
						   int(strlen(aad)))
						== 1
				&& EVP_EncryptUpdate(ctx, buf, &len, (const unsigned char *)plain, plainLen) == 1;
		total = len;
		ok = ok && EVP_EncryptFinal_ex(ctx, buf + total, &len) == 1;
		total += len;
		ok = ok && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, int(sizeof(tag)), tag) == 1;
		EVP_CIPHER_CTX_free(ctx);
		checkInt("ChaCha20-Poly1305 ciphertext length", total, plainLen);

		unsigned char plainOut[128];
		ctx = EVP_CIPHER_CTX_new();
		bool dec = ok && ctx
				&& EVP_DecryptInit_ex(ctx, EVP_chacha20_poly1305(), nullptr, key, nonce) == 1
				&& EVP_DecryptUpdate(ctx, nullptr, &len, (const unsigned char *)aad,
						   int(strlen(aad)))
						== 1
				&& EVP_DecryptUpdate(ctx, plainOut, &len, buf, total) == 1
				&& EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, int(sizeof(tag)), tag) == 1;
		int outTotal = dec ? len : 0;
		int finalRet = dec ? EVP_DecryptFinal_ex(ctx, plainOut + outTotal, &len) : -1;
		EVP_CIPHER_CTX_free(ctx);
		if (finalRet > 0) {
			plainOut[outTotal + len] = 0;
			checkStr("ChaCha20-Poly1305 round trip", (const char *)plainOut, plain);
		} else {
			fail("ChaCha20-Poly1305 round trip", "decryption or tag check failed");
		}
	}

	// Fetch by name (provider path) for the cipher curl negotiates most often.
	auto fetched = EVP_CIPHER_fetch(nullptr, "AES-256-GCM", nullptr);
	check(fetched != nullptr, "EVP_CIPHER_fetch(\"AES-256-GCM\")");
	if (fetched) {
		checkInt("EVP_CIPHER_get_key_length(AES-256-GCM)", EVP_CIPHER_get_key_length(fetched), 32);
		EVP_CIPHER_free(fetched);
	}
}

//
// libcrypto: key derivation (RFC 6070 / RFC 5869)
//

void performOpensslKdfTest() {
	unsigned char out[64];

	// RFC 6070, test case 3: PBKDF2-HMAC-SHA1, 4096 iterations.
	if (PKCS5_PBKDF2_HMAC("password", 8, (const unsigned char *)"salt", 4, 4096, EVP_sha1(), 20,
				out)
			== 1) {
		checkHex("PBKDF2-HMAC-SHA1 RFC6070 case 3", out, 20,
				"4b007901b765489abead49d926f721d065a429c1");
	} else {
		fail("PBKDF2-HMAC-SHA1 RFC6070 case 3", "PKCS5_PBKDF2_HMAC failed");
	}

	// RFC 5869, test case 1: HKDF-SHA256 through the 3.x EVP_KDF interface.
	auto kdf = EVP_KDF_fetch(nullptr, "HKDF", nullptr);
	check(kdf != nullptr, "EVP_KDF_fetch(\"HKDF\")");
	if (kdf) {
		auto kctx = EVP_KDF_CTX_new(kdf);
		unsigned char ikm[22], salt[13], info[10];
		fromHex("0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b", ikm, sizeof(ikm));
		fromHex("000102030405060708090a0b0c", salt, sizeof(salt));
		fromHex("f0f1f2f3f4f5f6f7f8f9", info, sizeof(info));

		OSSL_PARAM params[5];
		params[0] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST, (char *)"SHA256", 0);
		params[1] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_KEY, ikm, sizeof(ikm));
		params[2] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT, salt, sizeof(salt));
		params[3] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_INFO, info, sizeof(info));
		params[4] = OSSL_PARAM_construct_end();

		if (kctx && EVP_KDF_derive(kctx, out, 42, params) == 1) {
			checkHex("HKDF-SHA256 RFC5869 case 1", out, 42,
					"3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf34007208d5"
					"b887185865");
		} else {
			fail("HKDF-SHA256 RFC5869 case 1", "EVP_KDF_derive failed");
		}
		EVP_KDF_CTX_free(kctx);
		EVP_KDF_free(kdf);
	}

	// scrypt is an optional build-time feature; report rather than fail if absent.
	auto scrypt = EVP_KDF_fetch(nullptr, "SCRYPT", nullptr);
	if (scrypt) {
		auto kctx = EVP_KDF_CTX_new(scrypt);
		uint64_t n = 16, r = 1, p = 1;
		OSSL_PARAM params[5];
		params[0] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_PASSWORD, (void *)"", 0);
		params[1] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT, (void *)"", 0);
		params[2] = OSSL_PARAM_construct_uint64(OSSL_KDF_PARAM_SCRYPT_N, &n);
		params[3] = OSSL_PARAM_construct_end();
		(void)r;
		(void)p;
		// N alone is not a complete parameter set; only check the fetch succeeded
		// and the context can be created.
		check(kctx != nullptr, "EVP_KDF_CTX_new(SCRYPT)");
		EVP_KDF_CTX_free(kctx);
		EVP_KDF_free(scrypt);
	} else {
		skip("SCRYPT", "not provided by this build");
	}
}

//
// libcrypto: BIGNUM
//

void performOpensslBignumTest() {
	auto ctx = BN_CTX_new();
	auto a = BN_new(), b = BN_new(), m = BN_new(), r = BN_new();
	check(ctx && a && b && m && r, "BN_new / BN_CTX_new");

	// The textbook RSA example: 4^13 mod 497 == 445.
	BN_set_word(a, 4);
	BN_set_word(b, 13);
	BN_set_word(m, 497);
	check(BN_mod_exp(r, a, b, m, ctx) == 1, "BN_mod_exp");
	checkInt("4^13 mod 497", (long long)BN_get_word(r), 445);

	// hex round trip over a value that needs more than one word.
	static const char *hex = "FEDCBA9876543210FEDCBA9876543210FEDCBA9876543210";
	BIGNUM *big = nullptr;
	check(BN_hex2bn(&big, hex) == int(strlen(hex)), "BN_hex2bn consumed the whole string");
	checkInt("BN_num_bits", BN_num_bits(big), 192);
	auto backHex = BN_bn2hex(big);
	checkStr("BN_bn2hex round trip", backHex, hex);
	OPENSSL_free(backHex);

	// Modular inverse: (x * x^-1) mod m == 1.
	BN_set_word(m, 0);
	check(BN_hex2bn(&m, "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEFFFFFC2F") > 0,
			"BN_hex2bn(secp256k1 p)");
	auto inv = BN_mod_inverse(nullptr, big, m, ctx);
	check(inv != nullptr, "BN_mod_inverse");
	if (inv) {
		check(BN_mod_mul(r, big, inv, m, ctx) == 1, "BN_mod_mul");
		check(BN_is_one(r) == 1, "x * x^-1 == 1 (mod p)");
		BN_free(inv);
	}

	// Primality: the value above is the secp256k1 field prime.
	checkInt("BN_check_prime(secp256k1 p)", BN_check_prime(m, ctx, nullptr), 1);
	BN_set_word(a, 4);
	checkInt("BN_check_prime(4)", BN_check_prime(a, ctx, nullptr), 0);

	BN_free(big);
	BN_free(r);
	BN_free(m);
	BN_free(b);
	BN_free(a);
	BN_CTX_free(ctx);
}

//
// libcrypto: RNG
//

void performOpensslRandomTest() {
	checkInt("RAND_status", RAND_status(), 1);

	unsigned char a[32], b[32];
	memset(a, 0, sizeof(a));
	memset(b, 0, sizeof(b));
	checkInt("RAND_bytes", RAND_bytes(a, sizeof(a)), 1);
	checkInt("RAND_bytes (second draw)", RAND_bytes(b, sizeof(b)), 1);
	check(memcmp(a, b, sizeof(a)) != 0, "two RAND_bytes draws differ");

	// An all-zero buffer would also "differ" from another all-zero buffer never;
	// check explicitly that the DRBG produced something.
	bool nonZero = false;
	for (auto v : a) {
		if (v) {
			nonZero = true;
			break;
		}
	}
	check(nonZero, "RAND_bytes output is not all zeroes");

	checkInt("RAND_priv_bytes", RAND_priv_bytes(b, sizeof(b)), 1);
}

//
// libcrypto: base64 / hex helpers
//

void performOpensslEncodingTest() {
	// RFC 4648 section 10 vectors.
	struct {
		const char *plain;
		const char *encoded;
	} vectors[] = {
		{"f", "Zg=="},
		{"fo", "Zm8="},
		{"foo", "Zm9v"},
		{"foob", "Zm9vYg=="},
		{"fooba", "Zm9vYmE="},
		{"foobar", "Zm9vYmFy"},
	};

	char encoded[64];
	for (auto &v : vectors) {
		auto len = EVP_EncodeBlock((unsigned char *)encoded, (const unsigned char *)v.plain,
				int(strlen(v.plain)));
		encoded[len] = 0;
		checkStr("EVP_EncodeBlock", encoded, v.encoded);
	}

	// Decoding drops the padding but keeps the block length, so compare only the
	// bytes the input actually carried.
	unsigned char decoded[64];
	auto decodedLen = EVP_DecodeBlock(decoded, (const unsigned char *)"Zm9vYmFy", 8);
	checkInt("EVP_DecodeBlock length", decodedLen, 6);
	if (decodedLen >= 6) {
		decoded[6] = 0;
		checkStr("EVP_DecodeBlock round trip", (const char *)decoded, "foobar");
	}

	// OPENSSL_buf2hexstr / hexstr2buf (used everywhere in the library's own tests).
	unsigned char raw[4] = {0xde, 0xad, 0xbe, 0xef};
	auto hex = OPENSSL_buf2hexstr(raw, sizeof(raw));
	checkStr("OPENSSL_buf2hexstr", hex, "DE:AD:BE:EF");
	OPENSSL_free(hex);

	long backLen = 0;
	auto back = OPENSSL_hexstr2buf("DEADBEEF", &backLen);
	checkInt("OPENSSL_hexstr2buf length", backLen, 4);
	check(back && memcmp(back, raw, sizeof(raw)) == 0, "OPENSSL_hexstr2buf round trip");
	OPENSSL_free(back);
}

//
// libcrypto: RSA
//

void performOpensslRsaTest() {
	auto key = EVP_PKEY_Q_keygen(nullptr, nullptr, "RSA", (size_t)2048);
	if (!key) {
		fail("RSA-2048 keygen", "EVP_PKEY_Q_keygen returned null");
		return;
	}
	check(true, "RSA-2048 keygen");
	checkInt("EVP_PKEY_get_bits", EVP_PKEY_get_bits(key), 2048);
	checkInt("EVP_PKEY_get_id", EVP_PKEY_get_id(key), EVP_PKEY_RSA);

	static const char *message = "the message that gets signed";
	auto messageLen = strlen(message);

	// PKCS#1 v1.5 signature over SHA-256.
	unsigned char sig[512];
	size_t sigLen = sizeof(sig);
	auto mdctx = EVP_MD_CTX_new();
	bool ok = mdctx
			&& EVP_DigestSignInit(mdctx, nullptr, EVP_sha256(), nullptr, key) == 1
			&& EVP_DigestSign(mdctx, sig, &sigLen, (const unsigned char *)message, messageLen) == 1;
	EVP_MD_CTX_free(mdctx);
	check(ok, "RSA PKCS#1 v1.5 sign");
	checkInt("RSA signature length", (long long)sigLen, 256);

	mdctx = EVP_MD_CTX_new();
	int verified = -1;
	if (mdctx && EVP_DigestVerifyInit(mdctx, nullptr, EVP_sha256(), nullptr, key) == 1) {
		verified = EVP_DigestVerify(mdctx, sig, sigLen, (const unsigned char *)message, messageLen);
	}
	EVP_MD_CTX_free(mdctx);
	checkInt("RSA PKCS#1 v1.5 verify", verified, 1);

	// A single flipped bit must break it.
	sig[0] ^= 0x01;
	mdctx = EVP_MD_CTX_new();
	int tampered = 1;
	if (mdctx && EVP_DigestVerifyInit(mdctx, nullptr, EVP_sha256(), nullptr, key) == 1) {
		tampered = EVP_DigestVerify(mdctx, sig, sigLen, (const unsigned char *)message, messageLen);
	}
	EVP_MD_CTX_free(mdctx);
	check(tampered != 1, "RSA verify rejects a tampered signature");
	ERR_clear_error();
	sig[0] ^= 0x01;

	// RSA-PSS through the signing context.
	EVP_PKEY_CTX *pctx = nullptr;
	mdctx = EVP_MD_CTX_new();
	size_t pssLen = sizeof(sig);
	unsigned char pssSig[512];
	ok = mdctx && EVP_DigestSignInit(mdctx, &pctx, EVP_sha256(), nullptr, key) == 1
			&& EVP_PKEY_CTX_set_rsa_padding(pctx, RSA_PKCS1_PSS_PADDING) == 1
			&& EVP_DigestSign(mdctx, pssSig, &pssLen, (const unsigned char *)message, messageLen)
					== 1;
	EVP_MD_CTX_free(mdctx);
	check(ok, "RSA-PSS sign");

	pctx = nullptr;
	mdctx = EVP_MD_CTX_new();
	int pssVerified = -1;
	if (mdctx && EVP_DigestVerifyInit(mdctx, &pctx, EVP_sha256(), nullptr, key) == 1
			&& EVP_PKEY_CTX_set_rsa_padding(pctx, RSA_PKCS1_PSS_PADDING) == 1) {
		pssVerified = EVP_DigestVerify(mdctx, pssSig, pssLen, (const unsigned char *)message,
				messageLen);
	}
	EVP_MD_CTX_free(mdctx);
	checkInt("RSA-PSS verify", pssVerified, 1);

	// OAEP encrypt / decrypt.
	unsigned char cipher[256];
	size_t cipherLen = sizeof(cipher);
	auto encCtx = EVP_PKEY_CTX_new(key, nullptr);
	ok = encCtx && EVP_PKEY_encrypt_init(encCtx) == 1
			&& EVP_PKEY_CTX_set_rsa_padding(encCtx, RSA_PKCS1_OAEP_PADDING) == 1
			&& EVP_PKEY_encrypt(encCtx, cipher, &cipherLen, (const unsigned char *)message,
					   messageLen)
					== 1;
	EVP_PKEY_CTX_free(encCtx);
	check(ok, "RSA-OAEP encrypt");

	unsigned char plain[256];
	size_t plainLen = sizeof(plain);
	auto decCtx = EVP_PKEY_CTX_new(key, nullptr);
	ok = ok && decCtx && EVP_PKEY_decrypt_init(decCtx) == 1
			&& EVP_PKEY_CTX_set_rsa_padding(decCtx, RSA_PKCS1_OAEP_PADDING) == 1
			&& EVP_PKEY_decrypt(decCtx, plain, &plainLen, cipher, cipherLen) == 1;
	EVP_PKEY_CTX_free(decCtx);
	if (ok) {
		plain[plainLen] = 0;
		checkStr("RSA-OAEP decrypt round trip", (const char *)plain, message);
	} else {
		fail("RSA-OAEP decrypt round trip", "EVP_PKEY_decrypt failed");
	}

	// PEM round trip through a memory BIO.
	auto bio = BIO_new(BIO_s_mem());
	check(bio && PEM_write_bio_PrivateKey(bio, key, nullptr, nullptr, 0, nullptr, nullptr) == 1,
			"PEM_write_bio_PrivateKey");
	auto reread = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
	check(reread != nullptr, "PEM_read_bio_PrivateKey");
	if (reread) {
		checkInt("re-read key equals the original", EVP_PKEY_eq(key, reread), 1);
		EVP_PKEY_free(reread);
	}
	BIO_free(bio);

	EVP_PKEY_free(key);
}

//
// libcrypto: elliptic curves
//

void performOpensslEcTest() {
	auto key = EVP_PKEY_Q_keygen(nullptr, nullptr, "EC", "P-256");
	if (!key) {
		fail("EC P-256 keygen", "EVP_PKEY_Q_keygen returned null");
		return;
	}
	check(true, "EC P-256 keygen");
	checkInt("EVP_PKEY_get_bits(P-256)", EVP_PKEY_get_bits(key), 256);

	char groupName[64] = {0};
	size_t groupLen = 0;
	check(EVP_PKEY_get_utf8_string_param(key, OSSL_PKEY_PARAM_GROUP_NAME, groupName,
				  sizeof(groupName), &groupLen)
					== 1
					&& strcmp(groupName, "prime256v1") == 0,
			"EC group name is prime256v1");

	static const char *message = "ecdsa payload";
	unsigned char sig[128];
	size_t sigLen = sizeof(sig);
	auto mdctx = EVP_MD_CTX_new();
	bool ok = mdctx && EVP_DigestSignInit(mdctx, nullptr, EVP_sha256(), nullptr, key) == 1
			&& EVP_DigestSign(mdctx, sig, &sigLen, (const unsigned char *)message,
					   strlen(message))
					== 1;
	EVP_MD_CTX_free(mdctx);
	check(ok, "ECDSA sign");

	mdctx = EVP_MD_CTX_new();
	int verified = -1;
	if (mdctx && EVP_DigestVerifyInit(mdctx, nullptr, EVP_sha256(), nullptr, key) == 1) {
		verified = EVP_DigestVerify(mdctx, sig, sigLen, (const unsigned char *)message,
				strlen(message));
	}
	EVP_MD_CTX_free(mdctx);
	checkInt("ECDSA verify", verified, 1);

	mdctx = EVP_MD_CTX_new();
	int otherMessage = 1;
	if (mdctx && EVP_DigestVerifyInit(mdctx, nullptr, EVP_sha256(), nullptr, key) == 1) {
		otherMessage = EVP_DigestVerify(mdctx, sig, sigLen, (const unsigned char *)"other", 5);
	}
	EVP_MD_CTX_free(mdctx);
	check(otherMessage != 1, "ECDSA verify rejects a different message");
	ERR_clear_error();

	// ECDH: both sides must derive the same secret.
	auto peer = EVP_PKEY_Q_keygen(nullptr, nullptr, "EC", "P-256");
	check(peer != nullptr, "EC P-256 peer keygen");
	if (peer) {
		unsigned char secretA[64], secretB[64];
		size_t lenA = sizeof(secretA), lenB = sizeof(secretB);

		auto ctxA = EVP_PKEY_CTX_new(key, nullptr);
		bool okA = ctxA && EVP_PKEY_derive_init(ctxA) == 1
				&& EVP_PKEY_derive_set_peer(ctxA, peer) == 1
				&& EVP_PKEY_derive(ctxA, secretA, &lenA) == 1;
		EVP_PKEY_CTX_free(ctxA);

		auto ctxB = EVP_PKEY_CTX_new(peer, nullptr);
		bool okB = ctxB && EVP_PKEY_derive_init(ctxB) == 1
				&& EVP_PKEY_derive_set_peer(ctxB, key) == 1
				&& EVP_PKEY_derive(ctxB, secretB, &lenB) == 1;
		EVP_PKEY_CTX_free(ctxB);

		check(okA && okB, "ECDH derive on both sides");
		checkInt("ECDH secret length", (long long)lenA, 32);
		check(okA && okB && lenA == lenB && memcmp(secretA, secretB, lenA) == 0,
				"ECDH secrets agree");
		EVP_PKEY_free(peer);
	}

	EVP_PKEY_free(key);

	// X25519 and Ed25519 are what modern TLS actually uses.
	auto x25519 = EVP_PKEY_Q_keygen(nullptr, nullptr, "X25519");
	check(x25519 != nullptr, "X25519 keygen");
	if (x25519) {
		auto x25519Peer = EVP_PKEY_Q_keygen(nullptr, nullptr, "X25519");
		unsigned char secret[64];
		size_t secretLen = sizeof(secret);
		auto dctx = EVP_PKEY_CTX_new(x25519, nullptr);
		bool okd = x25519Peer && dctx && EVP_PKEY_derive_init(dctx) == 1
				&& EVP_PKEY_derive_set_peer(dctx, x25519Peer) == 1
				&& EVP_PKEY_derive(dctx, secret, &secretLen) == 1;
		EVP_PKEY_CTX_free(dctx);
		check(okd, "X25519 derive");
		checkInt("X25519 secret length", (long long)secretLen, 32);
		EVP_PKEY_free(x25519Peer);
		EVP_PKEY_free(x25519);
	}

	auto ed25519 = EVP_PKEY_Q_keygen(nullptr, nullptr, "ED25519");
	check(ed25519 != nullptr, "Ed25519 keygen");
	if (ed25519) {
		unsigned char edSig[128];
		size_t edSigLen = sizeof(edSig);
		auto edCtx = EVP_MD_CTX_new();
		// Ed25519 signs the message directly: no digest, one-shot only.
		bool oked = edCtx && EVP_DigestSignInit(edCtx, nullptr, nullptr, nullptr, ed25519) == 1
				&& EVP_DigestSign(edCtx, edSig, &edSigLen, (const unsigned char *)"payload", 7)
						== 1;
		EVP_MD_CTX_free(edCtx);
		check(oked, "Ed25519 sign");
		checkInt("Ed25519 signature length", (long long)edSigLen, 64);

		edCtx = EVP_MD_CTX_new();
		int edVerified = -1;
		if (edCtx && EVP_DigestVerifyInit(edCtx, nullptr, nullptr, nullptr, ed25519) == 1) {
			edVerified = EVP_DigestVerify(edCtx, edSig, edSigLen, (const unsigned char *)"payload",
					7);
		}
		EVP_MD_CTX_free(edCtx);
		checkInt("Ed25519 verify", edVerified, 1);
		EVP_PKEY_free(ed25519);
	}
}

//
// libcrypto: X.509
//

void performOpensslX509Test() {
	TestCredentials creds;
	if (!makeSelfSignedCredentials(creds, "thirdparty-test", "DNS:localhost,IP:127.0.0.1")) {
		fail("self-signed certificate", "makeSelfSignedCredentials failed");
		ERR_print_errors_fp(stdout);
		return;
	}
	check(true, "self-signed certificate");

	checkInt("X509_get_version", (long long)X509_get_version(creds.cert), 2);
	checkInt("X509_verify with its own key", X509_verify(creds.cert, creds.key), 1);

	char subject[256];
	X509_NAME_oneline(X509_get_subject_name(creds.cert), subject, sizeof(subject));
	checkStr("X509 subject", subject, "/CN=thirdparty-test");

	char issuer[256];
	X509_NAME_oneline(X509_get_issuer_name(creds.cert), issuer, sizeof(issuer));
	checkStr("X509 issuer equals subject", issuer, subject);

	checkInt("X509_check_host(localhost)",
			X509_check_host(creds.cert, "localhost", 9, 0, nullptr), 1);
	checkInt("X509_check_host(example.com)",
			X509_check_host(creds.cert, "example.com", 11, 0, nullptr), 0);
	checkInt("X509_check_ip_asc(127.0.0.1)", X509_check_ip_asc(creds.cert, "127.0.0.1", 0), 1);

	// The signature must be the one we asked for.
	int mdNid = 0;
	check(X509_get_signature_info(creds.cert, &mdNid, nullptr, nullptr, nullptr) == 1,
			"X509_get_signature_info");
	checkInt("certificate is signed with SHA-256", mdNid, NID_sha256);

	// DER round trip.
	unsigned char *der = nullptr;
	auto derLen = i2d_X509(creds.cert, &der);
	check(derLen > 0 && der != nullptr, "i2d_X509");
	if (derLen > 0) {
		const unsigned char *p = der;
		auto reread = d2i_X509(nullptr, &p, derLen);
		check(reread != nullptr, "d2i_X509");
		if (reread) {
			checkInt("DER round trip preserves the certificate", X509_cmp(creds.cert, reread), 0);
			X509_free(reread);
		}
		OPENSSL_free(der);
	}

	// PEM round trip.
	char pem[4096];
	auto pemLen = pemEncodeCert(creds.cert, pem, sizeof(pem));
	check(pemLen > 0, "PEM_write_bio_X509");
	check(pemLen > 0 && strncmp(pem, "-----BEGIN CERTIFICATE-----", 27) == 0,
			"PEM output starts with the certificate header");

	// Verification through an X509_STORE, the path libssl itself takes.
	auto store = X509_STORE_new();
	auto storeCtx = X509_STORE_CTX_new();
	int verifyResult = -1;
	if (store && storeCtx && X509_STORE_add_cert(store, creds.cert) == 1
			&& X509_STORE_CTX_init(storeCtx, store, creds.cert, nullptr) == 1) {
		verifyResult = X509_verify_cert(storeCtx);
		if (verifyResult != 1) {
			printf("       verify error: %s\n",
					X509_verify_cert_error_string(X509_STORE_CTX_get_error(storeCtx)));
		}
	}
	checkInt("X509_verify_cert against a store holding it", verifyResult, 1);
	X509_STORE_CTX_free(storeCtx);
	X509_STORE_free(store);
}

//
// libcrypto: the error stack
//

void performOpensslErrorTest() {
	ERR_clear_error();
	checkInt("error stack starts empty", (long long)ERR_peek_error(), 0);

	// Feed d2i garbage - it must both fail and leave a diagnosable error.
	static const unsigned char garbage[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
	const unsigned char *p = garbage;
	auto cert = d2i_X509(nullptr, &p, sizeof(garbage));
	check(cert == nullptr, "d2i_X509 rejects garbage");
	if (cert) {
		X509_free(cert);
	}

	auto code = ERR_peek_error();
	check(code != 0, "a failed call pushes onto the error stack");

	char buf[256] = {0};
	ERR_error_string_n(code, buf, sizeof(buf));
	check(strlen(buf) > 0, "ERR_error_string_n produces a message");
	printf("       %s\n", buf);

	check(ERR_lib_error_string(code) != nullptr, "ERR_lib_error_string");
	check(ERR_reason_error_string(code) != nullptr, "ERR_reason_error_string");

	ERR_clear_error();
	checkInt("ERR_clear_error empties the stack", (long long)ERR_get_error(), 0);
}

//
// libssl: a complete handshake over a BIO pair (no sockets, no threads)
//

// Drives both ends until each reports the handshake finished, or until the step
// budget runs out. The BIO pair moves the bytes, so no I/O of our own is needed.
static bool pumpHandshake(SSL *client, SSL *server) {
	for (int i = 0; i < 64; ++i) {
		auto clientDone = SSL_is_init_finished(client);
		auto serverDone = SSL_is_init_finished(server);
		if (clientDone && serverDone) {
			return true;
		}
		if (!clientDone) {
			auto ret = SSL_do_handshake(client);
			if (ret <= 0) {
				auto err = SSL_get_error(client, ret);
				if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE) {
					printf("       client handshake error %d\n", err);
					return false;
				}
			}
		}
		if (!serverDone) {
			auto ret = SSL_do_handshake(server);
			if (ret <= 0) {
				auto err = SSL_get_error(server, ret);
				if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE) {
					printf("       server handshake error %d\n", err);
					return false;
				}
			}
		}
	}
	return false;
}

// SSL_CTX_new() is the first thing every TLS user does, and on a build where the
// TLS group machinery is broken it is also the first thing that fails - with a
// bare ERR_R_SSL_LIB that names no cause. Print what is known to be involved so
// a failure here is actionable instead of opaque. See README.adoc.
static void reportSslCtxFailure() {
	ERR_print_errors_fp(stdout);
	printf("       SSL_CTX_new() failed. It builds its TLS group list from the provider\n"
		   "       capability list and then parses OpenSSL's built-in default group\n"
		   "       string; a failure there aborts SSL_CTX_new for every method.\n");
	printf("       default provider available: %d\n",
			OSSL_PROVIDER_available(nullptr, "default"));
	printf("       default ciphersuites: %s\n", OSSL_default_ciphersuites());
}

void performOpensslTlsMemoryTest() {
	TestCredentials creds;
	if (!makeSelfSignedCredentials(creds, "localhost", "DNS:localhost,IP:127.0.0.1")) {
		fail("TLS credentials", "makeSelfSignedCredentials failed");
		return;
	}

	auto serverCtx = SSL_CTX_new(TLS_server_method());
	auto clientCtx = SSL_CTX_new(TLS_client_method());
	if (!serverCtx || !clientCtx) {
		fail("SSL_CTX_new", "returned null");
		reportSslCtxFailure();
		SSL_CTX_free(serverCtx);
		SSL_CTX_free(clientCtx);
		return;
	}
	check(true, "SSL_CTX_new for both ends");

	check(SSL_CTX_use_certificate(serverCtx, creds.cert) == 1, "SSL_CTX_use_certificate");
	check(SSL_CTX_use_PrivateKey(serverCtx, creds.key) == 1, "SSL_CTX_use_PrivateKey");
	check(SSL_CTX_check_private_key(serverCtx) == 1, "SSL_CTX_check_private_key");

	// The client trusts exactly the certificate the server presents, and nothing else.
	SSL_CTX_set_verify(clientCtx, SSL_VERIFY_PEER, nullptr);
	check(X509_STORE_add_cert(SSL_CTX_get_cert_store(clientCtx), creds.cert) == 1,
			"X509_STORE_add_cert into the client store");
	check(SSL_CTX_set_min_proto_version(clientCtx, TLS1_2_VERSION) == 1,
			"SSL_CTX_set_min_proto_version");

	auto client = SSL_new(clientCtx);
	auto server = SSL_new(serverCtx);
	check(client && server, "SSL_new for both ends");
	check(SSL_set1_host(client, "localhost") == 1, "SSL_set1_host");
	check(SSL_set_tlsext_host_name(client, "localhost") == 1, "SNI (SSL_set_tlsext_host_name)");

	BIO *clientBio = nullptr, *serverBio = nullptr;
	check(BIO_new_bio_pair(&clientBio, 0, &serverBio, 0) == 1, "BIO_new_bio_pair");

	SSL_set_bio(client, clientBio, clientBio);
	SSL_set_bio(server, serverBio, serverBio);
	SSL_set_connect_state(client);
	SSL_set_accept_state(server);

	if (!pumpHandshake(client, server)) {
		fail("TLS handshake over a BIO pair", "handshake did not finish");
		ERR_print_errors_fp(stdout);
	} else {
		check(true, "TLS handshake over a BIO pair");
		printf("       %s / %s\n", SSL_get_version(client),
				SSL_get_cipher_name(client));

		checkStr("negotiated protocol", SSL_get_version(client), "TLSv1.3");
		check(SSL_get_cipher_name(client) != nullptr && strlen(SSL_get_cipher_name(client)) > 0,
				"a cipher suite was negotiated");
		checkInt("SSL_get_verify_result", (long long)SSL_get_verify_result(client), X509_V_OK);

		auto peer = SSL_get1_peer_certificate(client);
		check(peer != nullptr, "the client received the peer certificate");
		if (peer) {
			checkInt("peer certificate is the one we generated", X509_cmp(peer, creds.cert), 0);
			X509_free(peer);
		}

		// Application data in both directions.
		static const char *request = "ping";
		checkInt("SSL_write (client)", SSL_write(client, request, 4), 4);

		char received[64] = {0};
		auto readLen = SSL_read(server, received, sizeof(received) - 1);
		checkInt("SSL_read (server)", readLen, 4);
		if (readLen > 0) {
			received[readLen] = 0;
			checkStr("server received the request", received, request);
		}

		static const char *response = "pong";
		checkInt("SSL_write (server)", SSL_write(server, response, 4), 4);
		memset(received, 0, sizeof(received));
		readLen = SSL_read(client, received, sizeof(received) - 1);
		checkInt("SSL_read (client)", readLen, 4);
		if (readLen > 0) {
			received[readLen] = 0;
			checkStr("client received the response", received, response);
		}
	}

	SSL_free(client);
	SSL_free(server);
	SSL_CTX_free(clientCtx);
	SSL_CTX_free(serverCtx);
}

} // namespace thirdparty::test
