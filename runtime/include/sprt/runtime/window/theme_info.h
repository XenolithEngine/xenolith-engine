/**
 Copyright (c) 2026 Xenolith Team <admin@stappler.org>

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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_WINDOW_THEME_INFO_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_WINDOW_THEME_INFO_H_

#include <sprt/runtime/stringview.h>
#include <sprt/runtime/geom.h>
#include <sprt/runtime/window/types.h>

namespace sprt::window {

/** DecorationInfo - Data for user-space decoration drawing

On some platforms, application should assist for WM to draw correct rounded corners and shadows

if resizeInset > 0, resizing layers (when WindowState::AllowedResize is set) should be placed inside user window space with
specified inset (and no other controls under this insets will receive events)

If borderRadius > 0, ronded coners should be drawn by application, actial radius is borderRadius * constraints.surfaceDensity

if userShadows is true, shadows should be drawn under rounded corners, using shadowWidth, shadowCurrentValue and shadowOffset
*/
struct SPRT_API DecorationInfo {
	float resizeInset = 0.0f;
	float borderRadius = 0.0f;
	float shadowWidth = 0.0f;
	float shadowMinValue = 0.1f;
	float shadowMaxValue = 0.25f;
	Vec2 shadowOffset;

	// check if shadows drawen in user space
	bool hasShadows() const { return borderRadius > 0.0f || shadowWidth > 0.0f; }

	bool operator==(const DecorationInfo &) const = default;
	bool operator!=(const DecorationInfo &) const = default;
};

struct SPRT_API ThemeInfo {
	static constexpr StringView SchemePreferDark = StringView("prefer-dark");
	static constexpr StringView SchemePreferLight = StringView("prefer-light");
	static constexpr StringView SchemeDefault = StringView("default");

	memory::dynstring colorScheme;
	memory::dynstring systemTheme;
	memory::dynstring systemFontName;
	int32_t cursorSize = 0;
	float cursorScaling = 1.0f;
	float textScaling = 1.0f;
	float scrollModifier = 1.0f;
	bool leftHandedMouse = false;
	uint32_t doubleClickInterval = 500'000; // in microseconds

	DecorationInfo decorations;

	bool operator==(const ThemeInfo &) const = default;
	bool operator!=(const ThemeInfo &) const = default;
};

} // namespace sprt::window

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_WINDOW_THEME_INFO_H_
