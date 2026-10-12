// Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
//
// api/process_api.h <-> Windows SDK function-signature parity. Compile-time only; see check.sh.

// keep psapi.h on its plain (non-K32*) spellings
#define PSAPI_VERSION 1

#define SPRT_API_HEADER <sprt/gen/wrappers/windows/api/process_api.h>
#include "api_check.h"

#include <windows.h>
#include <shellapi.h>
#include <psapi.h>


SPRT_FN(GetCurrentProcess);
SPRT_FN(GetCurrentProcessId);
SPRT_FN(OpenProcess);
SPRT_FN(GetExitCodeProcess);
SPRT_FN(GetPriorityClass);
SPRT_FN(InitializeProcThreadAttributeList);
SPRT_FN(UpdateProcThreadAttribute);
SPRT_FN(DeleteProcThreadAttributeList);
SPRT_FN(GetCommandLineW);
SPRT_FN(CommandLineToArgvW);
SPRT_FN(CreateProcessW);
SPRT_FN(GetCommandLineA);
SPRT_FN(CreateProcessA);
SPRT_FN(TerminateProcess);
SPRT_FN(GetProcessMemoryInfo);
SPRT_FN(EnumProcessModulesEx);
SPRT_FN(IsProcessorFeaturePresent);
SPRT_FN(ShellExecuteExW);
SPRT_FN(QueryFullProcessImageNameW);
SPRT_FN(QueryFullProcessImageNameA);
