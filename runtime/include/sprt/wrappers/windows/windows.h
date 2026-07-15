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

#ifndef SPRT_WRAPPERS_WINDOWS_WINDOWS_H_
#define SPRT_WRAPPERS_WINDOWS_WINDOWS_H_

#include <sprt/wrappers/windows/complex_types.h>
#include <sprt/wrappers/windows/constants.h>
#include <sprt/wrappers/windows/basic_api.h>
#include <sprt/wrappers/windows/file_api.h>
#include <sprt/wrappers/windows/thread_api.h>
#include <sprt/wrappers/windows/dl_api.h>
#include <sprt/wrappers/windows/message_api.h>
#include <sprt/wrappers/windows/process_api.h>
#include <sprt/wrappers/windows/time_api.h>
#include <sprt/wrappers/windows/context_api.h>
#include <sprt/wrappers/windows/user_api.h>
#include <sprt/wrappers/windows/security_api.h>

#include <sprt/wrappers/windows/winver.h>
#include <sprt/wrappers/windows/tchar.h>

#include <sprt/wrappers/windows/__sprt_threads.h>

#include <sprt/wrappers/windows/abi/windows.h>

/* Clean public names (materialized __SPRT_ values live in abi/windows.h) */
#define WINBASEAPI __SPRT_WINBASEAPI
#define ALL_PROCESSOR_GROUPS __SPRT_ALL_PROCESSOR_GROUPS
#define LOCALE_NAME_MAX_LENGTH __SPRT_LOCALE_NAME_MAX_LENGTH
#define CSTR_LESS_THAN __SPRT_CSTR_LESS_THAN
#define CSTR_EQUAL __SPRT_CSTR_EQUAL
#define CSTR_GREATER_THAN __SPRT_CSTR_GREATER_THAN
#define NORM_IGNORECASE __SPRT_NORM_IGNORECASE
#define NORM_IGNORENONSPACE __SPRT_NORM_IGNORENONSPACE
#define NORM_IGNORESYMBOLS __SPRT_NORM_IGNORESYMBOLS
#define LINGUISTIC_IGNORECASE __SPRT_LINGUISTIC_IGNORECASE
#define LINGUISTIC_IGNOREDIACRITIC __SPRT_LINGUISTIC_IGNOREDIACRITIC
#define NORM_IGNOREKANATYPE __SPRT_NORM_IGNOREKANATYPE
#define NORM_IGNOREWIDTH __SPRT_NORM_IGNOREWIDTH
#define NORM_LINGUISTIC_CASING __SPRT_NORM_LINGUISTIC_CASING
#define MAP_FOLDCZONE __SPRT_MAP_FOLDCZONE
#define MAP_PRECOMPOSED __SPRT_MAP_PRECOMPOSED
#define MAP_COMPOSITE __SPRT_MAP_COMPOSITE
#define MAP_FOLDDIGITS __SPRT_MAP_FOLDDIGITS
#define MAP_EXPAND_LIGATURES __SPRT_MAP_EXPAND_LIGATURES
#define LCMAP_LOWERCASE __SPRT_LCMAP_LOWERCASE
#define LCMAP_UPPERCASE __SPRT_LCMAP_UPPERCASE
#define LCMAP_TITLECASE __SPRT_LCMAP_TITLECASE
#define LCMAP_SORTKEY __SPRT_LCMAP_SORTKEY
#define LCMAP_BYTEREV __SPRT_LCMAP_BYTEREV
#define LCMAP_HIRAGANA __SPRT_LCMAP_HIRAGANA
#define LCMAP_KATAKANA __SPRT_LCMAP_KATAKANA
#define LCMAP_HALFWIDTH __SPRT_LCMAP_HALFWIDTH
#define LCMAP_FULLWIDTH __SPRT_LCMAP_FULLWIDTH
#define LCMAP_LINGUISTIC_CASING __SPRT_LCMAP_LINGUISTIC_CASING
#define LCMAP_SIMPLIFIED_CHINESE __SPRT_LCMAP_SIMPLIFIED_CHINESE
#define LCMAP_TRADITIONAL_CHINESE __SPRT_LCMAP_TRADITIONAL_CHINESE
#define LCMAP_SORTHANDLE __SPRT_LCMAP_SORTHANDLE
#define LCMAP_HASH __SPRT_LCMAP_HASH
#define LOCALE_NAME_USER_DEFAULT __SPRT_LOCALE_NAME_USER_DEFAULT
#define LOCALE_NAME_INVARIANT __SPRT_LOCALE_NAME_INVARIANT
#define LOCALE_NAME_SYSTEM_DEFAULT __SPRT_LOCALE_NAME_SYSTEM_DEFAULT
#define LOCALE_NOUSEROVERRIDE __SPRT_LOCALE_NOUSEROVERRIDE
#define LOCALE_USE_CP_ACP __SPRT_LOCALE_USE_CP_ACP
#define LOCALE_RETURN_NUMBER __SPRT_LOCALE_RETURN_NUMBER
#define LOCALE_RETURN_GENITIVE_NAMES __SPRT_LOCALE_RETURN_GENITIVE_NAMES
#define LOCALE_ALLOW_NEUTRAL_NAMES __SPRT_LOCALE_ALLOW_NEUTRAL_NAMES
#define LOCALE_SLOCALIZEDDISPLAYNAME __SPRT_LOCALE_SLOCALIZEDDISPLAYNAME
#define LOCALE_SENGLISHDISPLAYNAME __SPRT_LOCALE_SENGLISHDISPLAYNAME
#define LOCALE_SNATIVEDISPLAYNAME __SPRT_LOCALE_SNATIVEDISPLAYNAME
#define LOCALE_SLOCALIZEDLANGUAGENAME __SPRT_LOCALE_SLOCALIZEDLANGUAGENAME
#define LOCALE_SENGLISHLANGUAGENAME __SPRT_LOCALE_SENGLISHLANGUAGENAME
#define LOCALE_SNATIVELANGUAGENAME __SPRT_LOCALE_SNATIVELANGUAGENAME
#define LOCALE_SLOCALIZEDCOUNTRYNAME __SPRT_LOCALE_SLOCALIZEDCOUNTRYNAME
#define LOCALE_SENGLISHCOUNTRYNAME __SPRT_LOCALE_SENGLISHCOUNTRYNAME
#define LOCALE_SNATIVECOUNTRYNAME __SPRT_LOCALE_SNATIVECOUNTRYNAME
#define LOCALE_IDIALINGCODE __SPRT_LOCALE_IDIALINGCODE
#define LOCALE_SLIST __SPRT_LOCALE_SLIST
#define LOCALE_IMEASURE __SPRT_LOCALE_IMEASURE
#define LOCALE_SDECIMAL __SPRT_LOCALE_SDECIMAL
#define LOCALE_STHOUSAND __SPRT_LOCALE_STHOUSAND
#define LOCALE_SGROUPING __SPRT_LOCALE_SGROUPING
#define LOCALE_IDIGITS __SPRT_LOCALE_IDIGITS
#define LOCALE_ILZERO __SPRT_LOCALE_ILZERO
#define LOCALE_INEGNUMBER __SPRT_LOCALE_INEGNUMBER
#define LOCALE_SNATIVEDIGITS __SPRT_LOCALE_SNATIVEDIGITS
#define LOCALE_SCURRENCY __SPRT_LOCALE_SCURRENCY
#define LOCALE_SINTLSYMBOL __SPRT_LOCALE_SINTLSYMBOL
#define LOCALE_SMONDECIMALSEP __SPRT_LOCALE_SMONDECIMALSEP
#define LOCALE_SMONTHOUSANDSEP __SPRT_LOCALE_SMONTHOUSANDSEP
#define LOCALE_SMONGROUPING __SPRT_LOCALE_SMONGROUPING
#define LOCALE_ICURRDIGITS __SPRT_LOCALE_ICURRDIGITS
#define LOCALE_ICURRENCY __SPRT_LOCALE_ICURRENCY
#define LOCALE_INEGCURR __SPRT_LOCALE_INEGCURR
#define LOCALE_SSHORTDATE __SPRT_LOCALE_SSHORTDATE
#define LOCALE_SLONGDATE __SPRT_LOCALE_SLONGDATE
#define LOCALE_STIMEFORMAT __SPRT_LOCALE_STIMEFORMAT
#define LOCALE_SAM __SPRT_LOCALE_SAM
#define LOCALE_SPM __SPRT_LOCALE_SPM
#define LOCALE_ICALENDARTYPE __SPRT_LOCALE_ICALENDARTYPE
#define LOCALE_IOPTIONALCALENDAR __SPRT_LOCALE_IOPTIONALCALENDAR
#define LOCALE_IFIRSTDAYOFWEEK __SPRT_LOCALE_IFIRSTDAYOFWEEK
#define LOCALE_IFIRSTWEEKOFYEAR __SPRT_LOCALE_IFIRSTWEEKOFYEAR
#define LOCALE_SDAYNAME1 __SPRT_LOCALE_SDAYNAME1
#define LOCALE_SDAYNAME2 __SPRT_LOCALE_SDAYNAME2
#define LOCALE_SDAYNAME3 __SPRT_LOCALE_SDAYNAME3
#define LOCALE_SDAYNAME4 __SPRT_LOCALE_SDAYNAME4
#define LOCALE_SDAYNAME5 __SPRT_LOCALE_SDAYNAME5
#define LOCALE_SDAYNAME6 __SPRT_LOCALE_SDAYNAME6
#define LOCALE_SDAYNAME7 __SPRT_LOCALE_SDAYNAME7
#define LOCALE_SABBREVDAYNAME1 __SPRT_LOCALE_SABBREVDAYNAME1
#define LOCALE_SABBREVDAYNAME2 __SPRT_LOCALE_SABBREVDAYNAME2
#define LOCALE_SABBREVDAYNAME3 __SPRT_LOCALE_SABBREVDAYNAME3
#define LOCALE_SABBREVDAYNAME4 __SPRT_LOCALE_SABBREVDAYNAME4
#define LOCALE_SABBREVDAYNAME5 __SPRT_LOCALE_SABBREVDAYNAME5
#define LOCALE_SABBREVDAYNAME6 __SPRT_LOCALE_SABBREVDAYNAME6
#define LOCALE_SABBREVDAYNAME7 __SPRT_LOCALE_SABBREVDAYNAME7
#define LOCALE_SMONTHNAME1 __SPRT_LOCALE_SMONTHNAME1
#define LOCALE_SMONTHNAME2 __SPRT_LOCALE_SMONTHNAME2
#define LOCALE_SMONTHNAME3 __SPRT_LOCALE_SMONTHNAME3
#define LOCALE_SMONTHNAME4 __SPRT_LOCALE_SMONTHNAME4
#define LOCALE_SMONTHNAME5 __SPRT_LOCALE_SMONTHNAME5
#define LOCALE_SMONTHNAME6 __SPRT_LOCALE_SMONTHNAME6
#define LOCALE_SMONTHNAME7 __SPRT_LOCALE_SMONTHNAME7
#define LOCALE_SMONTHNAME8 __SPRT_LOCALE_SMONTHNAME8
#define LOCALE_SMONTHNAME9 __SPRT_LOCALE_SMONTHNAME9
#define LOCALE_SMONTHNAME10 __SPRT_LOCALE_SMONTHNAME10
#define LOCALE_SMONTHNAME11 __SPRT_LOCALE_SMONTHNAME11
#define LOCALE_SMONTHNAME12 __SPRT_LOCALE_SMONTHNAME12
#define LOCALE_SMONTHNAME13 __SPRT_LOCALE_SMONTHNAME13
#define LOCALE_SABBREVMONTHNAME1 __SPRT_LOCALE_SABBREVMONTHNAME1
#define LOCALE_SABBREVMONTHNAME2 __SPRT_LOCALE_SABBREVMONTHNAME2
#define LOCALE_SABBREVMONTHNAME3 __SPRT_LOCALE_SABBREVMONTHNAME3
#define LOCALE_SABBREVMONTHNAME4 __SPRT_LOCALE_SABBREVMONTHNAME4
#define LOCALE_SABBREVMONTHNAME5 __SPRT_LOCALE_SABBREVMONTHNAME5
#define LOCALE_SABBREVMONTHNAME6 __SPRT_LOCALE_SABBREVMONTHNAME6
#define LOCALE_SABBREVMONTHNAME7 __SPRT_LOCALE_SABBREVMONTHNAME7
#define LOCALE_SABBREVMONTHNAME8 __SPRT_LOCALE_SABBREVMONTHNAME8
#define LOCALE_SABBREVMONTHNAME9 __SPRT_LOCALE_SABBREVMONTHNAME9
#define LOCALE_SABBREVMONTHNAME10 __SPRT_LOCALE_SABBREVMONTHNAME10
#define LOCALE_SABBREVMONTHNAME11 __SPRT_LOCALE_SABBREVMONTHNAME11
#define LOCALE_SABBREVMONTHNAME12 __SPRT_LOCALE_SABBREVMONTHNAME12
#define LOCALE_SABBREVMONTHNAME13 __SPRT_LOCALE_SABBREVMONTHNAME13
#define LOCALE_SPOSITIVESIGN __SPRT_LOCALE_SPOSITIVESIGN
#define LOCALE_SNEGATIVESIGN __SPRT_LOCALE_SNEGATIVESIGN
#define LOCALE_IPOSSIGNPOSN __SPRT_LOCALE_IPOSSIGNPOSN
#define LOCALE_INEGSIGNPOSN __SPRT_LOCALE_INEGSIGNPOSN
#define LOCALE_IPOSSYMPRECEDES __SPRT_LOCALE_IPOSSYMPRECEDES
#define LOCALE_IPOSSEPBYSPACE __SPRT_LOCALE_IPOSSEPBYSPACE
#define LOCALE_INEGSYMPRECEDES __SPRT_LOCALE_INEGSYMPRECEDES
#define LOCALE_INEGSEPBYSPACE __SPRT_LOCALE_INEGSEPBYSPACE
#define LOCALE_FONTSIGNATURE __SPRT_LOCALE_FONTSIGNATURE
#define LOCALE_SISO639LANGNAME __SPRT_LOCALE_SISO639LANGNAME
#define LOCALE_SISO3166CTRYNAME __SPRT_LOCALE_SISO3166CTRYNAME
#define LOCALE_IPAPERSIZE __SPRT_LOCALE_IPAPERSIZE
#define LOCALE_SENGCURRNAME __SPRT_LOCALE_SENGCURRNAME
#define LOCALE_SNATIVECURRNAME __SPRT_LOCALE_SNATIVECURRNAME
#define LOCALE_SYEARMONTH __SPRT_LOCALE_SYEARMONTH
#define LOCALE_SSORTNAME __SPRT_LOCALE_SSORTNAME
#define LOCALE_IDIGITSUBSTITUTION __SPRT_LOCALE_IDIGITSUBSTITUTION
#define LOCALE_SNAME __SPRT_LOCALE_SNAME
#define LOCALE_SDURATION __SPRT_LOCALE_SDURATION
#define LOCALE_SSHORTESTDAYNAME1 __SPRT_LOCALE_SSHORTESTDAYNAME1
#define LOCALE_SSHORTESTDAYNAME2 __SPRT_LOCALE_SSHORTESTDAYNAME2
#define LOCALE_SSHORTESTDAYNAME3 __SPRT_LOCALE_SSHORTESTDAYNAME3
#define LOCALE_SSHORTESTDAYNAME4 __SPRT_LOCALE_SSHORTESTDAYNAME4
#define LOCALE_SSHORTESTDAYNAME5 __SPRT_LOCALE_SSHORTESTDAYNAME5
#define LOCALE_SSHORTESTDAYNAME6 __SPRT_LOCALE_SSHORTESTDAYNAME6
#define LOCALE_SSHORTESTDAYNAME7 __SPRT_LOCALE_SSHORTESTDAYNAME7
#define LOCALE_SISO639LANGNAME2 __SPRT_LOCALE_SISO639LANGNAME2
#define LOCALE_SISO3166CTRYNAME2 __SPRT_LOCALE_SISO3166CTRYNAME2
#define LOCALE_SNAN __SPRT_LOCALE_SNAN
#define LOCALE_SPOSINFINITY __SPRT_LOCALE_SPOSINFINITY
#define LOCALE_SNEGINFINITY __SPRT_LOCALE_SNEGINFINITY
#define LOCALE_SSCRIPTS __SPRT_LOCALE_SSCRIPTS
#define LOCALE_SPARENT __SPRT_LOCALE_SPARENT
#define LOCALE_SCONSOLEFALLBACKNAME __SPRT_LOCALE_SCONSOLEFALLBACKNAME
#define LOCALE_IREADINGLAYOUT __SPRT_LOCALE_IREADINGLAYOUT
#define LOCALE_INEUTRAL __SPRT_LOCALE_INEUTRAL
#define LOCALE_INEGATIVEPERCENT __SPRT_LOCALE_INEGATIVEPERCENT
#define LOCALE_IPOSITIVEPERCENT __SPRT_LOCALE_IPOSITIVEPERCENT
#define LOCALE_SPERCENT __SPRT_LOCALE_SPERCENT
#define LOCALE_SPERMILLE __SPRT_LOCALE_SPERMILLE
#define LOCALE_SMONTHDAY __SPRT_LOCALE_SMONTHDAY
#define LOCALE_SSHORTTIME __SPRT_LOCALE_SSHORTTIME
#define LOCALE_SOPENTYPELANGUAGETAG __SPRT_LOCALE_SOPENTYPELANGUAGETAG
#define LOCALE_SSORTLOCALE __SPRT_LOCALE_SSORTLOCALE
#define LOCALE_SRELATIVELONGDATE __SPRT_LOCALE_SRELATIVELONGDATE
#define LOCALE_ICONSTRUCTEDLOCALE __SPRT_LOCALE_ICONSTRUCTEDLOCALE
#define LOCALE_SSHORTESTAM __SPRT_LOCALE_SSHORTESTAM
#define LOCALE_SSHORTESTPM __SPRT_LOCALE_SSHORTESTPM
#define LOCALE_IUSEUTF8LEGACYACP __SPRT_LOCALE_IUSEUTF8LEGACYACP
#define LOCALE_IUSEUTF8LEGACYOEMCP __SPRT_LOCALE_IUSEUTF8LEGACYOEMCP
#define LOCALE_IDEFAULTCODEPAGE __SPRT_LOCALE_IDEFAULTCODEPAGE
#define LOCALE_IDEFAULTANSICODEPAGE __SPRT_LOCALE_IDEFAULTANSICODEPAGE
#define LOCALE_IDEFAULTMACCODEPAGE __SPRT_LOCALE_IDEFAULTMACCODEPAGE
#define LOCALE_IDEFAULTEBCDICCODEPAGE __SPRT_LOCALE_IDEFAULTEBCDICCODEPAGE
#define LOCALE_ILANGUAGE __SPRT_LOCALE_ILANGUAGE
#define LOCALE_SABBREVLANGNAME __SPRT_LOCALE_SABBREVLANGNAME
#define LOCALE_SABBREVCTRYNAME __SPRT_LOCALE_SABBREVCTRYNAME
#define LOCALE_IGEOID __SPRT_LOCALE_IGEOID
#define LOCALE_IDEFAULTLANGUAGE __SPRT_LOCALE_IDEFAULTLANGUAGE
#define LOCALE_IDEFAULTCOUNTRY __SPRT_LOCALE_IDEFAULTCOUNTRY
#define LOCALE_IINTLCURRDIGITS __SPRT_LOCALE_IINTLCURRDIGITS
#define LOCALE_SDATE __SPRT_LOCALE_SDATE
#define LOCALE_STIME __SPRT_LOCALE_STIME
#define LOCALE_IDATE __SPRT_LOCALE_IDATE
#define LOCALE_ILDATE __SPRT_LOCALE_ILDATE
#define LOCALE_ITIME __SPRT_LOCALE_ITIME
#define LOCALE_ITIMEMARKPOSN __SPRT_LOCALE_ITIMEMARKPOSN
#define LOCALE_ICENTURY __SPRT_LOCALE_ICENTURY
#define LOCALE_ITLZERO __SPRT_LOCALE_ITLZERO
#define LOCALE_IDAYLZERO __SPRT_LOCALE_IDAYLZERO
#define LOCALE_IMONLZERO __SPRT_LOCALE_IMONLZERO
#define LOCALE_SKEYBOARDSTOINSTALL __SPRT_LOCALE_SKEYBOARDSTOINSTALL
#define LOCALE_SLANGUAGE __SPRT_LOCALE_SLANGUAGE
#define LOCALE_SLANGDISPLAYNAME __SPRT_LOCALE_SLANGDISPLAYNAME
#define LOCALE_SENGLANGUAGE __SPRT_LOCALE_SENGLANGUAGE
#define LOCALE_SNATIVELANGNAME __SPRT_LOCALE_SNATIVELANGNAME
#define LOCALE_SCOUNTRY __SPRT_LOCALE_SCOUNTRY
#define LOCALE_SENGCOUNTRY __SPRT_LOCALE_SENGCOUNTRY
#define LOCALE_SNATIVECTRYNAME __SPRT_LOCALE_SNATIVECTRYNAME
#define LOCALE_ICOUNTRY __SPRT_LOCALE_ICOUNTRY
#define LOCALE_S1159 __SPRT_LOCALE_S1159
#define LOCALE_S2359 __SPRT_LOCALE_S2359
#define CREATE_WAITABLE_TIMER_MANUAL_RESET __SPRT_CREATE_WAITABLE_TIMER_MANUAL_RESET
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION __SPRT_CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define TIMER_QUERY_STATE __SPRT_TIMER_QUERY_STATE
#define TIMER_MODIFY_STATE __SPRT_TIMER_MODIFY_STATE
#define TIMER_ALL_ACCESS __SPRT_TIMER_ALL_ACCESS
#define MB_PRECOMPOSED __SPRT_MB_PRECOMPOSED
#define MB_COMPOSITE __SPRT_MB_COMPOSITE
#define MB_USEGLYPHCHARS __SPRT_MB_USEGLYPHCHARS
#define MB_ERR_INVALID_CHARS __SPRT_MB_ERR_INVALID_CHARS

#define LTP_PC_SMT __SPRT_LTP_PC_SMT

// GetDriveType return values (winbase.h), used by llvm's Path.inc.
#define DRIVE_UNKNOWN __SPRT_DRIVE_UNKNOWN
#define DRIVE_NO_ROOT_DIR __SPRT_DRIVE_NO_ROOT_DIR
#define DRIVE_REMOVABLE __SPRT_DRIVE_REMOVABLE
#define DRIVE_FIXED __SPRT_DRIVE_FIXED
#define DRIVE_REMOTE __SPRT_DRIVE_REMOTE
#define DRIVE_CDROM __SPRT_DRIVE_CDROM
#define DRIVE_RAMDISK __SPRT_DRIVE_RAMDISK

// GetFinalPathNameByHandle flags (winbase.h).
#define VOLUME_NAME_DOS __SPRT_VOLUME_NAME_DOS
#define VOLUME_NAME_GUID __SPRT_VOLUME_NAME_GUID
#define VOLUME_NAME_NT __SPRT_VOLUME_NAME_NT
#define VOLUME_NAME_NONE __SPRT_VOLUME_NAME_NONE

#define FIND_FIRST_EX_CASE_SENSITIVE __SPRT_FIND_FIRST_EX_CASE_SENSITIVE
#define FIND_FIRST_EX_LARGE_FETCH __SPRT_FIND_FIRST_EX_LARGE_FETCH
#define FIND_FIRST_EX_ON_DISK_ENTRIES_ONLY __SPRT_FIND_FIRST_EX_ON_DISK_ENTRIES_ONLY


/* ============================================================ */
/* WinAPI Function Declarations                                 */
/* Based on Microsoft documentation: https://docs.microsoft.com/ */
/* ============================================================ */

__SPRT_BEGIN_DECL

/**
 * Waits for the specified object to be signaled or for the time-out interval to elapse.
 * @param hHandle Handle to the object.
 * @param dwMilliseconds Time-out interval in milliseconds (INFINITE = wait forever).
 * @return One of WAIT_OBJECT_0, WAIT_ABANDONED, WAIT_TIMEOUT, or WAIT_FAILED.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitsingleobject
 */
__SPRT_WIN_IMPORT WINAPI DWORD WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds);

__SPRT_WIN_IMPORT WINAPI DWORD WaitForSingleObjectEx(HANDLE hHandle, DWORD dwMilliseconds,
		BOOL bAlertable);

/**
 * Opens the access token associated with a process.
 * @param ProcessHandle Handle to a process object.
 * @param DesiredAccess Access mode for the token.
 * @param TokenHandle Pointer to receive opened token handle.
 * @return TRUE on success, FALSE on failure.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/securitybaseapi/nf-securitybaseapi-openprocesstoken
 */
__SPRT_WIN_IMPORT WINAPI BOOL OpenProcessToken(HANDLE ProcessHandle, DWORD DesiredAccess,
		PHANDLE TokenHandle);

/**
 * Retrieves specified information about a security token.
 * @param TokenHandle Handle to a token object.
 * @param TokenInformationClass Type of information to retrieve.
 * @param TokenInformation Buffer for returned information.
 * @param TokenInformationLength Size of the buffer in bytes.
 * @param ReturnLength Pointer to receive actual bytes returned.
 * @return TRUE on success, FALSE on failure.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/securitybaseapi/nf-securitybaseapi-gettokeninformation
 */
__SPRT_WIN_IMPORT WINAPI BOOL GetTokenInformation(HANDLE TokenHandle,
		TOKEN_INFORMATION_CLASS TokenInformationClass, LPVOID TokenInformation,
		DWORD TokenInformationLength, PDWORD ReturnLength);

SPRT_FORCEINLINE HANDLE GetCurrentProcessToken(VOID) { return (HANDLE)(LONG_PTR)-4; }

SPRT_FORCEINLINE HANDLE GetCurrentThreadToken(VOID) { return (HANDLE)(LONG_PTR)-5; }

SPRT_FORCEINLINE HANDLE GetCurrentThreadEffectiveToken(VOID) { return (HANDLE)(LONG_PTR)-6; }

/* ---- Process Snapshot (tlhelp32.h) ---- */
/**
 * Retrieves information about the first process encountered in a system snapshot.
 * @param hSnapshot Handle to the snapshot returned by CreateToolhelp32Snapshot.
 * @param lpEntry Pointer to PROCESSENTRY32 structure.
 * @return TRUE on success, FALSE on failure.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/tlhelp32/nf-tlhelp32-process32firstw
 */
__SPRT_WIN_IMPORT WINAPI BOOL Process32FirstW(HANDLE hSnapshot, LPPROCESSENTRY32W lpEntry);

__SPRT_WIN_IMPORT WINAPI BOOL Process32NextW(HANDLE hSnapshot, LPPROCESSENTRY32W lppe);

/* ---- User Environment (winbase.h) ---- */
/**
 * Retrieves the user name associated with the current thread of execution.
 * @param lpBuffer Buffer to receive the user name.
 * @param lpnSize On input, size of buffer in characters; on output, characters written.
 * @return TRUE on success, FALSE on failure.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-getusernamew
 */
__SPRT_WIN_IMPORT WINAPI BOOL GetUserNameW(LPWSTR lpBuffer, LPDWORD lpnSize);

/* ---- Memory Management (memoryapi.h) ---- */

/* ---- Timer Services (winbase.h) ---- */
/**
 * Creates a waitable timer object.
 * @param lpAttributes Security attributes (NULL = default).
 * @param bManualReset TRUE for manual reset, FALSE for automatic reset.
 * @param lpTimerName Name of the timer object.
 * @return Handle to the new timer object, or NULL on failure.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-creatwaitabletimerw
 */
__SPRT_WIN_IMPORT WINAPI HANDLE CreateWaitableTimerW(LPSECURITY_ATTRIBUTES lpAttributes,
		BOOL bManualReset, LPCWSTR lpTimerName);

__SPRT_WIN_IMPORT WINAPI HANDLE CreateWaitableTimerExW(LPSECURITY_ATTRIBUTES lpTimerAttributes,
		LPCWSTR lpTimerName, DWORD dwFlags, DWORD dwDesiredAccess);

/**
 * Sets a waitable timers state.
 * @param hTimer Handle to the timer object.
 * @param ftDueTime Time at which to signal the timer (negative = relative).
 * @param dwPeriod Period between signals in milliseconds (0 = single-shot).
 * @param lpCompletionRoutine Completion routine (NULL = no callback).
 * @param lpArgument Argument for completion routine.
 * @param fResume TRUE to resume system from low-power state.
 * @return TRUE on success, FALSE on failure.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-setwaitabletimer
 */
__SPRT_WIN_IMPORT WINAPI BOOL SetWaitableTimer(HANDLE hTimer, const LARGE_INTEGER *ftDueTime,
		LONG dwPeriod, PTIMERAPCROUTINE lpCompletionRoutine, LPVOID lpArgument, BOOL fResume);

__SPRT_WIN_IMPORT WINAPI BOOL SetWaitableTimerEx(HANDLE hTimer, const LARGE_INTEGER *lpDueTime,
		LONG lPeriod, PTIMERAPCROUTINE pfnCompletionRoutine, LPVOID lpArgToCompletionRoutine,
		PREASON_CONTEXT WakeContext, ULONG TolerableDelay);

__SPRT_WIN_IMPORT WINAPI BOOL CancelWaitableTimer(HANDLE hTimer);

/* ---- PSAPI (psapi.h) ---- */
/**
 * Retrieves a list of module handles for the specified process.
 * @param hProcess Handle to the process.
 * @param lphModule Array to receive module handles.
 * @param cb Size of the array in bytes.
 * @param lpcbNeeded Pointer to receive bytes needed.
 * @return TRUE on success, FALSE on failure.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/psapi/nf-psapi-enumprocessmodules
 */
__SPRT_WIN_IMPORT WINAPI BOOL EnumProcessModules(HANDLE hProcess, HMODULE *lphModule, DWORD cb,
		LPDWORD lpcbNeeded);

/**
 * Extended working set query for memory monitoring.
 * @param hProcess Handle to the process.
 * @param pWsInfo Pointer to working set information buffer.
 * @param cb Size of the buffer in bytes.
 * @return TRUE on success, FALSE on failure.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/psapi/nf-psapi-queryworkingsetchain
 */
__SPRT_WIN_IMPORT WINAPI BOOL QueryWorkingSetEx(HANDLE hProcess, PVOID pWsInfo, DWORD cb);

/* ============================================================ */
/* Registry API (winreg.h)                                      */
/* ============================================================ */

/**
 * Opens the specified registry key. If the key does not exist, it is created.
 * @param hKey Handle to an open key, or one of the following predefined values:
 *             HKEY_CLASSES_ROOT, HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE, etc.
 * @param lpSubKey Null-terminated string specifying the name of the subkey to open.
 * @param Reserved Reserved; must be zero.
 * @param samDesired Access mode for the key.
 * @param phkResult Pointer to a handle that identifies the opened key.
 * @return ERROR_SUCCESS if successful, or a Win32 error code otherwise.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/winreg/nf-winreg-regopenkeyexw
 */
__SPRT_WIN_IMPORT WINAPI LONG RegOpenKeyExW(HKEY hKey, LPCWSTR lpSubKey, DWORD Reserved,
		DWORD samDesired, PHANDLE phkResult);

__SPRT_WIN_IMPORT WINAPI LONG RegOpenKeyExA(HKEY hKey, LPCSTR lpSubKey, DWORD ulOptions,
		DWORD samDesired, PHANDLE phkResult);

__SPRT_WIN_IMPORT WINAPI LSTATUS RegQueryValueExW(HKEY hKey, LPCWSTR lpValueName,
		LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData);

__SPRT_WIN_IMPORT WINAPI LSTATUS RegQueryValueExA(HKEY hKey, LPCSTR lpValueName, LPDWORD lpReserved,
		LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData);

__SPRT_WIN_IMPORT WINAPI LSTATUS RegEnumKeyExA(HKEY hKey, DWORD dwIndex, LPSTR lpName,
		LPDWORD lpcchName, LPDWORD lpReserved, LPSTR lpClass, LPDWORD lpcchClass,
		PFILETIME lpftLastWriteTime);

__SPRT_WIN_IMPORT WINAPI LSTATUS RegEnumKeyExW(HKEY hKey, DWORD dwIndex, LPWSTR lpName,
		LPDWORD lpcchName, LPDWORD lpReserved, LPWSTR lpClass, LPDWORD lpcchClass,
		PFILETIME lpftLastWriteTime);

/**
 * Retrieves data from a specified registry value.
 * @param hKey Handle to an open key, or one of the predefined values.
 * @param lpValueName Null-terminated string specifying the name of the registry value.
 * @param dwFlags Flags controlling retrieval (e.g., RRF_RT_REG_SZ).
 * @param lpType Pointer to a variable that receives the type of data stored.
 * @param lpData Buffer that receives the copied value.
 * @param lpcbData Pointer to a variable specifying the size of the buffer in bytes.
 * @return ERROR_SUCCESS if successful, or a Win32 error code otherwise.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/winreg/nf-winreg-reggetvaluew
 */
__SPRT_WIN_IMPORT WINAPI LSTATUS RegGetValueW(HKEY hkey, LPCWSTR lpSubKey, LPCWSTR lpValue,
		DWORD dwFlags, LPDWORD pdwType, PVOID pvData, LPDWORD pcbData);

__SPRT_WIN_IMPORT WINAPI LSTATUS RegEnumValueW(HKEY hKey, DWORD dwIndex, LPWSTR lpValueName,
		LPDWORD lpcchValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData,
		LPDWORD lpcbData);

/**
 * Closes a registry key handle.
 * @param hKey Handle to the registry key to close.
 * @return ERROR_SUCCESS if successful, or a Win32 error code otherwise.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/winreg/nf-winreg-regclosekey
 */
__SPRT_WIN_IMPORT WINAPI LONG RegCloseKey(HKEY hKey);

__SPRT_WIN_IMPORT WINAPI DWORD ExpandEnvironmentStringsW(LPCWSTR lpSrc, LPWSTR lpDst, DWORD nSize);

/* ============================================================ */
/* System Information API (winbase.h, sysinfoapi.h)             */
/* ============================================================ */

/**
 * Retrieves the name of the local computer.
 * @param lpBuffer Buffer to receive the computer name.
 * @param nSize On input, size of buffer in characters; on output, characters written.
 * @return TRUE on success, FALSE on failure.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-getcomputernamew
 */
__SPRT_WIN_IMPORT WINAPI BOOL GetComputerNameW(LPWSTR lpBuffer, LPDWORD nSize);

/**
 * Retrieves information about the processor architecture and other system details.
 * @param pSystemInfo Pointer to a SYSTEM_INFO structure that receives the information.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-getnativesysteminfo
 */
__SPRT_WIN_IMPORT WINAPI void GetNativeSystemInfo(LPSYSTEM_INFO pSystemInfo);

/* ============================================================ */
/* Memory Functions (memoryapi.h, winbase.h)                    */
/* ============================================================ */

__SPRT_WIN_IMPORT WINAPI void GetSystemInfo(LPSYSTEM_INFO lpSystemInfo);

__SPRT_WIN_IMPORT WINAPI DWORD GetActiveProcessorCount(WORD GroupNumber);

__SPRT_WIN_IMPORT WINAPI BOOL GetLogicalProcessorInformation(
		PSYSTEM_LOGICAL_PROCESSOR_INFORMATION Buffer, PDWORD ReturnedLength);

__SPRT_WIN_IMPORT WINAPI BOOL SetFileTime(HANDLE hFile, const FILETIME *lpCreationTime,
		const FILETIME *lpLastAccessTime, const FILETIME *lpLastWriteTime);

__SPRT_WIN_IMPORT WINAPI HANDLE FindFirstFileExW(LPCWSTR lpFileName,
		FINDEX_INFO_LEVELS fInfoLevelId, LPVOID lpFindFileData, FINDEX_SEARCH_OPS fSearchOp,
		LPVOID lpSearchFilter, DWORD dwAdditionalFlags);

__SPRT_WIN_IMPORT WINAPI BOOL GetLogicalProcessorInformationEx(
		LOGICAL_PROCESSOR_RELATIONSHIP RelationshipType,
		PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX Buffer, PDWORD ReturnedLength);

__SPRT_WIN_IMPORT WINAPI BOOL GetProcessGroupAffinity(HANDLE hProcess, PUSHORT GroupCount,
		PUSHORT GroupArray);

__SPRT_WIN_IMPORT WINAPI BOOL FlushInstructionCache(HANDLE hProcess, LPCVOID lpBaseAddress,
		SIZE_T dwSize);

__SPRT_WIN_IMPORT WINAPI SIZE_T GetLargePageMinimum(VOID);

__SPRT_WIN_IMPORT WINAPI DWORD GetLongPathNameW(LPCWSTR lpszShortPath, LPWSTR lpszLongPath,
		DWORD cchBuffer);

__SPRT_WIN_IMPORT WINAPI HANDLE CreateJobObjectW(LPSECURITY_ATTRIBUTES lpJobAttributes,
		LPCWSTR lpName);

__SPRT_WIN_IMPORT WINAPI BOOL AssignProcessToJobObject(HANDLE hJob, HANDLE hProcess);

__SPRT_WIN_IMPORT WINAPI BOOL SetInformationJobObject(HANDLE hJob,
		JOBOBJECTINFOCLASS JobObjectInformationClass, LPVOID lpJobObjectInformation,
		DWORD cbJobObjectInformationLength);

__SPRT_WIN_IMPORT WINAPI DWORD GetThreadId(HANDLE Thread);

__SPRT_WIN_IMPORT WINAPI BOOL GetProcessAffinityMask(HANDLE hProcess,
		PDWORD_PTR lpProcessAffinityMask, PDWORD_PTR lpSystemAffinityMask);

__SPRT_WIN_IMPORT WINAPI BOOL SetProcessAffinityMask(HANDLE hProcess,
		DWORD_PTR dwProcessAffinityMask);

__SPRT_WIN_IMPORT WINAPI BOOL SetThreadGroupAffinity(HANDLE hThread,
		const GROUP_AFFINITY *GroupAffinity, PGROUP_AFFINITY PreviousGroupAffinity);

__SPRT_WIN_IMPORT WINAPI BOOL GetThreadGroupAffinity(HANDLE hThread, PGROUP_AFFINITY GroupAffinity);

__SPRT_WIN_IMPORT WINAPI DWORD SearchPathW(LPCWSTR lpPath, LPCWSTR lpFileName, LPCWSTR lpExtension,
		DWORD nBufferLength, LPWSTR lpBuffer, LPWSTR *lpFilePart);

__SPRT_WIN_IMPORT WINAPI BOOL GlobalMemoryStatusEx(LPMEMORYSTATUSEX lpBuffer);

__SPRT_WIN_IMPORT WINAPI BOOL GetComputerNameExW(COMPUTER_NAME_FORMAT NameType, LPWSTR lpBuffer,
		LPDWORD nSize);

__SPRT_WIN_IMPORT WINAPI UINT SetErrorMode(UINT uMode);

__SPRT_WIN_IMPORT WINAPI UINT GetErrorMode(void);

__SPRT_WIN_IMPORT WINAPI HANDLE CreateMemoryResourceNotification(
		MEMORY_RESOURCE_NOTIFICATION_TYPE NotificationType);

__SPRT_WIN_IMPORT WINAPI BOOL QueryMemoryResourceNotification(HANDLE ResourceNotificationHandle,
		BOOL *ResourceState);

__SPRT_WIN_IMPORT WINAPI HANDLE CreateIoCompletionPort(HANDLE FileHandle,
		HANDLE ExistingCompletionPort, ULONG_PTR CompletionKey, DWORD NumberOfConcurrentThreads);

__SPRT_WIN_IMPORT WINAPI BOOL GetQueuedCompletionStatusEx(HANDLE CompletionPort,
		LPOVERLAPPED_ENTRY lpCompletionPortEntries, ULONG ulCount, PULONG ulNumEntriesRemoved,
		DWORD dwMilliseconds, BOOL fAlertable);

__SPRT_WIN_IMPORT WINAPI BOOL PostQueuedCompletionStatus(HANDLE CompletionPort,
		DWORD dwNumberOfBytesTransferred, ULONG_PTR dwCompletionKey, LPOVERLAPPED lpOverlapped);

__SPRT_WIN_IMPORT WINAPI NTSTATUS NtClose(HANDLE Handle);

__SPRT_WIN_IMPORT WINAPI int LCMapStringEx(LPCWSTR lpLocaleName, DWORD dwMapFlags, LPCWSTR lpSrcStr,
		int cchSrc, LPWSTR lpDestStr, int cchDest, LPNLSVERSIONINFO lpVersionInformation,
		LPVOID lpReserved, LPARAM sortHandle);

__SPRT_WIN_IMPORT WINAPI int CompareStringEx(LPCWSTR lpLocaleName, DWORD dwCmpFlags,
		LPCWCH lpString1, int cchCount1, LPCWCH lpString2, int cchCount2,
		LPNLSVERSIONINFO lpVersionInformation, LPVOID lpReserved, LPARAM lParam);

__SPRT_WIN_IMPORT WINAPI int CompareStringOrdinal(LPCWCH lpString1, int cchCount1, LPCWCH lpString2,
		int cchCount2, BOOL bIgnoreCase);

__SPRT_WIN_IMPORT WINAPI int MultiByteToWideChar(UINT CodePage, DWORD dwFlags, LPCCH lpMultiByteStr,
		int cbMultiByte, LPWSTR lpWideCharStr, int cchWideChar);

__SPRT_WIN_IMPORT WINAPI int WideCharToMultiByte(UINT CodePage, DWORD dwFlags, LPCWCH lpWideCharStr,
		int cchWideChar, LPSTR lpMultiByteStr, int cbMultiByte, LPCCH lpDefaultChar,
		LPBOOL lpUsedDefaultChar);

__SPRT_WIN_IMPORT WINAPI int GetLocaleInfoEx(LPCWSTR lpLocaleName, LCTYPE LCType, LPWSTR lpLCData,
		int cchData);

__SPRT_WIN_IMPORT WINAPI BOOL IsValidLocaleName(LPCWSTR lpLocaleName);

__SPRT_WIN_IMPORT WINAPI int GetUserDefaultLocaleName(LPWSTR lpLocaleName, int cchLocaleName);

__SPRT_WIN_IMPORT WINAPI int IdnToAscii(DWORD dwFlags, LPCWSTR lpUnicodeCharStr, int cchUnicodeChar,
		LPWSTR lpASCIICharStr, int cchASCIIChar);

__SPRT_WIN_IMPORT WINAPI int IdnToUnicode(DWORD dwFlags, LPCWSTR lpASCIICharStr, int cchASCIIChar,
		LPWSTR lpUnicodeCharStr, int cchUnicodeChar);

// Extended IOCP API from Windows 11, not supported on Wine for now

__SPRT_WIN_IMPORT WINAPI UINT GetACP(void);

__SPRT_WIN_IMPORT WINAPI BOOL ReadConsoleA(HANDLE hConsoleInput, LPVOID lpBuffer,
		DWORD nNumberOfCharsToRead, LPDWORD lpNumberOfCharsRead,
		PCONSOLE_READCONSOLE_CONTROL pInputControl);

__SPRT_WIN_IMPORT WINAPI BOOL ReadConsoleW(HANDLE hConsoleInput, LPVOID lpBuffer,
		DWORD nNumberOfCharsToRead, LPDWORD lpNumberOfCharsRead,
		PCONSOLE_READCONSOLE_CONTROL pInputControl);

__SPRT_WIN_IMPORT WINAPI UINT GetSystemDirectoryA(LPSTR lpBuffer, UINT uSize);

__SPRT_WIN_IMPORT WINAPI UINT GetSystemDirectoryW(LPWSTR lpBuffer, UINT uSize);

__SPRT_WIN_IMPORT WINAPI UINT GetWindowsDirectoryA(LPSTR lpBuffer, UINT uSize);

__SPRT_WIN_IMPORT WINAPI UINT GetWindowsDirectoryW(LPWSTR lpBuffer, UINT uSize);

__SPRT_WIN_IMPORT WINAPI LPWCH GetEnvironmentStringsW(void);

__SPRT_WIN_IMPORT WINAPI BOOL FreeEnvironmentStringsW(LPWCH penv);

__SPRT_WIN_IMPORT WINAPI BOOL ReadProcessMemory(HANDLE hProcess, LPCVOID lpBaseAddress,
		LPVOID lpBuffer, SIZE_T nSize, SIZE_T *lpNumberOfBytesRead);

__SPRT_WIN_IMPORT WINAPI BOOL WriteProcessMemory(HANDLE hProcess, LPVOID lpBaseAddress,
		LPCVOID lpBuffer, SIZE_T nSize, SIZE_T *lpNumberOfBytesWritten);

__SPRT_WIN_IMPORT WINAPI DWORD GetLogicalDriveStringsW(DWORD nBufferLength, LPWSTR lpBuffer);

__SPRT_WIN_IMPORT WINAPI DWORD QueryDosDeviceW(LPCWSTR lpDeviceName, LPWSTR lpTargetPath,
		DWORD ucchMax);

// RIP_INFO / hard-error severity levels ([debugapi] dwType).
#ifndef SLE_ERROR
#define SLE_ERROR __SPRT_SLE_ERROR
#define SLE_MINORERROR __SPRT_SLE_MINORERROR
#define SLE_WARNING __SPRT_SLE_WARNING
#endif

__SPRT_WIN_IMPORT WINAPI DWORD WaitForMultipleObjects(DWORD nCount, const HANDLE *lpHandles,
		BOOL bWaitAll, DWORD dwMilliseconds);

__SPRT_WIN_IMPORT WINAPI BOOL PeekNamedPipe(HANDLE hNamedPipe, LPVOID lpBuffer, DWORD nBufferSize,
		LPDWORD lpBytesRead, LPDWORD lpTotalBytesAvail, LPDWORD lpBytesLeftThisMessage);


/*
	Extended IOCP API for async event processing
*/

WINAPI NTSTATUS NtCreateWaitCompletionPacket(PHANDLE WaitCompletionPacketHandle,
		ACCESS_MASK DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes);

WINAPI NTSTATUS NtAssociateWaitCompletionPacket(HANDLE WaitCompletionPacketHandle,
		HANDLE IoCompletionHandle, HANDLE TargetObjectHandle, PVOID KeyContext, PVOID ApcContext,
		NTSTATUS IoStatus, ULONG_PTR IoStatusInformation, PBOOLEAN AlreadySignaled);

WINAPI NTSTATUS NtCancelWaitCompletionPacket(HANDLE WaitCompletionPacketHandle,
		BOOLEAN RemoveSignaledPacket);

/*
	Checks whether the functions required for fully asynchronous packet processing
	are available at runtime:
	- NtCreateWaitCompletionPacket
	- NtAssociateWaitCompletionPacket
	- NtCancelWaitCompletionPacket

	If this routines is not available, next APIs WILL crush:
	- __sprt_RestartEventCompletion2
	- __sprt_RestartEventCompletion
	- __sprt_CancelEventCompletion
	- __sprt_ReportEventAsCompletion
*/
WINAPI BOOL NtCompletionPacketAvailable();

WINAPI int __sprt_RestartEventCompletion2(void *hPacket, void *hIOCP, void *hEvent,
		DWORD dwNumberOfBytesTransferred, UINT_PTR dwCompletionKey, void *lpOverlapped);

WINAPI int __sprt_RestartEventCompletion(void *hPacket, void *hIOCP, void *hEvent,
		const void **ncompletion);

WINAPI int __sprt_CancelEventCompletion(void *hPacket, int cancel);

WINAPI void *__sprt_ReportEventAsCompletion(void *hIOCP, void *hEvent,
		DWORD dwNumberOfBytesTransferred, UINT_PTR dwCompletionKey, void *lpOverlapped);

#ifdef UNICODE
#define ReadConsole  ReadConsoleW
#define GetSystemDirectory  GetSystemDirectoryW
#else
#define ReadConsole  ReadConsoleA
#define GetSystemDirectory  GetSystemDirectoryA
#endif // !UNICODE

__SPRT_END_DECL

#endif // SPRT_WRAPPERS_WINDOWS_WINDOWS_H_
