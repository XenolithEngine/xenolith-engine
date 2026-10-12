// Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
//
// api/time_api.h <-> Windows SDK function-signature parity. Compile-time only; see check.sh.

#define SPRT_API_HEADER <sprt/gen/wrappers/windows/api/time_api.h>
#include "api_check.h"

#include <windows.h>
#include <timeapi.h>      // timeBeginPeriod / timeEndPeriod

SPRT_FN(GetDynamicTimeZoneInformation);
SPRT_FN(GetTimeZoneInformation);
SPRT_FN(QueryPerformanceFrequency);
SPRT_FN(QueryPerformanceCounter);
SPRT_FN(GetSystemTimePreciseAsFileTime);
SPRT_FN(GetSystemTimeAsFileTime);
SPRT_FN(GetSystemTime);
SPRT_FN(GetTickCount);
SPRT_FN(GetTickCount64);
SPRT_FN(GetProcessTimes);
SPRT_FN(GetThreadTimes);
SPRT_FN(SystemTimeToFileTime);
SPRT_FN(FileTimeToSystemTime);
SPRT_FN(SystemTimeToTzSpecificLocalTime);
SPRT_FN(GetTimeZoneInformationForYear);
SPRT_FN(timeBeginPeriod);
SPRT_FN(timeEndPeriod);
