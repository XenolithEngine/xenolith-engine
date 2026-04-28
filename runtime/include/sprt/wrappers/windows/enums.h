/**
 * Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 **/

#ifndef SPRT_WRAPPERS_WINDOWS_ENUMS_H_
#define SPRT_WRAPPERS_WINDOWS_ENUMS_H_

#include <sprt/wrappers/windows/basic_types.h>

/*
 * Windows enumeration types (winnt.h subset)
 */

/* System CPU set information class for CPU enumeration */
typedef enum _SYSTEM_CPU_SET_INFORMATION_CLASS {
	SystemCpuSetInformationBasic = 0,
	SystemCpuSetInformationGroup = 1
} SYSTEM_CPU_SET_INFORMATION_CLASS, *PSYSTEM_CPU_SET_INFORMATION_CLASS;

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

#endif // SPRT_WRAPPERS_WINDOWS_ENUMS_H_
