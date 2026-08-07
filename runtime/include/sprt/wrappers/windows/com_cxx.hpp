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

#ifndef SPRT_WRAPPERS_WINDOWS_COM_CXX_H_
#define SPRT_WRAPPERS_WINDOWS_COM_CXX_H_

#include <sprt/wrappers/windows/com_api.h>
// SIGDN / FILEOPENDIALOGOPTIONS / COMDLG_FILTERSPEC: the value tables IFileDialog's methods are
// spelled in terms of. They are ABI, so they live under abi/ and are pinned against the SDK there.
#include <sprt/wrappers/windows/abi/shlobj.h>

#ifdef __cplusplus

#define DECLSPEC_UUID(x)    __declspec(uuid(x))
#define DECLSPEC_NOVTABLE   __declspec(novtable)
#define MIDL_INTERFACE(x)   struct DECLSPEC_UUID(x) DECLSPEC_NOVTABLE
#define BEGIN_INTERFACE
#define END_INTERFACE
#define STDMETHODCALLTYPE       __stdcall
#define STDMETHODVCALLTYPE      __cdecl

typedef long CIMTYPE;

typedef struct _SHITEMID {
	USHORT cb;
	BYTE abID[1];
} SHITEMID;

typedef struct _ITEMIDLIST {
	SHITEMID mkid;
} ITEMIDLIST;

typedef ITEMIDLIST ITEMIDLIST_ABSOLUTE;
typedef ITEMIDLIST_ABSOLUTE *PIDLIST_ABSOLUTE;
typedef const ITEMIDLIST_ABSOLUTE *PCIDLIST_ABSOLUTE;

// A single-level ("child") id relative to some folder, and an array of them: what
// SHOpenFolderAndSelectItems takes to say which entries to highlight.
typedef ITEMIDLIST ITEMID_CHILD;
typedef const ITEMID_CHILD *PCUITEMID_CHILD;
typedef PCUITEMID_CHILD const *PCUITEMID_CHILD_ARRAY;

struct IWbemCallResult;
struct IWbemObjectSink;
struct IWbemQualifierSet;
struct IKnownFolder;

enum _KF_REDIRECT_FLAGS {
	KF_REDIRECT_USER_EXCLUSIVE = 0x1,
	KF_REDIRECT_COPY_SOURCE_DACL = 0x2,
	KF_REDIRECT_OWNER_USER = 0x4,
	KF_REDIRECT_SET_OWNER_EXPLICIT = 0x8,
	KF_REDIRECT_CHECK_ONLY = 0x10,
	KF_REDIRECT_WITH_UI = 0x20,
	KF_REDIRECT_UNPIN = 0x40,
	KF_REDIRECT_PIN = 0x80,
	KF_REDIRECT_COPY_CONTENTS = 0x200,
	KF_REDIRECT_DEL_SOURCE_CONTENTS = 0x400,
	KF_REDIRECT_EXCLUDE_ALL_KNOWN_SUBFOLDERS = 0x800
};
typedef DWORD KF_REDIRECT_FLAGS;

enum _KF_REDIRECTION_CAPABILITIES {
	KF_REDIRECTION_CAPABILITIES_ALLOW_ALL = 0xff,
	KF_REDIRECTION_CAPABILITIES_REDIRECTABLE = 0x1,
	KF_REDIRECTION_CAPABILITIES_DENY_ALL = 0xf'ff00,
	KF_REDIRECTION_CAPABILITIES_DENY_POLICY_REDIRECTED = 0x100,
	KF_REDIRECTION_CAPABILITIES_DENY_POLICY = 0x200,
	KF_REDIRECTION_CAPABILITIES_DENY_PERMISSIONS = 0x400
};
typedef DWORD KF_REDIRECTION_CAPABILITIES;

typedef enum FFFP_MODE {
	FFFP_EXACTMATCH = 0,
	FFFP_NEARESTPARENTMATCH = (FFFP_EXACTMATCH + 1)
} FFFP_MODE;

typedef enum tag_WBEM_TIMEOUT_TYPE {
	WBEM_NO_WAIT = 0,
	WBEM_INFINITE = 0xffff'ffff
} WBEM_TIMEOUT_TYPE;

typedef enum tag_WBEM_CONDITION_FLAG_TYPE {
	WBEM_FLAG_ALWAYS = 0,
	WBEM_FLAG_ONLY_IF_TRUE = 0x1,
	WBEM_FLAG_ONLY_IF_FALSE = 0x2,
	WBEM_FLAG_ONLY_IF_IDENTICAL = 0x3,
	WBEM_MASK_PRIMARY_CONDITION = 0x3,
	WBEM_FLAG_KEYS_ONLY = 0x4,
	WBEM_FLAG_REFS_ONLY = 0x8,
	WBEM_FLAG_LOCAL_ONLY = 0x10,
	WBEM_FLAG_PROPAGATED_ONLY = 0x20,
	WBEM_FLAG_SYSTEM_ONLY = 0x30,
	WBEM_FLAG_NONSYSTEM_ONLY = 0x40,
	WBEM_MASK_CONDITION_ORIGIN = 0x70,
	WBEM_FLAG_CLASS_OVERRIDES_ONLY = 0x100,
	WBEM_FLAG_CLASS_LOCAL_AND_OVERRIDES = 0x200,
	WBEM_MASK_CLASS_CONDITION = 0x300
} WBEM_CONDITION_FLAG_TYPE;

MIDL_INTERFACE("00000000-0000-0000-C000-000000000046")
IUnknown {
public:
	BEGIN_INTERFACE
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(
			/* [in] */ REFIID riid,
			/* [iid_is][out] */ void **ppvObject) = 0;

	virtual ULONG STDMETHODCALLTYPE AddRef(void) = 0;

	virtual ULONG STDMETHODCALLTYPE Release(void) = 0;

	template <class Q>
	HRESULT STDMETHODCALLTYPE QueryInterface(Q * *pp) {
		return QueryInterface(__uuidof(Q), (void **)pp);
	}

	END_INTERFACE
};


MIDL_INTERFACE("027947e1-d731-11ce-a357-000000000001")
IEnumWbemClassObject : public IUnknown {
public:
	virtual HRESULT STDMETHODCALLTYPE Reset(void) = 0;

	virtual HRESULT STDMETHODCALLTYPE Next(
			/* [in] */ long lTimeout,
			/* [in] */ ULONG uCount,
			/* [length_is][size_is][out] */ IWbemClassObject **apObjects,
			/* [out] */ ULONG *puReturned) = 0;

	virtual HRESULT STDMETHODCALLTYPE NextAsync(
			/* [in] */ ULONG uCount,
			/* [in] */ IWbemObjectSink * pSink) = 0;

	virtual HRESULT STDMETHODCALLTYPE Clone(
			/* [out] */ IEnumWbemClassObject * *ppEnum) = 0;

	virtual HRESULT STDMETHODCALLTYPE Skip(
			/* [in] */ long lTimeout,
			/* [in] */ ULONG nCount) = 0;
};

MIDL_INTERFACE("44aca674-e8fc-11d0-a07c-00c04fb68820")
IWbemContext : public IUnknown {
public:
	virtual HRESULT STDMETHODCALLTYPE Clone(
			/* [out] */ IWbemContext * *ppNewCopy) = 0;

	virtual HRESULT STDMETHODCALLTYPE GetNames(
			/* [in] */ long lFlags,
			/* [out] */ SAFEARRAY **pNames) = 0;

	virtual HRESULT STDMETHODCALLTYPE BeginEnumeration(
			/* [in] */ long lFlags) = 0;

	virtual HRESULT STDMETHODCALLTYPE Next(
			/* [in] */ long lFlags,
			/* [out] */ BSTR *pstrName,
			/* [out] */ VARIANT *pValue) = 0;

	virtual HRESULT STDMETHODCALLTYPE EndEnumeration(void) = 0;

	virtual HRESULT STDMETHODCALLTYPE SetValue(
			/* [string][in] */ LPCWSTR wszName,
			/* [in] */ long lFlags,
			/* [in] */ VARIANT *pValue) = 0;

	virtual HRESULT STDMETHODCALLTYPE GetValue(
			/* [string][in] */ LPCWSTR wszName,
			/* [in] */ long lFlags,
			/* [out] */ VARIANT *pValue) = 0;

	virtual HRESULT STDMETHODCALLTYPE DeleteValue(
			/* [string][in] */ LPCWSTR wszName,
			/* [in] */ long lFlags) = 0;

	virtual HRESULT STDMETHODCALLTYPE DeleteAll(void) = 0;
};

MIDL_INTERFACE("dc12a687-737f-11cf-884d-00aa004b2e24")
IWbemLocator : public IUnknown {
public:
	virtual HRESULT STDMETHODCALLTYPE ConnectServer(
			/* [in] */ const BSTR strNetworkResource,
			/* [in] */ const BSTR strUser,
			/* [in] */ const BSTR strPassword,
			/* [in] */ const BSTR strLocale,
			/* [in] */ long lSecurityFlags,
			/* [in] */ const BSTR strAuthority,
			/* [in] */ IWbemContext *pCtx,
			/* [out] */ IWbemServices **ppNamespace) = 0;
};

MIDL_INTERFACE("9556dc99-828c-11cf-a37e-00aa003240c7")
IWbemServices : public IUnknown {
public:
	virtual HRESULT STDMETHODCALLTYPE OpenNamespace(
			/* [in] */ const BSTR strNamespace,
			/* [in] */ long lFlags,
			/* [in] */ IWbemContext *pCtx,
			/* [unique][in][out] */ IWbemServices **ppWorkingNamespace,
			/* [unique][in][out] */ IWbemCallResult **ppResult) = 0;

	virtual HRESULT STDMETHODCALLTYPE CancelAsyncCall(
			/* [in] */ IWbemObjectSink * pSink) = 0;

	virtual HRESULT STDMETHODCALLTYPE QueryObjectSink(
			/* [in] */ long lFlags,
			/* [out] */ IWbemObjectSink **ppResponseHandler) = 0;

	virtual HRESULT STDMETHODCALLTYPE GetObject(
			/* [in] */ const BSTR strObjectPath,
			/* [in] */ long lFlags,
			/* [in] */ IWbemContext *pCtx,
			/* [unique][in][out] */ IWbemClassObject **ppObject,
			/* [unique][in][out] */ IWbemCallResult **ppCallResult) = 0;

	virtual HRESULT STDMETHODCALLTYPE GetObjectAsync(
			/* [in] */ const BSTR strObjectPath,
			/* [in] */ long lFlags,
			/* [in] */ IWbemContext *pCtx,
			/* [in] */ IWbemObjectSink *pResponseHandler) = 0;

	virtual HRESULT STDMETHODCALLTYPE PutClass(
			/* [in] */ IWbemClassObject * pObject,
			/* [in] */ long lFlags,
			/* [in] */ IWbemContext *pCtx,
			/* [unique][in][out] */ IWbemCallResult **ppCallResult) = 0;

	virtual HRESULT STDMETHODCALLTYPE PutClassAsync(
			/* [in] */ IWbemClassObject * pObject,
			/* [in] */ long lFlags,
			/* [in] */ IWbemContext *pCtx,
			/* [in] */ IWbemObjectSink *pResponseHandler) = 0;

	virtual HRESULT STDMETHODCALLTYPE DeleteClass(
			/* [in] */ const BSTR strClass,
			/* [in] */ long lFlags,
			/* [in] */ IWbemContext *pCtx,
			/* [unique][in][out] */ IWbemCallResult **ppCallResult) = 0;

	virtual HRESULT STDMETHODCALLTYPE DeleteClassAsync(
			/* [in] */ const BSTR strClass,
			/* [in] */ long lFlags,
			/* [in] */ IWbemContext *pCtx,
			/* [in] */ IWbemObjectSink *pResponseHandler) = 0;

	virtual HRESULT STDMETHODCALLTYPE CreateClassEnum(
			/* [in] */ const BSTR strSuperclass,
			/* [in] */ long lFlags,
			/* [in] */ IWbemContext *pCtx,
			/* [out] */ IEnumWbemClassObject **ppEnum) = 0;

	virtual HRESULT STDMETHODCALLTYPE CreateClassEnumAsync(
			/* [in] */ const BSTR strSuperclass,
			/* [in] */ long lFlags,
			/* [in] */ IWbemContext *pCtx,
			/* [in] */ IWbemObjectSink *pResponseHandler) = 0;

	virtual HRESULT STDMETHODCALLTYPE PutInstance(
			/* [in] */ IWbemClassObject * pInst,
			/* [in] */ long lFlags,
			/* [in] */ IWbemContext *pCtx,
			/* [unique][in][out] */ IWbemCallResult **ppCallResult) = 0;

	virtual HRESULT STDMETHODCALLTYPE PutInstanceAsync(
			/* [in] */ IWbemClassObject * pInst,
			/* [in] */ long lFlags,
			/* [in] */ IWbemContext *pCtx,
			/* [in] */ IWbemObjectSink *pResponseHandler) = 0;

	virtual HRESULT STDMETHODCALLTYPE DeleteInstance(
			/* [in] */ const BSTR strObjectPath,
			/* [in] */ long lFlags,
			/* [in] */ IWbemContext *pCtx,
			/* [unique][in][out] */ IWbemCallResult **ppCallResult) = 0;

	virtual HRESULT STDMETHODCALLTYPE DeleteInstanceAsync(
			/* [in] */ const BSTR strObjectPath,
			/* [in] */ long lFlags,
			/* [in] */ IWbemContext *pCtx,
			/* [in] */ IWbemObjectSink *pResponseHandler) = 0;

	virtual HRESULT STDMETHODCALLTYPE CreateInstanceEnum(
			/* [in] */ const BSTR strFilter,
			/* [in] */ long lFlags,
			/* [in] */ IWbemContext *pCtx,
			/* [out] */ IEnumWbemClassObject **ppEnum) = 0;

	virtual HRESULT STDMETHODCALLTYPE CreateInstanceEnumAsync(
			/* [in] */ const BSTR strFilter,
			/* [in] */ long lFlags,
			/* [in] */ IWbemContext *pCtx,
			/* [in] */ IWbemObjectSink *pResponseHandler) = 0;

	virtual HRESULT STDMETHODCALLTYPE ExecQuery(
			/* [in] */ const BSTR strQueryLanguage,
			/* [in] */ const BSTR strQuery,
			/* [in] */ long lFlags,
			/* [in] */ IWbemContext *pCtx,
			/* [out] */ IEnumWbemClassObject **ppEnum) = 0;

	virtual HRESULT STDMETHODCALLTYPE ExecQueryAsync(
			/* [in] */ const BSTR strQueryLanguage,
			/* [in] */ const BSTR strQuery,
			/* [in] */ long lFlags,
			/* [in] */ IWbemContext *pCtx,
			/* [in] */ IWbemObjectSink *pResponseHandler) = 0;

	virtual HRESULT STDMETHODCALLTYPE ExecNotificationQuery(
			/* [in] */ const BSTR strQueryLanguage,
			/* [in] */ const BSTR strQuery,
			/* [in] */ long lFlags,
			/* [in] */ IWbemContext *pCtx,
			/* [out] */ IEnumWbemClassObject **ppEnum) = 0;

	virtual HRESULT STDMETHODCALLTYPE ExecNotificationQueryAsync(
			/* [in] */ const BSTR strQueryLanguage,
			/* [in] */ const BSTR strQuery,
			/* [in] */ long lFlags,
			/* [in] */ IWbemContext *pCtx,
			/* [in] */ IWbemObjectSink *pResponseHandler) = 0;

	virtual HRESULT STDMETHODCALLTYPE ExecMethod(
			/* [in] */ const BSTR strObjectPath,
			/* [in] */ const BSTR strMethodName,
			/* [in] */ long lFlags,
			/* [in] */ IWbemContext *pCtx,
			/* [in] */ IWbemClassObject *pInParams,
			/* [unique][in][out] */ IWbemClassObject **ppOutParams,
			/* [unique][in][out] */ IWbemCallResult **ppCallResult) = 0;

	virtual HRESULT STDMETHODCALLTYPE ExecMethodAsync(
			/* [in] */ const BSTR strObjectPath,
			/* [in] */ const BSTR strMethodName,
			/* [in] */ long lFlags,
			/* [in] */ IWbemContext *pCtx,
			/* [in] */ IWbemClassObject *pInParams,
			/* [in] */ IWbemObjectSink *pResponseHandler) = 0;
};


MIDL_INTERFACE("dc12a681-737f-11cf-884d-00aa004b2e24")
IWbemClassObject : public IUnknown {
public:
	virtual HRESULT STDMETHODCALLTYPE GetQualifierSet(
			/* [out] */ IWbemQualifierSet * *ppQualSet) = 0;

	virtual HRESULT STDMETHODCALLTYPE Get(
			/* [string][in] */ LPCWSTR wszName,
			/* [in] */ long lFlags,
			/* [unique][in][out] */ VARIANT *pVal,
			/* [unique][in][out] */ CIMTYPE *pType,
			/* [unique][in][out] */ long *plFlavor) = 0;

	virtual HRESULT STDMETHODCALLTYPE Put(
			/* [string][in] */ LPCWSTR wszName,
			/* [in] */ long lFlags,
			/* [in] */ VARIANT *pVal,
			/* [in] */ CIMTYPE Type) = 0;

	virtual HRESULT STDMETHODCALLTYPE Delete(
			/* [string][in] */ LPCWSTR wszName) = 0;

	virtual HRESULT STDMETHODCALLTYPE GetNames(
			/* [string][in] */ LPCWSTR wszQualifierName,
			/* [in] */ long lFlags,
			/* [in] */ VARIANT *pQualifierVal,
			/* [out] */ SAFEARRAY **pNames) = 0;

	virtual HRESULT STDMETHODCALLTYPE BeginEnumeration(
			/* [in] */ long lEnumFlags) = 0;

	virtual HRESULT STDMETHODCALLTYPE Next(
			/* [in] */ long lFlags,
			/* [unique][in][out] */ BSTR *strName,
			/* [unique][in][out] */ VARIANT *pVal,
			/* [unique][in][out] */ CIMTYPE *pType,
			/* [unique][in][out] */ long *plFlavor) = 0;

	virtual HRESULT STDMETHODCALLTYPE EndEnumeration(void) = 0;

	virtual HRESULT STDMETHODCALLTYPE GetPropertyQualifierSet(
			/* [string][in] */ LPCWSTR wszProperty,
			/* [out] */ IWbemQualifierSet * *ppQualSet) = 0;

	virtual HRESULT STDMETHODCALLTYPE Clone(
			/* [out] */ IWbemClassObject * *ppCopy) = 0;

	virtual HRESULT STDMETHODCALLTYPE GetObjectText(
			/* [in] */ long lFlags,
			/* [out] */ BSTR *pstrObjectText) = 0;

	virtual HRESULT STDMETHODCALLTYPE SpawnDerivedClass(
			/* [in] */ long lFlags,
			/* [out] */ IWbemClassObject **ppNewClass) = 0;

	virtual HRESULT STDMETHODCALLTYPE SpawnInstance(
			/* [in] */ long lFlags,
			/* [out] */ IWbemClassObject **ppNewInstance) = 0;

	virtual HRESULT STDMETHODCALLTYPE CompareTo(
			/* [in] */ long lFlags,
			/* [in] */ IWbemClassObject *pCompareTo) = 0;

	virtual HRESULT STDMETHODCALLTYPE GetPropertyOrigin(
			/* [string][in] */ LPCWSTR wszName,
			/* [out] */ BSTR * pstrClassName) = 0;

	virtual HRESULT STDMETHODCALLTYPE InheritsFrom(
			/* [in] */ LPCWSTR strAncestor) = 0;

	virtual HRESULT STDMETHODCALLTYPE GetMethod(
			/* [string][in] */ LPCWSTR wszName,
			/* [in] */ long lFlags,
			/* [out] */ IWbemClassObject **ppInSignature,
			/* [out] */ IWbemClassObject **ppOutSignature) = 0;

	virtual HRESULT STDMETHODCALLTYPE PutMethod(
			/* [string][in] */ LPCWSTR wszName,
			/* [in] */ long lFlags,
			/* [in] */ IWbemClassObject *pInSignature,
			/* [in] */ IWbemClassObject *pOutSignature) = 0;

	virtual HRESULT STDMETHODCALLTYPE DeleteMethod(
			/* [string][in] */ LPCWSTR wszName) = 0;

	virtual HRESULT STDMETHODCALLTYPE BeginMethodEnumeration(
			/* [in] */ long lEnumFlags) = 0;

	virtual HRESULT STDMETHODCALLTYPE NextMethod(
			/* [in] */ long lFlags,
			/* [unique][in][out] */ BSTR *pstrName,
			/* [unique][in][out] */ IWbemClassObject **ppInSignature,
			/* [unique][in][out] */ IWbemClassObject **ppOutSignature) = 0;

	virtual HRESULT STDMETHODCALLTYPE EndMethodEnumeration(void) = 0;

	virtual HRESULT STDMETHODCALLTYPE GetMethodQualifierSet(
			/* [string][in] */ LPCWSTR wszMethod,
			/* [out] */ IWbemQualifierSet * *ppQualSet) = 0;

	virtual HRESULT STDMETHODCALLTYPE GetMethodOrigin(
			/* [string][in] */ LPCWSTR wszMethodName,
			/* [out] */ BSTR * pstrClassName) = 0;
};


MIDL_INTERFACE("3AA7AF7E-9B36-420c-A8E3-F77D4674A488")
IKnownFolder : public IUnknown {
public:
	virtual HRESULT STDMETHODCALLTYPE GetId(
			/* [out] */ KNOWNFOLDERID * pkfid) = 0;

	virtual HRESULT STDMETHODCALLTYPE GetCategory(
			/* [out] */ KF_CATEGORY * pCategory) = 0;

	virtual HRESULT STDMETHODCALLTYPE GetShellItem(
			/* [in] */ DWORD dwFlags,
			/* [in] */ REFIID riid,
			/* [iid_is][out] */ void **ppv) = 0;

	virtual HRESULT STDMETHODCALLTYPE GetPath(
			/* [in] */ DWORD dwFlags,
			/* [string][out] */ LPWSTR * ppszPath) = 0;

	virtual HRESULT STDMETHODCALLTYPE SetPath(
			/* [in] */ DWORD dwFlags,
			/* [string][in] */ LPCWSTR pszPath) = 0;

	virtual HRESULT STDMETHODCALLTYPE GetIDList(
			/* [in] */ DWORD dwFlags,
			/* [out] */ PIDLIST_ABSOLUTE * ppidl) = 0;

	virtual HRESULT STDMETHODCALLTYPE GetFolderType(
			/* [out] */ FOLDERTYPEID * pftid) = 0;

	virtual HRESULT STDMETHODCALLTYPE GetRedirectionCapabilities(
			/* [out] */ KF_REDIRECTION_CAPABILITIES * pCapabilities) = 0;

	virtual HRESULT STDMETHODCALLTYPE GetFolderDefinition(
			/* [out] */ KNOWNFOLDER_DEFINITION * pKFD) = 0;
};

MIDL_INTERFACE("8BE2D872-86AA-4d47-B776-32CCA40C7018")
IKnownFolderManager : public IUnknown {
public:
	virtual HRESULT STDMETHODCALLTYPE FolderIdFromCsidl(
			/* [in] */ int nCsidl,
			/* [out] */ KNOWNFOLDERID *pfid) = 0;

	virtual HRESULT STDMETHODCALLTYPE FolderIdToCsidl(
			/* [in] */ REFKNOWNFOLDERID rfid,
			/* [out] */ int *pnCsidl) = 0;

	virtual HRESULT STDMETHODCALLTYPE GetFolderIds(
			/* [size_is][size_is][out] */ KNOWNFOLDERID * *ppKFId,
			/* [out][in] */ UINT * pCount) = 0;

	virtual HRESULT STDMETHODCALLTYPE GetFolder(
			/* [in] */ REFKNOWNFOLDERID rfid,
			/* [out] */ IKnownFolder * *ppkf) = 0;

	virtual HRESULT STDMETHODCALLTYPE GetFolderByName(
			/* [string][in] */ LPCWSTR pszCanonicalName,
			/* [out] */ IKnownFolder * *ppkf) = 0;

	virtual HRESULT STDMETHODCALLTYPE RegisterFolder(
			/* [in] */ REFKNOWNFOLDERID rfid,
			/* [in] */ const KNOWNFOLDER_DEFINITION *pKFD) = 0;

	virtual HRESULT STDMETHODCALLTYPE UnregisterFolder(
			/* [in] */ REFKNOWNFOLDERID rfid) = 0;

	virtual HRESULT STDMETHODCALLTYPE FindFolderFromPath(
			/* [string][in] */ LPCWSTR pszPath,
			/* [in] */ FFFP_MODE mode,
			/* [out] */ IKnownFolder * *ppkf) = 0;

	virtual HRESULT STDMETHODCALLTYPE FindFolderFromIDList(
			/* [in] */ PCIDLIST_ABSOLUTE pidl,
			/* [out] */ IKnownFolder * *ppkf) = 0;

	virtual /* [local] */ HRESULT STDMETHODCALLTYPE Redirect(
			/* [annotation][in] */
			REFKNOWNFOLDERID rfid,
			/* [annotation][unique][in] */
			HANDLE hwnd,
			/* [annotation][in] */
			KF_REDIRECT_FLAGS flags,
			/* [annotation][string][unique][in] */
			LPCWSTR pszTargetPath,
			/* [annotation][in] */
			UINT cFolders,
			/* [annotation][unique][size_is][in] */
			const KNOWNFOLDERID *pExclusion,
			/* [annotation][string][out] */
			LPWSTR *ppszError) = 0;
};

template <typename T>
void **IID_PPV_ARGS_Helper(T **__pp) {
	(void)static_cast<IUnknown *>(*__pp); // compile-time check: T derives from IUnknown
	return reinterpret_cast<void **>(__pp);
}
#define IID_PPV_ARGS(ppType) __uuidof(**(ppType)), IID_PPV_ARGS_Helper(ppType)

// ---- IShellItem / IFileOperation ([shobjidl_core]) ------------------------
MIDL_INTERFACE("43826d1e-e718-42ee-bc55-a1e261c37bfe")
IShellItem : public IUnknown {
public:
	virtual HRESULT STDMETHODCALLTYPE BindToHandler(void *pbc, const GUID &bhid, REFIID riid,
			void **ppv) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetParent(IShellItem * *ppsi) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetDisplayName(DWORD sigdnName, LPWSTR * ppszName) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetAttributes(ULONG sfgaoMask, ULONG * psfgaoAttribs) = 0;
	virtual HRESULT STDMETHODCALLTYPE Compare(IShellItem * psi, DWORD hint, int *piOrder) = 0;
};

MIDL_INTERFACE("947aab5f-0a5c-4c13-b4d6-4bf7836fc9f8")
IFileOperation : public IUnknown {
public:
	virtual HRESULT STDMETHODCALLTYPE Advise(void *pfops, DWORD *pdwCookie) = 0;
	virtual HRESULT STDMETHODCALLTYPE Unadvise(DWORD dwCookie) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetOperationFlags(DWORD dwOperationFlags) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetProgressMessage(LPCWSTR pszMessage) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetProgressDialog(void *popd) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetProperties(void *pproparray) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetOwnerWindow(HANDLE hwndOwner) = 0;
	virtual HRESULT STDMETHODCALLTYPE ApplyPropertiesToItem(IShellItem * psiItem) = 0;
	virtual HRESULT STDMETHODCALLTYPE ApplyPropertiesToItems(void *punkItems) = 0;
	virtual HRESULT STDMETHODCALLTYPE RenameItem(IShellItem * psiItem, LPCWSTR pszNewName,
			void *pfopsItem) = 0;
	virtual HRESULT STDMETHODCALLTYPE RenameItems(void *pUnkItems, LPCWSTR pszNewName) = 0;
	virtual HRESULT STDMETHODCALLTYPE MoveItem(IShellItem * psiItem,
			IShellItem * psiDestinationFolder, LPCWSTR pszNewName, void *pfopsItem) = 0;
	virtual HRESULT STDMETHODCALLTYPE MoveItems(void *punkItems,
			IShellItem *psiDestinationFolder) = 0;
	virtual HRESULT STDMETHODCALLTYPE CopyItem(IShellItem * psiItem,
			IShellItem * psiDestinationFolder, LPCWSTR pszCopyName, void *pfopsItem) = 0;
	virtual HRESULT STDMETHODCALLTYPE CopyItems(void *punkItems,
			IShellItem *psiDestinationFolder) = 0;
	virtual HRESULT STDMETHODCALLTYPE DeleteItem(IShellItem * psiItem, void *pfopsItem) = 0;
	virtual HRESULT STDMETHODCALLTYPE DeleteItems(void *punkItems) = 0;
	virtual HRESULT STDMETHODCALLTYPE NewItem(IShellItem * psiDestinationFolder,
			DWORD dwFileAttributes, LPCWSTR pszName, LPCWSTR pszTemplateName, void *pfopsItem) = 0;
	virtual HRESULT STDMETHODCALLTYPE PerformOperations(void) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetAnyOperationsAborted(BOOL * pfAnyOperationsAborted) = 0;
};

// coclass {3ad05575-8857-4850-9277-11b85bdb8e09}. inline (C++17) so every including
// TU shares one definition and it is not flagged unused.
inline constexpr GUID CLSID_FileOperation = {0x3ad0'5575, 0x8857, 0x4850,
	{0x92, 0x77, 0x11, 0xb8, 0x5b, 0xdb, 0x8e, 0x09}};

// ---- IFileDialog and friends ([shobjidl_core]) ----------------------------
//
// SIGDN, FOS_* and COMDLG_FILTERSPEC live in abi/shlobj.h, pulled in above: they are plain values
// and layouts that have to be pinned against the SDK, which is what the abi/ half is for.

MIDL_INTERFACE("b4db1657-70d7-485e-8e3e-6fcb5a5c1802")
IModalWindow : public IUnknown {
public:
	// Runs its own modal message loop and does not return until the dialog closes. S_OK means the
	// user accepted; HRESULT_FROM_WIN32(ERROR_CANCELLED) means they dismissed it.
	virtual HRESULT STDMETHODCALLTYPE Show(HANDLE hwndOwner) = 0;
};

MIDL_INTERFACE("42f85136-db7e-439c-85f1-e4075d135fc8")
IFileDialog : public IModalWindow {
public:
	virtual HRESULT STDMETHODCALLTYPE SetFileTypes(UINT cFileTypes,
			const COMDLG_FILTERSPEC *rgFilterSpec) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetFileTypeIndex(UINT iFileType) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetFileTypeIndex(UINT * piFileType) = 0;
	virtual HRESULT STDMETHODCALLTYPE Advise(void *pfde, DWORD *pdwCookie) = 0;
	virtual HRESULT STDMETHODCALLTYPE Unadvise(DWORD dwCookie) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetOptions(DWORD fos) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetOptions(DWORD * pfos) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetDefaultFolder(IShellItem * psi) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetFolder(IShellItem * psi) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetFolder(IShellItem * *ppsi) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetCurrentSelection(IShellItem * *ppsi) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetFileName(LPCWSTR pszName) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetFileName(LPWSTR * pszName) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetTitle(LPCWSTR pszTitle) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetOkButtonLabel(LPCWSTR pszText) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetFileNameLabel(LPCWSTR pszLabel) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetResult(IShellItem * *ppsi) = 0;
	virtual HRESULT STDMETHODCALLTYPE AddPlace(IShellItem * psi, int fdap) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetDefaultExtension(LPCWSTR pszDefaultExtension) = 0;
	// Dismisses the dialog from another thread; hr becomes Show()'s return value.
	virtual HRESULT STDMETHODCALLTYPE Close(HRESULT hr) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetClientGuid(const GUID &guid) = 0;
	virtual HRESULT STDMETHODCALLTYPE ClearClientData(void) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetFilter(void *pFilter) = 0;
};

MIDL_INTERFACE("b63ea76d-1f85-456f-a19c-48159efa858b")
IShellItemArray : public IUnknown {
public:
	virtual HRESULT STDMETHODCALLTYPE BindToHandler(void *pbc, const GUID &bhid, REFIID riid,
			void **ppvOut) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetPropertyStore(int flags, REFIID riid, void **ppv) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetPropertyDescriptionList(const void *keyType, REFIID riid,
			void **ppv) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetAttributes(int AttribFlags, ULONG sfgaoMask,
			ULONG *psfgaoAttribs) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetCount(DWORD * pdwNumItems) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetItemAt(DWORD dwIndex, IShellItem * *ppsi) = 0;
	virtual HRESULT STDMETHODCALLTYPE EnumItems(void **ppenumShellItems) = 0;
};

MIDL_INTERFACE("d57c7288-d4ad-4768-be02-9d969532d960")
IFileOpenDialog : public IFileDialog {
public:
	virtual HRESULT STDMETHODCALLTYPE GetResults(IShellItemArray * *ppenum) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetSelectedItems(IShellItemArray * *ppsai) = 0;
};

MIDL_INTERFACE("84bccd23-5fde-4cdb-aea4-af64b83d78ab")
IFileSaveDialog : public IFileDialog {
public:
	virtual HRESULT STDMETHODCALLTYPE SetSaveAsItem(IShellItem * psi) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetProperties(void *pStore) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetCollectedProperties(void *pList, BOOL fAppendDefault) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetProperties(void **ppStore) = 0;
	virtual HRESULT STDMETHODCALLTYPE ApplyProperties(IShellItem * psi, void *pStore, HANDLE hwnd,
			void *pSink) = 0;
};

// coclass {dc1c5a9c-e88a-4dde-a5a1-60f82a20aef7}
inline constexpr GUID CLSID_FileOpenDialog = {0xdc1c'5a9c, 0xe88a, 0x4dde,
	{0xa5, 0xa1, 0x60, 0xf8, 0x2a, 0x20, 0xae, 0xf7}};

// coclass {c0b4e2f3-ba21-4773-8dba-335ec946eb8b}
inline constexpr GUID CLSID_FileSaveDialog = {0xc0b4'e2f3, 0xba21, 0x4773,
	{0x8d, 0xba, 0x33, 0x5e, 0xc9, 0x46, 0xeb, 0x8b}};

#endif

#endif // SPRT_WRAPPERS_WINDOWS_COM_CXX_H_
