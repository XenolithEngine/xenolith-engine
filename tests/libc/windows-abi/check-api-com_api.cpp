// Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
//
// api/com_api.h <-> Windows SDK function-signature parity. Compile-time only; see check.sh.

#define SPRT_API_HEADER <sprt/gen/wrappers/windows/api/com_api.h>
#include "api_check.h"

#include <windows.h>
#include <objbase.h>
#include <oleauto.h>
#include <shlobj.h>

SPRT_FN(CoInitializeSecurity);
SPRT_FN(CoCreateInstance);
SPRT_FN(CoSetProxyBlanket);
SPRT_FN(CoInitializeEx);
SPRT_FN(CoUninitialize);
SPRT_FN(CoTaskMemFree);
SPRT_FN(SHGetKnownFolderPath);
SPRT_FN(VariantInit);
SPRT_FN(VariantClear);
SPRT_FN(VariantCopy);
SPRT_FN(SysAllocString);
SPRT_FN(SysStringLen);
SPRT_FN(SysFreeString);
SPRT_FN(StringFromCLSID);
SPRT_FN(CLSIDFromString);
SPRT_FN(IIDFromString);
