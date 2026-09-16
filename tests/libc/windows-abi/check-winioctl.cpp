// Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
//
// abi/winioctl.h <-> Windows SDK parity. Compile-time only; see check.sh.

#define SPRT_ABI_HEADER <sprt/wrappers/windows/abi/winioctl.h>
#include "abi_check.h"

#include <windows.h>
#include <winioctl.h> // FSCTL_GET_REPARSE_POINT, MAXIMUM_REPARSE_DATA_BUFFER_SIZE (not pulled by windows.h)

// === reparse-point device-IO controls ======================================
SPRT_CONST(FSCTL_GET_REPARSE_POINT);
SPRT_CONST(FSCTL_SET_SPARSE);
SPRT_CONST(MAXIMUM_REPARSE_DATA_BUFFER_SIZE);

// SYMLINK_FLAG_RELATIVE is declared only in the driver header ntifs.h, which is not
// part of the user-mode Windows SDK vendored here, so there is no SDK symbol to compare
// against. Its value is a stable part of the reparse-point ABI (see abi/winioctl.h).
