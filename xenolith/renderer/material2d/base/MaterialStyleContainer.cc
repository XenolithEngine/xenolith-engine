/**
 Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
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

#include "MaterialStyleContainer.h"
#include "XLScene.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::material2d {

XL_DECLARE_EVENT_CLASS(StyleContainer, onColorSchemeUpdate)

uint64_t StyleContainer::SystemFrameTag = System::GetNextSystemId();

bool StyleContainer::init() {
	if (!System::init()) {
		return false;
	}

	setFrameTag(SystemFrameTag);

	_schemes.emplace(PrimarySchemeTag,
			ColorScheme(ThemeType::LightTheme, ColorHCT(292, 100, 50, 1.0f), false));

	return true;
}

void StyleContainer::handleEnter(Scene *scene) {
	System::handleEnter(scene);
	_scene = scene;
}

void StyleContainer::handleExit() {
	_scene = nullptr;
	System::handleExit();
}

void StyleContainer::setPrimaryScheme(ColorScheme &&scheme) {
	setScheme(PrimarySchemeTag, move(scheme));
}

void StyleContainer::setPrimaryScheme(ThemeType type, const CorePalette &palette) {
	setScheme(PrimarySchemeTag, type, palette);
}

void StyleContainer::setPrimaryScheme(ThemeType type, const Color4F &color, bool isContent) {
	setScheme(PrimarySchemeTag, type, color, isContent);
}

void StyleContainer::setPrimaryScheme(ThemeType type, const ColorHCT &color, bool isContent) {
	setScheme(PrimarySchemeTag, type, color, isContent);
}

const ColorScheme &StyleContainer::getPrimaryScheme() const { return *getScheme(PrimarySchemeTag); }

const ColorScheme *StyleContainer::setScheme(uint32_t tag, ColorScheme &&scheme) {
	auto it = _schemes.find(tag);
	if (it != _schemes.end()) {
		it->second = move(scheme);
	} else {
		it = _schemes.emplace(tag, move(scheme)).first;
	}
	if (_running) {
		onColorSchemeUpdate(this, int64_t(tag));
	}
	return &it->second;
}

const ColorScheme *StyleContainer::setScheme(uint32_t tag, ThemeType type,
		const CorePalette &palette) {
	auto it = _schemes.find(tag);
	if (it != _schemes.end()) {
		it->second.set(type, palette);
	} else {
		it = _schemes.emplace(tag, ColorScheme(type, palette)).first;
	}
	if (_running) {
		onColorSchemeUpdate(this, int64_t(tag));
	}
	return &it->second;
}

const ColorScheme *StyleContainer::setScheme(uint32_t tag, ThemeType type, const Color4F &color,
		bool isContent) {
	auto it = _schemes.find(tag);
	if (it != _schemes.end()) {
		it->second.set(type, color, isContent);
	} else {
		it = _schemes.emplace(tag, ColorScheme(type, color, isContent)).first;
	}
	if (_running) {
		onColorSchemeUpdate(this, int64_t(tag));
	}
	return &it->second;
}

const ColorScheme *StyleContainer::setScheme(uint32_t tag, ThemeType type, const ColorHCT &color,
		bool isContent) {
	auto it = _schemes.find(tag);
	if (it != _schemes.end()) {
		it->second.set(type, color, isContent);
	} else {
		it = _schemes.emplace(tag, ColorScheme(type, color, isContent)).first;
	}
	if (_running) {
		onColorSchemeUpdate(this, int64_t(tag));
	}
	return &it->second;
}

const ColorScheme *StyleContainer::getScheme(uint32_t tag) const {
	auto it = _schemes.find(tag);
	if (it != _schemes.end()) {
		return &it->second;
	}
	return nullptr;
}

} // namespace stappler::xenolith::material2d
