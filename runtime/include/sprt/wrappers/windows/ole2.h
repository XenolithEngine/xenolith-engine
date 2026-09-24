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

#ifndef SPRT_WRAPPERS_WINDOWS_OLE2_H_
#define SPRT_WRAPPERS_WINDOWS_OLE2_H_

#include <sprt/wrappers/windows/basic_api.h>
#include <sprt/wrappers/windows/abi/oleidl.h>

#define DROPEFFECT_NONE __SPRT_DROPEFFECT_NONE
#define DROPEFFECT_COPY __SPRT_DROPEFFECT_COPY
#define DROPEFFECT_MOVE __SPRT_DROPEFFECT_MOVE
#define DROPEFFECT_LINK __SPRT_DROPEFFECT_LINK

#define CF_TEXT __SPRT_CF_TEXT
#define CF_UNICODETEXT __SPRT_CF_UNICODETEXT
#define CF_HDROP __SPRT_CF_HDROP

#define TYMED_HGLOBAL __SPRT_TYMED_HGLOBAL
#define DVASPECT_CONTENT __SPRT_DVASPECT_CONTENT

#define MK_SHIFT __SPRT_MK_SHIFT
#define MK_CONTROL __SPRT_MK_CONTROL
#define MK_ALT __SPRT_MK_ALT

/* The ole32 entry points are not in import.lib: a caller resolves OleInitialize, RegisterDragDrop,
RevokeDragDrop and ReleaseStgMedium from ole32.dll at run time. */

#ifdef __cplusplus

#include <sprt/wrappers/windows/com_cxx.hpp>

struct IEnumFORMATETC;
struct IAdviseSink;
struct IEnumSTATDATA;

// ---- IDataObject / IDropTarget ([objidl], [oleidl]) ------------------------
MIDL_INTERFACE("0000010e-0000-0000-C000-000000000046")
IDataObject : public IUnknown {
public:
	virtual HRESULT STDMETHODCALLTYPE GetData(FORMATETC * pformatetcIn, STGMEDIUM * pmedium) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetDataHere(FORMATETC * pformatetc, STGMEDIUM * pmedium) = 0;
	virtual HRESULT STDMETHODCALLTYPE QueryGetData(FORMATETC * pformatetc) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetCanonicalFormatEtc(FORMATETC * pformatectIn,
			FORMATETC * pformatetcOut) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetData(FORMATETC * pformatetc, STGMEDIUM * pmedium,
			BOOL fRelease) = 0;
	virtual HRESULT STDMETHODCALLTYPE EnumFormatEtc(DWORD dwDirection,
			IEnumFORMATETC * *ppenumFormatEtc) = 0;
	virtual HRESULT STDMETHODCALLTYPE DAdvise(FORMATETC * pformatetc, DWORD advf,
			IAdviseSink * pAdvSink, DWORD * pdwConnection) = 0;
	virtual HRESULT STDMETHODCALLTYPE DUnadvise(DWORD dwConnection) = 0;
	virtual HRESULT STDMETHODCALLTYPE EnumDAdvise(IEnumSTATDATA * *ppenumAdvise) = 0;
};

MIDL_INTERFACE("00000122-0000-0000-C000-000000000046")
IDropTarget : public IUnknown {
public:
	virtual HRESULT STDMETHODCALLTYPE DragEnter(IDataObject * pDataObj, DWORD grfKeyState, POINTL pt,
			DWORD * pdwEffect) = 0;
	virtual HRESULT STDMETHODCALLTYPE DragOver(DWORD grfKeyState, POINTL pt, DWORD * pdwEffect) = 0;
	virtual HRESULT STDMETHODCALLTYPE DragLeave(void) = 0;
	virtual HRESULT STDMETHODCALLTYPE Drop(IDataObject * pDataObj, DWORD grfKeyState, POINTL pt,
			DWORD * pdwEffect) = 0;
};

#endif

#endif // SPRT_WRAPPERS_WINDOWS_OLE2_H_
