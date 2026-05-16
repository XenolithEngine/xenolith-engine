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

#include <sprt/wrappers/windows/time_api.h>

#include "dllloader.h"

extern "C" {

WINAPI DWORD GetDynamicTimeZoneInformation(PDYNAMIC_TIME_ZONE_INFORMATION pTimeZoneInformation) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&GetDynamicTimeZoneInformation)>(
			loader->kernel32.GetDynamicTimeZoneInformation, pTimeZoneInformation);
}

// ---- Time Zone Functions (kernel32) ----

WINAPI DWORD GetTimeZoneInformation(LPTIME_ZONE_INFORMATION lpTimeZoneInformation) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&GetTimeZoneInformation)>(
			loader->kernel32.GetTimeZoneInformation, lpTimeZoneInformation);
}

// ---- Performance Counter Functions (kernel32) ----

WINAPI BOOL QueryPerformanceFrequency(LARGE_INTEGER *lpFrequency) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&QueryPerformanceFrequency)>(
			loader->kernel32.QueryPerformanceFrequency, lpFrequency);
}

WINAPI BOOL QueryPerformanceCounter(LARGE_INTEGER *lpPerformanceCount) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&QueryPerformanceCounter)>(
			loader->kernel32.QueryPerformanceCounter, lpPerformanceCount);
}

// ---- High-Precision System Time Functions (kernel32) ----

WINAPI VOID GetSystemTimePreciseAsFileTime(LPFILETIME lpSystemTimeAsFileTime) {
	auto loader = sprt::DllLoader::get();
	loader->kernel32.call<decltype(&GetSystemTimePreciseAsFileTime)>(
			loader->kernel32.GetSystemTimePreciseAsFileTime, lpSystemTimeAsFileTime);
}

// ---- System Time Functions (kernel32) ----

WINAPI VOID GetSystemTimeAsFileTime(LPFILETIME lpSystemTimeAsFileTime) {
	auto loader = sprt::DllLoader::get();
	loader->kernel32.call<decltype(&GetSystemTimeAsFileTime)>(
			loader->kernel32.GetSystemTimeAsFileTime, lpSystemTimeAsFileTime);
}

// ---- Tick Count Functions (kernel32) ----

WINAPI ULONGLONG GetTickCount64() {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&GetTickCount64)>(loader->kernel32.GetTickCount64);
}

// ---- Process and Thread Timing Functions (kernel32) ----

WINAPI BOOL GetProcessTimes(HANDLE hProcess, LPFILETIME lpCreationTime, LPFILETIME lpExitTime,
		LPFILETIME lpKernelTime, LPFILETIME lpUserTime) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&GetProcessTimes)>(loader->kernel32.GetProcessTimes,
			hProcess, lpCreationTime, lpExitTime, lpKernelTime, lpUserTime);
}

WINAPI BOOL GetThreadTimes(HANDLE hThread, LPFILETIME lpCreationTime, LPFILETIME lpExitTime,
		LPFILETIME lpKernelTime, LPFILETIME lpUserTime) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&GetThreadTimes)>(loader->kernel32.GetThreadTimes,
			hThread, lpCreationTime, lpExitTime, lpKernelTime, lpUserTime);
}

WINAPI BOOL SystemTimeToFileTime(const SYSTEMTIME *lpSystemTime, LPFILETIME lpFileTime) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&SystemTimeToFileTime)>(
			loader->kernel32.SystemTimeToFileTime, lpSystemTime, lpFileTime);
}

WINAPI BOOL FileTimeToSystemTime(const FILETIME *lpFileTime, LPSYSTEMTIME lpSystemTime) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&FileTimeToSystemTime)>(
			loader->kernel32.FileTimeToSystemTime, lpFileTime, lpSystemTime);
}

WINAPI BOOL SystemTimeToTzSpecificLocalTime(const TIME_ZONE_INFORMATION *lpTimeZoneInformation,
		const SYSTEMTIME *lpUniversalTime, LPSYSTEMTIME lpLocalTime) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&SystemTimeToTzSpecificLocalTime)>(
			loader->kernel32.SystemTimeToTzSpecificLocalTime, lpTimeZoneInformation,
			lpUniversalTime, lpLocalTime);
}

WINAPI BOOL GetTimeZoneInformationForYear(USHORT wYear, PDYNAMIC_TIME_ZONE_INFORMATION pdtzi,
		LPTIME_ZONE_INFORMATION ptzi) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&GetTimeZoneInformationForYear)>(
			loader->kernel32.GetTimeZoneInformationForYear, wYear, pdtzi, ptzi);
}

/* ---- Multimedia Timer Services (winmm.dll / mmsystem.h) ---- */

/**
 * Sets the resolution of the timer interrupt.
 * @param uPeriod Timer period in milliseconds (1-255).
 * @return Error code if an error occurs, otherwise zero.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/mmsystem/nf-mmsystem-timebeginperiod
 */
WINAPI UINT timeBeginPeriod(UINT uPeriod) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&timeBeginPeriod)>(loader->kernel32.timeBeginPeriod,
			uPeriod);
}

/**
 * Restores the resolution of the timer interrupt to its default setting.
 * @param uPeriod Timer period previously set by timeBeginPeriod.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/mmsystem/nf-mmsystem-timeendperiod
 */
WINAPI VOID timeEndPeriod(UINT uPeriod) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&timeEndPeriod)>(loader->kernel32.timeEndPeriod, uPeriod);
}

} // extern "C"
