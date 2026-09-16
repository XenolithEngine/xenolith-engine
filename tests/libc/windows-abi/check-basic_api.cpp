// Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
//
// abi/basic_api.h <-> Windows SDK parity. Compile-time only; see check.sh.

#define SPRT_ABI_HEADER <sprt/wrappers/windows/abi/basic_api.h>
#include "abi_check.h"

#include <windows.h> // CP_*, STD_*_HANDLE, ENABLE_*, HEAP_*, MEM_*, PAGE_*,
// CONSOLE_SCREEN_BUFFER_INFO, TOKEN_INFORMATION_CLASS, ...
#include <psapi.h> // LIST_MODULES_64BIT

// enum-value parity: sprt_abi::<name> vs the SDK's <name> (both real C enumerators).
#define SPRT_ENUM(name) \
	static_assert((long long)(sprt_abi::name) == (long long)(name), "enum " #name " != SDK")

// === code-page constants ===================================================
SPRT_CONST(CP_ACP);
SPRT_CONST(CP_OEMCP);
SPRT_CONST(CP_MACCP);
SPRT_CONST(CP_THREAD_ACP);
SPRT_CONST(CP_SYMBOL);
SPRT_CONST(CP_UTF8);

// === standard handle ids ===================================================
// ((DWORD)-N): unsigned, but castable to integer, so assertable.
SPRT_CONST(STD_INPUT_HANDLE);
SPRT_CONST(STD_OUTPUT_HANDLE);
SPRT_CONST(STD_ERROR_HANDLE);

// === console input/output modes (ENABLE_*/DISABLE_*) =======================
SPRT_CONST(ENABLE_PROCESSED_INPUT);
SPRT_CONST(ENABLE_LINE_INPUT);
SPRT_CONST(ENABLE_ECHO_INPUT);
SPRT_CONST(ENABLE_WINDOW_INPUT);
SPRT_CONST(ENABLE_MOUSE_INPUT);
SPRT_CONST(ENABLE_INSERT_MODE);
SPRT_CONST(ENABLE_QUICK_EDIT_MODE);
SPRT_CONST(ENABLE_EXTENDED_FLAGS);
SPRT_CONST(ENABLE_AUTO_POSITION);
SPRT_CONST(ENABLE_VIRTUAL_TERMINAL_INPUT);
SPRT_CONST(ENABLE_PROCESSED_OUTPUT);
SPRT_CONST(ENABLE_WRAP_AT_EOL_OUTPUT);
SPRT_CONST(ENABLE_VIRTUAL_TERMINAL_PROCESSING);
SPRT_CONST(DISABLE_NEWLINE_AUTO_RETURN);
SPRT_CONST(ENABLE_LVB_GRID_WORLDWIDE);

// === console text attributes (SetConsoleTextAttribute / CHAR_INFO) =========
SPRT_CONST(FOREGROUND_BLUE);
SPRT_CONST(FOREGROUND_GREEN);
SPRT_CONST(FOREGROUND_RED);
SPRT_CONST(FOREGROUND_INTENSITY);
SPRT_CONST(BACKGROUND_BLUE);
SPRT_CONST(BACKGROUND_GREEN);
SPRT_CONST(BACKGROUND_RED);
SPRT_CONST(BACKGROUND_INTENSITY);
SPRT_CONST(COMMON_LVB_LEADING_BYTE);
SPRT_CONST(COMMON_LVB_TRAILING_BYTE);
SPRT_CONST(COMMON_LVB_GRID_HORIZONTAL);
SPRT_CONST(COMMON_LVB_GRID_LVERTICAL);
SPRT_CONST(COMMON_LVB_GRID_RVERTICAL);
SPRT_CONST(COMMON_LVB_REVERSE_VIDEO);
SPRT_CONST(COMMON_LVB_UNDERSCORE);

// === misc thread/version/job/module flags ==================================
SPRT_CONST(THREAD_MODE_BACKGROUND_BEGIN);
SPRT_CONST(THREAD_MODE_BACKGROUND_END);
SPRT_CONST(VER_NT_SERVER);
SPRT_CONST(JOB_OBJECT_LIMIT_PROCESS_MEMORY);
SPRT_CONST(LIST_MODULES_64BIT);

// === heap flags ============================================================
SPRT_CONST(HEAP_NO_SERIALIZE);
SPRT_CONST(HEAP_GROWABLE);
SPRT_CONST(HEAP_GENERATE_EXCEPTIONS);
SPRT_CONST(HEAP_ZERO_MEMORY);
SPRT_CONST(HEAP_REALLOC_IN_PLACE_ONLY);
SPRT_CONST(HEAP_TAIL_CHECKING_ENABLED);
SPRT_CONST(HEAP_FREE_CHECKING_ENABLED);
SPRT_CONST(HEAP_DISABLE_COALESCE_ON_FREE);
SPRT_CONST(HEAP_CREATE_ALIGN_16);
SPRT_CONST(HEAP_CREATE_ENABLE_TRACING);
SPRT_CONST(HEAP_CREATE_ENABLE_EXECUTE);
SPRT_CONST(HEAP_MAXIMUM_TAG);
SPRT_CONST(HEAP_PSEUDO_TAG_FLAG);
SPRT_CONST(HEAP_TAG_SHIFT);
SPRT_CONST(HEAP_CREATE_SEGMENT_HEAP);
SPRT_CONST(HEAP_CREATE_HARDENED);

// === virtual-memory allocation flags (MEM_*) ===============================
SPRT_CONST(MEM_COMMIT);
SPRT_CONST(MEM_RESERVE);
SPRT_CONST(MEM_REPLACE_PLACEHOLDER);
SPRT_CONST(MEM_RESERVE_PLACEHOLDER);
SPRT_CONST(MEM_RESET);
SPRT_CONST(MEM_TOP_DOWN);
SPRT_CONST(MEM_WRITE_WATCH);
SPRT_CONST(MEM_PHYSICAL);
SPRT_CONST(MEM_ROTATE);
SPRT_CONST(MEM_DIFFERENT_IMAGE_BASE_OK);
SPRT_CONST(MEM_RESET_UNDO);
SPRT_CONST(MEM_LARGE_PAGES);
SPRT_CONST(MEM_4MB_PAGES);
SPRT_CONST(MEM_64K_PAGES);
SPRT_CONST(MEM_UNMAP_WITH_TRANSIENT_BOOST);
SPRT_CONST(MEM_COALESCE_PLACEHOLDERS);
SPRT_CONST(MEM_PRESERVE_PLACEHOLDER);
SPRT_CONST(MEM_DECOMMIT);
SPRT_CONST(MEM_RELEASE);
SPRT_CONST(MEM_FREE);

// === memory-protection constants (PAGE_*) ==================================
SPRT_CONST(PAGE_NOACCESS);
SPRT_CONST(PAGE_READONLY);
SPRT_CONST(PAGE_READWRITE);
SPRT_CONST(PAGE_WRITECOPY);
SPRT_CONST(PAGE_EXECUTE);
SPRT_CONST(PAGE_EXECUTE_READ);
SPRT_CONST(PAGE_EXECUTE_READWRITE);
SPRT_CONST(PAGE_EXECUTE_WRITECOPY);
SPRT_CONST(PAGE_GUARD);
SPRT_CONST(PAGE_NOCACHE);
SPRT_CONST(PAGE_WRITECOMBINE);

// === global-heap allocation flags (GMEM_*) =================================
// The clipboard's data model is the global heap: SetClipboardData takes ownership of
// a GMEM_MOVEABLE block. A wrong flag here is not a failed allocation - it is a block
// handed to Windows under terms neither side agreed on.
SPRT_CONST(GMEM_FIXED);
SPRT_CONST(GMEM_MOVEABLE);
SPRT_CONST(GMEM_ZEROINIT);

// === predefined clipboard formats (CF_*) ===================================
// Format ids are the cross-process wire protocol: a peer pastes by asking for the
// number, so a divergence hands it the wrong format rather than failing.
SPRT_CONST(CF_TEXT);
SPRT_CONST(CF_UNICODETEXT);

// === local-memory flag combos =============================================
SPRT_CONST(LHND);
SPRT_CONST(LPTR);

// === CONSOLE_SCREEN_BUFFER_INFO ============================================
SPRT_SIZE(CONSOLE_SCREEN_BUFFER_INFO);
SPRT_OFFSET(CONSOLE_SCREEN_BUFFER_INFO, dwSize);
SPRT_OFFSET(CONSOLE_SCREEN_BUFFER_INFO, dwCursorPosition);
SPRT_OFFSET(CONSOLE_SCREEN_BUFFER_INFO, wAttributes);
SPRT_OFFSET(CONSOLE_SCREEN_BUFFER_INFO, srWindow);
SPRT_OFFSET(CONSOLE_SCREEN_BUFFER_INFO, dwMaximumWindowSize);

// === TOKEN_INFORMATION_CLASS enumerators ===================================
SPRT_ENUM(TokenUser);
SPRT_ENUM(TokenGroups);
SPRT_ENUM(TokenPrivileges);
SPRT_ENUM(TokenOwner);
SPRT_ENUM(TokenPrimaryGroup);
SPRT_ENUM(TokenDefaultDacl);
SPRT_ENUM(TokenSource);
SPRT_ENUM(TokenType);
SPRT_ENUM(TokenImpersonationLevel);
SPRT_ENUM(TokenStatistics);
SPRT_ENUM(TokenRestrictedSids);
SPRT_ENUM(TokenSessionId);
SPRT_ENUM(TokenGroupsAndPrivileges);
SPRT_ENUM(TokenSessionReference);
SPRT_ENUM(TokenSandBoxInert);
SPRT_ENUM(TokenAuditPolicy);
SPRT_ENUM(TokenOrigin);
SPRT_ENUM(TokenElevationType);
SPRT_ENUM(TokenLinkedToken);
SPRT_ENUM(TokenElevation);
SPRT_ENUM(TokenHasRestrictions);
SPRT_ENUM(TokenAccessInformation);
SPRT_ENUM(TokenVirtualizationAllowed);
SPRT_ENUM(TokenVirtualizationEnabled);
SPRT_ENUM(TokenIntegrityLevel);
SPRT_ENUM(TokenUIAccess);
SPRT_ENUM(TokenMandatoryPolicy);
SPRT_ENUM(TokenLogonSid);
SPRT_ENUM(TokenIsAppContainer);
SPRT_ENUM(TokenCapabilities);
SPRT_ENUM(TokenAppContainerSid);
SPRT_ENUM(TokenAppContainerNumber);
SPRT_ENUM(TokenUserClaimAttributes);
SPRT_ENUM(TokenDeviceClaimAttributes);
SPRT_ENUM(TokenRestrictedUserClaimAttributes);
SPRT_ENUM(TokenRestrictedDeviceClaimAttributes);
SPRT_ENUM(TokenDeviceGroups);
SPRT_ENUM(TokenRestrictedDeviceGroups);
SPRT_ENUM(TokenSecurityAttributes);
SPRT_ENUM(TokenIsRestricted);
SPRT_ENUM(TokenProcessTrustLevel);
SPRT_ENUM(TokenPrivateNameSpace);
SPRT_ENUM(TokenSingletonAttributes);
SPRT_ENUM(TokenBnoIsolation);
SPRT_ENUM(TokenChildProcessFlags);
SPRT_ENUM(TokenIsLessPrivilegedAppContainer);
SPRT_ENUM(TokenIsSandboxed);
SPRT_ENUM(TokenIsAppSilo);
SPRT_ENUM(TokenLoggingInformation);
SPRT_ENUM(TokenLearningMode);
SPRT_ENUM(MaxTokenInfoClass);

// === omitted ===============================================================
// - HGLOBAL/HLOCAL: typedefs, not constants.
// - WINAPI_PROVIDER (WinApiProvider*): SPRT-specific enum, not in the SDK.
// - SYSTEM_CPU_SET_INFORMATION_CLASS (SystemCpuSetInformation*): SPRT-specific
//   enumerators, absent from the SDK (SDK uses CPU_SET_INFORMATION_TYPE instead).
