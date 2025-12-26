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

#ifndef XENOLITH_CORE_XLCOREMONITORINFO_H_
#define XENOLITH_CORE_XLCOREMONITORINFO_H_

#include "XLCoreInfo.h" // IWYU pragma: keep

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

enum class FullscreenFlags : uint32_t {
	None,
	Current = 1 << 0,
	Exclusive = 1 << 1,
};

SP_DEFINE_ENUM_AS_MASK(FullscreenFlags)

struct EdidInfo {
	static EdidInfo parse(BytesView);
	static StringView getVendorName(StringView);

	String vendorId;
	String model;
	String serial;
	StringView vendor;

	auto operator<=>(const EdidInfo &) const = default;
};

struct ModeInfo {
	static const ModeInfo Current;
	static const ModeInfo Preferred;

	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t rate = 0; // FPS multiplied by 1000
	float scale = 1.0f;

	auto operator<=>(const ModeInfo &) const = default;
};

struct MonitorId {
	// Use this to fullscreen into primary monitor
	static const MonitorId Primary;

	// Use this to disable fullscreen
	static const MonitorId None;

	String name;
	EdidInfo edid;

	auto operator<=>(const MonitorId &) const = default;
};

struct SP_PUBLIC FullscreenInfo {
	// Exit fullscreen mode
	static const FullscreenInfo None;

	// Use current monitor for the fullscreen
	static const FullscreenInfo Current;

	MonitorId id;
	ModeInfo mode;
	FullscreenFlags flags = FullscreenFlags::None;

	auto operator<=>(const FullscreenInfo &) const = default;
};

struct MonitorInfo : MonitorId {
	IRect rect;
	Extent2 mm;

	uint32_t preferredMode = 0;
	uint32_t currentMode = 0;

	Vector<ModeInfo> modes;
};

struct ScreenInfo : public Ref {
	Vector<MonitorInfo> monitors;
	uint32_t primaryMonitor = 0;
};

} // namespace stappler::xenolith::core

#endif /* XENOLITH_CORE_XLCOREMONITORINFO_H_ */
