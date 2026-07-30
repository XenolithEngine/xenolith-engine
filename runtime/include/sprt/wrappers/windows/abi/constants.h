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

#ifndef SPRT_WRAPPERS_WINDOWS_ABI_CONSTANTS_H_
#define SPRT_WRAPPERS_WINDOWS_ABI_CONSTANTS_H_


#include <sprt/wrappers/windows/abi/basic_types.h>
#include <sprt/wrappers/windows/abi/winerror.h>
#include <sprt/c/bits/__sprt_null.h>

#ifndef NULL
#define NULL __SPRT_NULL
#endif

#define __SPRT_ANYSIZE_ARRAY 1

#define __SPRT_FALSE ((BOOL)0)

#define __SPRT_TRUE ((BOOL)1)

/* WCHAR and MAX_PATH - defined for standalone compilation */
#define __SPRT_MAX_PATH 260

#define __SPRT_MAXIMUM_WAIT_OBJECTS 64

// clang-format off
#define __SPRT_HKEY_CLASSES_ROOT                   (( HKEY ) (ULONG_PTR)((LONG)0x80000000) )
#define __SPRT_HKEY_CURRENT_USER                   (( HKEY ) (ULONG_PTR)((LONG)0x80000001) )
#define __SPRT_HKEY_LOCAL_MACHINE                  (( HKEY ) (ULONG_PTR)((LONG)0x80000002) )
#define __SPRT_HKEY_USERS                          (( HKEY ) (ULONG_PTR)((LONG)0x80000003) )
#define __SPRT_HKEY_PERFORMANCE_DATA               (( HKEY ) (ULONG_PTR)((LONG)0x80000004) )
#define __SPRT_HKEY_PERFORMANCE_TEXT               (( HKEY ) (ULONG_PTR)((LONG)0x80000050) )
#define __SPRT_HKEY_PERFORMANCE_NLSTEXT            (( HKEY ) (ULONG_PTR)((LONG)0x80000060) )
#define __SPRT_HKEY_CURRENT_CONFIG                 (( HKEY ) (ULONG_PTR)((LONG)0x80000005) )
#define __SPRT_HKEY_DYN_DATA                       (( HKEY ) (ULONG_PTR)((LONG)0x80000006) )
#define __SPRT_HKEY_CURRENT_USER_LOCAL_SETTINGS    (( HKEY ) (ULONG_PTR)((LONG)0x80000007) )

/* Registry value retrieval flags */
#define __SPRT_RRF_RT_REG_NONE        0x00000001  // restrict type to REG_NONE      (other data types will not return ERROR_SUCCESS)
// Registry value types (winnt.h).
#define __SPRT_REG_NONE                 0
#define __SPRT_REG_SZ                   1
#define __SPRT_REG_EXPAND_SZ            2
#define __SPRT_REG_BINARY              3
#define __SPRT_REG_DWORD               4
#define __SPRT_REG_MULTI_SZ            7
#define __SPRT_REG_QWORD               11

#define __SPRT_RRF_RT_REG_SZ          0x00000002  // restrict type to REG_SZ        (other data types will not return ERROR_SUCCESS) (automatically converts REG_EXPAND_SZ to REG_SZ unless RRF_NOEXPAND is specified)
#define __SPRT_RRF_RT_REG_EXPAND_SZ   0x00000004  // restrict type to REG_EXPAND_SZ (other data types will not return ERROR_SUCCESS) (must specify RRF_NOEXPAND or RegGetValue will fail with ERROR_INVALID_PARAMETER)
#define __SPRT_RRF_RT_REG_BINARY      0x00000008  // restrict type to REG_BINARY    (other data types will not return ERROR_SUCCESS)
#define __SPRT_RRF_RT_REG_DWORD       0x00000010  // restrict type to REG_DWORD     (other data types will not return ERROR_SUCCESS)
#define __SPRT_RRF_RT_REG_MULTI_SZ    0x00000020  // restrict type to REG_MULTI_SZ  (other data types will not return ERROR_SUCCESS)
#define __SPRT_RRF_RT_REG_QWORD       0x00000040  // restrict type to REG_QWORD     (other data types will not return ERROR_SUCCESS)

#define __SPRT_RRF_RT_DWORD           (__SPRT_RRF_RT_REG_BINARY | __SPRT_RRF_RT_REG_DWORD) // restrict type to *32-bit* RRF_RT_REG_BINARY or RRF_RT_REG_DWORD (other data types will not return ERROR_SUCCESS)
#define __SPRT_RRF_RT_QWORD           (__SPRT_RRF_RT_REG_BINARY | __SPRT_RRF_RT_REG_QWORD) // restrict type to *64-bit* RRF_RT_REG_BINARY or RRF_RT_REG_DWORD (other data types will not return ERROR_SUCCESS)
#define __SPRT_RRF_RT_ANY             0x0000ffff                             // no type restriction

#define __SPRT_RRF_SUBKEY_WOW6464KEY  0x00010000  // when opening the subkey (if provided) force open from the 64bit location (only one SUBKEY_WOW64* flag can be set or RegGetValue will fail with ERROR_INVALID_PARAMETER)
#define __SPRT_RRF_SUBKEY_WOW6432KEY  0x00020000  // when opening the subkey (if provided) force open from the 32bit location (only one SUBKEY_WOW64* flag can be set or RegGetValue will fail with ERROR_INVALID_PARAMETER)
#define __SPRT_RRF_WOW64_MASK         0x00030000

#define __SPRT_RRF_NOEXPAND           0x10000000  // do not automatically expand environment strings if value is of type REG_EXPAND_SZ
#define __SPRT_RRF_ZEROONFAILURE      0x20000000  // if pvData is not NULL, set content to all zeros on failure

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
#define __SPRT_VARIANT_TRUE          ((VARIANT_BOOL)-1)
#define __SPRT_VARIANT_FALSE         ((VARIANT_BOOL)0)


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
	CLSCTX_PS_DLL = 0x80000000,

	CLSCTX_ALL = CLSCTX_INPROC_SERVER | CLSCTX_INPROC_HANDLER
		| CLSCTX_LOCAL_SERVER | CLSCTX_REMOTE_SERVER
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
#define __SPRT_DUPLICATE_CLOSE_SOURCE      0x00000001  
#define __SPRT_DUPLICATE_SAME_ACCESS       0x00000002  

#define __SPRT_WAIT_FAILED       ((DWORD)0xFFFFFFFF)
#define __SPRT_WAIT_OBJECT_0     ((__SPRT_STATUS_WAIT_0 ) + 0 )
#define __SPRT_WAIT_ABANDONED    ((__SPRT_STATUS_ABANDONED_WAIT_0 ) + 0 )
#define __SPRT_WAIT_ABANDONED_0  ((__SPRT_STATUS_ABANDONED_WAIT_0 ) + 0 )

#define __SPRT_WAIT_IO_COMPLETION                  __SPRT_STATUS_USER_APC

/* Special values */
#define __SPRT_INVALID_HANDLE_VALUE        ((HANDLE)(ULONG_PTR)-1)
#define __SPRT_INVALID_FILE_ATTRIBUTES     ((DWORD)-1)
#define __SPRT_INFINITE                    0xFFFFFFFF

#define __SPRT_HANDLE_FLAG_INHERIT             0x00000001
#define __SPRT_HANDLE_FLAG_PROTECT_FROM_CLOSE  0x00000002
 
/* Local Memory Flags */
#define __SPRT_LMEM_FIXED          0x0000
#define __SPRT_LMEM_MOVEABLE       0x0002
#define __SPRT_LMEM_NOCOMPACT      0x0010
#define __SPRT_LMEM_NODISCARD      0x0020
#define __SPRT_LMEM_ZEROINIT       0x0040
#define __SPRT_LMEM_MODIFY         0x0080
#define __SPRT_LMEM_DISCARDABLE    0x0F00
#define __SPRT_LMEM_VALID_FLAGS    0x0F72
#define __SPRT_LMEM_INVALID_HANDLE 0x8000

#define __SPRT_STACK_SIZE_PARAM_IS_A_RESERVATION   0x00010000

#define __SPRT_SEM_FAILCRITICALERRORS      0x0001
#define __SPRT_SEM_NOGPFAULTERRORBOX       0x0002
#define __SPRT_SEM_NOALIGNMENTFAULTEXCEPT  0x0004
#define __SPRT_SEM_NOOPENFILEERRORBOX      0x8000

#define __SPRT_ObjectBasicInformation           0

#define __SPRT_GENERIC_READ                     (0x80000000L)
#define __SPRT_GENERIC_WRITE                    (0x40000000L)
#define __SPRT_GENERIC_EXECUTE                  (0x20000000L)
#define __SPRT_GENERIC_ALL                      (0x10000000L)

//#define DELETE                           (0x00010000L)
#define __SPRT_DELETE                          (0x00010000L)
#define __SPRT_READ_CONTROL                     (0x00020000L)
#define __SPRT_WRITE_DAC                        (0x00040000L)
#define __SPRT_WRITE_OWNER                      (0x00080000L)
#define __SPRT_SYNCHRONIZE                      (0x00100000L)

#define __SPRT_STANDARD_RIGHTS_REQUIRED         (0x000F0000L)
#define __SPRT_STANDARD_RIGHTS_READ             (__SPRT_READ_CONTROL)
#define __SPRT_STANDARD_RIGHTS_WRITE            (__SPRT_READ_CONTROL)
#define __SPRT_STANDARD_RIGHTS_EXECUTE          (__SPRT_READ_CONTROL)
#define __SPRT_STANDARD_RIGHTS_ALL              (0x001F0000L)
#define __SPRT_SPECIFIC_RIGHTS_ALL              (0x0000FFFFL)

#define __SPRT_KEY_QUERY_VALUE         (0x0001)
#define __SPRT_KEY_SET_VALUE           (0x0002)
#define __SPRT_KEY_CREATE_SUB_KEY      (0x0004)
#define __SPRT_KEY_ENUMERATE_SUB_KEYS  (0x0008)
#define __SPRT_KEY_NOTIFY              (0x0010)
#define __SPRT_KEY_CREATE_LINK         (0x0020)
#define __SPRT_KEY_WOW64_32KEY         (0x0200)
#define __SPRT_KEY_WOW64_64KEY         (0x0100)
#define __SPRT_KEY_WOW64_RES           (0x0300)

#define __SPRT_KEY_READ                ((__SPRT_STANDARD_RIGHTS_READ       |\
                                  __SPRT_KEY_QUERY_VALUE            |\
                                  __SPRT_KEY_ENUMERATE_SUB_KEYS     |\
                                  __SPRT_KEY_NOTIFY)                 \
                                  &                           \
                                 (~__SPRT_SYNCHRONIZE))


#define __SPRT_KEY_WRITE               ((__SPRT_STANDARD_RIGHTS_WRITE      |\
                                  __SPRT_KEY_SET_VALUE              |\
                                  __SPRT_KEY_CREATE_SUB_KEY)         \
                                  &                           \
                                 (~__SPRT_SYNCHRONIZE))

#define __SPRT_KEY_EXECUTE             ((__SPRT_KEY_READ)                   \
                                  &                           \
                                 (~__SPRT_SYNCHRONIZE))

#define __SPRT_KEY_ALL_ACCESS          ((__SPRT_STANDARD_RIGHTS_ALL        |\
                                  __SPRT_KEY_QUERY_VALUE            |\
                                  __SPRT_KEY_SET_VALUE              |\
                                  __SPRT_KEY_CREATE_SUB_KEY         |\
                                  __SPRT_KEY_ENUMERATE_SUB_KEYS     |\
                                  __SPRT_KEY_NOTIFY                 |\
                                  __SPRT_KEY_CREATE_LINK)            \
                                  &                           \
                                 (~__SPRT_SYNCHRONIZE))

#define __SPRT_VS_FFI_STRUCVERSION     0x00010000L
#define __SPRT_VS_FFI_FILEFLAGSMASK    0x0000003FL

#define __SPRT_VFT_UNKNOWN             0x00000000L
#define __SPRT_VFT_APP                 0x00000001L
#define __SPRT_VFT_DLL                 0x00000002L
#define __SPRT_VFT_DRV                 0x00000003L
#define __SPRT_VFT_FONT                0x00000004L
#define __SPRT_VFT_VXD                 0x00000005L
#define __SPRT_VFT_STATIC_LIB          0x00000007L
// clang-format on

/* Processor architecture constants */
#define __SPRT_PROCESSOR_ARCHITECTURE_INTEL            0
#define __SPRT_PROCESSOR_ARCHITECTURE_MIPS             1
#define __SPRT_PROCESSOR_ARCHITECTURE_ALPHA            2
#define __SPRT_PROCESSOR_ARCHITECTURE_PPC              3
#define __SPRT_PROCESSOR_ARCHITECTURE_SHX              4
#define __SPRT_PROCESSOR_ARCHITECTURE_ARM              5
#define __SPRT_PROCESSOR_ARCHITECTURE_IA64             6
#define __SPRT_PROCESSOR_ARCHITECTURE_ALPHA64          7
#define __SPRT_PROCESSOR_ARCHITECTURE_MSIL             8
#define __SPRT_PROCESSOR_ARCHITECTURE_AMD64            9
#define __SPRT_PROCESSOR_ARCHITECTURE_IA32_ON_WIN64    10
#define __SPRT_PROCESSOR_ARCHITECTURE_NEUTRAL          11
#define __SPRT_PROCESSOR_ARCHITECTURE_ARM64            12
#define __SPRT_PROCESSOR_ARCHITECTURE_ARM32_ON_WIN64   13
#define __SPRT_PROCESSOR_ARCHITECTURE_IA32_ON_ARM64    14

/* Computer name limits */
#define __SPRT_MAX_COMPUTERNAME_LENGTH  15

/* HRESULT helper macros */
#define __SPRT_SUCCEEDED(hr)   (((long)(hr)) >= 0)
#define __SPRT_FAILED(hr)      (((long)(hr)) < 0)
#define __SPRT_HRESULT_CODE(hr) ((hr) & 0xFFFF)

/* ============================================================ */
/* Memory Functions (memoryapi.h, winbase.h)                    */
/* ============================================================ */

/* Access rights */
#define __SPRT_TOKEN_ASSIGN_PRIMARY    (0x0001)
#define __SPRT_TOKEN_DUPLICATE         (0x0002)
#define __SPRT_TOKEN_IMPERSONATE       (0x0004)
#define __SPRT_TOKEN_QUERY             (0x0008)
#define __SPRT_TOKEN_QUERY_SOURCE      (0x0010)
#define __SPRT_TOKEN_ADJUST_PRIVILEGES (0x0020)
#define __SPRT_TOKEN_ADJUST_GROUPS     (0x0040)
#define __SPRT_TOKEN_ADJUST_DEFAULT    (0x0080)
#define __SPRT_TOKEN_ADJUST_SESSIONID  (0x0100)

#define __SPRT_SECTION_QUERY                0x0001
#define __SPRT_SECTION_MAP_WRITE            0x0002
#define __SPRT_SECTION_MAP_READ             0x0004
#define __SPRT_SECTION_MAP_EXECUTE          0x0008
#define __SPRT_SECTION_EXTEND_SIZE          0x0010
#define __SPRT_SECTION_MAP_EXECUTE_EXPLICIT 0x0020 // not included in SECTION_ALL_ACCESS

#define __SPRT_SECTION_ALL_ACCESS (__SPRT_STANDARD_RIGHTS_REQUIRED|__SPRT_SECTION_QUERY|\
                            __SPRT_SECTION_MAP_WRITE |      \
                            __SPRT_SECTION_MAP_READ |       \
                            __SPRT_SECTION_MAP_EXECUTE |    \
                            __SPRT_SECTION_EXTEND_SIZE)

#define __SPRT_VER_PLATFORM_WIN32s             0
#define __SPRT_VER_PLATFORM_WIN32_WINDOWS      1
#define __SPRT_VER_PLATFORM_WIN32_NT           2

#define __SPRT_FILE_TYPE_UNKNOWN   0x0000
#define __SPRT_FILE_TYPE_DISK      0x0001
#define __SPRT_FILE_TYPE_CHAR      0x0002
#define __SPRT_FILE_TYPE_PIPE      0x0003
#define __SPRT_FILE_TYPE_REMOTE    0x8000

// Processor-feature identifiers for IsProcessorFeaturePresent (winnt.h PF_* values).
#define __SPRT_PF_FLOATING_POINT_PRECISION_ERRATA           0
#define __SPRT_PF_FLOATING_POINT_EMULATED                   1
#define __SPRT_PF_COMPARE_EXCHANGE_DOUBLE                   2
#define __SPRT_PF_MMX_INSTRUCTIONS_AVAILABLE                3
#define __SPRT_PF_PPC_MOVEMEM_64BIT_OK                      4
#define __SPRT_PF_ALPHA_BYTE_INSTRUCTIONS                   5
#define __SPRT_PF_XMMI_INSTRUCTIONS_AVAILABLE               6
#define __SPRT_PF_3DNOW_INSTRUCTIONS_AVAILABLE              7
#define __SPRT_PF_RDTSC_INSTRUCTION_AVAILABLE               8
#define __SPRT_PF_PAE_ENABLED                               9
#define __SPRT_PF_XMMI64_INSTRUCTIONS_AVAILABLE            10
#define __SPRT_PF_SSE_DAZ_MODE_AVAILABLE                   11
#define __SPRT_PF_NX_ENABLED                               12
#define __SPRT_PF_SSE3_INSTRUCTIONS_AVAILABLE              13
#define __SPRT_PF_COMPARE_EXCHANGE128                      14
#define __SPRT_PF_COMPARE64_EXCHANGE128                    15
#define __SPRT_PF_CHANNELS_ENABLED                         16
#define __SPRT_PF_XSAVE_ENABLED                            17
#define __SPRT_PF_ARM_VFP_32_REGISTERS_AVAILABLE           18
#define __SPRT_PF_ARM_NEON_INSTRUCTIONS_AVAILABLE          19
#define __SPRT_PF_SECOND_LEVEL_ADDRESS_TRANSLATION         20
#define __SPRT_PF_VIRT_FIRMWARE_ENABLED                    21
#define __SPRT_PF_RDWRFSGSBASE_AVAILABLE                   22
#define __SPRT_PF_FASTFAIL_AVAILABLE                       23
#define __SPRT_PF_ARM_DIVIDE_INSTRUCTION_AVAILABLE         24
#define __SPRT_PF_ARM_64BIT_LOADSTORE_ATOMIC               25
#define __SPRT_PF_ARM_EXTERNAL_CACHE_AVAILABLE             26
#define __SPRT_PF_ARM_FMAC_INSTRUCTIONS_AVAILABLE          27
#define __SPRT_PF_RDRAND_INSTRUCTION_AVAILABLE             28
#define __SPRT_PF_ARM_V8_INSTRUCTIONS_AVAILABLE            29
#define __SPRT_PF_ARM_V8_CRYPTO_INSTRUCTIONS_AVAILABLE     30
#define __SPRT_PF_ARM_V8_CRC32_INSTRUCTIONS_AVAILABLE      31
#define __SPRT_PF_RDTSCP_INSTRUCTION_AVAILABLE             32
#define __SPRT_PF_RDPID_INSTRUCTION_AVAILABLE              33
#define __SPRT_PF_ARM_V81_ATOMIC_INSTRUCTIONS_AVAILABLE    34
#define __SPRT_PF_MONITORX_INSTRUCTION_AVAILABLE           35
#define __SPRT_PF_SSSE3_INSTRUCTIONS_AVAILABLE             36
#define __SPRT_PF_SSE4_1_INSTRUCTIONS_AVAILABLE            37
#define __SPRT_PF_SSE4_2_INSTRUCTIONS_AVAILABLE            38
#define __SPRT_PF_AVX_INSTRUCTIONS_AVAILABLE               39
#define __SPRT_PF_AVX2_INSTRUCTIONS_AVAILABLE              40
#define __SPRT_PF_AVX512F_INSTRUCTIONS_AVAILABLE           41
#define __SPRT_PF_ERMS_AVAILABLE                           42
#define __SPRT_PF_ARM_V82_DP_INSTRUCTIONS_AVAILABLE        43
#define __SPRT_PF_ARM_V83_JSCVT_INSTRUCTIONS_AVAILABLE     44
#define __SPRT_PF_ARM_V83_LRCPC_INSTRUCTIONS_AVAILABLE     45
#define __SPRT_PF_ARM_SVE_INSTRUCTIONS_AVAILABLE           46
#define __SPRT_PF_ARM_SVE2_INSTRUCTIONS_AVAILABLE          47
#define __SPRT_PF_ARM_SVE2_1_INSTRUCTIONS_AVAILABLE        48
#define __SPRT_PF_ARM_SVE_AES_INSTRUCTIONS_AVAILABLE       49
#define __SPRT_PF_ARM_SVE_PMULL128_INSTRUCTIONS_AVAILABLE  50
#define __SPRT_PF_ARM_SVE_BITPERM_INSTRUCTIONS_AVAILABLE   51
#define __SPRT_PF_ARM_SVE_BF16_INSTRUCTIONS_AVAILABLE      52
#define __SPRT_PF_ARM_SVE_EBF16_INSTRUCTIONS_AVAILABLE     53
#define __SPRT_PF_ARM_SVE_B16B16_INSTRUCTIONS_AVAILABLE    54
#define __SPRT_PF_ARM_SVE_SHA3_INSTRUCTIONS_AVAILABLE      55
#define __SPRT_PF_ARM_SVE_SM4_INSTRUCTIONS_AVAILABLE       56
#define __SPRT_PF_ARM_SVE_I8MM_INSTRUCTIONS_AVAILABLE      57
#define __SPRT_PF_ARM_SVE_F32MM_INSTRUCTIONS_AVAILABLE     58
#define __SPRT_PF_ARM_SVE_F64MM_INSTRUCTIONS_AVAILABLE     59
#define __SPRT_PF_BMI2_INSTRUCTIONS_AVAILABLE              60
#define __SPRT_PF_MOVDIR64B_INSTRUCTION_AVAILABLE          61
#define __SPRT_PF_ARM_LSE2_AVAILABLE                       62
#define __SPRT_PF_RESERVED_FEATURE                         63
#define __SPRT_PF_ARM_SHA3_INSTRUCTIONS_AVAILABLE          64
#define __SPRT_PF_ARM_SHA512_INSTRUCTIONS_AVAILABLE        65
#define __SPRT_PF_ARM_V82_I8MM_INSTRUCTIONS_AVAILABLE      66
#define __SPRT_PF_ARM_V82_FP16_INSTRUCTIONS_AVAILABLE      67
#define __SPRT_PF_ARM_V86_BF16_INSTRUCTIONS_AVAILABLE      68
#define __SPRT_PF_ARM_V86_EBF16_INSTRUCTIONS_AVAILABLE     69
#define __SPRT_PF_ARM_SME_INSTRUCTIONS_AVAILABLE           70
#define __SPRT_PF_ARM_SME2_INSTRUCTIONS_AVAILABLE          71
#define __SPRT_PF_ARM_SME2_1_INSTRUCTIONS_AVAILABLE        72
#define __SPRT_PF_ARM_SME2_2_INSTRUCTIONS_AVAILABLE        73
#define __SPRT_PF_ARM_SME_AES_INSTRUCTIONS_AVAILABLE       74
#define __SPRT_PF_ARM_SME_SBITPERM_INSTRUCTIONS_AVAILABLE  75
#define __SPRT_PF_ARM_SME_SF8MM4_INSTRUCTIONS_AVAILABLE    76
#define __SPRT_PF_ARM_SME_SF8MM8_INSTRUCTIONS_AVAILABLE    77
#define __SPRT_PF_ARM_SME_SF8DP2_INSTRUCTIONS_AVAILABLE    78
#define __SPRT_PF_ARM_SME_SF8DP4_INSTRUCTIONS_AVAILABLE    79
#define __SPRT_PF_ARM_SME_SF8FMA_INSTRUCTIONS_AVAILABLE    80
#define __SPRT_PF_ARM_SME_F8F32_INSTRUCTIONS_AVAILABLE     81
#define __SPRT_PF_ARM_SME_F8F16_INSTRUCTIONS_AVAILABLE     82
#define __SPRT_PF_ARM_SME_F16F16_INSTRUCTIONS_AVAILABLE    83
#define __SPRT_PF_ARM_SME_B16B16_INSTRUCTIONS_AVAILABLE    84
#define __SPRT_PF_ARM_SME_F64F64_INSTRUCTIONS_AVAILABLE    85
#define __SPRT_PF_ARM_SME_I16I64_INSTRUCTIONS_AVAILABLE    86
#define __SPRT_PF_ARM_SME_LUTv2_INSTRUCTIONS_AVAILABLE     87
#define __SPRT_PF_ARM_SME_FA64_INSTRUCTIONS_AVAILABLE      88

#endif // SPRT_WRAPPERS_WINDOWS_ABI_CONSTANTS_H_
