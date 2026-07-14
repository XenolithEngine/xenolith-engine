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

#ifndef SPRT_WRAPPERS_WINDOWS_ABI_WINDOWS_H_
#define SPRT_WRAPPERS_WINDOWS_ABI_WINDOWS_H_


#include <sprt/wrappers/windows/abi/complex_types.h>
#include <sprt/wrappers/windows/abi/constants.h>

// clang-format off
#define __SPRT_WINBASEAPI

#define __SPRT_ALL_PROCESSOR_GROUPS        0xffff

#define __SPRT_LOCALE_NAME_MAX_LENGTH   85

#define __SPRT_CSTR_LESS_THAN            1           // string 1 less than string 2
#define __SPRT_CSTR_EQUAL                2           // string 1 equal to string 2
#define __SPRT_CSTR_GREATER_THAN         3           // string 1 greater than string 2

#define __SPRT_NORM_IGNORECASE           0x00000001  // ignore case
#define __SPRT_NORM_IGNORENONSPACE       0x00000002  // ignore nonspacing chars
#define __SPRT_NORM_IGNORESYMBOLS        0x00000004  // ignore symbols

#define __SPRT_LINGUISTIC_IGNORECASE     0x00000010  // linguistically appropriate 'ignore case'
#define __SPRT_LINGUISTIC_IGNOREDIACRITIC 0x00000020  // linguistically appropriate 'ignore nonspace'

#define __SPRT_NORM_IGNOREKANATYPE       0x00010000  // ignore kanatype
#define __SPRT_NORM_IGNOREWIDTH          0x00020000  // ignore width
#define __SPRT_NORM_LINGUISTIC_CASING    0x08000000  // use linguistic rules for casing

#define __SPRT_MAP_FOLDCZONE             0x00000010  // fold compatibility zone chars
#define __SPRT_MAP_PRECOMPOSED           0x00000020  // convert to precomposed chars
#define __SPRT_MAP_COMPOSITE             0x00000040  // convert to composite chars
#define __SPRT_MAP_FOLDDIGITS            0x00000080  // all digits to ASCII 0-9
#define __SPRT_MAP_EXPAND_LIGATURES      0x00002000  // expand all ligatures

#define __SPRT_LCMAP_LOWERCASE           0x00000100  // lower case letters
#define __SPRT_LCMAP_UPPERCASE           0x00000200  // UPPER CASE LETTERS
#define __SPRT_LCMAP_TITLECASE           0x00000300  // Title Case Letters
#define __SPRT_LCMAP_SORTKEY             0x00000400  // WC sort key (normalize)
#define __SPRT_LCMAP_BYTEREV             0x00000800  // byte reversal
#define __SPRT_LCMAP_HIRAGANA            0x00100000  // map katakana to hiragana
#define __SPRT_LCMAP_KATAKANA            0x00200000  // map hiragana to katakana
#define __SPRT_LCMAP_HALFWIDTH           0x00400000  // map double byte to single byte
#define __SPRT_LCMAP_FULLWIDTH           0x00800000  // map single byte to double byte
#define __SPRT_LCMAP_LINGUISTIC_CASING   0x01000000  // use linguistic rules for casing
#define __SPRT_LCMAP_SIMPLIFIED_CHINESE  0x02000000  // map traditional chinese to simplified chinese
#define __SPRT_LCMAP_TRADITIONAL_CHINESE 0x04000000  // map simplified chinese to traditional chinese

#define __SPRT_LCMAP_SORTHANDLE   0x20000000
#define __SPRT_LCMAP_HASH         0x00040000

#define __SPRT_LOCALE_NAME_USER_DEFAULT            nullptr
#define __SPRT_LOCALE_NAME_INVARIANT               L""
#define __SPRT_LOCALE_NAME_SYSTEM_DEFAULT          L"!x-sys-default-locale"

#define __SPRT_LOCALE_NOUSEROVERRIDE         0x80000000   // Not Recommended - do not use user overrides
#define __SPRT_LOCALE_USE_CP_ACP             0x40000000   // DEPRECATED, call Unicode APIs instead: use the system ACP
#define __SPRT_LOCALE_RETURN_NUMBER          0x20000000   // return number instead of string
#define __SPRT_LOCALE_RETURN_GENITIVE_NAMES  0x10000000   //Flag to return the Genitive forms of month names
#define __SPRT_LOCALE_ALLOW_NEUTRAL_NAMES    0x08000000   //Flag to allow returning neutral names/lcids for name conversion
#define __SPRT_LOCALE_SLOCALIZEDDISPLAYNAME  0x00000002   // localized name of locale, eg "German (Germany)" in UI language
#define __SPRT_LOCALE_SENGLISHDISPLAYNAME    0x00000072   // Display name (language + country/region usually) in English, eg "German (Germany)"
#define __SPRT_LOCALE_SNATIVEDISPLAYNAME     0x00000073   // Display name in native locale language, eg "Deutsch (Deutschland)
#define __SPRT_LOCALE_SLOCALIZEDLANGUAGENAME 0x0000006f   // Language Display Name for a language, eg "German" in UI language
#define __SPRT_LOCALE_SENGLISHLANGUAGENAME   0x00001001   // English name of language, eg "German"
#define __SPRT_LOCALE_SNATIVELANGUAGENAME    0x00000004   // native name of language, eg "Deutsch"
#define __SPRT_LOCALE_SLOCALIZEDCOUNTRYNAME  0x00000006   // localized name of country/region, eg "Germany" in UI language
#define __SPRT_LOCALE_SENGLISHCOUNTRYNAME    0x00001002   // English name of country/region, eg "Germany"
#define __SPRT_LOCALE_SNATIVECOUNTRYNAME     0x00000008   // native name of country/region, eg "Deutschland"
#define __SPRT_LOCALE_IDIALINGCODE           0x00000005   // country/region dialing code, example: en-US and en-CA return 1.
#define __SPRT_LOCALE_SLIST                  0x0000000C   // list item separator, eg "," for "1,2,3,4"
#define __SPRT_LOCALE_IMEASURE               0x0000000D   // 0 = metric, 1 = US measurement system
#define __SPRT_LOCALE_SDECIMAL               0x0000000E   // decimal separator, eg "." for 1,234.00
#define __SPRT_LOCALE_STHOUSAND              0x0000000F   // thousand separator, eg "," for 1,234.00
#define __SPRT_LOCALE_SGROUPING              0x00000010   // digit grouping, eg "3;0" for 1,000,000
#define __SPRT_LOCALE_IDIGITS                0x00000011   // number of fractional digits eg 2 for 1.00
#define __SPRT_LOCALE_ILZERO                 0x00000012   // leading zeros for decimal, 0 for .97, 1 for 0.97
#define __SPRT_LOCALE_INEGNUMBER             0x00001010   // negative number mode, 0-4, see documentation
#define __SPRT_LOCALE_SNATIVEDIGITS          0x00000013   // native digits for 0-9, eg "0123456789"
#define __SPRT_LOCALE_SCURRENCY              0x00000014   // local monetary symbol, eg "$"
#define __SPRT_LOCALE_SINTLSYMBOL            0x00000015   // intl monetary symbol, eg "USD"
#define __SPRT_LOCALE_SMONDECIMALSEP         0x00000016   // monetary decimal separator, eg "." for $1,234.00
#define __SPRT_LOCALE_SMONTHOUSANDSEP        0x00000017   // monetary thousand separator, eg "," for $1,234.00
#define __SPRT_LOCALE_SMONGROUPING           0x00000018   // monetary grouping, eg "3;0" for $1,000,000.00
#define __SPRT_LOCALE_ICURRDIGITS            0x00000019   // # local monetary digits, eg 2 for $1.00
#define __SPRT_LOCALE_ICURRENCY              0x0000001B   // positive currency mode, 0-3, see documentation
#define __SPRT_LOCALE_INEGCURR               0x0000001C   // negative currency mode, 0-15, see documentation
#define __SPRT_LOCALE_SSHORTDATE             0x0000001F   // short date format string, eg "MM/dd/yyyy"
#define __SPRT_LOCALE_SLONGDATE              0x00000020   // long date format string, eg "dddd, MMMM dd, yyyy"
#define __SPRT_LOCALE_STIMEFORMAT            0x00001003   // time format string, eg "HH:mm:ss"
#define __SPRT_LOCALE_SAM                    0x00000028   // AM designator, eg "AM"
#define __SPRT_LOCALE_SPM                    0x00000029   // PM designator, eg "PM"
#define __SPRT_LOCALE_ICALENDARTYPE          0x00001009   // type of calendar specifier, eg CAL_GREGORIAN
#define __SPRT_LOCALE_IOPTIONALCALENDAR      0x0000100B   // additional calendar types specifier, eg CAL_GREGORIAN_US
#define __SPRT_LOCALE_IFIRSTDAYOFWEEK        0x0000100C   // first day of week specifier, 0-6, 0=Monday, 6=Sunday
#define __SPRT_LOCALE_IFIRSTWEEKOFYEAR       0x0000100D   // first week of year specifier, 0-2, see documentation
#define __SPRT_LOCALE_SDAYNAME1              0x0000002A   // long name for Monday
#define __SPRT_LOCALE_SDAYNAME2              0x0000002B   // long name for Tuesday
#define __SPRT_LOCALE_SDAYNAME3              0x0000002C   // long name for Wednesday
#define __SPRT_LOCALE_SDAYNAME4              0x0000002D   // long name for Thursday
#define __SPRT_LOCALE_SDAYNAME5              0x0000002E   // long name for Friday
#define __SPRT_LOCALE_SDAYNAME6              0x0000002F   // long name for Saturday
#define __SPRT_LOCALE_SDAYNAME7              0x00000030   // long name for Sunday
#define __SPRT_LOCALE_SABBREVDAYNAME1        0x00000031   // abbreviated name for Monday
#define __SPRT_LOCALE_SABBREVDAYNAME2        0x00000032   // abbreviated name for Tuesday
#define __SPRT_LOCALE_SABBREVDAYNAME3        0x00000033   // abbreviated name for Wednesday
#define __SPRT_LOCALE_SABBREVDAYNAME4        0x00000034   // abbreviated name for Thursday
#define __SPRT_LOCALE_SABBREVDAYNAME5        0x00000035   // abbreviated name for Friday
#define __SPRT_LOCALE_SABBREVDAYNAME6        0x00000036   // abbreviated name for Saturday
#define __SPRT_LOCALE_SABBREVDAYNAME7        0x00000037   // abbreviated name for Sunday
#define __SPRT_LOCALE_SMONTHNAME1            0x00000038   // long name for January
#define __SPRT_LOCALE_SMONTHNAME2            0x00000039   // long name for February
#define __SPRT_LOCALE_SMONTHNAME3            0x0000003A   // long name for March
#define __SPRT_LOCALE_SMONTHNAME4            0x0000003B   // long name for April
#define __SPRT_LOCALE_SMONTHNAME5            0x0000003C   // long name for May
#define __SPRT_LOCALE_SMONTHNAME6            0x0000003D   // long name for June
#define __SPRT_LOCALE_SMONTHNAME7            0x0000003E   // long name for July
#define __SPRT_LOCALE_SMONTHNAME8            0x0000003F   // long name for August
#define __SPRT_LOCALE_SMONTHNAME9            0x00000040   // long name for September
#define __SPRT_LOCALE_SMONTHNAME10           0x00000041   // long name for October
#define __SPRT_LOCALE_SMONTHNAME11           0x00000042   // long name for November
#define __SPRT_LOCALE_SMONTHNAME12           0x00000043   // long name for December
#define __SPRT_LOCALE_SMONTHNAME13           0x0000100E   // long name for 13th month (if exists)
#define __SPRT_LOCALE_SABBREVMONTHNAME1      0x00000044   // abbreviated name for January
#define __SPRT_LOCALE_SABBREVMONTHNAME2      0x00000045   // abbreviated name for February
#define __SPRT_LOCALE_SABBREVMONTHNAME3      0x00000046   // abbreviated name for March
#define __SPRT_LOCALE_SABBREVMONTHNAME4      0x00000047   // abbreviated name for April
#define __SPRT_LOCALE_SABBREVMONTHNAME5      0x00000048   // abbreviated name for May
#define __SPRT_LOCALE_SABBREVMONTHNAME6      0x00000049   // abbreviated name for June
#define __SPRT_LOCALE_SABBREVMONTHNAME7      0x0000004A   // abbreviated name for July
#define __SPRT_LOCALE_SABBREVMONTHNAME8      0x0000004B   // abbreviated name for August
#define __SPRT_LOCALE_SABBREVMONTHNAME9      0x0000004C   // abbreviated name for September
#define __SPRT_LOCALE_SABBREVMONTHNAME10     0x0000004D   // abbreviated name for October
#define __SPRT_LOCALE_SABBREVMONTHNAME11     0x0000004E   // abbreviated name for November
#define __SPRT_LOCALE_SABBREVMONTHNAME12     0x0000004F   // abbreviated name for December
#define __SPRT_LOCALE_SABBREVMONTHNAME13     0x0000100F   // abbreviated name for 13th month (if exists)
#define __SPRT_LOCALE_SPOSITIVESIGN          0x00000050   // positive sign, eg ""
#define __SPRT_LOCALE_SNEGATIVESIGN          0x00000051   // negative sign, eg "-"
#define __SPRT_LOCALE_IPOSSIGNPOSN           0x00000052   // positive sign position (derived from INEGCURR)
#define __SPRT_LOCALE_INEGSIGNPOSN           0x00000053   // negative sign position (derived from INEGCURR)
#define __SPRT_LOCALE_IPOSSYMPRECEDES        0x00000054   // mon sym precedes pos amt (derived from ICURRENCY)
#define __SPRT_LOCALE_IPOSSEPBYSPACE         0x00000055   // mon sym sep by space from pos amt (derived from ICURRENCY)
#define __SPRT_LOCALE_INEGSYMPRECEDES        0x00000056   // mon sym precedes neg amt (derived from INEGCURR)
#define __SPRT_LOCALE_INEGSEPBYSPACE         0x00000057   // mon sym sep by space from neg amt (derived from INEGCURR)
#define __SPRT_LOCALE_FONTSIGNATURE          0x00000058   // font signature
#define __SPRT_LOCALE_SISO639LANGNAME        0x00000059   // ISO abbreviated language name, eg "en"
#define __SPRT_LOCALE_SISO3166CTRYNAME       0x0000005A   // ISO abbreviated country/region name, eg "US"
#define __SPRT_LOCALE_IPAPERSIZE             0x0000100A   // 1 = letter, 5 = legal, 8 = a3, 9 = a4
#define __SPRT_LOCALE_SENGCURRNAME           0x00001007   // english name of currency, eg "Euro"
#define __SPRT_LOCALE_SNATIVECURRNAME        0x00001008   // native name of currency, eg "euro"
#define __SPRT_LOCALE_SYEARMONTH             0x00001006   // year month format string, eg "MM/yyyy"
#define __SPRT_LOCALE_SSORTNAME              0x00001013   // sort name, usually "", eg "Dictionary" in UI Language
#define __SPRT_LOCALE_IDIGITSUBSTITUTION     0x00001014   // 0 = context, 1 = none, 2 = national
#define __SPRT_LOCALE_SNAME                  0x0000005c   // locale name (ie: en-us)
#define __SPRT_LOCALE_SDURATION              0x0000005d   // time duration format, eg "hh:mm:ss"
#define __SPRT_LOCALE_SSHORTESTDAYNAME1      0x00000060   // Shortest day name for Monday
#define __SPRT_LOCALE_SSHORTESTDAYNAME2      0x00000061   // Shortest day name for Tuesday
#define __SPRT_LOCALE_SSHORTESTDAYNAME3      0x00000062   // Shortest day name for Wednesday
#define __SPRT_LOCALE_SSHORTESTDAYNAME4      0x00000063   // Shortest day name for Thursday
#define __SPRT_LOCALE_SSHORTESTDAYNAME5      0x00000064   // Shortest day name for Friday
#define __SPRT_LOCALE_SSHORTESTDAYNAME6      0x00000065   // Shortest day name for Saturday
#define __SPRT_LOCALE_SSHORTESTDAYNAME7      0x00000066   // Shortest day name for Sunday
#define __SPRT_LOCALE_SISO639LANGNAME2       0x00000067   // 3 character ISO abbreviated language name, eg "eng"
#define __SPRT_LOCALE_SISO3166CTRYNAME2      0x00000068   // 3 character ISO country/region name, eg "USA"
#define __SPRT_LOCALE_SNAN                   0x00000069   // Not a Number, eg "NaN"
#define __SPRT_LOCALE_SPOSINFINITY           0x0000006a   // + Infinity, eg "infinity"
#define __SPRT_LOCALE_SNEGINFINITY           0x0000006b   // - Infinity, eg "-infinity"
#define __SPRT_LOCALE_SSCRIPTS               0x0000006c   // Typical scripts in the locale: ; delimited script codes, eg "Latn;"
#define __SPRT_LOCALE_SPARENT                0x0000006d   // Fallback name for resources, eg "en" for "en-US"
#define __SPRT_LOCALE_SCONSOLEFALLBACKNAME   0x0000006e   // Fallback name for within the console for Unicode Only locales, eg "en" for bn-IN
#define __SPRT_LOCALE_IREADINGLAYOUT         0x00000070   // Returns one of the following 4 reading layout values:
                                                   // 0 - Left to right (eg en-US)
                                                   // 1 - Right to left (eg arabic locales)
                                                   // 2 - Vertical top to bottom with columns to the left and also left to right (ja-JP locales)
                                                   // 3 - Vertical top to bottom with columns proceeding to the right
#define __SPRT_LOCALE_INEUTRAL               0x00000071   // Returns 0 for specific cultures, 1 for neutral cultures.
#define __SPRT_LOCALE_INEGATIVEPERCENT       0x00000074   // Returns 0-11 for the negative percent format
#define __SPRT_LOCALE_IPOSITIVEPERCENT       0x00000075   // Returns 0-3 for the positive percent formatIPOSITIVEPERCENT
#define __SPRT_LOCALE_SPERCENT               0x00000076   // Returns the percent symbol
#define __SPRT_LOCALE_SPERMILLE              0x00000077   // Returns the permille (U+2030) symbol
#define __SPRT_LOCALE_SMONTHDAY              0x00000078   // Returns the preferred month/day format
#define __SPRT_LOCALE_SSHORTTIME             0x00000079   // Returns the preferred short time format (ie: no seconds, just h:mm)
#define __SPRT_LOCALE_SOPENTYPELANGUAGETAG   0x0000007a   // Open type language tag, eg: "latn" or "dflt"
#define __SPRT_LOCALE_SSORTLOCALE            0x0000007b   // Name of locale to use for sorting/collation/casing behavior.
#define __SPRT_LOCALE_SRELATIVELONGDATE      0x0000007c   // Long date without year, day of week, month, date, eg: for lock screen
#define __SPRT_LOCALE_ICONSTRUCTEDLOCALE     0x0000007d   // Flags if this locale is constructed.  Avoid using.
#define __SPRT_LOCALE_SSHORTESTAM            0x0000007e   // Shortest AM designator, eg "A"
#define __SPRT_LOCALE_SSHORTESTPM            0x0000007f   // Shortest PM designator, eg "P"
#define __SPRT_LOCALE_IUSEUTF8LEGACYACP     0x00000666   // default ansi code page (use of Unicode is recommended instead)
#define __SPRT_LOCALE_IUSEUTF8LEGACYOEMCP   0x00000999   // default oem code page (use of Unicode is recommended instead)
#define __SPRT_LOCALE_IDEFAULTCODEPAGE       0x0000000B   // default oem code page for locale (user may configure as UTF-8, use of Unicode is recommended instead)
#define __SPRT_LOCALE_IDEFAULTANSICODEPAGE   0x00001004   // default ansi code page for locale (user may configure as UTF-8, use of Unicode is recommended instead)
#define __SPRT_LOCALE_IDEFAULTMACCODEPAGE    0x00001011   // default mac code page for locale (user may configure as UTF-8, use of Unicode is recommended instead)
#define __SPRT_LOCALE_IDEFAULTEBCDICCODEPAGE 0x00001012   // default ebcdic code page for a locale (use of Unicode is recommended instead)
#define __SPRT_LOCALE_ILANGUAGE              0x00000001   // DEPRECATED language id (LCID), LOCALE_SNAME preferred
#define __SPRT_LOCALE_SABBREVLANGNAME        0x00000003   // DEPRECATED arbitrary abbreviated language name, LOCALE_SISO639LANGNAME instead.
#define __SPRT_LOCALE_SABBREVCTRYNAME        0x00000007   // DEPRECATED arbitrary abbreviated country/region name, LOCALE_SISO3166CTRYNAME instead.
#define __SPRT_LOCALE_IGEOID                 0x0000005B   // DEPRECATED geographical location id, use LOCALE_SISO3166CTRYNAME instead.
#define __SPRT_LOCALE_IDEFAULTLANGUAGE       0x00000009   // DEPRECATED default language id, deprecated
#define __SPRT_LOCALE_IDEFAULTCOUNTRY        0x0000000A   // DEPRECATED default country/region code, deprecated
#define __SPRT_LOCALE_IINTLCURRDIGITS        0x0000001A   // DEPRECATED, use LOCALE_ICURRDIGITS # intl monetary digits, eg 2 for $1.00
#define __SPRT_LOCALE_SDATE                  0x0000001D   // DEPRECATED date separator (derived from LOCALE_SSHORTDATE, use that instead)
#define __SPRT_LOCALE_STIME                  0x0000001E   // DEPRECATED time separator (derived from LOCALE_STIMEFORMAT, use that instead)
#define __SPRT_LOCALE_IDATE                  0x00000021   // DEPRECATED short date format ordering (derived from LOCALE_SSHORTDATE, use that instead)
#define __SPRT_LOCALE_ILDATE                 0x00000022   // DEPRECATED long date format ordering (derived from LOCALE_SLONGDATE, use that instead)
#define __SPRT_LOCALE_ITIME                  0x00000023   // DEPRECATED time format specifier (derived from LOCALE_STIMEFORMAT, use that instead)
#define __SPRT_LOCALE_ITIMEMARKPOSN          0x00001005   // DEPRECATED time marker position (derived from LOCALE_STIMEFORMAT, use that instead)
#define __SPRT_LOCALE_ICENTURY               0x00000024   // DEPRECATED century format specifier (short date, LOCALE_SSHORTDATE is preferred)
#define __SPRT_LOCALE_ITLZERO                0x00000025   // DEPRECATED leading zeros in time field (derived from LOCALE_STIMEFORMAT, use that instead)
#define __SPRT_LOCALE_IDAYLZERO              0x00000026   // DEPRECATED leading zeros in day field (short date, LOCALE_SSHORTDATE is preferred)
#define __SPRT_LOCALE_IMONLZERO              0x00000027   // DEPRECATED leading zeros in month field (short date, LOCALE_SSHORTDATE is preferred)
#define __SPRT_LOCALE_SKEYBOARDSTOINSTALL    0x0000005e   // Used internally, see GetKeyboardLayoutName() function
#define __SPRT_LOCALE_SLANGUAGE              __SPRT_LOCALE_SLOCALIZEDDISPLAYNAME   // DEPRECATED as new name is more readable.
#define __SPRT_LOCALE_SLANGDISPLAYNAME       __SPRT_LOCALE_SLOCALIZEDLANGUAGENAME  // DEPRECATED as new name is more readable.
#define __SPRT_LOCALE_SENGLANGUAGE           __SPRT_LOCALE_SENGLISHLANGUAGENAME    // DEPRECATED as new name is more readable.
#define __SPRT_LOCALE_SNATIVELANGNAME        __SPRT_LOCALE_SNATIVELANGUAGENAME     // DEPRECATED as new name is more readable.
#define __SPRT_LOCALE_SCOUNTRY               __SPRT_LOCALE_SLOCALIZEDCOUNTRYNAME   // DEPRECATED as new name is more readable.
#define __SPRT_LOCALE_SENGCOUNTRY            __SPRT_LOCALE_SENGLISHCOUNTRYNAME     // DEPRECATED as new name is more readable.
#define __SPRT_LOCALE_SNATIVECTRYNAME        __SPRT_LOCALE_SNATIVECOUNTRYNAME      // DEPRECATED as new name is more readable.
#define __SPRT_LOCALE_ICOUNTRY               __SPRT_LOCALE_IDIALINGCODE   // Deprecated synonym for LOCALE_IDIALINGCODE
#define __SPRT_LOCALE_S1159                  __SPRT_LOCALE_SAM   // DEPRECATED: Please use LOCALE_SAM, which is more readable.
#define __SPRT_LOCALE_S2359                  __SPRT_LOCALE_SPM   // DEPRECATED: Please use LOCALE_SPM, which is more readable.

#define __SPRT_CREATE_WAITABLE_TIMER_MANUAL_RESET  0x00000001
#define __SPRT_CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002

#define __SPRT_TIMER_QUERY_STATE       0x0001
#define __SPRT_TIMER_MODIFY_STATE      0x0002

#define __SPRT_TIMER_ALL_ACCESS (__SPRT_STANDARD_RIGHTS_REQUIRED | __SPRT_SYNCHRONIZE | __SPRT_TIMER_QUERY_STATE | __SPRT_TIMER_MODIFY_STATE)

#define __SPRT_MB_PRECOMPOSED            0x00000001  // DEPRECATED: use single precomposed characters when possible.
#define __SPRT_MB_COMPOSITE              0x00000002  // DEPRECATED: use multiple discrete characters when possible.
#define __SPRT_MB_USEGLYPHCHARS          0x00000004  // DEPRECATED: use glyph chars, not ctrl chars
#define __SPRT_MB_ERR_INVALID_CHARS      0x00000008  // error for invalid chars

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

typedef struct _CONSOLE_READCONSOLE_CONTROL {
	ULONG nLength;
	ULONG nInitialChars;
	ULONG dwCtrlWakeupMask;
	ULONG dwControlKeyState;
} CONSOLE_READCONSOLE_CONTROL, *PCONSOLE_READCONSOLE_CONTROL;


#endif // SPRT_WRAPPERS_WINDOWS_ABI_WINDOWS_H_
