// Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
//
// api/wincrypt.h <-> Windows SDK function-signature parity. Compile-time only; see check.sh.

#define SPRT_API_HEADER <sprt/gen/wrappers/windows/api/wincrypt.h>
#include "api_check.h"

#include <windows.h>
#include <wincrypt.h>

SPRT_FN(CryptAcquireContextW);
SPRT_FN(CryptReleaseContext);
SPRT_FN(CryptGenRandom);
SPRT_FN(CertFreeCertificateContext);
SPRT_FN(CertEnumCertificatesInStore);
SPRT_FN(CertFindCertificateInStore);
SPRT_FN(CertGetIntendedKeyUsage);
SPRT_FN(CertGetEnhancedKeyUsage);
SPRT_FN(CertOpenSystemStoreA);
SPRT_FN(CertOpenSystemStoreW);
SPRT_FN(CertCloseStore);
SPRT_FN(CryptExportKey);
SPRT_FN(CryptImportKey);
SPRT_FN(CryptEncrypt);
SPRT_FN(CryptDecrypt);
SPRT_FN(CryptDestroyKey);
SPRT_FN(CryptGetHashParam);
SPRT_FN(CryptCreateHash);
SPRT_FN(CryptHashData);
SPRT_FN(CryptHashSessionKey);
SPRT_FN(CryptDestroyHash);
