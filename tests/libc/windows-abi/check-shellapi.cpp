// Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
//
// abi/shellapi.h <-> Windows SDK parity. Compile-time only; see check.sh.
//
// FILEOP_FLAGS bits used by llvm's Path.inc no-UI recycle delete: the FOF_* bits
// live in <shellapi.h>, FOF_NO_UI is the SDK's convenience aggregate, and the
// extended FOFX_NOCOPYHOOKS bit lives in <ShObjIdl_core.h>.

#define SPRT_ABI_HEADER <sprt/wrappers/windows/abi/shellapi.h>
#include "abi_check.h"

#include <windows.h>
#include <shellapi.h>       // FOF_* + FOF_NO_UI (FILEOP_FLAGS)
#include <ShObjIdl_core.h>  // FOFX_NOCOPYHOOKS

// === FOF_* individual bits =================================================
SPRT_CONST(FOF_SILENT);
SPRT_CONST(FOF_NOCONFIRMATION);
SPRT_CONST(FOF_NOCONFIRMMKDIR);
SPRT_CONST(FOF_NOERRORUI);

// === FOF_NO_UI aggregate ===================================================
// __SPRT_FOF_NO_UI expands to the OR of the __SPRT_FOF_* bits; the SDK's FOF_NO_UI
// ORs the same four flags. Compare the resolved numeric values.
SPRT_CONST(FOF_NO_UI);

// === FOFX_ extended flag ===================================================
SPRT_CONST(FOFX_NOCOPYHOOKS);
