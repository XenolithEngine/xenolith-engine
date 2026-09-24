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

#include "SPRTWinWindowsDropTarget.h"
#include "SPRTWinWindowsWindow.h"

#include <sprt/runtime/log.h>
#include <sprt/runtime/unicode.h>
#include <sprt/runtime/utils/dso.h>

#include <sprt/wrappers/windows/basic_api.h>
#include <sprt/wrappers/windows/comdef.h> // E_NOINTERFACE

namespace sprt::window {

namespace {

struct WindowsOle {
	using InitializeFn = HRESULT(WINAPI *)(LPVOID);
	using UninitializeFn = void(WINAPI *)(void);
	using RegisterFn = HRESULT(WINAPI *)(HWND, IDropTarget *);
	using RevokeFn = HRESULT(WINAPI *)(HWND);
	using ReleaseFn = void(WINAPI *)(STGMEDIUM *);

	static const WindowsOle &get() {
		static WindowsOle s_instance;
		return s_instance;
	}

	WindowsOle() : dso(StringView("ole32.dll")) {
		if (dso) {
			initialize = dso.sym<InitializeFn>("OleInitialize");
			uninitialize = dso.sym<UninitializeFn>("OleUninitialize");
			registerDragDrop = dso.sym<RegisterFn>("RegisterDragDrop");
			revokeDragDrop = dso.sym<RevokeFn>("RevokeDragDrop");
			releaseStgMedium = dso.sym<ReleaseFn>("ReleaseStgMedium");
		}
	}

	Dso dso;
	InitializeFn initialize = nullptr;
	UninitializeFn uninitialize = nullptr;
	RegisterFn registerDragDrop = nullptr;
	RevokeFn revokeDragDrop = nullptr;
	ReleaseFn releaseStgMedium = nullptr;
};

} // namespace

HRESULT WindowsOle_initialize() {
	auto &ole = WindowsOle::get();
	return ole.initialize ? ole.initialize(nullptr) : E_NOTIMPL;
}

void WindowsOle_uninitialize() {
	auto &ole = WindowsOle::get();
	if (ole.uninitialize) {
		ole.uninitialize();
	}
}

HRESULT WindowsOle_registerDragDrop(HWND hwnd, IDropTarget *target) {
	auto &ole = WindowsOle::get();
	return ole.registerDragDrop ? ole.registerDragDrop(hwnd, target) : E_NOTIMPL;
}

HRESULT WindowsOle_revokeDragDrop(HWND hwnd) {
	auto &ole = WindowsOle::get();
	return ole.revokeDragDrop ? ole.revokeDragDrop(hwnd) : E_NOTIMPL;
}

void WindowsOle_releaseStgMedium(STGMEDIUM *medium) {
	auto &ole = WindowsOle::get();
	if (ole.releaseStgMedium) {
		ole.releaseStgMedium(medium);
	}
}

static FORMATETC WindowsDrop_format(CLIPFORMAT cf) {
	FORMATETC fmt;
	fmt.cfFormat = cf;
	fmt.ptd = nullptr;
	fmt.dwAspect = DVASPECT_CONTENT;
	fmt.lindex = -1;
	fmt.tymed = TYMED_HGLOBAL;
	return fmt;
}

static DragActions WindowsDrop_readEffect(DWORD effect) {
	auto ret = DragActions::None;
	if (effect & DROPEFFECT_COPY) {
		ret |= DragActions::Copy;
	}
	if (effect & DROPEFFECT_MOVE) {
		ret |= DragActions::Move;
	}
	if (effect & DROPEFFECT_LINK) {
		ret |= DragActions::Link;
	}
	return ret;
}

static DWORD WindowsDrop_writeEffect(DragActions action) {
	switch (action) {
	case DragActions::Copy: return DROPEFFECT_COPY;
	case DragActions::Move: return DROPEFFECT_MOVE;
	case DragActions::Link: return DROPEFFECT_LINK;
	default: break;
	}
	return DROPEFFECT_NONE;
}

// The shell's convention: Ctrl copies, Shift moves, both link
DragActions WindowsDrop_readKeys(DWORD keys) {
	const bool ctrl = (keys & MK_CONTROL) != 0;
	const bool shift = (keys & MK_SHIFT) != 0;
	if (ctrl && shift) {
		return DragActions::Link;
	} else if (ctrl) {
		return DragActions::Copy;
	} else if (shift) {
		return DragActions::Move;
	}
	return DragActions::None;
}

InputModifier WindowsDrop_readModifiers(DWORD keys) {
	auto ret = InputModifier::None;
	if (keys & MK_CONTROL) {
		ret |= InputModifier::Ctrl;
	}
	if (keys & MK_SHIFT) {
		ret |= InputModifier::Shift;
	}
	if (keys & MK_ALT) {
		ret |= InputModifier::Alt;
	}
	return ret;
}

// "C:\dir\file" -> "/C:/dir/file", the path part of a file URI
static String WindowsDrop_uriPath(WideStringView native) {
	String ret;
	unicode::toUtf8([&](StringView utf8) { ret = utf8.str<String>(); }, native);
	for (auto &c : ret) {
		if (c == '\\') {
			c = '/';
		}
	}
	if (ret.size() >= 2 && ret[1] == ':') {
		ret.insert(ret.begin(), '/');
	}
	return ret;
}

WindowsDropOffer::~WindowsDropOffer() { releaseData(); }

bool WindowsDropOffer::init(NotNull<dispatch::Looper> looper, IDataObject *data,
		DragActions allowed) {
	if (!data || !DropOffer::init(looper, readTypes(data), allowed)) {
		return false;
	}

	_data = data;
	_data->AddRef();
	return true;
}

void WindowsDropOffer::snapshot() {
	if (_snapped || !_data) {
		return;
	}

	_snapped = true;
	for (auto &it : _types) { _snapshot.emplace_back(Representation{it, readData(_data, it)}); }
}

void WindowsDropOffer::releaseData() {
	if (_data) {
		_data->Release();
		_data = nullptr;
	}
}

void WindowsDropOffer::handleRead(StringView type, ReadCallback &&cb) {
	if (_snapped) {
		for (auto &it : _snapshot) {
			if (StringView(it.type) == type) {
				cb(it.data.empty() ? Status::ErrorNotFound : Status::Ok, it.data);
				return;
			}
		}
		cb(Status::ErrorNotFound, BytesView());
	} else if (_data) {
		auto bytes = readData(_data, type);
		cb(bytes.empty() ? Status::ErrorNotFound : Status::Ok, bytes);
	} else {
		cb(Status::ErrorCancelled, BytesView());
	}
}

Vector<String> WindowsDropOffer::readTypes(IDataObject *data) {
	Vector<String> ret;

	auto files = WindowsDrop_format(CF_HDROP);
	if (data->QueryGetData(&files) == S_OK) {
		ret.emplace_back("text/uri-list");
	}

	auto text = WindowsDrop_format(CF_UNICODETEXT);
	if (data->QueryGetData(&text) == S_OK) {
		ret.emplace_back("text/plain");
	}

	return ret;
}

Bytes WindowsDropOffer::readData(IDataObject *data, StringView type) {
	const bool isFiles = (type == "text/uri-list");
	if (!isFiles && type != "text/plain") {
		return Bytes();
	}

	auto fmt = WindowsDrop_format(isFiles ? CF_HDROP : CF_UNICODETEXT);
	STGMEDIUM medium;
	__sprt_memset(&medium, 0, sizeof(medium));
	if (data->GetData(&fmt, &medium) != S_OK) {
		return Bytes();
	}

	Bytes ret;
	if (medium.tymed == TYMED_HGLOBAL && medium.hGlobal) {
		auto size = size_t(GlobalSize(medium.hGlobal));
		if (auto ptr = reinterpret_cast<const uint8_t *>(GlobalLock(medium.hGlobal))) {
			if (isFiles && size >= sizeof(DROPFILES)) {
				auto header = reinterpret_cast<const DROPFILES *>(ptr);
				Vector<String> paths;
				if (header->fWide && header->pFiles < size) {
					auto chars = reinterpret_cast<const char16_t *>(ptr + header->pFiles);
					auto count = (size - header->pFiles) / sizeof(char16_t);
					size_t i = 0;
					while (i < count && chars[i] != 0) {
						size_t len = 0;
						while (i + len < count && chars[i + len] != 0) { ++len; }
						paths.emplace_back(WindowsDrop_uriPath(WideStringView(chars + i, len)));
						i += len + 1;
					}
				}

				Vector<StringView> views;
				for (auto &it : paths) { views.emplace_back(it); }
				auto list = makeFileUriList(views);
				ret = Bytes(reinterpret_cast<const uint8_t *>(list.data()),
						reinterpret_cast<const uint8_t *>(list.data()) + list.size());
			} else if (!isFiles) {
				auto chars = reinterpret_cast<const char16_t *>(ptr);
				size_t len = 0;
				while (len < size / sizeof(char16_t) && chars[len] != 0) { ++len; }
				unicode::toUtf8([&](StringView str) {
					ret = Bytes(reinterpret_cast<const uint8_t *>(str.data()),
							reinterpret_cast<const uint8_t *>(str.data()) + str.size());
				}, WideStringView(chars, len));
			}
			GlobalUnlock(medium.hGlobal);
		}
	}

	WindowsOle_releaseStgMedium(&medium);
	return ret;
}

WindowsDropTarget::WindowsDropTarget(WindowsWindow *window) : _window(window) { }

void WindowsDropTarget::detach() {
	if (_offer) {
		_offer->releaseData();
		_offer = nullptr;
	}
	_window = nullptr;
}

HRESULT STDMETHODCALLTYPE WindowsDropTarget::QueryInterface(REFIID riid, void **ppvObject) {
	if (!ppvObject) {
		return E_INVALIDARG;
	}

	if (__sprt_memcmp(&riid, &__uuidof(IUnknown), sizeof(IID)) == 0
			|| __sprt_memcmp(&riid, &__uuidof(IDropTarget), sizeof(IID)) == 0) {
		*ppvObject = static_cast<IDropTarget *>(this);
		AddRef();
		return S_OK;
	}

	*ppvObject = nullptr;
	return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE WindowsDropTarget::AddRef(void) { return ++_refCount; }

ULONG STDMETHODCALLTYPE WindowsDropTarget::Release(void) {
	auto count = --_refCount;
	if (count == 0) {
		sprt::__delete(this);
	}
	return count;
}

HRESULT STDMETHODCALLTYPE WindowsDropTarget::DragEnter(IDataObject *pDataObj, DWORD grfKeyState,
		POINTL pt, DWORD *pdwEffect) {
	if (!pdwEffect) {
		return E_INVALIDARG;
	}

	if (_offer) {
		_offer->releaseData();
		_offer = nullptr;
	}

	auto allowed = WindowsDrop_readEffect(*pdwEffect);
	auto looper = dispatch::Looper::getIfExists();
	if (_window && pDataObj && looper && allowed != DragActions::None) {
		auto offer = Rc<WindowsDropOffer>::create(looper, pDataObj, allowed);
		if (offer && !offer->getTypes().empty()) {
			_offer = sprt::move(offer);
			_window->emitDropEvent(DropPhase::Enter, _offer, pt, grfKeyState);
		}
	}

	*pdwEffect = answer(*pdwEffect);
	return S_OK;
}

HRESULT STDMETHODCALLTYPE WindowsDropTarget::DragOver(DWORD grfKeyState, POINTL pt,
		DWORD *pdwEffect) {
	if (!pdwEffect) {
		return E_INVALIDARG;
	}

	if (_offer && _window) {
		_window->emitDropEvent(DropPhase::Motion, _offer, pt, grfKeyState);
	}

	*pdwEffect = answer(*pdwEffect);
	return S_OK;
}

HRESULT STDMETHODCALLTYPE WindowsDropTarget::DragLeave(void) {
	if (_offer) {
		if (_window) {
			_window->emitDropEvent(DropPhase::Leave, _offer, POINTL{0, 0}, 0);
		}
		_offer->releaseData();
		_offer = nullptr;
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE WindowsDropTarget::Drop(IDataObject *pDataObj, DWORD grfKeyState,
		POINTL pt, DWORD *pdwEffect) {
	if (!pdwEffect) {
		return E_INVALIDARG;
	}

	if (!_offer || !_window) {
		*pdwEffect = DROPEFFECT_NONE;
		return S_OK;
	}

	// The answer is the one given for the last position: the application decides on its own
	// thread, after this call has returned
	*pdwEffect = answer(*pdwEffect);

	auto offer = sprt::move(_offer);
	_offer = nullptr;

	offer->snapshot();
	offer->releaseData();
	_window->emitDropEvent(DropPhase::Drop, offer, pt, grfKeyState);
	return S_OK;
}

DWORD WindowsDropTarget::answer(DWORD allowed) const {
	if (!_offer) {
		return DROPEFFECT_NONE;
	}
	return WindowsDrop_writeEffect(_offer->getStatus()) & allowed;
}

} // namespace sprt::window
