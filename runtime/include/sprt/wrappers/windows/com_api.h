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

#ifndef SPRT_WRAPPERS_WINDOWS_COM_API_H_
#define SPRT_WRAPPERS_WINDOWS_COM_API_H_

#include <sprt/wrappers/windows/structures.h>
#include <sprt/wrappers/windows/constants.h>

// clang-format off
#define RPC_C_AUTHZ_NONE    0
#define RPC_C_AUTHZ_NAME    1
#define RPC_C_AUTHZ_DCE     2
#define RPC_C_AUTHZ_DEFAULT 0xffffffff

#define RPC_C_AUTHN_NONE          0
#define RPC_C_AUTHN_DCE_PRIVATE   1
#define RPC_C_AUTHN_DCE_PUBLIC    2
#define RPC_C_AUTHN_DEC_PUBLIC    4
#define RPC_C_AUTHN_GSS_NEGOTIATE 9
#define RPC_C_AUTHN_WINNT        10
#define RPC_C_AUTHN_GSS_SCHANNEL 14
#define RPC_C_AUTHN_GSS_KERBEROS 16
#define RPC_C_AUTHN_DPA          17
#define RPC_C_AUTHN_MSN          18

#define RPC_C_AUTHN_LEVEL_DEFAULT       0
#define RPC_C_AUTHN_LEVEL_NONE          1
#define RPC_C_AUTHN_LEVEL_CONNECT       2
#define RPC_C_AUTHN_LEVEL_CALL          3
#define RPC_C_AUTHN_LEVEL_PKT           4
#define RPC_C_AUTHN_LEVEL_PKT_INTEGRITY 5
#define RPC_C_AUTHN_LEVEL_PKT_PRIVACY   6

#define RPC_C_IMP_LEVEL_DEFAULT      0
#define RPC_C_IMP_LEVEL_ANONYMOUS    1
#define RPC_C_IMP_LEVEL_IDENTIFY     2
#define RPC_C_IMP_LEVEL_IMPERSONATE  3
#define RPC_C_IMP_LEVEL_DELEGATE     4


typedef enum {
	KF_FLAG_DEFAULT = 0x00000000,

	// When called from packaged app, LocalAppData/RoamingAppData folders are redirected to
	// app private locations that match the paths returned from WinRT API:
	//   Windows.Storage.ApplicationData.Current.{LocalFolder|RoamingFolder}
	// A few other folders are redirected to subdirectories of LocalAppData.
	KF_FLAG_FORCE_APP_DATA_REDIRECTION = 0x00080000,

	// When running in a Centennial process, some file system locations are redirected to
	// package-specific locations by the file system. Specifying this flag will cause the
	// target of the redirection to be returned for these locations. This is useful in
	// cases where the real location in the file system needs to be known.
	KF_FLAG_RETURN_FILTER_REDIRECTION_TARGET = 0x00040000,

	// New form for KF_FLAG_FORCE_APPCONTAINER_REDIRECTION and KF_FLAG_NO_APPCONTAINER_REDIRECTION
	// The new forms should be used and will exhibit the same behavior as the old flags.

	// When running inside an AppContainer process, or when providing an AppContainer token,
	// some folders are redirected to AppContainer-specific locations within the packages location.
	// Specifying this flag will force this redirection for folders that are not normally
	// redirected for centennial processes. Useful for sharing files between between UWA and
	// Centennial apps that are within the same package.
	KF_FLAG_FORCE_PACKAGE_REDIRECTION = 0x00020000,

	// When running inside a packaged process (Centennial,  AppContainer, etc.), or when providing
	// a packaged process token, some folders are redirected to package-specific locations.
	// Specifying this flag will disable redirection on locations where it applied,
	// and instead return the non-redirected location.
	KF_FLAG_NO_PACKAGE_REDIRECTION = 0x00010000,

	// see comment for KF_FLAG_FORCE_PACKAGE_REDIRECTION
	KF_FLAG_FORCE_APPCONTAINER_REDIRECTION = 0x00020000,

	// When running inside an AppContainer, or when poviding an AppContainer token, specifying this flag will prevent redirection to AppContainer
	// folders and instead return the path that would be returned when not running inside an AppContainer
	KF_FLAG_NO_APPCONTAINER_REDIRECTION = 0x00010000,

	// Make sure that the folder already exists or create it and apply security specified in folder definition
	// If folder can not be created then function will return failure and no folder path (IDList) will be returned
	// If folder is located on the network the function may take long time to execute
	KF_FLAG_CREATE = 0x00008000,

	// If this flag is specified then the folder path is returned and no verification is performed
	// Use this flag is you want to get folders path (IDList) and do not need to verify folders existence
	//
	// If this flag is NOT specified then Known Folder API will try to verify that the folder exists
	//     If folder does not exist or can not be accessed then function will return failure and no folder path (IDList) will be returned
	//     If folder is located on the network the function may take long time to execute
	KF_FLAG_DONT_VERIFY = 0x00004000,

	// Set folder path as is and do not try to substitute parts of the path with environments variables.
	// If flag is not specified then Known Folder will try to replace parts of the path with some
	// known environment variables (%USERPROFILE%, %APPDATA% etc.)
	KF_FLAG_DONT_UNEXPAND = 0x00002000,

	// Get file system based IDList if available. If the flag is not specified the Known Folder API
	// will try to return aliased IDList by default. Example for FOLDERID_Documents -
	// Aliased - [desktop]\[user]\[Documents] - exact location is determined by shell namespace layout and might change
	// Non aliased - [desktop]\[computer]\[disk_c]\[users]\[user]\[Documents] - location is determined by folder location in the file system
	KF_FLAG_NO_ALIAS = 0x00001000,

	// Initialize the folder with desktop.ini settings
	// If folder can not be initialized then function will return failure and no folder path will be returned
	// If folder is located on the network the function may take long time to execute
	KF_FLAG_INIT = 0x00000800,

	// Get the default path, will also verify folder existence unless KF_FLAG_DONT_VERIFY is also specified
	KF_FLAG_DEFAULT_PATH = 0x00000400,

	// Get the not-parent-relative default path. Only valid with KF_FLAG_DEFAULT_PATH
	KF_FLAG_NOT_PARENT_RELATIVE = 0x00000200,

	// Build simple IDList
	KF_FLAG_SIMPLE_IDLIST = 0x00000100,

	// only return the aliased IDLists, dont fallback to file system path
	KF_FLAG_ALIAS_ONLY = 0x80000000,
} KNOWN_FOLDER_FLAG;
// clang-format on

typedef enum tagCOINITBASE {
	COINITBASE_MULTITHREADED = 0x0, // OLE calls objects on any thread.
} COINITBASE;

typedef enum tagCOINIT {
	COINIT_APARTMENTTHREADED = 0x2, // Apartment model
	COINIT_MULTITHREADED = COINITBASE_MULTITHREADED,
	COINIT_DISABLE_OLE1DDE = 0x4, // Dont use DDE for Ole1 support.
	COINIT_SPEED_OVER_MEMORY = 0x8, // Trade memory for speed.
} COINIT;

typedef enum KF_CATEGORY {
	KF_CATEGORY_VIRTUAL = 1,
	KF_CATEGORY_FIXED = 2,
	KF_CATEGORY_COMMON = 3,
	KF_CATEGORY_PERUSER = 4
} KF_CATEGORY;

enum _KF_DEFINITION_FLAGS {
	KFDF_LOCAL_REDIRECT_ONLY = 0x2,
	KFDF_ROAMABLE = 0x4,
	KFDF_PRECREATE = 0x8,
	KFDF_STREAM = 0x10,
	KFDF_PUBLISHEXPANDEDPATH = 0x20,
	KFDF_NO_REDIRECT_UI = 0x40
};

typedef enum tagEOLE_AUTHENTICATION_CAPABILITIES {
	EOAC_NONE = 0,
	EOAC_MUTUAL_AUTH = 0x1,
	EOAC_STATIC_CLOAKING = 0x20,
	EOAC_DYNAMIC_CLOAKING = 0x40,
	EOAC_ANY_AUTHORITY = 0x80,
	EOAC_MAKE_FULLSIC = 0x100,
	EOAC_DEFAULT = 0x800,
	EOAC_SECURE_REFS = 0x2,
	EOAC_ACCESS_CONTROL = 0x4,
	EOAC_APPID = 0x8,
	EOAC_DYNAMIC = 0x10,
	EOAC_REQUIRE_FULLSIC = 0x200,
	EOAC_AUTO_IMPERSONATE = 0x400,
	EOAC_DISABLE_AAA = 0x1000,
	EOAC_NO_CUSTOM_MARSHAL = 0x2000,
	EOAC_RESERVED1 = 0x4000
} EOLE_AUTHENTICATION_CAPABILITIES;

typedef GUID FOLDERTYPEID;
typedef GUID KNOWNFOLDERID; // used to identify Known Folders within the system

typedef GUID IID;
typedef IID *LPIID;

typedef WCHAR OLECHAR;
typedef OLECHAR *LPOLESTR;
typedef const OLECHAR *LPCOLESTR;
typedef CLSID *LPCLSID;
typedef DWORD KF_DEFINITION_FLAGS;

typedef struct KNOWNFOLDER_DEFINITION {
	KF_CATEGORY category;
	LPWSTR pszName;
	LPWSTR pszDescription;
	KNOWNFOLDERID fidParent;
	LPWSTR pszRelativePath;
	LPWSTR pszParsingName;
	LPWSTR pszTooltip;
	LPWSTR pszLocalizedName;
	LPWSTR pszIcon;
	LPWSTR pszSecurity;
	DWORD dwAttributes;
	KF_DEFINITION_FLAGS kfdFlags;
	FOLDERTYPEID ftidType;
} KNOWNFOLDER_DEFINITION;

#ifdef __cplusplus
#define REFKNOWNFOLDERID const KNOWNFOLDERID &
#else // !__cplusplus
#define REFKNOWNFOLDERID const KNOWNFOLDERID *
#endif // __cplusplus

__SPRT_BEGIN_DECL

__SPRT_WIN_IMPORT WINAPI HRESULT CoInitializeSecurity(PSECURITY_DESCRIPTOR pSecDesc, LONG cAuthSvc,
		SOLE_AUTHENTICATION_SERVICE *asAuthSvc, void *pReserved1, DWORD dwAuthnLevel,
		DWORD dwImpLevel, void *pAuthList, DWORD dwCapabilities, void *pReserved3);

__SPRT_WIN_IMPORT WINAPI HRESULT CoCreateInstance(REFCLSID rclsid, IUnknown *pUnkOuter,
		DWORD dwClsContext, REFIID riid, LPVOID *ppv);

__SPRT_WIN_IMPORT WINAPI HRESULT CoSetProxyBlanket(IUnknown *pProxy, DWORD dwAuthnSvc,
		DWORD dwAuthzSvc, LPCWSTR pServerPrincName, DWORD dwAuthnLevel, DWORD dwImpLevel,
		void *pAuthIdentity, DWORD dwCapabilities);

__SPRT_WIN_IMPORT WINAPI HRESULT CoInitializeEx(LPVOID pvReserved, DWORD dwCoInit);

__SPRT_WIN_IMPORT WINAPI void CoUninitialize();

__SPRT_WIN_IMPORT WINAPI void CoTaskMemFree(LPVOID pv);

__SPRT_WIN_IMPORT WINAPI HRESULT SHGetKnownFolderPath(REFKNOWNFOLDERID rfid, DWORD dwFlags,
		HANDLE hToken, PWSTR *ppszPath);

__SPRT_WIN_IMPORT WINAPI void VariantInit(VARIANTARG *pvarg);

__SPRT_WIN_IMPORT WINAPI HRESULT VariantClear(VARIANTARG *pvarg);

__SPRT_WIN_IMPORT WINAPI HRESULT VariantCopy(VARIANTARG *pvargDest, const VARIANTARG *pvargSrc);

__SPRT_WIN_IMPORT WINAPI void SysFreeString(BSTR bstrString);

__SPRT_WIN_IMPORT WINAPI HRESULT StringFromCLSID(REFCLSID rclsid, LPOLESTR *lplpsz);

__SPRT_WIN_IMPORT WINAPI HRESULT CLSIDFromString(LPCOLESTR lpsz, LPCLSID pclsid);

__SPRT_WIN_IMPORT WINAPI HRESULT IIDFromString(LPCOLESTR lpsz, LPIID lpiid);

__SPRT_END_DECL

#endif
