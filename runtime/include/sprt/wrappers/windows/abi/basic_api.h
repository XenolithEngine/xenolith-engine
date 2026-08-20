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

#ifndef SPRT_WRAPPERS_WINDOWS_ABI_BASIC_API_H_
#define SPRT_WRAPPERS_WINDOWS_ABI_BASIC_API_H_

#include <sprt/wrappers/windows/abi/structures.h>
#include <sprt/wrappers/windows/abi/constants.h>

typedef HANDLE HGLOBAL;
typedef HANDLE HLOCAL;

// clang-format off
#define __SPRT_CP_ACP                    0
#define __SPRT_CP_OEMCP                  1
#define __SPRT_CP_MACCP                  2
#define __SPRT_CP_THREAD_ACP             3
#define __SPRT_CP_SYMBOL                 42
#define __SPRT_CP_UTF8                   65001

#define __SPRT_STD_INPUT_HANDLE    ((DWORD)-10)
#define __SPRT_STD_OUTPUT_HANDLE   ((DWORD)-11)
#define __SPRT_STD_ERROR_HANDLE    ((DWORD)-12)

#define __SPRT_ENABLE_PROCESSED_INPUT              0x0001
#define __SPRT_ENABLE_LINE_INPUT                   0x0002
#define __SPRT_ENABLE_ECHO_INPUT                   0x0004
#define __SPRT_ENABLE_WINDOW_INPUT                 0x0008
#define __SPRT_ENABLE_MOUSE_INPUT                  0x0010
#define __SPRT_ENABLE_INSERT_MODE                  0x0020
#define __SPRT_ENABLE_QUICK_EDIT_MODE              0x0040
#define __SPRT_ENABLE_EXTENDED_FLAGS               0x0080
#define __SPRT_ENABLE_AUTO_POSITION                0x0100
#define __SPRT_ENABLE_VIRTUAL_TERMINAL_INPUT       0x0200

#define __SPRT_ENABLE_PROCESSED_OUTPUT             0x0001
#define __SPRT_ENABLE_WRAP_AT_EOL_OUTPUT           0x0002
#define __SPRT_ENABLE_VIRTUAL_TERMINAL_PROCESSING  0x0004
#define __SPRT_DISABLE_NEWLINE_AUTO_RETURN         0x0008
#define __SPRT_ENABLE_LVB_GRID_WORLDWIDE           0x0010

#define __SPRT_FOREGROUND_BLUE                     0x0001
#define __SPRT_FOREGROUND_GREEN                    0x0002
#define __SPRT_FOREGROUND_RED                      0x0004
#define __SPRT_FOREGROUND_INTENSITY                0x0008
#define __SPRT_BACKGROUND_BLUE                     0x0010
#define __SPRT_BACKGROUND_GREEN                    0x0020
#define __SPRT_BACKGROUND_RED                      0x0040
#define __SPRT_BACKGROUND_INTENSITY                0x0080
#define __SPRT_COMMON_LVB_LEADING_BYTE             0x0100
#define __SPRT_COMMON_LVB_TRAILING_BYTE            0x0200
#define __SPRT_COMMON_LVB_GRID_HORIZONTAL          0x0400
#define __SPRT_COMMON_LVB_GRID_LVERTICAL           0x0800
#define __SPRT_COMMON_LVB_GRID_RVERTICAL           0x1000
#define __SPRT_COMMON_LVB_REVERSE_VIDEO            0x4000
#define __SPRT_COMMON_LVB_UNDERSCORE               0x8000

#define __SPRT_THREAD_MODE_BACKGROUND_BEGIN        0x00010000
#define __SPRT_THREAD_MODE_BACKGROUND_END          0x00020000
#define __SPRT_VER_NT_SERVER                       0x0000003
#define __SPRT_JOB_OBJECT_LIMIT_PROCESS_MEMORY     0x00000100
#define __SPRT_LIST_MODULES_64BIT                  0x02

#define __SPRT_HEAP_NO_SERIALIZE               0x00000001
#define __SPRT_HEAP_GROWABLE                   0x00000002
#define __SPRT_HEAP_GENERATE_EXCEPTIONS        0x00000004
#define __SPRT_HEAP_ZERO_MEMORY                0x00000008
#define __SPRT_HEAP_REALLOC_IN_PLACE_ONLY      0x00000010
#define __SPRT_HEAP_TAIL_CHECKING_ENABLED      0x00000020
#define __SPRT_HEAP_FREE_CHECKING_ENABLED      0x00000040
#define __SPRT_HEAP_DISABLE_COALESCE_ON_FREE   0x00000080
#define __SPRT_HEAP_CREATE_ALIGN_16            0x00010000
#define __SPRT_HEAP_CREATE_ENABLE_TRACING      0x00020000
#define __SPRT_HEAP_CREATE_ENABLE_EXECUTE      0x00040000
#define __SPRT_HEAP_MAXIMUM_TAG                0x0FFF
#define __SPRT_HEAP_PSEUDO_TAG_FLAG            0x8000
#define __SPRT_HEAP_TAG_SHIFT                  18
#define __SPRT_HEAP_CREATE_SEGMENT_HEAP        0x00000100
#define __SPRT_HEAP_CREATE_HARDENED            0x00000200

#define __SPRT_MEM_COMMIT                      0x00001000
#define __SPRT_MEM_RESERVE                     0x00002000
#define __SPRT_MEM_REPLACE_PLACEHOLDER         0x00004000
#define __SPRT_MEM_RESERVE_PLACEHOLDER         0x00040000
#define __SPRT_MEM_RESET                       0x00080000
#define __SPRT_MEM_TOP_DOWN                    0x00100000
#define __SPRT_MEM_WRITE_WATCH                 0x00200000
#define __SPRT_MEM_PHYSICAL                    0x00400000
#define __SPRT_MEM_ROTATE                      0x00800000
#define __SPRT_MEM_DIFFERENT_IMAGE_BASE_OK     0x00800000
#define __SPRT_MEM_RESET_UNDO                  0x01000000
#define __SPRT_MEM_LARGE_PAGES                 0x20000000
#define __SPRT_MEM_4MB_PAGES                   0x80000000
#define __SPRT_MEM_64K_PAGES                   (__SPRT_MEM_LARGE_PAGES | __SPRT_MEM_PHYSICAL)
#define __SPRT_MEM_UNMAP_WITH_TRANSIENT_BOOST  0x00000001
#define __SPRT_MEM_COALESCE_PLACEHOLDERS       0x00000001
#define __SPRT_MEM_PRESERVE_PLACEHOLDER        0x00000002
#define __SPRT_MEM_DECOMMIT                    0x00004000
#define __SPRT_MEM_RELEASE                     0x00008000
#define __SPRT_MEM_FREE                        0x00010000

#define __SPRT_PAGE_NOACCESS           0x01
#define __SPRT_PAGE_READONLY           0x02
#define __SPRT_PAGE_READWRITE          0x04
#define __SPRT_PAGE_WRITECOPY          0x08
#define __SPRT_PAGE_EXECUTE            0x10
#define __SPRT_PAGE_EXECUTE_READ       0x20
#define __SPRT_PAGE_EXECUTE_READWRITE  0x40
#define __SPRT_PAGE_EXECUTE_WRITECOPY  0x80
#define __SPRT_PAGE_GUARD             0x100
#define __SPRT_PAGE_NOCACHE           0x200
#define __SPRT_PAGE_WRITECOMBINE      0x400

#define __SPRT_MINCHAR     0x80
#define __SPRT_MAXCHAR     0x7f
#define __SPRT_MINSHORT    0x8000
#define __SPRT_MAXSHORT    0x7fff
#define __SPRT_MINLONG     0x80000000
#define __SPRT_MAXLONG     0x7fffffff
#define __SPRT_MAXBYTE     0xff
#define __SPRT_MAXWORD     0xffff
#define __SPRT_MAXDWORD    0xffffffff
// clang-format on

/* Token information classes */
typedef enum _TOKEN_INFORMATION_CLASS {
	TokenUser = 1,
	TokenGroups,
	TokenPrivileges,
	TokenOwner,
	TokenPrimaryGroup,
	TokenDefaultDacl,
	TokenSource,
	TokenType,
	TokenImpersonationLevel,
	TokenStatistics,
	TokenRestrictedSids,
	TokenSessionId,
	TokenGroupsAndPrivileges,
	TokenSessionReference,
	TokenSandBoxInert,
	TokenAuditPolicy,
	TokenOrigin,
	TokenElevationType,
	TokenLinkedToken,
	TokenElevation,
	TokenHasRestrictions,
	TokenAccessInformation,
	TokenVirtualizationAllowed,
	TokenVirtualizationEnabled,
	TokenIntegrityLevel,
	TokenUIAccess,
	TokenMandatoryPolicy,
	TokenLogonSid,
	TokenIsAppContainer,
	TokenCapabilities,
	TokenAppContainerSid,
	TokenAppContainerNumber,
	TokenUserClaimAttributes,
	TokenDeviceClaimAttributes,
	TokenRestrictedUserClaimAttributes,
	TokenRestrictedDeviceClaimAttributes,
	TokenDeviceGroups,
	TokenRestrictedDeviceGroups,
	TokenSecurityAttributes,
	TokenIsRestricted,
	TokenProcessTrustLevel,
	TokenPrivateNameSpace,
	TokenSingletonAttributes,
	TokenBnoIsolation,
	TokenChildProcessFlags,
	TokenIsLessPrivilegedAppContainer,
	TokenIsSandboxed,
	TokenIsAppSilo,
	TokenLoggingInformation,
	TokenLearningMode,
	MaxTokenInfoClass // MaxTokenInfoClass should always be the last enum
} TOKEN_INFORMATION_CLASS, *PTOKEN_INFORMATION_CLASS;

typedef enum _WINAPI_PROVIDER {
	WinApiProviderMicrosoft,
	WinApiProviderWine,
	WinApiProviderReactOS,
} WINAPI_PROVIDER;

// The global heap, which the clipboard still speaks: SetClipboardData takes ownership of a
// GMEM_MOVEABLE block and the caller must not free it afterwards.
#define __SPRT_GMEM_FIXED    0x0000
#define __SPRT_GMEM_MOVEABLE 0x0002
#define __SPRT_GMEM_ZEROINIT 0x0040

// Predefined clipboard formats. Only the two text ones are named here - everything else this tree
// puts on a clipboard is a MIME type registered by name through RegisterClipboardFormatW.
#define __SPRT_CF_TEXT        1
#define __SPRT_CF_UNICODETEXT 13

#define __SPRT_LHND  (__SPRT_LMEM_MOVEABLE | __SPRT_LMEM_ZEROINIT)
#define __SPRT_LPTR  (__SPRT_LMEM_FIXED | __SPRT_LMEM_ZEROINIT)

typedef struct _CONSOLE_SCREEN_BUFFER_INFO {
	COORD dwSize;
	COORD dwCursorPosition;
	WORD wAttributes;
	SMALL_RECT srWindow;
	COORD dwMaximumWindowSize;
} CONSOLE_SCREEN_BUFFER_INFO, *PCONSOLE_SCREEN_BUFFER_INFO;


#endif // SPRT_WRAPPERS_WINDOWS_ABI_BASIC_API_H_
