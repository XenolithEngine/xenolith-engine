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

#ifndef XENOLITH_APPLICATION_RESOURCES_XLFRAMECAPTURE_H_
#define XENOLITH_APPLICATION_RESOURCES_XLFRAMECAPTURE_H_

#include "XLTexture.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

namespace core {
class RenderServerChannel;
}

class AppThread;

/* One cutout of a rendered frame, as an ordinary Texture: lets a widget show what is already on
screen (a drag ghost, a transition snapshot) without redrawing it.

Each cutout owns a GPU image sized exactly to the requested rectangle, filled once by a copy.

It uses a DynamicImage, since materials hold `MaterialImage::image` as a raw pointer and the
`Rc<DynamicImageInstance>` keeps the data alive; ResourceCache::addImage is avoided because its
entries are never removed.

The copy is recorded between the content and overlay passes, so RenderingLevel::Overlay content
(Node::setOverlay, window decorations, the drag ghost itself) never appears in a cutout.

The image is first compiled with a transparent payload via `RenderServerChannel::compileImage`, so
it is in ShaderReadOnlyOptimal with defined contents and can be sampled before any capture lands. */
class SP_PUBLIC FrameCaptureTarget : public Ref {
public:
	enum class State {
		Allocating, // the image is being compiled; there is no texture yet
		Armed, // the image exists and is transparent; waiting for a frame to copy into it
		Ready, // the copy landed: the texture shows what was on screen
		Failed,
	};

	virtual ~FrameCaptureTarget() = default;

	// `region` is in swapchain-image pixels, y-down (as damage and input events), not the scene's
	// y-up space.
	//
	// `format` must match the source image (Loop::getCommonFormat()): cmdCopyImage requires it, and
	// there is no converting blit.
	virtual bool init(StringView key, const URect &region, core::ImageFormat format);

	State getState() const { return _state; }
	bool isReady() const { return _state == State::Ready; }

	// Null while Allocating, non-null in every later state (a transparent rectangle when Failed).
	Texture *getTexture() const { return _texture; }

	Extent2 getExtent() const { return Extent2(_region.width, _region.height); }
	const URect &getRegion() const { return _region; }

	// The live GPU object the copy writes into; null while Allocating.
	core::ImageObject *getImage() const;

protected:
	friend class FrameCapture;

	// Both run on the app thread, and both run at most once.
	void handleCompiled(bool success);
	void handleCaptured(bool success);

	URect _region;
	State _state = State::Allocating;
	Rc<core::DynamicImage> _dynamic;
	Rc<Texture> _texture;
	Function<void(FrameCaptureTarget *)> _callback;
};

/* The per-window half: arms captures and hands pending ones to the frame being built. One per
AppWindow; it owns no images (each belongs to its target). App-thread only. */
class SP_PUBLIC FrameCapture : public Ref {
public:
	virtual ~FrameCapture() = default;

	virtual bool init(NotNull<AppThread>, NotNull<core::RenderServerChannel>);

	/* A rect in the scene's world space converted to the region this class takes, via world -> clip
	-> pixels (correct on pre-rotated surfaces, as in XL2dDamage.cc toPixels). `viewProjection`
	is Director::getGeneralProjection() for the general 2d space.

	Use Node::getWorldBoundingBox(), not world origin plus content size: the scene root is scaled
	by surface density, so that mixes pixels and logical units. The result is clamped to
	`extent`; a rect entirely off-screen comes back empty. */
	static URect makeRegion(const Rect &world, const Mat4 &viewProjection, Extent2 extent);

	/* False when no cutout can be produced; callers should draw a fallback. Only the backend
	decides this (Vulkan has the copy path); surface support only selects which frame the copy
	comes from (see isSurfaceSupported()). */
	bool isAvailable() const { return _backendSupported; }

	bool isBackendSupported() const { return _backendSupported; }

	/* Whether the presented image can be copied in place (SwapchainConfig::transferSrc), re-set on
	every swapchain change. When false, the copy comes from an extra offscreen frame, which costs a
	frame and shows a freshly drawn scene (without a drag ghost). */
	void setSurfaceSupported(bool value) { _surfaceSupported = value; }
	bool isSurfaceSupported() const { return _surfaceSupported; }

	/* Arm a capture for the next frame that renders. The target is returned immediately; its
	texture is null until the image is compiled and transparent until the copy lands. `cb` runs
	on the app thread exactly once, whatever the outcome; check the target's state.

	Returns null when the region is empty after clamping, or when isAvailable() is false. */
	Rc<FrameCaptureTarget> request(const URect &region, Function<void(FrameCaptureTarget *)> &&);

	// What the frame being built should copy into; empty when nothing is armed (the pass then
	// records nothing).
	SpanView<Rc<FrameCaptureTarget>> getPending() const { return _pending; }
	bool hasPending() const;

	// Hand the armed targets to the frame; they leave the pending list and wait for their copy.
	Vector<Rc<FrameCaptureTarget>> takePending();

	// The frame reports what happened to the batch takePending() returned.
	void handleCaptured(SpanView<Rc<FrameCaptureTarget>>, bool success);

protected:
	// Both raw: this object is owned by the window, owned by the thread; RenderServerChannel is not
	// a Ref.
	AppThread *_application = nullptr;
	core::RenderServerChannel *_channel = nullptr;

	Vector<Rc<FrameCaptureTarget>> _pending;
	uint64_t _nextId = 1;
	bool _backendSupported = false;
	bool _surfaceSupported = false;
};

} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_RESOURCES_XLFRAMECAPTURE_H_ */
