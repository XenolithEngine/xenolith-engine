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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_WINDOW_SURFACE_INFO_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_WINDOW_SURFACE_INFO_H_

#include <sprt/runtime/geom.h>
#include <sprt/runtime/ref.h>
#include <sprt/runtime/window/mode.h>
#include <sprt/runtime/mem/vector.h>
#include <sprt/runtime/bitset.h>

namespace sprt::window {

enum class SurfaceTransformFlags : uint32_t {
	None,
	Identity = 0x0000'0001,
	Rotate90 = 0x0000'0002,
	Rotate180 = 0x0000'0004,
	Rotate270 = 0x0000'0008,
	Mirror = 0x0000'0010,
	MirrorRotate90 = 0x0000'0020,
	MirrorRotate180 = 0x0000'0040,
	MirrorRotate270 = 0x0000'0080,
	Inherit = 0x0000'0100,
	PreRotated = 0x0100'0000,
	TransformMask = 0x0000'01FF,
};

SPRT_DEFINE_ENUM_AS_MASK(SurfaceTransformFlags)

inline SurfaceTransformFlags getPureTransform(SurfaceTransformFlags flags) {
	return flags & SurfaceTransformFlags::TransformMask;
}


enum class CompositeAlphaFlags : uint32_t {
	None = 0,
	Opaque = 0x0000'0001,
	Premultiplied = 0x0000'0002,
	Postmultiplied = 0x0000'0004,
	Inherit = 0x0000'0008,
};

SPRT_DEFINE_ENUM_AS_MASK(CompositeAlphaFlags)


enum class FullScreenExclusiveMode : uint32_t {
	Default = 0,
	Allowed = 1,
	Disallowed = 2,
	ApplicationControlled = 3,
};

SPRT_DEFINE_ENUM_AS_MASK(FullScreenExclusiveMode)

enum class ImageUsage : uint32_t {
	None,
	TransferSrc = 0x0000'0001,
	TransferDst = 0x0000'0002,
	Sampled = 0x0000'0004,
	Storage = 0x0000'0008,
	ColorAttachment = 0x0000'0010,
	DepthStencilAttachment = 0x0000'0020,
	TransientAttachment = 0x0000'0040,
	InputAttachment = 0x0000'0080,
};

SPRT_DEFINE_ENUM_AS_MASK(ImageUsage)

// OS Windows Manager constraints for a frame presentation
struct SPRT_API FrameConstraints {
	Extent3 extent;
	Padding contentPadding;
	SurfaceTransformFlags transform = SurfaceTransformFlags::Identity;

	float density = 1.0f;
	float surfaceDensity = 1.0f;

	// WM frame interval (it's not an engine limit, but OS limit)
	uint64_t frameInterval = 1'000'000 / 60;

	Size2 getScreenSize() const {
		if (hasFlag(transform, SurfaceTransformFlags::PreRotated)) {
			switch (getPureTransform(transform)) {
			case SurfaceTransformFlags::Rotate90:
			case SurfaceTransformFlags::Rotate270:
			case SurfaceTransformFlags::MirrorRotate90:
			case SurfaceTransformFlags::MirrorRotate270:
				return Size2(extent.height, extent.width);
				break;
			default: break;
			}
		}
		return Size2(extent.width, extent.height);
	}

	Padding getRotatedPadding() const {
		Padding out = contentPadding;
		switch (transform) {
		case SurfaceTransformFlags::Rotate90:
			out.left = contentPadding.top;
			out.top = contentPadding.right;
			out.right = contentPadding.bottom;
			out.bottom = contentPadding.left;
			break;
		case SurfaceTransformFlags::Rotate180:
			out.left = contentPadding.right;
			out.top = contentPadding.bottom;
			out.right = contentPadding.left;
			out.bottom = contentPadding.top;
			break;
		case SurfaceTransformFlags::Rotate270:
			out.left = contentPadding.bottom;
			out.top = contentPadding.left;
			out.right = contentPadding.top;
			out.bottom = contentPadding.right;
			break;
		case SurfaceTransformFlags::Mirror:
			out.left = contentPadding.right;
			out.right = contentPadding.left;
			break;
		case SurfaceTransformFlags::MirrorRotate90: break;
		case SurfaceTransformFlags::MirrorRotate180:
			out.top = contentPadding.bottom;
			out.bottom = contentPadding.top;
			break;
		case SurfaceTransformFlags::MirrorRotate270: break;
		default: break;
		}
		return out;
	}

	constexpr bool operator==(const FrameConstraints &) const = default;
	constexpr bool operator!=(const FrameConstraints &) const = default;
};

struct SPRT_API SwapchainConfig {
	PresentMode presentMode = PresentMode::Mailbox;
	PresentMode presentModeFast = PresentMode::Unsupported;
	ImageFormat imageFormat = ImageFormat::R8G8B8A8_UNORM;
	ColorSpace colorSpace = ColorSpace::SRGB_NONLINEAR_KHR;
	CompositeAlphaFlags alpha = CompositeAlphaFlags::Opaque;
	SurfaceTransformFlags transform = SurfaceTransformFlags::Identity;
	uint32_t imageCount = 3;
	Extent2 extent;
	bool clipped = false;
	bool transfer = true;
	bool liveResize = false;

	// Used when gAPI support for the fullscreen mode required
	FullScreenExclusiveMode fullscreenMode = FullScreenExclusiveMode::Default;
	void *fullscreenHandle = nullptr;

	void description(const callback<void(StringView)> &stream) const;

	constexpr bool operator==(const SwapchainConfig &) const = default;
	constexpr bool operator!=(const SwapchainConfig &) const = default;
};

struct SPRT_API SurfaceInfo {
	uint32_t minImageCount;
	uint32_t maxImageCount;
	Extent2 currentExtent;
	Extent2 minImageExtent;
	Extent2 maxImageExtent;
	uint32_t maxImageArrayLayers;
	CompositeAlphaFlags supportedCompositeAlpha;
	SurfaceTransformFlags supportedTransforms;
	SurfaceTransformFlags currentTransform;
	ImageUsage supportedUsageFlags;
	memory::dynvector<pair<ImageFormat, ColorSpace>> formats;
	memory::dynvector<PresentMode> presentModes;

	// Used when gAPI support for the fullscreen mode required
	FullScreenExclusiveMode fullscreenMode = FullScreenExclusiveMode::Default;
	void *fullscreenHandle = nullptr;

	bool isSupported(const SwapchainConfig &) const;

	void description(const callback<void(StringView)> &stream) const;
};

enum class SurfaceBackend {
	Surface,
	Android,
	Wayland,
	Win32,
	Xcb,
	XLib,
	DirectFb,
	Fuchsia,
	GoogleGames,
	IOS,
	MacOS,
	VI,
	Metal,
	QNX,
	OpenHarmony,
	Display,
	Max
};

using SurfaceBackendMask = sprt::bitset<toInt(SurfaceBackend::Max)>;

struct SPRT_API SurfaceSupportInfo {
	SurfaceBackendMask backendMask;

	struct {
		void *connection = nullptr;
		uint32_t visual_id = 0;
	} xcb;
	struct {
		void *display = nullptr;
	} wayland;
};

struct SPRT_API SurfaceInterfaceInfo {
	FullScreenExclusiveMode fullscreenMode = FullScreenExclusiveMode::Default;
	void *fullscreenHandle = nullptr;

	SurfaceBackend backend;

	union {
		struct {
			void *connection;
			uint32_t window;
		} xcb;
		struct {
			void *display;
			void *surface;
		} wayland;
		struct {
			void *window;
		} android;
		struct {
			void *hinstance;
			void *hwnd;
		} windows;
		struct {
			void *layer;
		} metal;
	};
};

SPRT_API void getCompositeAlphaFlagsDescription(const callback<void(StringView)> &stream,
		CompositeAlphaFlags fmt);
SPRT_API void getSurfaceTransformFlagsDescription(const callback<void(StringView)> &stream,
		SurfaceTransformFlags fmt);
SPRT_API void getImageUsageDescription(const callback<void(StringView)> &stream, ImageUsage fmt);

} // namespace sprt::window

namespace sprt {

inline const callback<void(StringView)> &operator<<(const callback<void(StringView)> &cb,
		window::CompositeAlphaFlags state) {
	getCompositeAlphaFlagsDescription(cb, state);
	return cb;
}

inline const callback<void(StringView)> &operator<<(const callback<void(StringView)> &cb,
		window::SurfaceTransformFlags state) {
	getSurfaceTransformFlagsDescription(cb, state);
	return cb;
}

inline const callback<void(StringView)> &operator<<(const callback<void(StringView)> &cb,
		window::ImageUsage state) {
	getImageUsageDescription(cb, state);
	return cb;
}

inline const callback<void(StringView)> &operator<<(const callback<void(StringView)> &cb,
		const window::SwapchainConfig &cfg) {
	cfg.description(cb);
	return cb;
}

inline const callback<void(StringView)> &operator<<(const callback<void(StringView)> &cb,
		const window::SurfaceInfo &info) {
	info.description(cb);
	return cb;
}

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_WINDOW_SURFACE_INFO_H_
