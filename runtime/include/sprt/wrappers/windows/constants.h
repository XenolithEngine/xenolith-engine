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

#ifndef SPRT_WRAPPERS_WINDOWS_CONSTANTS_H_
#define SPRT_WRAPPERS_WINDOWS_CONSTANTS_H_

#include <sprt/wrappers/windows/basic_types.h>
#include <sprt/wrappers/windows/errors.h>

#ifndef NULL
#if __cplusplus
#define NULL nullptr
#else
#define NULL ((void *)0)
#endif
#endif

#define FALSE ((BOOL)0)
#define TRUE ((BOOL)1)

/* WCHAR and MAX_PATH - defined for standalone compilation */
#ifndef MAX_PATH
#define MAX_PATH 260
#endif

/* ============================================================ */
/* Registry Types (winreg.h)                                    */
/* ============================================================ */

/**
 * HKEY - Handle to a registry key.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-hkey__tag
 */
typedef HANDLE HKEY;

/* Registry constants */

#define HKEY_CLASSES_ROOT                   (( HKEY ) (ULONG_PTR)((LONG)0x8000'0000) )
#define HKEY_CURRENT_USER                   (( HKEY ) (ULONG_PTR)((LONG)0x8000'0001) )
#define HKEY_LOCAL_MACHINE                  (( HKEY ) (ULONG_PTR)((LONG)0x8000'0002) )
#define HKEY_USERS                          (( HKEY ) (ULONG_PTR)((LONG)0x8000'0003) )
#define HKEY_PERFORMANCE_DATA               (( HKEY ) (ULONG_PTR)((LONG)0x8000'0004) )
#define HKEY_PERFORMANCE_TEXT               (( HKEY ) (ULONG_PTR)((LONG)0x8000'0050) )
#define HKEY_PERFORMANCE_NLSTEXT            (( HKEY ) (ULONG_PTR)((LONG)0x8000'0060) )
#define HKEY_CURRENT_CONFIG                 (( HKEY ) (ULONG_PTR)((LONG)0x8000'0005) )
#define HKEY_DYN_DATA                       (( HKEY ) (ULONG_PTR)((LONG)0x8000'0006) )
#define HKEY_CURRENT_USER_LOCAL_SETTINGS    (( HKEY ) (ULONG_PTR)((LONG)0x8000'0007) )

/* Registry access rights */
#define KEY_READ                    0x2'0019UL
#define KEY_WRITE                   0x2'0006UL
#define KEY_CREATE_SUB_KEY          0x0004UL
#define KEY_ENUMERATE_SUB_KEYS      0x0008UL
#define KEY_NOTIFY                  0x0010UL
#define KEY_QUERY_VALUE             0x0001UL

/* Registry value retrieval flags */
#define RRF_RT_REG_NONE             0x0000'0001UL
#define RRF_RT_REG_SZ               0x0000'0002UL
#define RRF_RT_REG_EXPAND_SZ        0x0000'0004UL
#define RRF_RT_REG_BINARY           0x0000'0010UL
#define RRF_RT_REG_DWORD            0x0000'0020UL
#define RRF_RT_REG_MULTI_SZ         0x0000'0040UL
#define RRF_RT_REG_QWORD            0x0000'0080UL
#define RRF_RT_DWORD                (RRF_RT_REG_BINARY | RRF_RT_REG_DWORD)
#define RRF_RT_QWORD                (RRF_RT_REG_BINARY | RRF_RT_REG_QWORD)
#define RRF_RT_ANY                  0xFFFF
#define RRF_NOEXPAND                0x1000'0000UL
#define RRF_ZEROONFAILURE           0x2000'0000UL

/* ============================================================ */
/* COM Types (wtypes.h, objbase.h)                              */
/* ============================================================ */

/**
 * BSTR - Basic String type for COM interop.
 * A pointer to a null-terminated wide character string with length prefix.
 */
typedef WCHAR *BSTR;

/* Variant data types */
typedef unsigned short VARTYPE;
#define VT_EMPTY              0x0000
#define VT_NULL               0x0001
#define VT_I2                 0x0002
#define VT_I4                 0x0003
#define VT_R4                 0x0004
#define VT_R8                 0x0005
#define VT_CY                 0x0006
#define VT_DATE               0x0007
#define VT_BSTR               0x0008
#define VT_DISPATCH           0x0009
#define VT_ERROR              0x000A
#define VT_BOOL               0x000B
#define VT_VARIANT            0x000C
#define VT_UNKNOWN            0x000D
#define VT_I1                 0x0010
#define VT_UI1                0x0011
#define VT_UI2                0x0012
#define VT_UI4                0x0013
#define VT_I8                 0x0014
#define VT_UI8                0x0015
#define VT_INT                0x0016
#define VT_UINT               0x0017
#define VT_VOID               0x0018
#define VT_HRESULT            0x0019
#define VT_PTR                0x0020
#define VT_SAFEARRAY          0x0021
#define VT_CARRAY             0x0022
#define VT_USERDEFINED        0x0023
#define VT_LPSTR              0x004E
#define VT_LPWSTR             0x005F

/* VARIANT_BOOL type */
typedef SHORT VARIANT_BOOL;
#define VARIANT_TRUE          ((VARIANT_BOOL)-1)
#define VARIANT_FALSE         ((VARIANT_BOOL)0)

/**
 * VARIANTARG/VARIANT - Variant data type for COM automation.
 * Can hold different types of data including primitives, strings, and objects.
 */
typedef union tagVARIANTARG {
	struct {
		VARTYPE vt;
		WORD wReserved1;
		WORD wReserved2;
		WORD wReserved3;
		union {
			BYTE bVal;
			SHORT iVal;
			LONG lVal;
			FLOAT fltVal;
			DOUBLE dblVal;
			VARIANT_BOOL boolVal;
			struct IUnknown *punkVal;
			BYTE *pbVal;
			SHORT *piVal;
			LONG *plVal;
			FLOAT *pfltVal;
			DOUBLE *pdblVal;
			VARIANT_BOOL *pboolVal;
			struct IUnknown **ppunkVal;
			struct {
				BSTR bstrVal;
			} __VARIANT_NAME_9;
		} __VARIANT_NAME_10;
	};
} VARIANTARG, *PVARIANTARG, *LPVARIANTARG;

typedef VARIANTARG VARIANT;
typedef PVARIANTARG PVARIANT;
typedef LPVARIANTARG LPVARIANT;

/* Variant helper macros */
#define V_VARIANT(v)          ((v).vt)
#define V_BSTR(v)             ((v).__VARIANT_NAME_9.bstrVal)
#define V_I4(v)               ((v).lVal)
#define V_BOOL(v)             ((v).boolVal)

/* COM interface forward declarations */
struct IWbemLocator;
struct IWbemServices;
struct IEnumWbemClassObject;
struct IWbemClassObject;
struct IUnknown;

/* Security descriptor type for COM initialization */
typedef void *PSECURITY_DESCRIPTOR;

/* GUID type for CLSID/IID definitions - must be defined before REFCLSID/REFIID */
typedef struct _GUID {
	DWORD Data1;
	WORD Data2;
	WORD Data3;
	BYTE Data4[8];
} GUID, *PGUID, *LPGUID, IID, CLSID;

/* SOLE_AUTHENTICATION_SERVICE - Authentication service info for CoInitializeSecurity */
typedef struct _SOLE_AUTHENTICATION_SERVICE {
	LPWSTR pAuthenticatorAddress;
	DWORD dwCapabilities;
	DWORD AuthnSvc;
	DWORD AuthzSvc;
} SOLE_AUTHENTICATION_SERVICE, *PSOLE_AUTHENTICATION_SERVICE;

/* COM reference types (CLSID/IID as references) - C++ only */
#ifdef __cplusplus
typedef const GUID &REFCLSID;
typedef const GUID &REFIID;
#else
#define REFCLSID const GUID *
#define REFIID const GUID *
#endif

typedef enum tagCLSCTX {
	CLSCTX_INPROC_SERVER = 0x1,
	CLSCTX_INPROC_HANDLER = 0x2,
	CLSCTX_LOCAL_SERVER = 0x4,
	CLSCTX_INPROC_SERVER16 = 0x8,
	CLSCTX_REMOTE_SERVER = 0x10,
	CLSCTX_INPROC_HANDLER16 = 0x20,
	CLSCTX_RESERVED1 = 0x40,
	CLSCTX_RESERVED2 = 0x80,
	CLSCTX_RESERVED3 = 0x100,
	CLSCTX_RESERVED4 = 0x200,
	CLSCTX_NO_CODE_DOWNLOAD = 0x400,
	CLSCTX_RESERVED5 = 0x800,
	CLSCTX_NO_CUSTOM_MARSHAL = 0x1000,
	CLSCTX_ENABLE_CODE_DOWNLOAD = 0x2000,
	CLSCTX_NO_FAILURE_LOG = 0x4000,
	CLSCTX_DISABLE_AAA = 0x8000,
	CLSCTX_ENABLE_AAA = 0x1'0000,
	CLSCTX_FROM_DEFAULT_CONTEXT = 0x2'0000,
	CLSCTX_ACTIVATE_X86_SERVER = 0x4'0000,
	CLSCTX_ACTIVATE_32_BIT_SERVER = CLSCTX_ACTIVATE_X86_SERVER,
	CLSCTX_ACTIVATE_64_BIT_SERVER = 0x8'0000,
	CLSCTX_ENABLE_CLOAKING = 0x10'0000,
	CLSCTX_APPCONTAINER = 0x40'0000,
	CLSCTX_ACTIVATE_AAA_AS_IU = 0x80'0000,
	CLSCTX_RESERVED6 = 0x100'0000,
	CLSCTX_ACTIVATE_ARM32_SERVER = 0x200'0000,
	CLSCTX_ALLOW_LOWER_TRUST_REGISTRATION = 0x400'0000,
	CLSCTX_SERVER_MUST_BE_EQUAL_OR_GREATER_PRIVILEGE = 0x800'0000,
	CLSCTX_DO_NOT_ELEVATE_SERVER = 0x1000'0000,
	CLSCTX_PS_DLL = 0x8000'0000
} CLSCTX;

/* ============================================================ */
/* System Information Types (winbase.h)                         */
/* ============================================================ */

/* Processor architecture constants */
#define PROCESSOR_ARCHITECTURE_INTEL          0
#define PROCESSOR_ARCHITECTURE_MIPS           1
#define PROCESSOR_ARCHITECTURE_ALPHA          2
#define PROCESSOR_ARCHITECTURE_PPC            3
#define PROCESSOR_ARCHITECTURE_SHX            4
#define PROCESSOR_ARCHITECTURE_ARM            5
#define PROCESSOR_ARCHITECTURE_IA64           6
#define PROCESSOR_ARCHITECTURE_ALPHA64        7
#define PROCESSOR_ARCHITECTURE_MSIL           8
#define PROCESSOR_ARCHITECTURE_AMD64          9
#define PROCESSOR_ARCHITECTURE_IA32_ON_WIN64  10
#define PROCESSOR_ARCHITECTURE_NEUTRAL        11
#define PROCESSOR_ARCHITECTURE_ARM64          12
#define PROCESSOR_ARCHITECTURE_ARM32_ON_WIN64 13
#define PROCESSOR_ARCHITECTURE_IA32_ON_ARM64  14

/* Computer name limits */
#ifndef MAX_COMPUTERNAME_LENGTH
#define MAX_COMPUTERNAME_LENGTH  15
#endif

#define MAX_DOMAIN_NAME_LENGTH     64
#define MAX_WORKSTATION_DOMAIN_NAME_LENGTH  15

/* ============================================================ */
/* COM Security Constants (combaseapi.h)                        */
/* ============================================================ */

#define RPC_C_AUTHN_LEVEL_DEFAULT     0
#define RPC_C_AUTHN_LEVEL_CONNECT     1
#define RPC_C_AUTHN_LEVEL_CALL        2
#define RPC_C_AUTHN_LEVEL_PKT         3
#define RPC_C_AUTHN_LEVEL_PKT_INTEGRITY 4
#define RPC_C_AUTHN_LEVEL_PKT_PRIVACY 5

#define RPC_C_AUTHN_WINNT             10
#define RPC_C_AUTHN_GSS_NEGOTIATE     9
#define RPC_C_AUTHN_NONE              0

#define RPC_C_IMP_LEVEL_DEFAULT       0
#define RPC_C_IMP_LEVEL_IDENTIFY      1
#define RPC_C_IMP_LEVEL_IMPERSONATE   2
#define RPC_C_IMP_LEVEL_DELEGATE      3

#define EOAC_NONE                     0x0000
#define EOAC_RESERVED_1               0x0001
#define EOAC_RESERVED_2               0x0002
#define EOAC_RESERVED_3               0x0004
#define EOAC_RESERVED_4               0x0008
#define EOAC_RESERVED_5               0x0010
#define EOAC_RESERVED_6               0x0020

/* ============================================================ */
/* WMI Constants (wbemcli.h)                                    */
/* ============================================================ */

#define WBEM_FLAG_FORWARD_ONLY        0x0000'0020UL
#define WBEM_FLAG_RETURN_IMMEDIATELY  0x0000'1000UL
#define WBEM_INFINITE                 0xFFFF'FFFFUL
#define WBEM_FLAG_NONSYSTEM_ONLY      0x0000'0040UL

/* HRESULT helper macros */
#define SUCCEEDED(hr)   (((HRESULT)(hr)) >= 0)
#define FAILED(hr)      (((HRESULT)(hr)) < 0)

/* ============================================================ */
/* Memory Functions (memoryapi.h, winbase.h)                    */
/* ============================================================ */

/**
 * ZeroMemory - Fills a block of memory with zeros.
 * @param Destination Pointer to the start of the memory block.
 * @param Length Number of bytes to zero.
 */
#define ZeroMemory(Destination, Length) \
    RtlZeroMemory((Destination), (Length))

/* Access rights */
#define TOKEN_QUERY                 0x0008

/* Handle duplication flags */
#define DUPLICATE_SAME_ACCESS       0x0000'0002

/* File creation dispositions */
#define CREATE_NEW                  1
#define CREATE_ALWAYS               2
#define OPEN_EXISTING               3
#define OPEN_ALWAYS                 4
#define TRUNCATE_EXISTING           5

/* Waitable timer flags */
#define TIMER_SET_STATE             1
#define TIMER_CANCELLED             2

/* Wait result codes */
#define WAIT_OBJECT_0               0x0000'0000L
#define WAIT_ABANDONED              0x0000'0080L
#define WAIT_FAILED                 0xFFFF'FFFFL

/* Special values */
#define INVALID_HANDLE_VALUE        ((HANDLE)(ULONG_PTR)-1)
#define INVALID_FILE_ATTRIBUTES     ((DWORD)-1)
#define INFINITE                    0xFFFF'FFFF

#define HANDLE_FLAG_INHERIT             0x0000'0001
#define HANDLE_FLAG_PROTECT_FROM_CLOSE  0x0000'0002

#define LMEM_FIXED          0x0000
#define LMEM_MOVEABLE       0x0002
#define LMEM_NOCOMPACT      0x0010
#define LMEM_NODISCARD      0x0020
#define LMEM_ZEROINIT       0x0040
#define LMEM_MODIFY         0x0080
#define LMEM_DISCARDABLE    0x0F00
#define LMEM_VALID_FLAGS    0x0F72
#define LMEM_INVALID_HANDLE 0x8000

#define STACK_SIZE_PARAM_IS_A_RESERVATION   0x0001'0000

#endif // SPRT_WRAPPERS_WINDOWS_CONSTANTS_H_
