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

/* ============================================================ */
/* WinAPI Function Declarations                                 */
/* Based on Microsoft documentation: https://docs.microsoft.com/ */
/* ============================================================ */

// clang-format off
#define ALL_PROCESSOR_GROUPS        0xffff

#define LOCALE_NAME_MAX_LENGTH   85

#define CSTR_LESS_THAN            1           // string 1 less than string 2
#define CSTR_EQUAL                2           // string 1 equal to string 2
#define CSTR_GREATER_THAN         3           // string 1 greater than string 2

#define NORM_IGNORECASE           0x00000001  // ignore case
#define NORM_IGNORENONSPACE       0x00000002  // ignore nonspacing chars
#define NORM_IGNORESYMBOLS        0x00000004  // ignore symbols

#define LINGUISTIC_IGNORECASE     0x00000010  // linguistically appropriate 'ignore case'
#define LINGUISTIC_IGNOREDIACRITIC 0x00000020  // linguistically appropriate 'ignore nonspace'

#define NORM_IGNOREKANATYPE       0x00010000  // ignore kanatype
#define NORM_IGNOREWIDTH          0x00020000  // ignore width
#define NORM_LINGUISTIC_CASING    0x08000000  // use linguistic rules for casing

#define MAP_FOLDCZONE             0x00000010  // fold compatibility zone chars
#define MAP_PRECOMPOSED           0x00000020  // convert to precomposed chars
#define MAP_COMPOSITE             0x00000040  // convert to composite chars
#define MAP_FOLDDIGITS            0x00000080  // all digits to ASCII 0-9
#define MAP_EXPAND_LIGATURES      0x00002000  // expand all ligatures

#define LCMAP_LOWERCASE           0x00000100  // lower case letters
#define LCMAP_UPPERCASE           0x00000200  // UPPER CASE LETTERS
#define LCMAP_TITLECASE           0x00000300  // Title Case Letters
#define LCMAP_SORTKEY             0x00000400  // WC sort key (normalize)
#define LCMAP_BYTEREV             0x00000800  // byte reversal
#define LCMAP_HIRAGANA            0x00100000  // map katakana to hiragana
#define LCMAP_KATAKANA            0x00200000  // map hiragana to katakana
#define LCMAP_HALFWIDTH           0x00400000  // map double byte to single byte
#define LCMAP_FULLWIDTH           0x00800000  // map single byte to double byte
#define LCMAP_LINGUISTIC_CASING   0x01000000  // use linguistic rules for casing
#define LCMAP_SIMPLIFIED_CHINESE  0x02000000  // map traditional chinese to simplified chinese
#define LCMAP_TRADITIONAL_CHINESE 0x04000000  // map simplified chinese to traditional chinese

#define LCMAP_SORTHANDLE   0x20000000
#define LCMAP_HASH         0x00040000

#define LOCALE_NAME_USER_DEFAULT            nullptr
#define LOCALE_NAME_INVARIANT               L""
#define LOCALE_NAME_SYSTEM_DEFAULT          L"!x-sys-default-locale"

#define LOCALE_NOUSEROVERRIDE         0x80000000   // Not Recommended - do not use user overrides
#define LOCALE_USE_CP_ACP             0x40000000   // DEPRECATED, call Unicode APIs instead: use the system ACP
#define LOCALE_RETURN_NUMBER          0x20000000   // return number instead of string
#define LOCALE_RETURN_GENITIVE_NAMES  0x10000000   //Flag to return the Genitive forms of month names
#define LOCALE_ALLOW_NEUTRAL_NAMES    0x08000000   //Flag to allow returning neutral names/lcids for name conversion
#define LOCALE_SLOCALIZEDDISPLAYNAME  0x00000002   // localized name of locale, eg "German (Germany)" in UI language
#define LOCALE_SENGLISHDISPLAYNAME    0x00000072   // Display name (language + country/region usually) in English, eg "German (Germany)"
#define LOCALE_SNATIVEDISPLAYNAME     0x00000073   // Display name in native locale language, eg "Deutsch (Deutschland)
#define LOCALE_SLOCALIZEDLANGUAGENAME 0x0000006f   // Language Display Name for a language, eg "German" in UI language
#define LOCALE_SENGLISHLANGUAGENAME   0x00001001   // English name of language, eg "German"
#define LOCALE_SNATIVELANGUAGENAME    0x00000004   // native name of language, eg "Deutsch"
#define LOCALE_SLOCALIZEDCOUNTRYNAME  0x00000006   // localized name of country/region, eg "Germany" in UI language
#define LOCALE_SENGLISHCOUNTRYNAME    0x00001002   // English name of country/region, eg "Germany"
#define LOCALE_SNATIVECOUNTRYNAME     0x00000008   // native name of country/region, eg "Deutschland"
#define LOCALE_IDIALINGCODE           0x00000005   // country/region dialing code, example: en-US and en-CA return 1.
#define LOCALE_SLIST                  0x0000000C   // list item separator, eg "," for "1,2,3,4"
#define LOCALE_IMEASURE               0x0000000D   // 0 = metric, 1 = US measurement system
#define LOCALE_SDECIMAL               0x0000000E   // decimal separator, eg "." for 1,234.00
#define LOCALE_STHOUSAND              0x0000000F   // thousand separator, eg "," for 1,234.00
#define LOCALE_SGROUPING              0x00000010   // digit grouping, eg "3;0" for 1,000,000
#define LOCALE_IDIGITS                0x00000011   // number of fractional digits eg 2 for 1.00
#define LOCALE_ILZERO                 0x00000012   // leading zeros for decimal, 0 for .97, 1 for 0.97
#define LOCALE_INEGNUMBER             0x00001010   // negative number mode, 0-4, see documentation
#define LOCALE_SNATIVEDIGITS          0x00000013   // native digits for 0-9, eg "0123456789"
#define LOCALE_SCURRENCY              0x00000014   // local monetary symbol, eg "$"
#define LOCALE_SINTLSYMBOL            0x00000015   // intl monetary symbol, eg "USD"
#define LOCALE_SMONDECIMALSEP         0x00000016   // monetary decimal separator, eg "." for $1,234.00
#define LOCALE_SMONTHOUSANDSEP        0x00000017   // monetary thousand separator, eg "," for $1,234.00
#define LOCALE_SMONGROUPING           0x00000018   // monetary grouping, eg "3;0" for $1,000,000.00
#define LOCALE_ICURRDIGITS            0x00000019   // # local monetary digits, eg 2 for $1.00
#define LOCALE_ICURRENCY              0x0000001B   // positive currency mode, 0-3, see documentation
#define LOCALE_INEGCURR               0x0000001C   // negative currency mode, 0-15, see documentation
#define LOCALE_SSHORTDATE             0x0000001F   // short date format string, eg "MM/dd/yyyy"
#define LOCALE_SLONGDATE              0x00000020   // long date format string, eg "dddd, MMMM dd, yyyy"
#define LOCALE_STIMEFORMAT            0x00001003   // time format string, eg "HH:mm:ss"
#define LOCALE_SAM                    0x00000028   // AM designator, eg "AM"
#define LOCALE_SPM                    0x00000029   // PM designator, eg "PM"
#define LOCALE_ICALENDARTYPE          0x00001009   // type of calendar specifier, eg CAL_GREGORIAN
#define LOCALE_IOPTIONALCALENDAR      0x0000100B   // additional calendar types specifier, eg CAL_GREGORIAN_US
#define LOCALE_IFIRSTDAYOFWEEK        0x0000100C   // first day of week specifier, 0-6, 0=Monday, 6=Sunday
#define LOCALE_IFIRSTWEEKOFYEAR       0x0000100D   // first week of year specifier, 0-2, see documentation
#define LOCALE_SDAYNAME1              0x0000002A   // long name for Monday
#define LOCALE_SDAYNAME2              0x0000002B   // long name for Tuesday
#define LOCALE_SDAYNAME3              0x0000002C   // long name for Wednesday
#define LOCALE_SDAYNAME4              0x0000002D   // long name for Thursday
#define LOCALE_SDAYNAME5              0x0000002E   // long name for Friday
#define LOCALE_SDAYNAME6              0x0000002F   // long name for Saturday
#define LOCALE_SDAYNAME7              0x00000030   // long name for Sunday
#define LOCALE_SABBREVDAYNAME1        0x00000031   // abbreviated name for Monday
#define LOCALE_SABBREVDAYNAME2        0x00000032   // abbreviated name for Tuesday
#define LOCALE_SABBREVDAYNAME3        0x00000033   // abbreviated name for Wednesday
#define LOCALE_SABBREVDAYNAME4        0x00000034   // abbreviated name for Thursday
#define LOCALE_SABBREVDAYNAME5        0x00000035   // abbreviated name for Friday
#define LOCALE_SABBREVDAYNAME6        0x00000036   // abbreviated name for Saturday
#define LOCALE_SABBREVDAYNAME7        0x00000037   // abbreviated name for Sunday
#define LOCALE_SMONTHNAME1            0x00000038   // long name for January
#define LOCALE_SMONTHNAME2            0x00000039   // long name for February
#define LOCALE_SMONTHNAME3            0x0000003A   // long name for March
#define LOCALE_SMONTHNAME4            0x0000003B   // long name for April
#define LOCALE_SMONTHNAME5            0x0000003C   // long name for May
#define LOCALE_SMONTHNAME6            0x0000003D   // long name for June
#define LOCALE_SMONTHNAME7            0x0000003E   // long name for July
#define LOCALE_SMONTHNAME8            0x0000003F   // long name for August
#define LOCALE_SMONTHNAME9            0x00000040   // long name for September
#define LOCALE_SMONTHNAME10           0x00000041   // long name for October
#define LOCALE_SMONTHNAME11           0x00000042   // long name for November
#define LOCALE_SMONTHNAME12           0x00000043   // long name for December
#define LOCALE_SMONTHNAME13           0x0000100E   // long name for 13th month (if exists)
#define LOCALE_SABBREVMONTHNAME1      0x00000044   // abbreviated name for January
#define LOCALE_SABBREVMONTHNAME2      0x00000045   // abbreviated name for February
#define LOCALE_SABBREVMONTHNAME3      0x00000046   // abbreviated name for March
#define LOCALE_SABBREVMONTHNAME4      0x00000047   // abbreviated name for April
#define LOCALE_SABBREVMONTHNAME5      0x00000048   // abbreviated name for May
#define LOCALE_SABBREVMONTHNAME6      0x00000049   // abbreviated name for June
#define LOCALE_SABBREVMONTHNAME7      0x0000004A   // abbreviated name for July
#define LOCALE_SABBREVMONTHNAME8      0x0000004B   // abbreviated name for August
#define LOCALE_SABBREVMONTHNAME9      0x0000004C   // abbreviated name for September
#define LOCALE_SABBREVMONTHNAME10     0x0000004D   // abbreviated name for October
#define LOCALE_SABBREVMONTHNAME11     0x0000004E   // abbreviated name for November
#define LOCALE_SABBREVMONTHNAME12     0x0000004F   // abbreviated name for December
#define LOCALE_SABBREVMONTHNAME13     0x0000100F   // abbreviated name for 13th month (if exists)
#define LOCALE_SPOSITIVESIGN          0x00000050   // positive sign, eg ""
#define LOCALE_SNEGATIVESIGN          0x00000051   // negative sign, eg "-"
#define LOCALE_IPOSSIGNPOSN           0x00000052   // positive sign position (derived from INEGCURR)
#define LOCALE_INEGSIGNPOSN           0x00000053   // negative sign position (derived from INEGCURR)
#define LOCALE_IPOSSYMPRECEDES        0x00000054   // mon sym precedes pos amt (derived from ICURRENCY)
#define LOCALE_IPOSSEPBYSPACE         0x00000055   // mon sym sep by space from pos amt (derived from ICURRENCY)
#define LOCALE_INEGSYMPRECEDES        0x00000056   // mon sym precedes neg amt (derived from INEGCURR)
#define LOCALE_INEGSEPBYSPACE         0x00000057   // mon sym sep by space from neg amt (derived from INEGCURR)
#define LOCALE_FONTSIGNATURE          0x00000058   // font signature
#define LOCALE_SISO639LANGNAME        0x00000059   // ISO abbreviated language name, eg "en"
#define LOCALE_SISO3166CTRYNAME       0x0000005A   // ISO abbreviated country/region name, eg "US"
#define LOCALE_IPAPERSIZE             0x0000100A   // 1 = letter, 5 = legal, 8 = a3, 9 = a4
#define LOCALE_SENGCURRNAME           0x00001007   // english name of currency, eg "Euro"
#define LOCALE_SNATIVECURRNAME        0x00001008   // native name of currency, eg "euro"
#define LOCALE_SYEARMONTH             0x00001006   // year month format string, eg "MM/yyyy"
#define LOCALE_SSORTNAME              0x00001013   // sort name, usually "", eg "Dictionary" in UI Language
#define LOCALE_IDIGITSUBSTITUTION     0x00001014   // 0 = context, 1 = none, 2 = national
#define LOCALE_SNAME                  0x0000005c   // locale name (ie: en-us)
#define LOCALE_SDURATION              0x0000005d   // time duration format, eg "hh:mm:ss"
#define LOCALE_SSHORTESTDAYNAME1      0x00000060   // Shortest day name for Monday
#define LOCALE_SSHORTESTDAYNAME2      0x00000061   // Shortest day name for Tuesday
#define LOCALE_SSHORTESTDAYNAME3      0x00000062   // Shortest day name for Wednesday
#define LOCALE_SSHORTESTDAYNAME4      0x00000063   // Shortest day name for Thursday
#define LOCALE_SSHORTESTDAYNAME5      0x00000064   // Shortest day name for Friday
#define LOCALE_SSHORTESTDAYNAME6      0x00000065   // Shortest day name for Saturday
#define LOCALE_SSHORTESTDAYNAME7      0x00000066   // Shortest day name for Sunday
#define LOCALE_SISO639LANGNAME2       0x00000067   // 3 character ISO abbreviated language name, eg "eng"
#define LOCALE_SISO3166CTRYNAME2      0x00000068   // 3 character ISO country/region name, eg "USA"
#define LOCALE_SNAN                   0x00000069   // Not a Number, eg "NaN"
#define LOCALE_SPOSINFINITY           0x0000006a   // + Infinity, eg "infinity"
#define LOCALE_SNEGINFINITY           0x0000006b   // - Infinity, eg "-infinity"
#define LOCALE_SSCRIPTS               0x0000006c   // Typical scripts in the locale: ; delimited script codes, eg "Latn;"
#define LOCALE_SPARENT                0x0000006d   // Fallback name for resources, eg "en" for "en-US"
#define LOCALE_SCONSOLEFALLBACKNAME   0x0000006e   // Fallback name for within the console for Unicode Only locales, eg "en" for bn-IN
#define LOCALE_IREADINGLAYOUT         0x00000070   // Returns one of the following 4 reading layout values:
                                                   // 0 - Left to right (eg en-US)
                                                   // 1 - Right to left (eg arabic locales)
                                                   // 2 - Vertical top to bottom with columns to the left and also left to right (ja-JP locales)
                                                   // 3 - Vertical top to bottom with columns proceeding to the right
#define LOCALE_INEUTRAL               0x00000071   // Returns 0 for specific cultures, 1 for neutral cultures.
#define LOCALE_INEGATIVEPERCENT       0x00000074   // Returns 0-11 for the negative percent format
#define LOCALE_IPOSITIVEPERCENT       0x00000075   // Returns 0-3 for the positive percent formatIPOSITIVEPERCENT
#define LOCALE_SPERCENT               0x00000076   // Returns the percent symbol
#define LOCALE_SPERMILLE              0x00000077   // Returns the permille (U+2030) symbol
#define LOCALE_SMONTHDAY              0x00000078   // Returns the preferred month/day format
#define LOCALE_SSHORTTIME             0x00000079   // Returns the preferred short time format (ie: no seconds, just h:mm)
#define LOCALE_SOPENTYPELANGUAGETAG   0x0000007a   // Open type language tag, eg: "latn" or "dflt"
#define LOCALE_SSORTLOCALE            0x0000007b   // Name of locale to use for sorting/collation/casing behavior.
#define LOCALE_SRELATIVELONGDATE      0x0000007c   // Long date without year, day of week, month, date, eg: for lock screen
#define LOCALE_ICONSTRUCTEDLOCALE     0x0000007d   // Flags if this locale is constructed.  Avoid using.
#define LOCALE_SSHORTESTAM            0x0000007e   // Shortest AM designator, eg "A"
#define LOCALE_SSHORTESTPM            0x0000007f   // Shortest PM designator, eg "P"
#define LOCALE_IUSEUTF8LEGACYACP     0x00000666   // default ansi code page (use of Unicode is recommended instead)
#define LOCALE_IUSEUTF8LEGACYOEMCP   0x00000999   // default oem code page (use of Unicode is recommended instead)
#define LOCALE_IDEFAULTCODEPAGE       0x0000000B   // default oem code page for locale (user may configure as UTF-8, use of Unicode is recommended instead)
#define LOCALE_IDEFAULTANSICODEPAGE   0x00001004   // default ansi code page for locale (user may configure as UTF-8, use of Unicode is recommended instead)
#define LOCALE_IDEFAULTMACCODEPAGE    0x00001011   // default mac code page for locale (user may configure as UTF-8, use of Unicode is recommended instead)
#define LOCALE_IDEFAULTEBCDICCODEPAGE 0x00001012   // default ebcdic code page for a locale (use of Unicode is recommended instead)
#define LOCALE_ILANGUAGE              0x00000001   // DEPRECATED language id (LCID), LOCALE_SNAME preferred
#define LOCALE_SABBREVLANGNAME        0x00000003   // DEPRECATED arbitrary abbreviated language name, LOCALE_SISO639LANGNAME instead.
#define LOCALE_SABBREVCTRYNAME        0x00000007   // DEPRECATED arbitrary abbreviated country/region name, LOCALE_SISO3166CTRYNAME instead.
#define LOCALE_IGEOID                 0x0000005B   // DEPRECATED geographical location id, use LOCALE_SISO3166CTRYNAME instead.
#define LOCALE_IDEFAULTLANGUAGE       0x00000009   // DEPRECATED default language id, deprecated
#define LOCALE_IDEFAULTCOUNTRY        0x0000000A   // DEPRECATED default country/region code, deprecated
#define LOCALE_IINTLCURRDIGITS        0x0000001A   // DEPRECATED, use LOCALE_ICURRDIGITS # intl monetary digits, eg 2 for $1.00
#define LOCALE_SDATE                  0x0000001D   // DEPRECATED date separator (derived from LOCALE_SSHORTDATE, use that instead)
#define LOCALE_STIME                  0x0000001E   // DEPRECATED time separator (derived from LOCALE_STIMEFORMAT, use that instead)
#define LOCALE_IDATE                  0x00000021   // DEPRECATED short date format ordering (derived from LOCALE_SSHORTDATE, use that instead)
#define LOCALE_ILDATE                 0x00000022   // DEPRECATED long date format ordering (derived from LOCALE_SLONGDATE, use that instead)
#define LOCALE_ITIME                  0x00000023   // DEPRECATED time format specifier (derived from LOCALE_STIMEFORMAT, use that instead)
#define LOCALE_ITIMEMARKPOSN          0x00001005   // DEPRECATED time marker position (derived from LOCALE_STIMEFORMAT, use that instead)
#define LOCALE_ICENTURY               0x00000024   // DEPRECATED century format specifier (short date, LOCALE_SSHORTDATE is preferred)
#define LOCALE_ITLZERO                0x00000025   // DEPRECATED leading zeros in time field (derived from LOCALE_STIMEFORMAT, use that instead)
#define LOCALE_IDAYLZERO              0x00000026   // DEPRECATED leading zeros in day field (short date, LOCALE_SSHORTDATE is preferred)
#define LOCALE_IMONLZERO              0x00000027   // DEPRECATED leading zeros in month field (short date, LOCALE_SSHORTDATE is preferred)
#define LOCALE_SKEYBOARDSTOINSTALL    0x0000005e   // Used internally, see GetKeyboardLayoutName() function
#define LOCALE_SLANGUAGE              LOCALE_SLOCALIZEDDISPLAYNAME   // DEPRECATED as new name is more readable.
#define LOCALE_SLANGDISPLAYNAME       LOCALE_SLOCALIZEDLANGUAGENAME  // DEPRECATED as new name is more readable.
#define LOCALE_SENGLANGUAGE           LOCALE_SENGLISHLANGUAGENAME    // DEPRECATED as new name is more readable.
#define LOCALE_SNATIVELANGNAME        LOCALE_SNATIVELANGUAGENAME     // DEPRECATED as new name is more readable.
#define LOCALE_SCOUNTRY               LOCALE_SLOCALIZEDCOUNTRYNAME   // DEPRECATED as new name is more readable.
#define LOCALE_SENGCOUNTRY            LOCALE_SENGLISHCOUNTRYNAME     // DEPRECATED as new name is more readable.
#define LOCALE_SNATIVECTRYNAME        LOCALE_SNATIVECOUNTRYNAME      // DEPRECATED as new name is more readable.
#define LOCALE_ICOUNTRY               LOCALE_IDIALINGCODE   // Deprecated synonym for LOCALE_IDIALINGCODE
#define LOCALE_S1159                  LOCALE_SAM   // DEPRECATED: Please use LOCALE_SAM, which is more readable.
#define LOCALE_S2359                  LOCALE_SPM   // DEPRECATED: Please use LOCALE_SPM, which is more readable.


// clang-format on

typedef DWORD LCTYPE;

typedef struct _OBJECT_ATTRIBUTES OBJECT_ATTRIBUTES;
typedef OBJECT_ATTRIBUTES *POBJECT_ATTRIBUTES;

typedef struct _SYSTEM_INFO {
	union {
		DWORD dwOemId; // Obsolete field...do not use
		struct {
			WORD wProcessorArchitecture;
			WORD wReserved;
		};
	};
	DWORD dwPageSize;
	LPVOID lpMinimumApplicationAddress;
	LPVOID lpMaximumApplicationAddress;
	DWORD_PTR dwActiveProcessorMask;
	DWORD dwNumberOfProcessors;
	DWORD dwProcessorType;
	DWORD dwAllocationGranularity;
	WORD wProcessorLevel;
	WORD wProcessorRevision;
} SYSTEM_INFO, *LPSYSTEM_INFO;

typedef enum _LOGICAL_PROCESSOR_RELATIONSHIP {
	RelationProcessorCore,
	RelationNumaNode,
	RelationCache,
	RelationProcessorPackage,
	RelationGroup,
	RelationProcessorDie,
	RelationNumaNodeEx,
	RelationProcessorModule,
	RelationAll = 0xffff
} LOGICAL_PROCESSOR_RELATIONSHIP;

typedef enum _PROCESSOR_CACHE_TYPE {
	CacheUnified,
	CacheInstruction,
	CacheData,
	CacheTrace,
	CacheUnknown
} PROCESSOR_CACHE_TYPE, *PPROCESSOR_CACHE_TYPE;

typedef enum _COMPUTER_NAME_FORMAT {
	ComputerNameNetBIOS,
	ComputerNameDnsHostname,
	ComputerNameDnsDomain,
	ComputerNameDnsFullyQualified,
	ComputerNamePhysicalNetBIOS,
	ComputerNamePhysicalDnsHostname,
	ComputerNamePhysicalDnsDomain,
	ComputerNamePhysicalDnsFullyQualified,
	ComputerNameMax
} COMPUTER_NAME_FORMAT;

typedef struct _CACHE_DESCRIPTOR {
	BYTE Level;
	BYTE Associativity;
	WORD LineSize;
	DWORD Size;
	PROCESSOR_CACHE_TYPE Type;
} CACHE_DESCRIPTOR, *PCACHE_DESCRIPTOR;

typedef struct _SYSTEM_LOGICAL_PROCESSOR_INFORMATION {
	ULONG_PTR ProcessorMask;
	LOGICAL_PROCESSOR_RELATIONSHIP Relationship;
	union {
		struct {
			BYTE Flags;
		} ProcessorCore;
		struct {
			DWORD NodeNumber;
		} NumaNode;
		CACHE_DESCRIPTOR Cache;
		ULONGLONG Reserved[2];
	} DUMMYUNIONNAME;
} SYSTEM_LOGICAL_PROCESSOR_INFORMATION, *PSYSTEM_LOGICAL_PROCESSOR_INFORMATION;

typedef struct _MEMORYSTATUSEX {
	DWORD dwLength;
	DWORD dwMemoryLoad;
	DWORDLONG ullTotalPhys;
	DWORDLONG ullAvailPhys;
	DWORDLONG ullTotalPageFile;
	DWORDLONG ullAvailPageFile;
	DWORDLONG ullTotalVirtual;
	DWORDLONG ullAvailVirtual;
	DWORDLONG ullAvailExtendedVirtual;
} MEMORYSTATUSEX, *LPMEMORYSTATUSEX;

typedef struct _nlsversioninfo {
	DWORD dwNLSVersionInfoSize; // sizeof(NLSVERSIONINFO) == 32 bytes
	DWORD dwNLSVersion;
	DWORD dwDefinedVersion; // Deprecated, use dwNLSVersion instead
	DWORD dwEffectiveId; // Deprecated, use guidCustomVerison instead
	GUID guidCustomVersion; // Explicit sort version
} NLSVERSIONINFO, *LPNLSVERSIONINFO;

typedef struct _REASON_CONTEXT {
	ULONG Version;
	DWORD Flags;
	union {
		struct {
			HMODULE LocalizedReasonModule;
			ULONG LocalizedReasonId;
			ULONG ReasonStringCount;
			LPWSTR *ReasonStrings;

		} Detailed;

		LPWSTR SimpleReasonString;
	} Reason;
} REASON_CONTEXT, *PREASON_CONTEXT;

__SPRT_BEGIN_DECL

/**
 * Waits for the specified object to be signaled or for the time-out interval to elapse.
 * @param hHandle Handle to the object.
 * @param dwMilliseconds Time-out interval in milliseconds (INFINITE = wait forever).
 * @return One of WAIT_OBJECT_0, WAIT_ABANDONED, WAIT_TIMEOUT, or WAIT_FAILED.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitsingleobject
 */
WINAPI DWORD WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds);

/**
 * Duplicates an object handle.
 * @param hSourceProcessHandle Handle to the source process.
 * @param hSourceHandle Handle to duplicate.
 * @param hTargetProcessHandle Handle to target process.
 * @param lpTargetHandle Pointer to receive duplicated handle.
 * @param dwDesiredAccess Desired access for new handle.
 * @param bInheritHandle TRUE if new handle is inheritable.
 * @param dwOptions Duplication options.
 * @return TRUE on success, FALSE on failure.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/handleapi/nf-handleapi-duplicatehandle
 */
WINAPI BOOL DuplicateHandle(HANDLE hSourceProcessHandle, HANDLE hSourceHandle,
		HANDLE hTargetProcessHandle, PHANDLE lpTargetHandle, DWORD dwDesiredAccess,
		BOOL bInheritHandle, DWORD dwOptions);

/**
 * Opens the access token associated with a process.
 * @param ProcessHandle Handle to a process object.
 * @param DesiredAccess Access mode for the token.
 * @param TokenHandle Pointer to receive opened token handle.
 * @return TRUE on success, FALSE on failure.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/securitybaseapi/nf-securitybaseapi-openprocesstoken
 */
WINAPI BOOL OpenProcessToken(HANDLE ProcessHandle, DWORD DesiredAccess, PHANDLE TokenHandle);

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
WINAPI BOOL GetTokenInformation(HANDLE TokenHandle, TOKEN_INFORMATION_CLASS TokenInformationClass,
		LPVOID TokenInformation, DWORD TokenInformationLength, PDWORD ReturnLength);

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
WINAPI BOOL Process32FirstW(HANDLE hSnapshot, LPPROCESSENTRY32W lpEntry);

WINAPI BOOL Process32NextW(HANDLE hSnapshot, LPPROCESSENTRY32W lppe);

/* ---- User Environment (winbase.h) ---- */
/**
 * Retrieves the user name associated with the current thread of execution.
 * @param lpBuffer Buffer to receive the user name.
 * @param lpnSize On input, size of buffer in characters; on output, characters written.
 * @return TRUE on success, FALSE on failure.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-getusernamew
 */
WINAPI BOOL GetUserNameW(LPWSTR lpBuffer, LPDWORD lpnSize);

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
WINAPI HANDLE CreateWaitableTimerW(LPSECURITY_ATTRIBUTES lpAttributes, BOOL bManualReset,
		LPCWSTR lpTimerName);

WINAPI HANDLE CreateWaitableTimerExW(LPSECURITY_ATTRIBUTES lpTimerAttributes, LPCWSTR lpTimerName,
		DWORD dwFlags, DWORD dwDesiredAccess);

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
WINAPI BOOL SetWaitableTimer(HANDLE hTimer, const LARGE_INTEGER *ftDueTime, LONG dwPeriod,
		PTIMERAPCROUTINE lpCompletionRoutine, LPVOID lpArgument, BOOL fResume);

WINAPI BOOL SetWaitableTimerEx(HANDLE hTimer, const LARGE_INTEGER *lpDueTime, LONG lPeriod,
		PTIMERAPCROUTINE pfnCompletionRoutine, LPVOID lpArgToCompletionRoutine,
		PREASON_CONTEXT WakeContext, ULONG TolerableDelay);

WINAPI BOOL CancelWaitableTimer(HANDLE hTimer);

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
WINAPI BOOL EnumProcessModules(HANDLE hProcess, HMODULE *lphModule, DWORD cb, LPDWORD lpcbNeeded);

/**
 * Extended working set query for memory monitoring.
 * @param hProcess Handle to the process.
 * @param pWsInfo Pointer to working set information buffer.
 * @param cb Size of the buffer in bytes.
 * @return TRUE on success, FALSE on failure.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/psapi/nf-psapi-queryworkingsetchain
 */
WINAPI BOOL QueryWorkingSetEx(HANDLE hProcess, PVOID pWsInfo, DWORD cb);

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
WINAPI LONG RegOpenKeyExW(HKEY hKey, LPCWSTR lpSubKey, DWORD Reserved, DWORD samDesired,
		PHANDLE phkResult);

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
WINAPI LSTATUS RegGetValueW(HKEY hkey, LPCWSTR lpSubKey, LPCWSTR lpValue, DWORD dwFlags,
		LPDWORD pdwType, PVOID pvData, LPDWORD pcbData);

WINAPI LSTATUS RegEnumValueW(HKEY hKey, DWORD dwIndex, LPWSTR lpValueName, LPDWORD lpcchValueName,
		LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData);

/**
 * Closes a registry key handle.
 * @param hKey Handle to the registry key to close.
 * @return ERROR_SUCCESS if successful, or a Win32 error code otherwise.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/winreg/nf-winreg-regclosekey
 */
WINAPI LONG RegCloseKey(HKEY hKey);

/* ============================================================ */
/* COM/CoInitialize API (combaseapi.h)                          */
/* ============================================================ */

/**
 * Sets the security levels on a process for incoming calls.
 * @param pSecDesc Pointer to a security descriptor (NULL = default).
 * @param cAuthSvc Security services array (NULL = none).
 * @param asAuthSvc Array of authentication service structures.
 * @param pReserved1 Reserved; must be NULL.
 * @param dwAuthnLevel Authentication level for incoming calls.
 * @param dwImpLevel Impersonation level for incoming calls.
 * @param pAuthList Pointer to an array of authentication identities.
 * @param dwCapabilities Capability flags (e.g., EOAC_NONE).
 * @param pReserved2 Reserved; must be NULL.
 * @return S_OK on success, or an HRESULT error code otherwise.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/combaseapi/nf-combaseapi-coinitializesecurity
 */
WINAPI HRESULT CoInitializeSecurity(PSECURITY_DESCRIPTOR pSecDesc, LONG cAuthSvc,
		SOLE_AUTHENTICATION_SERVICE *asAuthSvc, void *pReserved1, DWORD dwAuthnLevel,
		DWORD dwImpLevel, void *pAuthList, DWORD dwCapabilities, void *pReserved3);

/**
 * Creates an instance of a COM object.
 * @param rclsid Class identifier (CLSID) for the object to create.
 * @param pUnkOuter Pointer to IUnknown interface of aggregated object (NULL if not aggregating).
 * @param dwClsContext Context in which to run the code.
 * @param riid Interface identifier (IID) requested from the new object.
 * @param ppv Address of pointer variable that receives the interface pointer.
 * @return S_OK on success, or an HRESULT error code otherwise.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/combaseapi/nf-combaseapi-ocreateinstance
 */
WINAPI HRESULT CoCreateInstance(REFCLSID rclsid, IUnknown *pUnkOuter, DWORD dwClsContext,
		REFIID riid, LPVOID *ppv);

/**
 * Sets security capabilities on a proxy.
 * @param pProxy Pointer to the proxy to set capabilities on.
 * @param dwAuthnSvc Authentication service to use for the proxy.
 * @param dwAuthzSvc Authorization service to use for the proxy.
 * @param pServerPrincName Server principal name.
 * @param dwAuthnLevel Authentication level for calls through the proxy.
 * @param dwImpLevel Impersonation level for the proxy.
 * @param pAuthIdentity Client identity.
 * @param dwCapabilities Capability flags (e.g., EOAC_NONE).
 * @return S_OK on success, or an HRESULT error code otherwise.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/combaseapi/nf-combaseapi-cosetproxyblanket
 */
WINAPI HRESULT CoSetProxyBlanket(IUnknown *pProxy, DWORD dwAuthnSvc, DWORD dwAuthzSvc,
		LPCWSTR pServerPrincName, DWORD dwAuthnLevel, DWORD dwImpLevel, void *pAuthIdentity,
		DWORD dwCapabilities);

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
WINAPI BOOL GetComputerNameW(LPWSTR lpBuffer, LPDWORD nSize);

/**
 * Retrieves information about the processor architecture and other system details.
 * @param pSystemInfo Pointer to a SYSTEM_INFO structure that receives the information.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-getnativesysteminfo
 */
WINAPI void GetNativeSystemInfo(LPSYSTEM_INFO pSystemInfo);

/* ============================================================ */
/* Memory Functions (memoryapi.h, winbase.h)                    */
/* ============================================================ */

/**
 * Copies a block of memory from one location to another.
 * @param Destination Pointer to the destination block of memory.
 * @param Source Pointer to the source block of memory.
 * @param Length Number of bytes to copy.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-copymemory
 */
WINAPI void CopyMemory(LPVOID Destination, const VOID *Source, SIZE_T Length);

/**
 * Sets a block of memory to a specified value.
 * @param Destination Pointer to the destination block of memory.
 * @param Value The value to set (typically 0 for ZeroMemory).
 * @param Length Number of bytes to set.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-fillmemory
 */
WINAPI void FillMemory(LPVOID Destination, SIZE_T Length, BYTE Value);

WINAPI void GetSystemInfo(LPSYSTEM_INFO lpSystemInfo);

WINAPI DWORD GetActiveProcessorCount(WORD GroupNumber);

WINAPI BOOL GetLogicalProcessorInformation(PSYSTEM_LOGICAL_PROCESSOR_INFORMATION Buffer,
		PDWORD ReturnedLength);

WINAPI BOOL GlobalMemoryStatusEx(LPMEMORYSTATUSEX lpBuffer);

WINAPI BOOL GetComputerNameExW(COMPUTER_NAME_FORMAT NameType, LPWSTR lpBuffer, LPDWORD nSize);

WINAPI UINT SetErrorMode(UINT uMode);

WINAPI HANDLE CreateIoCompletionPort(HANDLE FileHandle, HANDLE ExistingCompletionPort,
		ULONG_PTR CompletionKey, DWORD NumberOfConcurrentThreads);

WINAPI BOOL GetQueuedCompletionStatusEx(HANDLE CompletionPort,
		LPOVERLAPPED_ENTRY lpCompletionPortEntries, ULONG ulCount, PULONG ulNumEntriesRemoved,
		DWORD dwMilliseconds, BOOL fAlertable);

WINAPI BOOL PostQueuedCompletionStatus(HANDLE CompletionPort, DWORD dwNumberOfBytesTransferred,
		ULONG_PTR dwCompletionKey, LPOVERLAPPED lpOverlapped);

WINAPI NTSTATUS NtCreateWaitCompletionPacket(PHANDLE WaitCompletionPacketHandle,
		ACCESS_MASK DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes);

WINAPI NTSTATUS NtAssociateWaitCompletionPacket(HANDLE WaitCompletionPacketHandle,
		HANDLE IoCompletionHandle, HANDLE TargetObjectHandle, PVOID KeyContext, PVOID ApcContext,
		NTSTATUS IoStatus, ULONG_PTR IoStatusInformation, PBOOLEAN AlreadySignaled);

WINAPI NTSTATUS NtCancelWaitCompletionPacket(HANDLE WaitCompletionPacketHandle,
		BOOLEAN RemoveSignaledPacket);

WINAPI NTSTATUS NtClose(HANDLE Handle);

WINAPI int LCMapStringEx(LPCWSTR lpLocaleName, DWORD dwMapFlags, LPCWSTR lpSrcStr, int cchSrc,
		LPWSTR lpDestStr, int cchDest, LPNLSVERSIONINFO lpVersionInformation, LPVOID lpReserved,
		LPARAM sortHandle);

WINAPI int CompareStringEx(LPCWSTR lpLocaleName, DWORD dwCmpFlags, LPCWCH lpString1, int cchCount1,
		LPCWCH lpString2, int cchCount2, LPNLSVERSIONINFO lpVersionInformation, LPVOID lpReserved,
		LPARAM lParam);

WINAPI int GetLocaleInfoEx(LPCWSTR lpLocaleName, LCTYPE LCType, LPWSTR lpLCData, int cchData);

WINAPI BOOL IsValidLocaleName(LPCWSTR lpLocaleName);

WINAPI int GetUserDefaultLocaleName(LPWSTR lpLocaleName, int cchLocaleName);

WINAPI int IdnToAscii(DWORD dwFlags, LPCWSTR lpUnicodeCharStr, int cchUnicodeChar,
		LPWSTR lpASCIICharStr, int cchASCIIChar);

WINAPI int IdnToUnicode(DWORD dwFlags, LPCWSTR lpASCIICharStr, int cchASCIIChar,
		LPWSTR lpUnicodeCharStr, int cchUnicodeChar);

/* RtlZeroMemory is the underlying implementation for ZeroMemory macro */
#define RtlZeroMemory(Destination, Length) \
    FillMemory((Destination), (Length), 0)
/**
 * ZeroMemory - Fills a block of memory with zeros.
 * @param Destination Pointer to the start of the memory block.
 * @param Length Number of bytes to zero.
 */
#define ZeroMemory(Destination, Length) \
    RtlZeroMemory((Destination), (Length))

__SPRT_END_DECL

#endif // SPRT_WRAPPERS_WINDOWS_WINDOWS_H_
