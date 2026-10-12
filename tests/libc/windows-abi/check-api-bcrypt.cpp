// Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
//
// api/bcrypt.h <-> Windows SDK function-signature parity. Compile-time only; see check.sh.

#define SPRT_API_HEADER <sprt/gen/wrappers/windows/api/bcrypt.h>
#include "api_check.h"

#include <windows.h>
#include <bcrypt.h>

SPRT_FN(BCryptGenRandom);
SPRT_FN(BCryptGenerateSymmetricKey);
SPRT_FN(BCryptDestroyKey);
SPRT_FN(BCryptCreateHash);
SPRT_FN(BCryptHashData);
SPRT_FN(BCryptFinishHash);
SPRT_FN(BCryptDestroyHash);
SPRT_FN(BCryptGetProperty);
SPRT_FN(BCryptSetProperty);
SPRT_FN(BCryptOpenAlgorithmProvider);
SPRT_FN(BCryptCloseAlgorithmProvider);
SPRT_FN(BCryptDeriveKeyPBKDF2);
SPRT_FN(BCryptEncrypt);
SPRT_FN(BCryptDecrypt);
