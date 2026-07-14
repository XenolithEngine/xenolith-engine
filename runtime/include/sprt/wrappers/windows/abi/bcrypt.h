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

#ifndef SPRT_WRAPPERS_WINDOWS_ABI_BCRYPT_H_
#define SPRT_WRAPPERS_WINDOWS_ABI_BCRYPT_H_


#include <sprt/wrappers/windows/abi/structures.h>
#include <sprt/wrappers/windows/abi/constants.h>

// clang-format off
#define __SPRT_BCRYPT_RNG_USE_ENTROPY_IN_BUFFER    0x00000001
#define __SPRT_BCRYPT_USE_SYSTEM_PREFERRED_RNG     0x00000002

#define __SPRT_BCRYPT_ALG_HANDLE_HMAC_FLAG             0x00000008
#define __SPRT_BCRYPT_HASH_REUSABLE_FLAG               0x00000020
// clang-format on

#define __SPRT_BCRYPT_RSA_ALGORITHM                    L"RSA"
#define __SPRT_BCRYPT_RSA_SIGN_ALGORITHM               L"RSA_SIGN"
#define __SPRT_BCRYPT_DH_ALGORITHM                     L"DH"
#define __SPRT_BCRYPT_DSA_ALGORITHM                    L"DSA"
#define __SPRT_BCRYPT_RC2_ALGORITHM                    L"RC2"
#define __SPRT_BCRYPT_RC4_ALGORITHM                    L"RC4"
#define __SPRT_BCRYPT_AES_ALGORITHM                    L"AES"
#define __SPRT_BCRYPT_DES_ALGORITHM                    L"DES"
#define __SPRT_BCRYPT_DESX_ALGORITHM                   L"DESX"
#define __SPRT_BCRYPT_3DES_ALGORITHM                   L"3DES"
#define __SPRT_BCRYPT_3DES_112_ALGORITHM               L"3DES_112"
#define __SPRT_BCRYPT_MD2_ALGORITHM                    L"MD2"
#define __SPRT_BCRYPT_MD4_ALGORITHM                    L"MD4"
#define __SPRT_BCRYPT_MD5_ALGORITHM                    L"MD5"
#define __SPRT_BCRYPT_SHA1_ALGORITHM                   L"SHA1"
#define __SPRT_BCRYPT_SHA256_ALGORITHM                 L"SHA256"
#define __SPRT_BCRYPT_SHA384_ALGORITHM                 L"SHA384"
#define __SPRT_BCRYPT_SHA512_ALGORITHM                 L"SHA512"
#define __SPRT_BCRYPT_AES_GMAC_ALGORITHM               L"AES-GMAC"
#define __SPRT_BCRYPT_AES_CMAC_ALGORITHM               L"AES-CMAC"
#define __SPRT_BCRYPT_ECDSA_P256_ALGORITHM             L"ECDSA_P256"
#define __SPRT_BCRYPT_ECDSA_P384_ALGORITHM             L"ECDSA_P384"
#define __SPRT_BCRYPT_ECDSA_P521_ALGORITHM             L"ECDSA_P521"
#define __SPRT_BCRYPT_ECDH_P256_ALGORITHM              L"ECDH_P256"
#define __SPRT_BCRYPT_ECDH_P384_ALGORITHM              L"ECDH_P384"
#define __SPRT_BCRYPT_ECDH_P521_ALGORITHM              L"ECDH_P521"
#define __SPRT_BCRYPT_RNG_ALGORITHM                    L"RNG"
#define __SPRT_BCRYPT_RNG_FIPS186_DSA_ALGORITHM        L"FIPS186DSARNG"
#define __SPRT_BCRYPT_RNG_DUAL_EC_ALGORITHM            L"DUALECRNG"

#define __SPRT_BCRYPT_OBJECT_LENGTH        L"ObjectLength"
#define __SPRT_BCRYPT_ALGORITHM_NAME       L"AlgorithmName"
#define __SPRT_BCRYPT_PROVIDER_HANDLE      L"ProviderHandle"
#define __SPRT_BCRYPT_CHAINING_MODE        L"ChainingMode"
#define __SPRT_BCRYPT_BLOCK_LENGTH         L"BlockLength"
#define __SPRT_BCRYPT_KEY_LENGTH           L"KeyLength"
#define __SPRT_BCRYPT_KEY_OBJECT_LENGTH    L"KeyObjectLength"
#define __SPRT_BCRYPT_KEY_STRENGTH         L"KeyStrength"
#define __SPRT_BCRYPT_KEY_LENGTHS          L"KeyLengths"
#define __SPRT_BCRYPT_BLOCK_SIZE_LIST      L"BlockSizeList"
#define __SPRT_BCRYPT_EFFECTIVE_KEY_LENGTH L"EffectiveKeyLength"
#define __SPRT_BCRYPT_HASH_LENGTH          L"HashDigestLength"
#define __SPRT_BCRYPT_HASH_OID_LIST        L"HashOIDList"
#define __SPRT_BCRYPT_PADDING_SCHEMES      L"PaddingSchemes"
#define __SPRT_BCRYPT_SIGNATURE_LENGTH     L"SignatureLength"
#define __SPRT_BCRYPT_HASH_BLOCK_LENGTH    L"HashBlockLength"
#define __SPRT_BCRYPT_AUTH_TAG_LENGTH      L"AuthTagLength"

#define __SPRT_BCRYPT_CHAIN_MODE_NA        L"ChainingModeN/A"
#define __SPRT_BCRYPT_CHAIN_MODE_CBC       L"ChainingModeCBC"
#define __SPRT_BCRYPT_CHAIN_MODE_ECB       L"ChainingModeECB"
#define __SPRT_BCRYPT_CHAIN_MODE_CFB       L"ChainingModeCFB"
#define __SPRT_BCRYPT_CHAIN_MODE_CCM       L"ChainingModeCCM"
#define __SPRT_BCRYPT_CHAIN_MODE_GCM       L"ChainingModeGCM"

#define __SPRT_BCRYPT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)

typedef PVOID BCRYPT_HANDLE;
typedef PVOID BCRYPT_ALG_HANDLE;
typedef PVOID BCRYPT_KEY_HANDLE;
typedef PVOID BCRYPT_HASH_HANDLE;
typedef PVOID BCRYPT_SECRET_HANDLE;


#endif // SPRT_WRAPPERS_WINDOWS_ABI_BCRYPT_H_
