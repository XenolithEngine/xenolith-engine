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

#ifndef SPRT_WRAPPERS_WINDOWS_TIME_API_H_
#define SPRT_WRAPPERS_WINDOWS_TIME_API_H_

#include <sprt/wrappers/windows/structures.h>
#include <sprt/wrappers/windows/constants.h>

// clang-format off
#define TIME_ZONE_ID_UNKNOWN  0
#define TIME_ZONE_ID_STANDARD 1
#define TIME_ZONE_ID_DAYLIGHT 2

#define TIME_ZONE_ID_INVALID ((DWORD)0xFFFFFFFF)
// clang-format on

typedef struct _SYSTEMTIME {
	WORD wYear;
	WORD wMonth;
	WORD wDayOfWeek;
	WORD wDay;
	WORD wHour;
	WORD wMinute;
	WORD wSecond;
	WORD wMilliseconds;
} SYSTEMTIME, *PSYSTEMTIME, *LPSYSTEMTIME;

typedef struct _TIME_ZONE_INFORMATION {
	LONG Bias;
	WCHAR StandardName[32];
	SYSTEMTIME StandardDate;
	LONG StandardBias;
	WCHAR DaylightName[32];
	SYSTEMTIME DaylightDate;
	LONG DaylightBias;
} TIME_ZONE_INFORMATION, *PTIME_ZONE_INFORMATION, *LPTIME_ZONE_INFORMATION;

typedef struct _TIME_DYNAMIC_ZONE_INFORMATION {
	LONG Bias;
	WCHAR StandardName[32];
	SYSTEMTIME StandardDate;
	LONG StandardBias;
	WCHAR DaylightName[32];
	SYSTEMTIME DaylightDate;
	LONG DaylightBias;
	WCHAR TimeZoneKeyName[128];
	BOOLEAN DynamicDaylightTimeDisabled;
} DYNAMIC_TIME_ZONE_INFORMATION, *PDYNAMIC_TIME_ZONE_INFORMATION;

__SPRT_BEGIN_DECL

WINAPI DWORD GetDynamicTimeZoneInformation(PDYNAMIC_TIME_ZONE_INFORMATION pTimeZoneInformation);

WINAPI DWORD GetTimeZoneInformation(LPTIME_ZONE_INFORMATION lpTimeZoneInformation);

WINAPI BOOL QueryPerformanceFrequency(LARGE_INTEGER *lpFrequency);

WINAPI BOOL QueryPerformanceCounter(LARGE_INTEGER *lpPerformanceCount);

WINAPI VOID GetSystemTimePreciseAsFileTime(LPFILETIME lpSystemTimeAsFileTime);

WINAPI VOID GetSystemTimeAsFileTime(LPFILETIME lpSystemTimeAsFileTime);

WINAPI ULONGLONG GetTickCount64();

WINAPI BOOL GetProcessTimes(HANDLE hProcess, LPFILETIME lpCreationTime, LPFILETIME lpExitTime,
		LPFILETIME lpKernelTime, LPFILETIME lpUserTime);

WINAPI BOOL GetThreadTimes(HANDLE hThread, LPFILETIME lpCreationTime, LPFILETIME lpExitTime,
		LPFILETIME lpKernelTime, LPFILETIME lpUserTime);

WINAPI BOOL SystemTimeToFileTime(const SYSTEMTIME *lpSystemTime, LPFILETIME lpFileTime);

WINAPI BOOL FileTimeToSystemTime(const FILETIME *lpFileTime, LPSYSTEMTIME lpSystemTime);

WINAPI BOOL SystemTimeToTzSpecificLocalTime(const TIME_ZONE_INFORMATION *lpTimeZoneInformation,
		const SYSTEMTIME *lpUniversalTime, LPSYSTEMTIME lpLocalTime);

WINAPI BOOL GetTimeZoneInformationForYear(USHORT wYear, PDYNAMIC_TIME_ZONE_INFORMATION pdtzi,
		LPTIME_ZONE_INFORMATION ptzi);

/* ---- Multimedia Timer Services (mmsystem.h) ---- */
/**
 * Sets the resolution of the timer interrupt.
 * @param uPeriod Timer period in milliseconds (1-255).
 * @return Error code if an error occurs, otherwise zero.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/mmsystem/nf-mmsystem-timebeginperiod
 */
WINAPI UINT timeBeginPeriod(UINT uPeriod);

/**
 * Restores the resolution of the timer interrupt to its default setting.
 * @param uPeriod Timer period previously set by timeBeginPeriod.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/mmsystem/nf-mmsystem-timeendperiod
 */
WINAPI VOID timeEndPeriod(UINT uPeriod);

__SPRT_END_DECL

#endif // SPRT_WRAPPERS_WINDOWS_TIME_API_H_
