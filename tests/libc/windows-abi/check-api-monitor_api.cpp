// Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
//
// api/monitor_api.h <-> Windows SDK function-signature parity. Compile-time only; see check.sh.

#define SPRT_API_HEADER <sprt/gen/wrappers/windows/api/monitor_api.h>
#include "api_check.h"

#include <windows.h>
#include <physicalmonitorenumerationapi.h>
#include <shellscalingapi.h>   // GetDpiForMonitor (shcore.h itself is not vendored by xwin)
#include <setupapi.h>

SPRT_FN(GetMonitorInfoW);
SPRT_FN(GetNumberOfPhysicalMonitorsFromHMONITOR);
SPRT_FN(GetPhysicalMonitorsFromHMONITOR);
// SetProcessDpiAwareness: shcore.h is not vendored in the xwin splat, so there is no
// SDK declaration to pin against (GetDpiForMonitor below IS pinned via shellscalingapi.h)
// SPRT_FN(SetProcessDpiAwareness);
SPRT_FN(GetProcessDpiAwareness);
SPRT_FN(GetDpiForMonitor);
SPRT_FN(EnumDisplayDevicesW);
SPRT_FN(CreateDCW);
SPRT_FN(GetDeviceCaps);
SPRT_FN(EnumDisplayMonitors);
SPRT_FN(DeleteDC);
SPRT_FN(EnumDisplaySettingsW);
SPRT_FN(ChangeDisplaySettingsExW);
SPRT_FN(DestroyPhysicalMonitor);
SPRT_FN(DestroyPhysicalMonitors);
SPRT_FN(SetProcessDpiAwarenessContext);
SPRT_FN(GetSystemMetricsForDpi);
SPRT_FN(SetupDiOpenDevRegKey);
SPRT_FN(SetupDiGetClassDevsExW);
SPRT_FN(SetupDiEnumDeviceInterfaces);
SPRT_FN(SetupDiGetDeviceInterfaceDetailW);
SPRT_FN(SetupDiDestroyDeviceInfoList);
