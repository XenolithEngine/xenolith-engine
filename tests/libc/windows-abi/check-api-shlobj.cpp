// Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
//
// api/shlobj.h <-> Windows SDK function-signature parity. Compile-time only; see check.sh.

// api/shlobj.h references PIDLIST_ABSOLUTE/PCUITEMID_CHILD_ARRAY but relies on the
// caller having included com_cxx.hpp first (its only definition in the wrapper tree)
#define SPRT_API_PRELUDE <sprt/wrappers/windows/com_cxx.hpp>
#define SPRT_API_HEADER <sprt/gen/wrappers/windows/api/shlobj.h>
#include "api_check.h"

#include <windows.h>
#include <shlobj.h>

SPRT_FN(SHGetKnownFolderPath);
SPRT_FN(ILCreateFromPathW);
SPRT_FN(ILFree);
SPRT_FN(SHCreateItemFromIDList);
SPRT_FN(SHCreateItemFromParsingName);
SPRT_FN(SHOpenFolderAndSelectItems);
