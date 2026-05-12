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
#include <sprt/c/bits/__sprt_null.h>

#ifndef NULL
#define NULL __SPRT_NULL
#endif

#define ANYSIZE_ARRAY 1

#define FALSE ((BOOL)0)
#define TRUE ((BOOL)1)

/* WCHAR and MAX_PATH - defined for standalone compilation */
#ifndef MAX_PATH
#define MAX_PATH 260
#endif

// clang-format off
#define HKEY_CLASSES_ROOT                   (( HKEY ) (ULONG_PTR)((LONG)0x80000000) )
#define HKEY_CURRENT_USER                   (( HKEY ) (ULONG_PTR)((LONG)0x80000001) )
#define HKEY_LOCAL_MACHINE                  (( HKEY ) (ULONG_PTR)((LONG)0x80000002) )
#define HKEY_USERS                          (( HKEY ) (ULONG_PTR)((LONG)0x80000003) )
#define HKEY_PERFORMANCE_DATA               (( HKEY ) (ULONG_PTR)((LONG)0x80000004) )
#define HKEY_PERFORMANCE_TEXT               (( HKEY ) (ULONG_PTR)((LONG)0x80000050) )
#define HKEY_PERFORMANCE_NLSTEXT            (( HKEY ) (ULONG_PTR)((LONG)0x80000060) )
#define HKEY_CURRENT_CONFIG                 (( HKEY ) (ULONG_PTR)((LONG)0x80000005) )
#define HKEY_DYN_DATA                       (( HKEY ) (ULONG_PTR)((LONG)0x80000006) )
#define HKEY_CURRENT_USER_LOCAL_SETTINGS    (( HKEY ) (ULONG_PTR)((LONG)0x80000007) )

/* Registry value retrieval flags */
#define RRF_RT_REG_NONE        0x00000001  // restrict type to REG_NONE      (other data types will not return ERROR_SUCCESS)
#define RRF_RT_REG_SZ          0x00000002  // restrict type to REG_SZ        (other data types will not return ERROR_SUCCESS) (automatically converts REG_EXPAND_SZ to REG_SZ unless RRF_NOEXPAND is specified)
#define RRF_RT_REG_EXPAND_SZ   0x00000004  // restrict type to REG_EXPAND_SZ (other data types will not return ERROR_SUCCESS) (must specify RRF_NOEXPAND or RegGetValue will fail with ERROR_INVALID_PARAMETER)
#define RRF_RT_REG_BINARY      0x00000008  // restrict type to REG_BINARY    (other data types will not return ERROR_SUCCESS)
#define RRF_RT_REG_DWORD       0x00000010  // restrict type to REG_DWORD     (other data types will not return ERROR_SUCCESS)
#define RRF_RT_REG_MULTI_SZ    0x00000020  // restrict type to REG_MULTI_SZ  (other data types will not return ERROR_SUCCESS)
#define RRF_RT_REG_QWORD       0x00000040  // restrict type to REG_QWORD     (other data types will not return ERROR_SUCCESS)

#define RRF_RT_DWORD           (RRF_RT_REG_BINARY | RRF_RT_REG_DWORD) // restrict type to *32-bit* RRF_RT_REG_BINARY or RRF_RT_REG_DWORD (other data types will not return ERROR_SUCCESS)
#define RRF_RT_QWORD           (RRF_RT_REG_BINARY | RRF_RT_REG_QWORD) // restrict type to *64-bit* RRF_RT_REG_BINARY or RRF_RT_REG_DWORD (other data types will not return ERROR_SUCCESS)
#define RRF_RT_ANY             0x0000ffff                             // no type restriction

#define RRF_SUBKEY_WOW6464KEY  0x00010000  // when opening the subkey (if provided) force open from the 64bit location (only one SUBKEY_WOW64* flag can be set or RegGetValue will fail with ERROR_INVALID_PARAMETER)
#define RRF_SUBKEY_WOW6432KEY  0x00020000  // when opening the subkey (if provided) force open from the 32bit location (only one SUBKEY_WOW64* flag can be set or RegGetValue will fail with ERROR_INVALID_PARAMETER)
#define RRF_WOW64_MASK         0x00030000

#define RRF_NOEXPAND           0x10000000  // do not automatically expand environment strings if value is of type REG_EXPAND_SZ
#define RRF_ZEROONFAILURE      0x20000000  // if pvData is not NULL, set content to all zeros on failure

/* Variant data types */
enum VARENUM {
	VT_EMPTY = 0,
	VT_NULL = 1,
	VT_I2 = 2,
	VT_I4 = 3,
	VT_R4 = 4,
	VT_R8 = 5,
	VT_CY = 6,
	VT_DATE = 7,
	VT_BSTR = 8,
	VT_DISPATCH = 9,
	VT_ERROR = 10,
	VT_BOOL = 11,
	VT_VARIANT = 12,
	VT_UNKNOWN = 13,
	VT_DECIMAL = 14,
	VT_I1 = 16,
	VT_UI1 = 17,
	VT_UI2 = 18,
	VT_UI4 = 19,
	VT_I8 = 20,
	VT_UI8 = 21,
	VT_INT = 22,
	VT_UINT = 23,
	VT_VOID = 24,
	VT_HRESULT  = 25,
	VT_PTR = 26,
	VT_SAFEARRAY = 27,
	VT_CARRAY = 28,
	VT_USERDEFINED = 29,
	VT_LPSTR = 30,
	VT_LPWSTR = 31,
	VT_FILETIME = 64,
	VT_BLOB = 65,
	VT_STREAM = 66,
	VT_STORAGE = 67,
	VT_STREAMED_OBJECT = 68,
	VT_STORED_OBJECT = 69,
	VT_BLOB_OBJECT = 70,
	VT_CF = 71,
	VT_CLSID = 72,
	VT_VECTOR = 0x1000,
	VT_ARRAY = 0x2000,
	VT_BYREF = 0x4000,
	VT_RESERVED = 0x8000,
	VT_ILLEGAL = 0xffff,
	VT_ILLEGALMASKED = 0xfff,
	VT_TYPEMASK = 0xfff
};

/* VARIANT_BOOL type */
#define VARIANT_TRUE          ((VARIANT_BOOL)-1)
#define VARIANT_FALSE         ((VARIANT_BOOL)0)


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
	CLSCTX_ENABLE_AAA = 0x10000,
	CLSCTX_FROM_DEFAULT_CONTEXT = 0x20000,
	CLSCTX_ACTIVATE_X86_SERVER = 0x40000,
	CLSCTX_ACTIVATE_32_BIT_SERVER = CLSCTX_ACTIVATE_X86_SERVER,
	CLSCTX_ACTIVATE_64_BIT_SERVER = 0x80000,
	CLSCTX_ENABLE_CLOAKING = 0x100000,
	CLSCTX_APPCONTAINER = 0x400000,
	CLSCTX_ACTIVATE_AAA_AS_IU = 0x800000,
	CLSCTX_RESERVED6 = 0x1000000,
	CLSCTX_ACTIVATE_ARM32_SERVER = 0x2000000,
	CLSCTX_ALLOW_LOWER_TRUST_REGISTRATION = 0x4000000,
	CLSCTX_SERVER_MUST_BE_EQUAL_OR_GREATER_PRIVILEGE = 0x8000000,
	CLSCTX_DO_NOT_ELEVATE_SERVER = 0x10000000,
	CLSCTX_PS_DLL = 0x80000000
} CLSCTX;

typedef enum tag_WBEM_GENERIC_FLAG_TYPE {
	WBEM_FLAG_RETURN_IMMEDIATELY	= 0x10,
	WBEM_FLAG_RETURN_WBEM_COMPLETE	= 0,
	WBEM_FLAG_BIDIRECTIONAL	= 0,
	WBEM_FLAG_FORWARD_ONLY	= 0x20,
	WBEM_FLAG_NO_ERROR_OBJECT	= 0x40,
	WBEM_FLAG_RETURN_ERROR_OBJECT	= 0,
	WBEM_FLAG_SEND_STATUS	= 0x80,
	WBEM_FLAG_DONT_SEND_STATUS	= 0,
	WBEM_FLAG_ENSURE_LOCATABLE	= 0x100,
	WBEM_FLAG_DIRECT_READ	= 0x200,
	WBEM_FLAG_SEND_ONLY_SELECTED	= 0,
	WBEM_RETURN_WHEN_COMPLETE	= 0,
	WBEM_RETURN_IMMEDIATELY	= 0x10,
	WBEM_MASK_RESERVED_FLAGS	= 0x1f000,
	WBEM_FLAG_USE_AMENDED_QUALIFIERS	= 0x20000,
	WBEM_FLAG_STRONG_VALIDATION	= 0x100000
} WBEM_GENERIC_FLAG_TYPE;

/* Handle duplication flags */
#define DUPLICATE_CLOSE_SOURCE      0x00000001  
#define DUPLICATE_SAME_ACCESS       0x00000002  

#define WAIT_FAILED       ((DWORD)0xFFFFFFFF)
#define WAIT_OBJECT_0     ((STATUS_WAIT_0 ) + 0 )
#define WAIT_ABANDONED    ((STATUS_ABANDONED_WAIT_0 ) + 0 )
#define WAIT_ABANDONED_0  ((STATUS_ABANDONED_WAIT_0 ) + 0 )

/* Special values */
#define INVALID_HANDLE_VALUE        ((HANDLE)(ULONG_PTR)-1)
#define INVALID_FILE_ATTRIBUTES     ((DWORD)-1)
#define INFINITE                    0xFFFFFFFF

#define HANDLE_FLAG_INHERIT             0x00000001
#define HANDLE_FLAG_PROTECT_FROM_CLOSE  0x00000002

/* Local Memory Flags */
#define LMEM_FIXED          0x0000
#define LMEM_MOVEABLE       0x0002
#define LMEM_NOCOMPACT      0x0010
#define LMEM_NODISCARD      0x0020
#define LMEM_ZEROINIT       0x0040
#define LMEM_MODIFY         0x0080
#define LMEM_DISCARDABLE    0x0F00
#define LMEM_VALID_FLAGS    0x0F72
#define LMEM_INVALID_HANDLE 0x8000

#define STACK_SIZE_PARAM_IS_A_RESERVATION   0x00010000

#define SEM_FAILCRITICALERRORS      0x0001
#define SEM_NOGPFAULTERRORBOX       0x0002
#define SEM_NOALIGNMENTFAULTEXCEPT  0x0004
#define SEM_NOOPENFILEERRORBOX      0x8000

#define GENERIC_READ                     (0x80000000L)
#define GENERIC_WRITE                    (0x40000000L)
#define GENERIC_EXECUTE                  (0x20000000L)
#define GENERIC_ALL                      (0x10000000L)

//#define DELETE                           (0x00010000L)
#define _DELETE                          (0x00010000L)
#define READ_CONTROL                     (0x00020000L)
#define WRITE_DAC                        (0x00040000L)
#define WRITE_OWNER                      (0x00080000L)
#define SYNCHRONIZE                      (0x00100000L)

#define STANDARD_RIGHTS_REQUIRED         (0x000F0000L)
#define STANDARD_RIGHTS_READ             (READ_CONTROL)
#define STANDARD_RIGHTS_WRITE            (READ_CONTROL)
#define STANDARD_RIGHTS_EXECUTE          (READ_CONTROL)
#define STANDARD_RIGHTS_ALL              (0x001F0000L)
#define SPECIFIC_RIGHTS_ALL              (0x0000FFFFL)

#define KEY_QUERY_VALUE         (0x0001)
#define KEY_SET_VALUE           (0x0002)
#define KEY_CREATE_SUB_KEY      (0x0004)
#define KEY_ENUMERATE_SUB_KEYS  (0x0008)
#define KEY_NOTIFY              (0x0010)
#define KEY_CREATE_LINK         (0x0020)
#define KEY_WOW64_32KEY         (0x0200)
#define KEY_WOW64_64KEY         (0x0100)
#define KEY_WOW64_RES           (0x0300)

#define KEY_READ                ((STANDARD_RIGHTS_READ       |\
                                  KEY_QUERY_VALUE            |\
                                  KEY_ENUMERATE_SUB_KEYS     |\
                                  KEY_NOTIFY)                 \
                                  &                           \
                                 (~SYNCHRONIZE))


#define KEY_WRITE               ((STANDARD_RIGHTS_WRITE      |\
                                  KEY_SET_VALUE              |\
                                  KEY_CREATE_SUB_KEY)         \
                                  &                           \
                                 (~SYNCHRONIZE))

#define KEY_EXECUTE             ((KEY_READ)                   \
                                  &                           \
                                 (~SYNCHRONIZE))

#define KEY_ALL_ACCESS          ((STANDARD_RIGHTS_ALL        |\
                                  KEY_QUERY_VALUE            |\
                                  KEY_SET_VALUE              |\
                                  KEY_CREATE_SUB_KEY         |\
                                  KEY_ENUMERATE_SUB_KEYS     |\
                                  KEY_NOTIFY                 |\
                                  KEY_CREATE_LINK)            \
                                  &                           \
                                 (~SYNCHRONIZE))

// clang-format on

/* Processor architecture constants */
#define PROCESSOR_ARCHITECTURE_INTEL            0
#define PROCESSOR_ARCHITECTURE_MIPS             1
#define PROCESSOR_ARCHITECTURE_ALPHA            2
#define PROCESSOR_ARCHITECTURE_PPC              3
#define PROCESSOR_ARCHITECTURE_SHX              4
#define PROCESSOR_ARCHITECTURE_ARM              5
#define PROCESSOR_ARCHITECTURE_IA64             6
#define PROCESSOR_ARCHITECTURE_ALPHA64          7
#define PROCESSOR_ARCHITECTURE_MSIL             8
#define PROCESSOR_ARCHITECTURE_AMD64            9
#define PROCESSOR_ARCHITECTURE_IA32_ON_WIN64    10
#define PROCESSOR_ARCHITECTURE_NEUTRAL          11
#define PROCESSOR_ARCHITECTURE_ARM64            12
#define PROCESSOR_ARCHITECTURE_ARM32_ON_WIN64   13
#define PROCESSOR_ARCHITECTURE_IA32_ON_ARM64    14

/* Computer name limits */
#define MAX_COMPUTERNAME_LENGTH  15

#define RPC_C_AUTHN_LEVEL_DEFAULT       0
#define RPC_C_AUTHN_LEVEL_NONE          1
#define RPC_C_AUTHN_LEVEL_CONNECT       2
#define RPC_C_AUTHN_LEVEL_CALL          3
#define RPC_C_AUTHN_LEVEL_PKT           4
#define RPC_C_AUTHN_LEVEL_PKT_INTEGRITY 5
#define RPC_C_AUTHN_LEVEL_PKT_PRIVACY   6

/* HRESULT helper macros */
#define SUCCEEDED(hr)   (((HRESULT)(hr)) >= 0)
#define FAILED(hr)      (((HRESULT)(hr)) < 0)

/* ============================================================ */
/* Memory Functions (memoryapi.h, winbase.h)                    */
/* ============================================================ */

/* Access rights */
#define TOKEN_ASSIGN_PRIMARY    (0x0001)
#define TOKEN_DUPLICATE         (0x0002)
#define TOKEN_IMPERSONATE       (0x0004)
#define TOKEN_QUERY             (0x0008)
#define TOKEN_QUERY_SOURCE      (0x0010)
#define TOKEN_ADJUST_PRIVILEGES (0x0020)
#define TOKEN_ADJUST_GROUPS     (0x0040)
#define TOKEN_ADJUST_DEFAULT    (0x0080)
#define TOKEN_ADJUST_SESSIONID  (0x0100)

#define SECTION_QUERY                0x0001
#define SECTION_MAP_WRITE            0x0002
#define SECTION_MAP_READ             0x0004
#define SECTION_MAP_EXECUTE          0x0008
#define SECTION_EXTEND_SIZE          0x0010
#define SECTION_MAP_EXECUTE_EXPLICIT 0x0020 // not included in SECTION_ALL_ACCESS

#define SECTION_ALL_ACCESS (STANDARD_RIGHTS_REQUIRED|SECTION_QUERY|\
                            SECTION_MAP_WRITE |      \
                            SECTION_MAP_READ |       \
                            SECTION_MAP_EXECUTE |    \
                            SECTION_EXTEND_SIZE)

#endif // SPRT_WRAPPERS_WINDOWS_CONSTANTS_H_
