/**
 Copyright (c) 2024 Stappler LLC <admin@stappler.dev>

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

#include "SPFontLibrary.h"

#include "ft2build.h"
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_MULTIPLE_MASTERS_H
#include FT_SFNT_NAMES_H
#include FT_ADVANCES_H

#include "SPFontFace.h"

#include <sprt/runtime/dispatch/thread.h>

namespace STAPPLER_VERSIONIZED stappler::font {

#include "SPFont-RobotoMono-Italic-VariableFont_wght.ttf.cc"
#include "SPFont-RobotoMono-VariableFont_wght.ttf.cc"
#include "SPFont-RobotoFlex-VariableFont.ttf.cc"
#include "SPFont-DejaVuSansStappler.cc"

FontLibraryHandle::FontLibraryHandle() { FT_Init_FreeType(&_library); }

FontLibraryHandle::~FontLibraryHandle() {
	if (_library) {
		FT_Done_FreeType(_library);
		_library = nullptr;
	}
}

FontFaceObjectHandle::~FontFaceObjectHandle() {
	if (_onDestroy) {
		_onDestroy(this);
		_onDestroy = nullptr;
	}
	_library = nullptr;
	_face = nullptr;
}

bool FontFaceObjectHandle::init(const Rc<FontLibrary> &lib, Rc<FontFaceObject> &&obj,
		Function<void(const FontFaceObjectHandle *)> &&onDestroy) {
	_face = move(obj);
	_library = lib;
	_onDestroy = sp::move(onDestroy);
	return true;
}

bool FontFaceObjectHandle::acquireTexture(char32_t theChar,
		const Callback<void(const CharTexture &)> &cb) {
	return _face->acquireTextureUnsafe(theChar, cb);
}

BytesView FontLibrary::getFont(DefaultFontName name) {
	switch (name) {
	case DefaultFontName::None: return BytesView(); break;
	case DefaultFontName::RobotoFlex_VariableFont:
		return BytesView(reinterpret_cast<const uint8_t *>(s_font_RobotoFlex_VariableFont),
				sizeof(s_font_RobotoFlex_VariableFont));
		break;
	case DefaultFontName::RobotoMono_VariableFont:
		return BytesView(reinterpret_cast<const uint8_t *>(s_font_RobotoMono_VariableFont),
				sizeof(s_font_RobotoMono_VariableFont));
		break;
	case DefaultFontName::RobotoMono_Italic_VariableFont:
		return BytesView(reinterpret_cast<const uint8_t *>(s_font_RobotoMono_Italic_VariableFont),
				sizeof(s_font_RobotoMono_Italic_VariableFont));
		break;
	case DefaultFontName::DejaVuSans:
		return BytesView(reinterpret_cast<const uint8_t *>(s_font_DejaVuSansStappler),
				sizeof(s_font_DejaVuSansStappler));
		break;
	}
	return BytesView();
}

StringView FontLibrary::getFontName(DefaultFontName name) {
	switch (name) {
	case DefaultFontName::None: return StringView(); break;
	case DefaultFontName::RobotoFlex_VariableFont:
		return StringView("RobotoFlex_VariableFont");
		break;
	case DefaultFontName::RobotoMono_VariableFont:
		return StringView("RobotoMono_VariableFont");
		break;
	case DefaultFontName::RobotoMono_Italic_VariableFont:
		return StringView("RobotoMono_Italic_VariableFont");
		break;
	case DefaultFontName::DejaVuSans: return StringView("DejaVuSans"); break;
	}
	return StringView();
}

FontLibrary::FontLibrary() {
	_library = Rc<FontLibraryHandle>::alloc(); //
}

FontLibrary::~FontLibrary() {
	_threadLibrary.clear();
	_library = nullptr;
}

Rc<FontFaceData> FontLibrary::openFontData(StringView dataName, FontLayoutParameters params,
		bool isParamsPreconfigured, const Callback<FontData()> &dataCallback) {
	sprt::unique_lock lock(_mutex);

	auto it = _data.find(dataName);
	if (it != _data.end()) {
		return it->second;
	}

	if (!dataCallback) {
		return nullptr;
	}

	lock.unlock();
	auto fontData = dataCallback();
	if (fontData.view.empty() && !fontData.callback) {
		return nullptr;
	}

	Rc<FontFaceData> dataObject;
	if (fontData.callback) {
		dataObject = Rc<FontFaceData>::create(dataName, sp::move(fontData.callback));
	} else if (fontData.persistent) {
		dataObject = Rc<FontFaceData>::create(dataName, sp::move(fontData.view), true);
	} else {
		dataObject = Rc<FontFaceData>::create(dataName, sp::move(fontData.bytes));
	}
	if (dataObject) {
		lock.lock();
		_data.emplace(dataObject->getName(), dataObject);

		auto face = newFontFace(_library, dataObject->getView());
		lock.unlock();
		if (!isParamsPreconfigured) {
			params = dataObject->acquireDefaultParams(face);
		}
		dataObject->inspectVariableFont(params, _library->getLibrary(), face);
		lock.lock();
		doneFontFace(face);
	}
	return dataObject;
}

Rc<FontFaceObject> FontLibrary::openFontFace(StringView dataName,
		const FontSpecializationVector &spec, const Callback<FontData()> &dataCallback) {
	String faceName = mem_std::toString(dataName, spec.getSpecializationArgs<Interface>());

	sprt::unique_lock lock(_mutex);
	do {
		auto it = _faces.find(faceName);
		if (it != _faces.end()) {
			return it->second;
		}
	} while (0);

	auto it = _data.find(dataName);
	if (it != _data.end()) {
		auto face = newFontFace(_library, it->second->getView());
		auto ret = Rc<FontFaceObject>::create(faceName, it->second, _library->getLibrary(), face,
				spec, getNextId());
		if (ret) {
			_faces.emplace(ret->getName(), ret);
		} else {
			doneFontFace(face);
		}
		return ret;
	}

	if (!dataCallback) {
		return nullptr;
	}

	auto fontData = dataCallback();
	if (fontData.view.empty()) {
		return nullptr;
	}

	Rc<FontFaceData> dataObject;
	if (fontData.persistent) {
		dataObject = Rc<FontFaceData>::create(dataName, sp::move(fontData.view), true);
	} else {
		dataObject = Rc<FontFaceData>::create(dataName, sp::move(fontData.bytes));
	}

	if (dataObject) {
		_data.emplace(dataObject->getName(), dataObject);
		auto face = newFontFace(_library, dataObject->getView());
		auto ret = Rc<FontFaceObject>::create(faceName, dataObject, _library->getLibrary(), face,
				spec, getNextId());
		if (ret) {
			_faces.emplace(ret->getName(), ret);
		} else {
			doneFontFace(face);
		}
		return ret;
	}

	return nullptr;
}

Rc<FontFaceObject> FontLibrary::openFontFace(const Rc<FontFaceData> &dataObject,
		const FontSpecializationVector &spec, uint16_t forcedId) {
	String faceName =
			mem_std::toString(dataObject->getName(), spec.getSpecializationArgs<Interface>());

	sprt::unique_lock lock(_mutex);
	do {
		auto it = _faces.find(faceName);
		if (it != _faces.end()) {
			// Already opened for this (data, spec); reuse it (its id is the previously-forced one).
			return it->second;
		}
	} while (0);

	bool isNewId = false;
	if (forcedId == sprt::Max<uint16_t>) {
		forcedId = getNextId();
		isNewId = true;
	}

	// Adopt the caller's id instead of minting one; mark it used so any getNextId() on this library
	// would not hand it out.
	if (forcedId != sprt::Max<uint16_t> && forcedId < _fontIds.size()) {
		_fontIds.set(forcedId);
	}

	auto face = newFontFace(_library, dataObject->getView());
	// Create the face with the forced id (NOT a freshly-minted one): the client baked CharIds with this
	// id, so the server's atlas must key glyphs under the same id or getObjectByName() misses and the
	// text renders blank.
	auto ret = Rc<FontFaceObject>::create(faceName, dataObject, _library->getLibrary(), face, spec,
			forcedId);
	if (ret) {
		_faces.emplace(ret->getName(), ret);
	} else {
		doneFontFace(face);
		if (isNewId) {
			releaseId(forcedId);
		}
	}
	return ret;
}

void FontLibrary::invalidate() {
	sprt::unique_lock uniqueLock(_sharedMutex);

	_threads.clear();
	_faces.clear();
	_data.clear();
}

void FontLibrary::update() {
	Vector<Rc<FontFaceObject>> erased;
	sprt::unique_lock lock(_mutex);

	do {
		auto it = _faces.begin();
		while (it != _faces.end()) {
			if (it->second->getReferenceCount() == 1) {
				releaseId(it->second->getId());
				doneFontFace(it->second->getFace());
				erased.emplace_back(it->second.get());
				it = _faces.erase(it);
			} else {
				++it;
			}
		}
	} while (0);

	do {
		auto it = _data.begin();
		while (it != _data.end()) {
			if (it->second->getReferenceCount() == 1) {
				it = _data.erase(it);
			} else {
				++it;
			}
		}
	} while (0);

	lock.unlock();

	sprt::unique_lock uniqueLock(_sharedMutex);
	for (auto &it : erased) { _threads.erase(it.get()); }
}

uint16_t FontLibrary::getNextId() {
	for (uint32_t i = 1; i < _fontIds.size(); ++i) {
		if (!_fontIds.test(i)) {
			_fontIds.set(i);
			return i;
		}
	}
	abort(); // active font limits exceeded
	return 0;
}

void FontLibrary::releaseId(uint16_t id) { _fontIds.reset(id); }

Rc<FontFaceObjectHandle> FontLibrary::makeThreadHandle(const Rc<FontFaceObject> &obj) {
	Rc<FontLibraryHandle> lib;

	sprt::shared_lock sharedLock(_sharedMutex);
	auto it = _threads.find(obj.get());
	if (it != _threads.end()) {
		auto iit = it->second.find(sprt::dispatch::Thread::getCurrentThreadId());
		if (iit != it->second.end()) {
			return iit->second;
		}
	}
	sharedLock.unlock();

	sprt::unique_lock uniqueLock(_sharedMutex);
	it = _threads.find(obj.get());
	if (it != _threads.end()) {
		auto iit = it->second.find(sprt::dispatch::Thread::getCurrentThreadId());
		if (iit != it->second.end()) {
			return iit->second;
		}
	}

	sprt::unique_lock lock(_mutex);

	auto threadId = sprt::this_thread::get_id();
	auto threadIt = _threadLibrary.find(threadId);
	if (threadIt == _threadLibrary.end()) {
		lib = _threadLibrary.emplace(threadId, Rc<FontLibraryHandle>::alloc()).first->second;
	} else {
		lib = threadIt->second;
	}

	auto face = newFontFace(lib, obj->getData()->getView());
	lock.unlock();
	auto target = Rc<FontFaceObject>::create(obj->getName(), obj->getData(), lib->getLibrary(),
			face, obj->getSpec(), obj->getId());

	if (it == _threads.end()) {
		it = _threads.emplace(obj.get(),
							 Map<sprt::dispatch::Thread::Id, Rc<FontFaceObjectHandle>>())
					 .first;
	}

	auto iit = it->second
					   .emplace(sprt::dispatch::Thread::getCurrentThreadId(),
							   Rc<FontFaceObjectHandle>::create(this, move(target),
									   [this](const FontFaceObjectHandle *obj) {
		sprt::unique_lock lock(_mutex);
		doneFontFace(obj->getFace());
	})).first->second;
	uniqueLock.unlock();
	return iit;
}

FT_Face FontLibrary::newFontFace(FontLibraryHandle *lib, BytesView data) {
	FT_Face ret = nullptr;
	FT_Error err = FT_Err_Ok;

	err = FT_New_Memory_Face(lib->getLibrary(), data.data(), data.size(), 0, &ret);
	if (err != FT_Err_Ok) {
		auto str = FT_Error_String(err);
		log::source().error("font::FontLibrary", str ? StringView(str) : "Unknown error");
		return nullptr;
	}

	return ret;
}

void FontLibrary::doneFontFace(FT_Face face) {
	if (face) {
		FT_Done_Face(face);
	}
}

} // namespace stappler::font
