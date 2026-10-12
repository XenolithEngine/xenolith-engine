// Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
//
// api/security_api.h <-> Windows SDK function-signature parity. Compile-time only; see check.sh.

#define SPRT_API_HEADER <sprt/gen/wrappers/windows/api/security_api.h>
#include "api_check.h"

#include <windows.h>
#include <aclapi.h>
#include <userenv.h>
#include <sddl.h>

SPRT_FN(AllocateAndInitializeSid);
SPRT_FN(GetSidSubAuthorityCount);
SPRT_FN(GetSidSubAuthority);
SPRT_FN(FreeSid);
SPRT_FN(GetSecurityInfo);
SPRT_FN(CreateAppContainerProfile);
SPRT_FN(DeriveAppContainerSidFromAppContainerName);
SPRT_FN(GetAppContainerFolderPath);
SPRT_FN(DeleteAppContainerProfile);
SPRT_FN(GetNamedSecurityInfoW);
SPRT_FN(SetEntriesInAclW);
SPRT_FN(SetNamedSecurityInfoW);
SPRT_FN(ConvertSidToStringSidW);
SPRT_FN(CloseWindowStation);
SPRT_FN(SetProcessWindowStation);
SPRT_FN(GetProcessWindowStation);
SPRT_FN(GetUserObjectInformationW);
SPRT_FN(RegisterEventSourceW);
SPRT_FN(DeregisterEventSource);
SPRT_FN(ReportEventW);
SPRT_FN(LookupPrivilegeValueA);
SPRT_FN(LookupPrivilegeValueW);
SPRT_FN(AdjustTokenPrivileges);
