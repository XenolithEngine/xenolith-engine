// Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
//
// abi/time_api.h <-> Windows SDK parity. Compile-time only; see check.sh.

#define SPRT_ABI_HEADER <sprt/wrappers/windows/abi/time_api.h>
#include "abi_check.h"

#include <windows.h>   // SYSTEMTIME, TIME_ZONE_INFORMATION, DYNAMIC_TIME_ZONE_INFORMATION, TIME_ZONE_ID_*

// === constants =============================================================
SPRT_CONST(TIME_ZONE_ID_UNKNOWN);
SPRT_CONST(TIME_ZONE_ID_STANDARD);
SPRT_CONST(TIME_ZONE_ID_DAYLIGHT);
SPRT_CONST(TIME_ZONE_ID_INVALID);

// === SYSTEMTIME ============================================================
SPRT_SIZE(SYSTEMTIME);
SPRT_OFFSET(SYSTEMTIME, wYear);
SPRT_OFFSET(SYSTEMTIME, wMonth);
SPRT_OFFSET(SYSTEMTIME, wDayOfWeek);
SPRT_OFFSET(SYSTEMTIME, wDay);
SPRT_OFFSET(SYSTEMTIME, wHour);
SPRT_OFFSET(SYSTEMTIME, wMinute);
SPRT_OFFSET(SYSTEMTIME, wSecond);
SPRT_OFFSET(SYSTEMTIME, wMilliseconds);

// === TIME_ZONE_INFORMATION =================================================
SPRT_SIZE(TIME_ZONE_INFORMATION);
SPRT_OFFSET(TIME_ZONE_INFORMATION, Bias);
SPRT_OFFSET(TIME_ZONE_INFORMATION, StandardName);
SPRT_OFFSET(TIME_ZONE_INFORMATION, StandardDate);
SPRT_OFFSET(TIME_ZONE_INFORMATION, StandardBias);
SPRT_OFFSET(TIME_ZONE_INFORMATION, DaylightName);
SPRT_OFFSET(TIME_ZONE_INFORMATION, DaylightDate);
SPRT_OFFSET(TIME_ZONE_INFORMATION, DaylightBias);

// === DYNAMIC_TIME_ZONE_INFORMATION =========================================
SPRT_SIZE(DYNAMIC_TIME_ZONE_INFORMATION);
SPRT_OFFSET(DYNAMIC_TIME_ZONE_INFORMATION, TimeZoneKeyName);
SPRT_OFFSET(DYNAMIC_TIME_ZONE_INFORMATION, DynamicDaylightTimeDisabled);
