/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 **/

#include <sprt/wrappers/windows/monitor_api.h>

#include "dllloader.h"

extern "C" {

// ---- Monitor Information Functions (user32) ----

WINAPI BOOL GetMonitorInfoW(HMONITOR hMonitor, LPMONITORINFO lpmi) {
	auto loader = sprt::DllLoader::get();
	return loader->user32.call<decltype(&GetMonitorInfoW)>(loader->user32.GetMonitorInfoW, hMonitor,
			lpmi);
}

// ---- Physical Monitor Functions (uxtheme) ----

WINAPI BOOL GetNumberOfPhysicalMonitorsFromHMONITOR(HMONITOR hMonitor,
		LPDWORD pdwNumberOfPhysicalMonitors) {
	auto loader = sprt::DllLoader::get();
	return loader->uxtheme.call<decltype(&GetNumberOfPhysicalMonitorsFromHMONITOR)>(
			loader->dxva2.GetNumberOfPhysicalMonitorsFromHMONITOR, hMonitor,
			pdwNumberOfPhysicalMonitors);
}

WINAPI BOOL GetPhysicalMonitorsFromHMONITOR(HMONITOR hMonitor, DWORD dwPhysicalMonitorArraySize,
		LPPHYSICAL_MONITOR pPhysicalMonitorArray) {
	auto loader = sprt::DllLoader::get();
	return loader->uxtheme.call<decltype(&GetPhysicalMonitorsFromHMONITOR)>(
			loader->dxva2.GetPhysicalMonitorsFromHMONITOR, hMonitor, dwPhysicalMonitorArraySize,
			pPhysicalMonitorArray);
}

WINAPI BOOL DestroyPhysicalMonitor(HANDLE hMonitor) {
	auto loader = sprt::DllLoader::get();
	return loader->uxtheme.call<decltype(&DestroyPhysicalMonitor)>(
			loader->dxva2.DestroyPhysicalMonitor, hMonitor);
}

WINAPI BOOL DestroyPhysicalMonitors(DWORD dwPhysicalMonitorArraySize,
		LPPHYSICAL_MONITOR pPhysicalMonitorArray) {
	auto loader = sprt::DllLoader::get();
	return loader->uxtheme.call<decltype(&DestroyPhysicalMonitors)>(
			loader->dxva2.DestroyPhysicalMonitors, dwPhysicalMonitorArraySize,
			pPhysicalMonitorArray);
}

// ---- DPI Functions (uxtheme/user32) ----

WINAPI HRESULT GetDpiForMonitor(HMONITOR hmonitor, MONITOR_DPI_TYPE dpiType, UINT *dpiX,
		UINT *dpiY) {
	auto loader = sprt::DllLoader::get();
	return loader->uxtheme.call<decltype(&GetDpiForMonitor)>(loader->shcore.GetDpiForMonitor,
			hmonitor, dpiType, dpiX, dpiY);
}

// ---- Display Device Functions (user32) ----

WINAPI BOOL EnumDisplayDevicesW(LPCWSTR lpDevice, DWORD iDevNum, PDISPLAY_DEVICEW lpDisplayDevice,
		DWORD dwFlags) {
	auto loader = sprt::DllLoader::get();
	return loader->user32.call<decltype(&EnumDisplayDevicesW)>(loader->user32.EnumDisplayDevicesW,
			lpDevice, iDevNum, lpDisplayDevice, dwFlags);
}

// ---- Device Context Functions (gdi32) ----

WINAPI HDC CreateDCW(LPCWSTR pwszDriver, LPCWSTR pwszDevice, LPCWSTR pszPort, const DEVMODEW *pdm) {
	auto loader = sprt::DllLoader::get();
	return loader->gdi32.call<decltype(&CreateDCW)>(loader->gdi32.CreateDCW, pwszDriver, pwszDevice,
			pszPort, pdm);
}

WINAPI int GetDeviceCaps(HDC hdc, int index) {
	auto loader = sprt::DllLoader::get();
	return loader->gdi32.call<decltype(&GetDeviceCaps)>(loader->gdi32.GetDeviceCaps, hdc, index);
}

WINAPI BOOL DeleteDC(HDC hdc) {
	auto loader = sprt::DllLoader::get();
	return loader->gdi32.call<decltype(&DeleteDC)>(loader->gdi32.DeleteDC, hdc);
}

// ---- Display Monitor Enumeration (user32) ----

WINAPI BOOL EnumDisplayMonitors(HDC hdc, LPCRECT lprcClip, MONITORENUMPROC lpfnEnum,
		LPARAM dwData) {
	auto loader = sprt::DllLoader::get();
	return loader->user32.call<decltype(&EnumDisplayMonitors)>(loader->user32.EnumDisplayMonitors,
			hdc, lprcClip, lpfnEnum, dwData);
}

// ---- Display Settings Functions (user32) ----

WINAPI BOOL EnumDisplaySettingsW(LPCWSTR lpszDeviceName, DWORD iModeNum, DEVMODEW *lpDevMode) {
	auto loader = sprt::DllLoader::get();
	return loader->user32.call<decltype(&EnumDisplaySettingsW)>(loader->user32.EnumDisplaySettingsW,
			lpszDeviceName, iModeNum, lpDevMode);
}

WINAPI LONG ChangeDisplaySettingsExW(LPCWSTR lpszDeviceName, DEVMODEW *lpDevMode, HWND hwnd,
		DWORD dwflags, LPVOID lParam) {
	auto loader = sprt::DllLoader::get();
	return loader->user32.call<decltype(&ChangeDisplaySettingsExW)>(
			loader->user32.ChangeDisplaySettingsExW, lpszDeviceName, lpDevMode, hwnd, dwflags,
			lParam);
}

// ---- DPI Awareness Functions (user32) ----

WINAPI BOOL SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT value) {
	auto loader = sprt::DllLoader::get();
	return loader->user32.call<decltype(&SetProcessDpiAwarenessContext)>(
			loader->user32.SetProcessDpiAwarenessContext, value);
}

WINAPI int GetSystemMetricsForDpi(int nIndex, UINT dpi) {
	auto loader = sprt::DllLoader::get();
	return loader->user32.call<decltype(&GetSystemMetricsForDpi)>(
			loader->user32.GetSystemMetricsForDpi, nIndex, dpi);
}

// ---- SetupAPI Device Functions (setupapi) ----

WINAPI HKEY SetupDiOpenDevRegKey(HDEVINFO DeviceInfoSet, PSP_DEVINFO_DATA DeviceInfoData,
		DWORD Scope, DWORD HwProfile, DWORD KeyType, REGSAM samDesired) {
	auto loader = sprt::DllLoader::get();
	return loader->setupapi.call<decltype(&SetupDiOpenDevRegKey)>(
			loader->setupapi.SetupDiOpenDevRegKey, DeviceInfoSet, DeviceInfoData, Scope, HwProfile,
			KeyType, samDesired);
}

WINAPI HDEVINFO SetupDiGetClassDevsExW(const GUID *ClassGuid, PCWSTR Enumerator, HWND hwndParent,
		DWORD Flags, HDEVINFO DeviceInfoSet, PCWSTR MachineName, PVOID Reserved) {
	auto loader = sprt::DllLoader::get();
	return loader->setupapi.call<decltype(&SetupDiGetClassDevsExW)>(
			loader->setupapi.SetupDiGetClassDevsExW, ClassGuid, Enumerator, hwndParent, Flags,
			DeviceInfoSet, MachineName, Reserved);
}

WINAPI BOOL SetupDiEnumDeviceInterfaces(HDEVINFO DeviceInfoSet, PSP_DEVINFO_DATA DeviceInfoData,
		const GUID *InterfaceClassGuid, DWORD MemberIndex,
		PSP_DEVICE_INTERFACE_DATA DeviceInterfaceData) {
	auto loader = sprt::DllLoader::get();
	return loader->setupapi.call<decltype(&SetupDiEnumDeviceInterfaces)>(
			loader->setupapi.SetupDiEnumDeviceInterfaces, DeviceInfoSet, DeviceInfoData,
			InterfaceClassGuid, MemberIndex, DeviceInterfaceData);
}

WINAPI BOOL SetupDiGetDeviceInterfaceDetailW(HDEVINFO DeviceInfoSet,
		PSP_DEVICE_INTERFACE_DATA DeviceInterfaceData,
		PSP_DEVICE_INTERFACE_DETAIL_DATA_W DeviceInterfaceDetailData,
		DWORD DeviceInterfaceDetailDataSize, PDWORD RequiredSize, PSP_DEVINFO_DATA DeviceInfoData) {
	auto loader = sprt::DllLoader::get();
	return loader->setupapi.call<decltype(&SetupDiGetDeviceInterfaceDetailW)>(
			loader->setupapi.SetupDiGetDeviceInterfaceDetailW, DeviceInfoSet, DeviceInterfaceData,
			DeviceInterfaceDetailData, DeviceInterfaceDetailDataSize, RequiredSize, DeviceInfoData);
}

} // extern "C"
