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

#include <sprt/wrappers/windows/windows.h>
#include <sprt/wrappers/windows/monitor_api.h>

#include "dllloader.h"

extern "C" {
WINAPI HKEY SetupDiOpenDevRegKey(HDEVINFO DeviceInfoSet, PSP_DEVINFO_DATA DeviceInfoData,
		DWORD Scope, DWORD HwProfile, DWORD KeyType, REGSAM samDesired) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, setupapi, SetupDiOpenDevRegKey, DeviceInfoSet, DeviceInfoData,
			Scope, HwProfile, KeyType, samDesired);
}

WINAPI HDEVINFO SetupDiGetClassDevsExW(const GUID *ClassGuid, PCWSTR Enumerator, HWND hwndParent,
		DWORD Flags, HDEVINFO DeviceInfoSet, PCWSTR MachineName, PVOID Reserved) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, setupapi, SetupDiGetClassDevsExW, ClassGuid, Enumerator,
			hwndParent, Flags, DeviceInfoSet, MachineName, Reserved);
}

WINAPI BOOL SetupDiEnumDeviceInterfaces(HDEVINFO DeviceInfoSet, PSP_DEVINFO_DATA DeviceInfoData,
		const GUID *InterfaceClassGuid, DWORD MemberIndex,
		PSP_DEVICE_INTERFACE_DATA DeviceInterfaceData) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, setupapi, SetupDiEnumDeviceInterfaces, DeviceInfoSet,
			DeviceInfoData, InterfaceClassGuid, MemberIndex, DeviceInterfaceData);
}

WINAPI BOOL SetupDiGetDeviceInterfaceDetailW(HDEVINFO DeviceInfoSet,
		PSP_DEVICE_INTERFACE_DATA DeviceInterfaceData,
		PSP_DEVICE_INTERFACE_DETAIL_DATA_W DeviceInterfaceDetailData,
		DWORD DeviceInterfaceDetailDataSize, PDWORD RequiredSize, PSP_DEVINFO_DATA DeviceInfoData) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, setupapi, SetupDiGetDeviceInterfaceDetailW, DeviceInfoSet,
			DeviceInterfaceData, DeviceInterfaceDetailData, DeviceInterfaceDetailDataSize,
			RequiredSize, DeviceInfoData);
}
}
