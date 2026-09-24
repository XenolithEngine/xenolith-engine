/**
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#ifndef CORE_RUNTIME_PRIVATE_WINDOW_WINDOWS_SPRTWINWINDOWSDROPTARGET_H_
#define CORE_RUNTIME_PRIVATE_WINDOW_WINDOWS_SPRTWINWINDOWSDROPTARGET_H_

#include "SPRTWinWindows.h" // IWYU pragma: keep

#if SPRT_WINDOWS

#include <sprt/runtime/window/drop.h>
#include <sprt/cxx/atomic>

#include <sprt/wrappers/windows/ole2.h>

namespace sprt::window {

class WindowsWindow;

DragActions WindowsDrop_readKeys(DWORD keys);
InputModifier WindowsDrop_readModifiers(DWORD keys);

// ole32, resolved from the DLL on first use; E_NOTIMPL when it cannot be loaded
HRESULT WindowsOle_initialize();
void WindowsOle_uninitialize();
HRESULT WindowsOle_registerDragDrop(HWND, IDropTarget *);
HRESULT WindowsOle_revokeDragDrop(HWND);
void WindowsOle_releaseStgMedium(STGMEDIUM *);

/** A drag from another application, over one window. The IDataObject is valid only while OLE is
inside one of the target's calls, so its data is copied out when the drop arrives; a read before
that asks the live object. */
class WindowsDropOffer : public DropOffer {
public:
	virtual ~WindowsDropOffer();

	virtual bool init(NotNull<dispatch::Looper>, IDataObject *, DragActions allowed);

	// Context thread, inside IDropTarget::Drop: copy every representation out
	void snapshot();

	// The object may not be used after the call that handed it over returned
	void releaseData();

protected:
	struct Representation {
		String type;
		Bytes data;
	};

	virtual void handleRead(StringView type, ReadCallback &&) override;

	static Vector<String> readTypes(IDataObject *);
	static Bytes readData(IDataObject *, StringView type);

	IDataObject *_data = nullptr;
	Vector<Representation> _snapshot;
	bool _snapped = false;
};

/** The window's IDropTarget, registered with RegisterDragDrop. Its lifetime is COM's: the window
detaches itself before it goes, and a late call from OLE then answers "no drop". */
class WindowsDropTarget : public IDropTarget {
public:
	WindowsDropTarget(WindowsWindow *);
	virtual ~WindowsDropTarget() = default;

	void detach();

	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) override;
	virtual ULONG STDMETHODCALLTYPE AddRef(void) override;
	virtual ULONG STDMETHODCALLTYPE Release(void) override;

	virtual HRESULT STDMETHODCALLTYPE DragEnter(IDataObject *pDataObj, DWORD grfKeyState, POINTL pt,
			DWORD *pdwEffect) override;
	virtual HRESULT STDMETHODCALLTYPE DragOver(DWORD grfKeyState, POINTL pt,
			DWORD *pdwEffect) override;
	virtual HRESULT STDMETHODCALLTYPE DragLeave(void) override;
	virtual HRESULT STDMETHODCALLTYPE Drop(IDataObject *pDataObj, DWORD grfKeyState, POINTL pt,
			DWORD *pdwEffect) override;

protected:
	DWORD answer(DWORD allowed) const;

	sprt::atomic<ULONG> _refCount = 1;
	WindowsWindow *_window = nullptr;
	Rc<WindowsDropOffer> _offer;
};

} // namespace sprt::window

#endif

#endif // CORE_RUNTIME_PRIVATE_WINDOW_WINDOWS_SPRTWINWINDOWSDROPTARGET_H_
