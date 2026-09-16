// Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
//
// abi/bcrypt.h <-> Windows SDK parity. Compile-time only; see check.sh.

#define SPRT_ABI_HEADER <sprt/wrappers/windows/abi/bcrypt.h>
#include "abi_check.h"

#include <windows.h>
#include <bcrypt.h>

// Omitted (not integer-valued, cannot go through the static_assert):
//   - BCRYPT_*_ALGORITHM, BCRYPT_* property names and BCRYPT_CHAIN_MODE_* are all
//     wide-string (L"...") constants.
//   - BCRYPT_SUCCESS(Status) is a function-like macro.
//   - BCRYPT_{,ALG_,KEY_,HASH_,SECRET_}HANDLE are PVOID handle typedefs (no value).
// abi/bcrypt.h defines no structs shared with the SDK.

// === integer constants =====================================================
SPRT_CONST(BCRYPT_RNG_USE_ENTROPY_IN_BUFFER);
SPRT_CONST(BCRYPT_USE_SYSTEM_PREFERRED_RNG);
SPRT_CONST(BCRYPT_ALG_HANDLE_HMAC_FLAG);
SPRT_CONST(BCRYPT_HASH_REUSABLE_FLAG);
