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

#endif

#endif // SPRT_WRAPPERS_WINDOWS_COM_CXX_H_
