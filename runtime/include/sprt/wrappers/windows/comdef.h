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

// Freestanding replacement for the MSVC <comdef.h> / <comip.h> / <comutil.h>
// trio, providing just enough of the COM support library for LLVM's
// WindowsDriver/MSVCPaths.cpp (the Visual Studio Setup Configuration COM query):
// the `_com_ptr_t` smart pointer, `_COM_SMARTPTR_TYPEDEF`, the `_bstr_t` BSTR
// wrapper, `_com_error`, and the com-error-handler hooks.
//
// Design notes vs. the real MS headers:
//  * The real headers put `_com_issue_error`/`_com_raise_error`/
//    `_set_com_error_handler` in comsupp(w).lib and let `_com_raise_error`
//    `throw _com_error`. LLVM builds with exceptions disabled and only ever runs
//    COM operations with a no-op error handler installed, so here everything is
//    header-only and the default handler is a non-throwing fatal report (never
//    reached in the LLVM code path). Keeping it inline avoids a support-lib dep.
//  * `_bstr_t` is a minimal single-representation BSTR owner — enough for the
//    GetAddress()/wide-string-conversion/length usage MSVCPaths needs, not the
//    full dual-char refcounted MS type.

#ifndef SPRT_WRAPPERS_WINDOWS_COMDEF_H_
#define SPRT_WRAPPERS_WINDOWS_COMDEF_H_

#include <sprt/wrappers/windows/windows.h>
#include <sprt/wrappers/windows/com_cxx.hpp>
#include <sprt/wrappers/windows/com_api.h>

#ifdef __cplusplus

// ---- generic Windows SDK spellings the COM interface headers rely on --------
// (comdef.h is, by convention, pulled in ahead of the *.h COM interface headers,
// so these belong here for a freestanding sysroot that lacks the full SDK.)
#ifndef EXTERN_C
#define EXTERN_C extern "C"
#endif
#ifndef STDMETHODIMP
#define STDMETHODIMP HRESULT STDMETHODCALLTYPE
#define STDMETHODIMP_(type) type STDMETHODCALLTYPE
#endif
#ifndef STDMETHOD
#define STDMETHOD(method) virtual HRESULT STDMETHODCALLTYPE method
#define STDMETHOD_(type, method) virtual type STDMETHODCALLTYPE method
#endif
#ifndef MAXUINT
#define MAXUINT ((UINT)~((UINT)0))
#endif

#ifndef __SPRT_COMDEF_LCID_DEFINED
#define __SPRT_COMDEF_LCID_DEFINED
typedef DWORD LCID;
#endif

// clang-format off
// Common COM HRESULT codes (the freestanding winerror.h only carries the ERROR_*
// range; the E_* facility-null codes live here — each guarded independently since
// a few of them are already provided by other headers).
#ifndef E_NOTIMPL
#define E_NOTIMPL ((HRESULT)0x80004001L)
#endif
#ifndef E_NOINTERFACE
#define E_NOINTERFACE ((HRESULT)0x80004002L)
#endif
#ifndef E_POINTER
#define E_POINTER ((HRESULT)0x80004003L)
#endif
#ifndef E_ABORT
#define E_ABORT ((HRESULT)0x80004004L)
#endif
#ifndef E_FAIL
#define E_FAIL ((HRESULT)0x80004005L)
#endif
#ifndef E_UNEXPECTED
#define E_UNEXPECTED ((HRESULT)0x8000FFFFL)
#endif
#ifndef E_ACCESSDENIED
#define E_ACCESSDENIED ((HRESULT)0x80070005L)
#endif
#ifndef E_HANDLE
#define E_HANDLE ((HRESULT)0x80070006L)
#endif
#ifndef E_OUTOFMEMORY
#define E_OUTOFMEMORY ((HRESULT)0x8007000EL)
#endif
#ifndef E_INVALIDARG
#define E_INVALIDARG ((HRESULT)0x80070057L)
#endif
#ifndef S_FALSE
#define S_FALSE ((HRESULT)1L)
#endif
// clang-format on

// IErrorInfo is only referenced through a pointer in the error-handler signature;
// a forward declaration is all MSVCPaths.cpp needs.
struct IErrorInfo;

// ---- com error object -------------------------------------------------------
class _com_error {
public:
	explicit _com_error(HRESULT hr, IErrorInfo *perrinfo = nullptr) noexcept
	: m_hresult(hr), m_perrinfo(perrinfo) { }

	_com_error(const _com_error &other) noexcept
	: m_hresult(other.m_hresult), m_perrinfo(other.m_perrinfo) { }

	_com_error &operator=(const _com_error &other) noexcept {
		m_hresult = other.m_hresult;
		m_perrinfo = other.m_perrinfo;
		return *this;
	}

	HRESULT Error() const noexcept { return m_hresult; }
	WORD WCode() const noexcept {
		// mirror MS: HRESULTs in [0x80040200,0x80040000+0xFFFF] carry a 16-bit code
		return (m_hresult >= 0x8004'0200 && m_hresult <= 0x8004'FFFF)
				? (WORD)(m_hresult - 0x8004'0000)
				: (WORD)0;
	}
	IErrorInfo *ErrorInfo() const noexcept { return m_perrinfo; }

private:
	HRESULT m_hresult;
	IErrorInfo *m_perrinfo;
};

// ---- com error handler hooks ------------------------------------------------
typedef void(__stdcall *_com_error_handler_t)(HRESULT hr, IErrorInfo *perrinfo);

void __stdcall _com_raise_error(HRESULT hr, IErrorInfo *perrinfo = nullptr) noexcept;

inline _com_error_handler_t &__sprt_com_error_handler() noexcept {
	static _com_error_handler_t s_handler = &_com_raise_error;
	return s_handler;
}

// Default handler: exceptions are disabled in this runtime, so it cannot
// `throw _com_error` the way MS does. It is never reached on the LLVM path
// (MSVCPaths installs a no-op handler around all its COM calls and checks every
// HRESULT itself); keep it a quiet no-op so a stray failure degrades to a null
// interface pointer rather than aborting.
inline void __stdcall _com_raise_error(HRESULT hr, IErrorInfo *perrinfo) noexcept {
	(void)hr;
	(void)perrinfo;
}

inline _com_error_handler_t _set_com_error_handler(_com_error_handler_t pHandler) noexcept {
	_com_error_handler_t previous = __sprt_com_error_handler();
	__sprt_com_error_handler() = pHandler ? pHandler : &_com_raise_error;
	return previous;
}

inline void _com_issue_error(HRESULT hr) noexcept { __sprt_com_error_handler()(hr, nullptr); }

inline void _com_issue_errorex(HRESULT hr, IUnknown *, REFIID) noexcept {
	__sprt_com_error_handler()(hr, nullptr);
}

// ---- tiny local type traits (kept self-contained so the freestanding Windows
//      headers need not pull in <type_traits>) ------------------------------
template <typename _A, typename _B>
struct __com_is_same {
	static constexpr bool value = false;
};
template <typename _A>
struct __com_is_same<_A, _A> {
	static constexpr bool value = true;
};
template <bool _B, typename _T = void>
struct __com_enable_if { };
template <typename _T>
struct __com_enable_if<true, _T> {
	typedef _T type;
};

// ---- _com_IIID: binds an interface type to its IID ---------------------------
template <typename _Interface, const IID *_IID>
struct _com_IIID {
	typedef _Interface Interface;
	static const IID &GetIID() noexcept { return *_IID; }
};

// ---- _com_ptr_t: reference-counted COM interface smart pointer ---------------
template <typename _IIID>
class _com_ptr_t {
public:
	typedef typename _IIID::Interface Interface;

	static const IID &GetIID() noexcept { return _IIID::GetIID(); }

	_com_ptr_t() noexcept : m_pInterface(nullptr) { }
	_com_ptr_t(decltype(nullptr)) noexcept : m_pInterface(nullptr) { }

	_com_ptr_t(Interface *pInterface, bool fAddRef = true) noexcept : m_pInterface(pInterface) {
		if (m_pInterface != nullptr && fAddRef) {
			m_pInterface->AddRef();
		}
	}

	_com_ptr_t(const _com_ptr_t &cp) noexcept : m_pInterface(cp.m_pInterface) { _AddRef(); }

	// QueryInterface-constructor: build this interface out of another COM pointer
	// (of a different interface type). Enables the MSVCPaths idiom
	// `ISetupConfiguration2Ptr(Query)`. Restricted to a *different* _IIID so it
	// never competes with the copy constructor — and, when Interface happens to be
	// IUnknown, never collides with the `Interface*` constructor above.
	template <typename _OtherIIID,
			typename = typename __com_enable_if<!__com_is_same<_OtherIIID, _IIID>::value>::type>
	explicit _com_ptr_t(const _com_ptr_t<_OtherIIID> &cp) : m_pInterface(nullptr) {
		HRESULT hr = _QueryFrom(cp.GetInterfacePtr());
		if (FAILED(hr) && hr != E_NOINTERFACE) {
			_com_issue_error(hr);
		}
	}

	~_com_ptr_t() noexcept { _Release(); }

	_com_ptr_t &operator=(Interface *pInterface) noexcept {
		if (pInterface != m_pInterface) {
			Interface *pOld = m_pInterface;
			m_pInterface = pInterface;
			_AddRef();
			if (pOld != nullptr) {
				pOld->Release();
			}
		}
		return *this;
	}

	_com_ptr_t &operator=(const _com_ptr_t &cp) noexcept { return operator=(cp.m_pInterface); }

	_com_ptr_t &operator=(decltype(nullptr)) noexcept {
		_Release();
		return *this;
	}

	// CoCreateInstance a coclass and hold the requested interface.
	HRESULT CreateInstance(REFCLSID rclsid, IUnknown *pOuter = nullptr,
			DWORD dwClsContext = CLSCTX_ALL) noexcept {
		_Release();
		return CoCreateInstance(rclsid, pOuter, dwClsContext, GetIID(),
				reinterpret_cast<void **>(&m_pInterface));
	}

	Interface *operator->() const noexcept { return m_pInterface; }
	operator Interface *() const noexcept { return m_pInterface; }
	Interface &operator*() const noexcept { return *m_pInterface; }

	// Release-then-expose-slot, so `&ptr` can be passed as an [out] Interface**.
	Interface **operator&() noexcept {
		_Release();
		return &m_pInterface;
	}

	Interface *GetInterfacePtr() const noexcept { return m_pInterface; }

	bool operator!() const noexcept { return m_pInterface == nullptr; }
	explicit operator bool() const noexcept { return m_pInterface != nullptr; }

	bool operator==(Interface *p) const noexcept { return m_pInterface == p; }
	bool operator!=(Interface *p) const noexcept { return m_pInterface != p; }
	bool operator==(decltype(nullptr)) const noexcept { return m_pInterface == nullptr; }
	bool operator!=(decltype(nullptr)) const noexcept { return m_pInterface != nullptr; }

	void Release() noexcept { _Release(); }
	void Attach(Interface *p) noexcept {
		_Release();
		m_pInterface = p;
	}
	Interface *Detach() noexcept {
		Interface *p = m_pInterface;
		m_pInterface = nullptr;
		return p;
	}

private:
	void _AddRef() noexcept {
		if (m_pInterface != nullptr) {
			m_pInterface->AddRef();
		}
	}
	void _Release() noexcept {
		if (m_pInterface != nullptr) {
			m_pInterface->Release();
			m_pInterface = nullptr;
		}
	}
	HRESULT _QueryFrom(IUnknown *pUnk) noexcept {
		_Release();
		if (pUnk == nullptr) {
			return E_POINTER;
		}
		return pUnk->QueryInterface(GetIID(), reinterpret_cast<void **>(&m_pInterface));
	}

	Interface *m_pInterface;
};

// Declares `InterfacePtr` as the smart pointer for `Interface` (bound to `iid`).
#define _COM_SMARTPTR_TYPEDEF(Interface, iid) \
	typedef _com_ptr_t<_com_IIID<Interface, &iid>> Interface##Ptr

// ---- _bstr_t: minimal BSTR owner --------------------------------------------
class _bstr_t {
public:
	_bstr_t() noexcept : m_str(nullptr) { }

	_bstr_t(const wchar_t *s) : m_str(s != nullptr ? SysAllocString(s) : nullptr) { }

	_bstr_t(const _bstr_t &other) : m_str(_Copy(other.m_str)) { }

	_bstr_t(_bstr_t &&other) noexcept : m_str(other.m_str) { other.m_str = nullptr; }

	~_bstr_t() noexcept { _Free(); }

	_bstr_t &operator=(const _bstr_t &other) {
		if (this != &other) {
			_Free();
			m_str = _Copy(other.m_str);
		}
		return *this;
	}

	_bstr_t &operator=(const wchar_t *s) {
		_Free();
		m_str = (s != nullptr) ? SysAllocString(s) : nullptr;
		return *this;
	}

	_bstr_t &operator=(_bstr_t &&other) noexcept {
		if (this != &other) {
			_Free();
			m_str = other.m_str;
			other.m_str = nullptr;
		}
		return *this;
	}

	// Frees any current value and returns the writable slot for a COM [out] BSTR.
	BSTR *GetAddress() noexcept {
		_Free();
		return &m_str;
	}
	BSTR &GetBSTR() noexcept { return m_str; }

	operator const wchar_t *() const noexcept { return m_str; }
	operator wchar_t *() const noexcept { return const_cast<wchar_t *>(m_str); }

	unsigned int length() const noexcept { return m_str != nullptr ? SysStringLen(m_str) : 0; }

	bool operator!() const noexcept { return m_str == nullptr; }

private:
	static BSTR _Copy(BSTR s) { return s != nullptr ? SysAllocString(s) : nullptr; }
	void _Free() noexcept {
		if (m_str != nullptr) {
			SysFreeString(m_str);
			m_str = nullptr;
		}
	}

	BSTR m_str;
};

typedef _bstr_t bstr_t;

#endif // __cplusplus

#endif // SPRT_WRAPPERS_WINDOWS_COMDEF_H_
