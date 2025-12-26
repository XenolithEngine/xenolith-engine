/**
 Copyright (c) 2024 Stappler LLC <admin@stappler.dev>

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

#if __CDT_PARSER__
#define MODULE_STAPPLER_CRYPTO_OPENSSL 1
#endif

#if MODULE_STAPPLER_CRYPTO_OPENSSL

// Вспомогательный код поддержки ГОСТ криптографии. Использует код проекта openssl-gost-engine

#include <openssl/err.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/evp.h>
#include <openssl/crypto.h>

# define SPGOSTerr(f, r) SP_ERR_GOST_error((f), (r), OPENSSL_FILE, OPENSSL_LINE)

/*
 * GOST function codes.
 */
# define SP_GOST_F_GOST_EC_SIGN   109

/*
 * GOST reason codes.
 */
# define SP_GOST_R_RNG_ERROR      126

#if __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-braces"
#endif

namespace GostR3410_2001_CryptoPro_A_ParamSet { namespace {
#include "thirdparty/openssl-gost-engine/ecp_id_GostR3410_2001_CryptoPro_A_ParamSet.cc"
} }

namespace GostR3410_2001_CryptoPro_B_ParamSet { namespace {
#include "thirdparty/openssl-gost-engine/ecp_id_GostR3410_2001_CryptoPro_B_ParamSet.cc"
} }

namespace GostR3410_2001_CryptoPro_С_ParamSet { namespace {
#include "thirdparty/openssl-gost-engine/ecp_id_GostR3410_2001_CryptoPro_C_ParamSet.cc"
} }

namespace GostR3410_2001_TestParamSet { namespace {
#include "thirdparty/openssl-gost-engine/ecp_id_GostR3410_2001_TestParamSet.cc"
} }

namespace tc26_gost_3410_2012_256_paramSetA { namespace {
#include "thirdparty/openssl-gost-engine/ecp_id_tc26_gost_3410_2012_256_paramSetA.cc"
} }

namespace tc26_gost_3410_2012_512_paramSetA { namespace {
#include "thirdparty/openssl-gost-engine/ecp_id_tc26_gost_3410_2012_512_paramSetA.cc"
} }

namespace tc26_gost_3410_2012_512_paramSetB { namespace {
#include "thirdparty/openssl-gost-engine/ecp_id_tc26_gost_3410_2012_512_paramSetB.cc"
} }

namespace tc26_gost_3410_2012_512_paramSetC { namespace {
#include "thirdparty/openssl-gost-engine/ecp_id_tc26_gost_3410_2012_512_paramSetC.cc"
} }

#if __clang__
#pragma clang diagnostic pop
#endif

namespace STAPPLER_VERSIONIZED stappler::crypto {

static ERR_STRING_DATA GOST_str_reasons[] = {
	{ERR_PACK(0, 0, SP_GOST_R_RNG_ERROR), "rng error"},
	{0, NULL}
};

static std::mutex s_gostMutex;
static int s_lib_code = 0;
static int s_error_loaded = 0;

int SP_ERR_load_GOST_strings(void) {
	std::unique_lock lock(s_gostMutex);
	if (s_lib_code == 0) {
		s_lib_code = ERR_get_next_error_library();
	}

	if (!s_error_loaded) {
#ifndef OPENSSL_NO_ERR
		ERR_load_strings(s_lib_code, GOST_str_reasons);
#endif
		s_error_loaded = 1;
	}
	return 1;
}

void SP_ERR_unload_GOST_strings(void) {
	std::unique_lock lock(s_gostMutex);
	if (s_error_loaded) {
#ifndef OPENSSL_NO_ERR
		ERR_unload_strings(s_lib_code, GOST_str_reasons);
#endif
		s_error_loaded = 0;
	}
}

void SP_ERR_GOST_error(int function, int reason, char *file, int line) {
	std::unique_lock lock(s_gostMutex);
	if (s_lib_code == 0) {
		s_lib_code = ERR_get_next_error_library();
	}
	lock.unlock();
	ERR_raise(s_lib_code, reason);
	ERR_set_debug(file, line, NULL);
}

/*
 * Pack bignum into byte buffer of given size, filling all leading bytes by
 * zeros
 */
int store_bignum(const BIGNUM *bn, unsigned char *buf, int len) {
	int bytes = BN_num_bytes(bn);

	if (bytes > len) {
		return 0;
	}

	memset(buf, 0, len);
	BN_bn2bin(bn, buf + len - bytes);
	return 1;
}

/*
 * Packs signature according to Cryptopro rules
 * and frees up ECDSA_SIG structure
 */
int pack_sign_cp(ECDSA_SIG *s, int order, unsigned char *sig, size_t *siglen) {
	const BIGNUM *sig_r = NULL, *sig_s = NULL;
	ECDSA_SIG_get0(s, &sig_r, &sig_s);
	*siglen = 2 * order;
	memset(sig, 0, *siglen);
	store_bignum(sig_s, sig, order);
	store_bignum(sig_r, sig + order, order);
	ECDSA_SIG_free(s);
	return 1;
}

int gost_ec_point_mul(const EC_GROUP *group, EC_POINT *r, const BIGNUM *n,
                      const EC_POINT *q, const BIGNUM *m, BN_CTX *ctx) {
	if (group == NULL || r == NULL || ctx == NULL) {
		return 0;
	}

	if (m != NULL && n != NULL) {
		/* verification */
		if (q == NULL)
			return 0;
		switch (EC_GROUP_get_curve_name(group)) {
		case NID_id_GostR3410_2001_CryptoPro_A_ParamSet:
		case NID_id_GostR3410_2001_CryptoPro_XchA_ParamSet:
		case NID_id_tc26_gost_3410_2012_256_paramSetB:
			return ::GostR3410_2001_CryptoPro_A_ParamSet::point_mul_two_id_GostR3410_2001_CryptoPro_A_ParamSet(group, r, n, q, m, ctx);
		case NID_id_GostR3410_2001_CryptoPro_B_ParamSet:
		case NID_id_tc26_gost_3410_2012_256_paramSetC:
			return ::GostR3410_2001_CryptoPro_B_ParamSet::point_mul_two_id_GostR3410_2001_CryptoPro_B_ParamSet(group, r, n, q, m, ctx);
		case NID_id_GostR3410_2001_CryptoPro_C_ParamSet:
		case NID_id_GostR3410_2001_CryptoPro_XchB_ParamSet:
		case NID_id_tc26_gost_3410_2012_256_paramSetD:
			return ::GostR3410_2001_CryptoPro_С_ParamSet::point_mul_two_id_GostR3410_2001_CryptoPro_C_ParamSet(group, r, n, q, m, ctx);
		case NID_id_GostR3410_2001_TestParamSet:
			return ::GostR3410_2001_TestParamSet::point_mul_two_id_GostR3410_2001_TestParamSet(group, r, n, q, m, ctx);
		case NID_id_tc26_gost_3410_2012_256_paramSetA:
			return ::tc26_gost_3410_2012_256_paramSetA::point_mul_two_id_tc26_gost_3410_2012_256_paramSetA(group, r, n, q, m, ctx);
		case NID_id_tc26_gost_3410_2012_512_paramSetA:
			return ::tc26_gost_3410_2012_512_paramSetA::point_mul_two_id_tc26_gost_3410_2012_512_paramSetA(group, r, n, q, m, ctx);
		case NID_id_tc26_gost_3410_2012_512_paramSetB:
			return ::tc26_gost_3410_2012_512_paramSetB::point_mul_two_id_tc26_gost_3410_2012_512_paramSetB(group, r, n, q, m, ctx);
		case NID_id_tc26_gost_3410_2012_512_paramSetC:
			return ::tc26_gost_3410_2012_512_paramSetC::point_mul_two_id_tc26_gost_3410_2012_512_paramSetC(group, r, n, q, m, ctx);
		default:
			return EC_POINT_mul(group, r, n, q, m, ctx);
		}
	} else if (n != NULL) {
		/* mul g */
		switch (EC_GROUP_get_curve_name(group)) {
		case NID_id_GostR3410_2001_CryptoPro_A_ParamSet:
		case NID_id_GostR3410_2001_CryptoPro_XchA_ParamSet:
		case NID_id_tc26_gost_3410_2012_256_paramSetB:
			return ::GostR3410_2001_CryptoPro_A_ParamSet::point_mul_g_id_GostR3410_2001_CryptoPro_A_ParamSet(group, r, n, ctx);
		case NID_id_GostR3410_2001_CryptoPro_B_ParamSet:
		case NID_id_tc26_gost_3410_2012_256_paramSetC:
			return ::GostR3410_2001_CryptoPro_B_ParamSet::point_mul_g_id_GostR3410_2001_CryptoPro_B_ParamSet(group, r, n, ctx);
		case NID_id_GostR3410_2001_CryptoPro_C_ParamSet:
		case NID_id_GostR3410_2001_CryptoPro_XchB_ParamSet:
		case NID_id_tc26_gost_3410_2012_256_paramSetD:
			return ::GostR3410_2001_CryptoPro_С_ParamSet::point_mul_g_id_GostR3410_2001_CryptoPro_C_ParamSet(group, r, n, ctx);
		case NID_id_GostR3410_2001_TestParamSet:
			return ::GostR3410_2001_TestParamSet::point_mul_g_id_GostR3410_2001_TestParamSet(group, r, n, ctx);
		case NID_id_tc26_gost_3410_2012_256_paramSetA:
			return ::tc26_gost_3410_2012_256_paramSetA::point_mul_g_id_tc26_gost_3410_2012_256_paramSetA(group, r, n, ctx);
		case NID_id_tc26_gost_3410_2012_512_paramSetA:
			return ::tc26_gost_3410_2012_512_paramSetA::point_mul_g_id_tc26_gost_3410_2012_512_paramSetA(group, r, n, ctx);
		case NID_id_tc26_gost_3410_2012_512_paramSetB:
			return ::tc26_gost_3410_2012_512_paramSetB::point_mul_g_id_tc26_gost_3410_2012_512_paramSetB(group, r, n, ctx);
		case NID_id_tc26_gost_3410_2012_512_paramSetC:
			return ::tc26_gost_3410_2012_512_paramSetC::point_mul_g_id_tc26_gost_3410_2012_512_paramSetC(group, r, n, ctx);
		default:
			return EC_POINT_mul(group, r, n, q, m, ctx);
		}
	} else if (m != NULL) {
		if (q == NULL)
			return 0;
		/* mul */
		switch (EC_GROUP_get_curve_name(group)) {
		case NID_id_GostR3410_2001_CryptoPro_A_ParamSet:
		case NID_id_GostR3410_2001_CryptoPro_XchA_ParamSet:
		case NID_id_tc26_gost_3410_2012_256_paramSetB:
			return ::GostR3410_2001_CryptoPro_A_ParamSet::point_mul_id_GostR3410_2001_CryptoPro_A_ParamSet(group, r, q, m, ctx);
		case NID_id_GostR3410_2001_CryptoPro_B_ParamSet:
		case NID_id_tc26_gost_3410_2012_256_paramSetC:
			return ::GostR3410_2001_CryptoPro_B_ParamSet::point_mul_id_GostR3410_2001_CryptoPro_B_ParamSet(group, r, q, m, ctx);
		case NID_id_GostR3410_2001_CryptoPro_C_ParamSet:
		case NID_id_GostR3410_2001_CryptoPro_XchB_ParamSet:
		case NID_id_tc26_gost_3410_2012_256_paramSetD:
			return ::GostR3410_2001_CryptoPro_С_ParamSet::point_mul_id_GostR3410_2001_CryptoPro_C_ParamSet(group, r, q, m, ctx);
		case NID_id_GostR3410_2001_TestParamSet:
			return ::GostR3410_2001_TestParamSet::point_mul_id_GostR3410_2001_TestParamSet(group, r, q, m, ctx);
		case NID_id_tc26_gost_3410_2012_256_paramSetA:
			return ::tc26_gost_3410_2012_256_paramSetA::point_mul_id_tc26_gost_3410_2012_256_paramSetA(group, r, q, m, ctx);
		case NID_id_tc26_gost_3410_2012_512_paramSetA:
			return ::tc26_gost_3410_2012_512_paramSetA::point_mul_id_tc26_gost_3410_2012_512_paramSetA(group, r, q, m, ctx);
		case NID_id_tc26_gost_3410_2012_512_paramSetB:
			return ::tc26_gost_3410_2012_512_paramSetB::point_mul_id_tc26_gost_3410_2012_512_paramSetB(group, r, q, m, ctx);
		case NID_id_tc26_gost_3410_2012_512_paramSetC:
			return ::tc26_gost_3410_2012_512_paramSetC::point_mul_id_tc26_gost_3410_2012_512_paramSetC(group, r, q, m, ctx);
		default:
			return EC_POINT_mul(group, r, n, q, m, ctx);
		}
	}
	return 0;
}

}

#endif // MODULE_STAPPLER_CRYPTO_OPENSSL
