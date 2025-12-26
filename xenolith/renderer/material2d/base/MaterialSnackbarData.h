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

#ifndef XENOLITH_RENDERER_MATERIAL2D_BASE_MATERIALSNACKBARDATA_H_
#define XENOLITH_RENDERER_MATERIAL2D_BASE_MATERIALSNACKBARDATA_H_

#include "MaterialConfig.h"
#include "XL2dIcons.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::material2d {

struct SP_PUBLIC SnackbarData {
	String text;
	Color textColor = Color::White;
	float textBlendValue = 0.0f;

	String buttonText;
	IconName buttonIcon = IconName::None;
	Function<void()> buttonCallback = nullptr;
	Color buttonColor = Color::White;
	float buttonBlendValue = 0.0f;
	float delayTime = 4.0f; // in float seconds

	SnackbarData() = default;
	SnackbarData(const SnackbarData &) = default;
	SnackbarData(SnackbarData &&) = default;
	SnackbarData &operator=(const SnackbarData &) = default;
	SnackbarData &operator=(SnackbarData &&) = default;

	SnackbarData(const StringView &str) : text(str.str<Interface>()) { }

	SnackbarData(const StringView &str, const Color &color, float blendValue)
	: text(str.str<Interface>()), textColor(color), textBlendValue(blendValue) { }

	SnackbarData &withButton(const StringView &str, Function<void()> &&cb,
			const Color &color = Color::White, float buttonBlend = 0.0f) {
		buttonText = str.str<Interface>();
		buttonIcon = IconName::None;
		buttonCallback = cb;
		buttonColor = color;
		buttonBlendValue = buttonBlend;
		return *this;
	}

	SnackbarData &withButton(const StringView &str, IconName ic, Function<void()> &&cb,
			const Color &color = Color::White, float buttonBlend = 0.0f) {
		buttonText = str.str<Interface>();
		buttonIcon = ic;
		buttonCallback = cb;
		buttonColor = color;
		buttonBlendValue = buttonBlend;
		return *this;
	}

	SnackbarData &withButton(IconName ic, Function<void()> &&cb, const Color &color = Color::White,
			float buttonBlend = 0.0f) {
		buttonText = String();
		buttonIcon = ic;
		buttonCallback = cb;
		buttonColor = color;
		buttonBlendValue = buttonBlend;
		return *this;
	}

	SnackbarData &delayFor(float value) {
		delayTime = value;
		return *this;
	}
};

} // namespace stappler::xenolith::material2d

#endif /* XENOLITH_RENDERER_MATERIAL2D_BASE_MATERIALSNACKBARDATA_H_ */
