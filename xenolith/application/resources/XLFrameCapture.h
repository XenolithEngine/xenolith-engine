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

/* One cutout of a rendered frame, as an ordinary Texture.

WHAT IT IS FOR. A widget that wants to show what is already on screen - the ghost that follows the
pointer during a drag, a transition that has to keep the old screen for a moment - can either draw
that content a second time or take the pixels. Drawing it again is what a hand-made ghost does, and
it is always a little wrong: it has to reproduce the row's icon, its indent, its selection and its
background, and it drifts from the original the moment any of those change.

ONE CUTOUT IS ONE IMAGE. The target owns a GPU image sized to exactly the requested rectangle,
filled once by a copy out of the frame and never written again. That is the whole reason there is no
full-screen "history" here: a rectangle-sized image costs tens of kilobytes (a tree row at 620x26
RGBA8 is 64 KB), it never has to be resized when the window is, and a Sprite shows it whole - there
is no UV arithmetic to get wrong.

WHY A DynamicImage RATHER THAN AN ImageData. A material holds `MaterialImage::image` as a RAW
pointer, so an image the target owns outright would dangle the moment the target died while a
material still referenced it. `DynamicImage` is the engine's answer to exactly that: the material
holds an `Rc<DynamicImageInstance>` and the data it points into is kept alive by it. It is also why
ResourceCache::addImage is not used - ResourceCache has no removal for those entries, so every
cutout ever taken would stay in the cache for the life of the process.

WHAT IS NEVER IN IT. The copy is recorded between the content pass and the overlay pass, so nothing
on RenderingLevel::Overlay (Node::setOverlay) can appear in a cutout - the drag ghost that is made
of one, the window decorations. That is what lets a ghost be built from these pixels at all: it
cannot photograph itself, and nothing has to be timed so that it doesn't.

WHY IT IS FILLED WITH ZEROES FIRST. The image is compiled through the ordinary
`RenderServerChannel::compileImage` path with a transparent payload. That costs one small
host-to-device transfer, and it buys the thing that matters: the image arrives in
ShaderReadOnlyOptimal with defined contents, so it is safe to sample from the very first frame it
exists - before, and even without, any capture landing in it. */
class SP_PUBLIC FrameCaptureTarget : public Ref {
public:
	enum class State {
		Allocating, // the image is being compiled; there is no texture yet
		Armed, // the image exists and is transparent; waiting for a frame to copy into it
		Ready, // the copy landed: the texture shows what was on screen
		Failed,
	};

	virtual ~FrameCaptureTarget() = default;

	// `region` is in SWAPCHAIN-IMAGE PIXELS, y-DOWN - the space damage and input events already use,
	// not the scene's y-up space. A caller holding a node rect converts.
	//
	// `format` must be the format of the image the copy will come out of (Loop::getCommonFormat()):
	// cmdCopyImage requires the two to match, and CommandBuffer wraps no blit that would convert.
	virtual bool init(StringView key, const URect &region, core::ImageFormat format);

	State getState() const { return _state; }
	bool isReady() const { return _state == State::Ready; }

	// Null while Allocating, non-null for every state after it - including Failed, where it is a
	// transparent rectangle. A caller that draws it unconditionally therefore cannot fault.
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

/* The per-window half: it arms captures and hands the pending ones to the frame being built.

One per AppWindow, because a capture is a copy out of THAT window's presented image. It holds no
image of its own - every allocation belongs to the target that asked for it and dies with it.

App-thread only, like everything else a scene talks to. */
class SP_PUBLIC FrameCapture : public Ref {
public:
	virtual ~FrameCapture() = default;

	virtual bool init(NotNull<AppThread>, NotNull<core::RenderServerChannel>);

	/* A rect in the scene's WORLD space, as the region this class takes.

	Not a y-flip: the engine projection bakes surface pre-rotation, so world -> clip -> pixels is
	the only conversion that is right on a rotated surface, and it is the same one the damage
	collector uses (XL2dDamage.cc, toPixels). `viewProjection` is Director::getGeneralProjection()
	for anything drawn in the general 2d space.

	WORLD space really means world space. Do NOT build the rect from a node's world origin plus its
	content size: the scene root is scaled by the surface density (Scene::setScale), so the origin
	is in surface pixels while the content size is in logical units, and on a HiDPI surface the
	region comes out at half the size it should be. Use Node::getWorldBoundingBox().

	The result is clamped to `extent`; a rect entirely off-screen comes back empty. */
	static URect makeRegion(const Rect &world, const Mat4 &viewProjection, Extent2 extent);

	/* False when nothing here can produce a cutout. A caller is expected to have something else to
	draw - a hand-made ghost, a plain rectangle - rather than to treat this as an error.

	Only the BACKEND decides this: Vulkan is the only one with a copy path. Whether the SURFACE
	allows its presented images to be read decides something else - WHICH frame the copy comes out
	of, not whether it can happen at all. See isSurfaceSupported(). */
	bool isAvailable() const { return _backendSupported; }

	bool isBackendSupported() const { return _backendSupported; }

	/* Whether the presented image can be copied out of in place, from SwapchainConfig::transferSrc.
	Re-set on every swapchain change.

	False does not disable capturing: it changes where the pixels come from. The copy then rides an
	extra frame rendered offscreen, which costs a full frame and shows a freshly drawn scene rather
	than the one the user was looking at - and, for a drag ghost, that is an improvement, because
	such a frame cannot contain the ghost. */
	void setSurfaceSupported(bool value) { _surfaceSupported = value; }
	bool isSurfaceSupported() const { return _surfaceSupported; }

	/* Arm a capture for the next frame that renders.

	The target comes back immediately so it can be held and inspected, but its texture is null until
	the image is compiled and its contents are transparent until the copy lands. `cb` runs on the app
	thread exactly once, whatever the outcome - read the target's state rather than assuming success.

	Returns null when the region is empty after clamping, or when isAvailable() is false. */
	Rc<FrameCaptureTarget> request(const URect &region, Function<void(FrameCaptureTarget *)> &&);

	// What the frame being built should copy into. Empty when nothing is armed - and then the pass
	// records nothing at all, which is what makes an idle capture free.
	SpanView<Rc<FrameCaptureTarget>> getPending() const { return _pending; }
	bool hasPending() const;

	// Hand the armed targets to the frame; they leave the pending list and wait for their copy.
	Vector<Rc<FrameCaptureTarget>> takePending();

	// The frame reports what happened to the batch takePending() returned.
	void handleCaptured(SpanView<Rc<FrameCaptureTarget>>, bool success);

protected:
	// Both raw: this object is owned by the window, which is owned by the thread - and
	// RenderServerChannel is a plain interface, not a Ref, so there is nothing to hold anyway.
	AppThread *_application = nullptr;
	core::RenderServerChannel *_channel = nullptr;

	Vector<Rc<FrameCaptureTarget>> _pending;
	uint64_t _nextId = 1;
	bool _backendSupported = false;
	bool _surfaceSupported = false;
};

} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_RESOURCES_XLFRAMECAPTURE_H_ */
