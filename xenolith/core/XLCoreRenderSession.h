/**
 Copyright (c) 2026 Stappler Team <admin@stappler.org>

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

#ifndef XENOLITH_CORE_XLCORERENDERSESSION_H_
#define XENOLITH_CORE_XLCORERENDERSESSION_H_

#include "XLCoreLoop.h"
#include "XLCoreFrameRequest.h"
#include "XLCoreFrameRequestProxy.h"
#include "XLCorePresentationEngine.h"
#include "XLCoreTextInput.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

//
// Render session boundary between the scene-graph CLIENT (Director/Scene/2D renderer)
// and the windowing + gapi-backend SERVER (NativeWindow/AppWindow/PresentationEngine/Loop).
//
// The split follows the X11 model: the server owns the OS window, input, GPU backend and
// presentation; the client owns the scene graph and produces per-frame command batches.
// The server's PresentationEngine drives the frame flow (pull model) and requests frame
// data from the client; the client keeps its scene synchronized with the render graph the
// server actually uses.
//
// These two channels are the complete protocol surface. Stage 1 is in-process only:
// payloads are existing core objects passed by Rc (no serialization), and `LocalRenderSession`
// forwards calls directly in a single process. A networked implementation will later provide a
// remote pair that serializes the same calls over the wire (see tests/quicp transport work).
//
// Direction convention:
//   RenderClientChannel - implemented by the CLIENT, called by the SERVER (server -> client)
//   RenderServerChannel - implemented by the SERVER, called by the CLIENT (client -> server)
//

// Read-only frame timing/stats mirrored from the server's PresentationEngine for the client
// (FPS counters, frame-time overlays, etc.).
struct FrameTimingInfo {
	uint64_t lastFrameInterval = 0;
	uint64_t avgFrameInterval = 0;
	uint64_t lastFrameTime = 0;
	uint64_t lastFenceFrameTime = 0;
	uint64_t lastTimestampFrameTime = 0;
};

// Implemented by the CLIENT (Director/scene). The SERVER calls into it.
class SP_PUBLIC RenderClientChannel {
public:
	// Out-of-line in the .cc: anchors the vtable (key function) and suppresses the
	// freestanding-delete warning there via __SPRT_*_ALLOW_CXXABI_ALLOC.
	virtual ~RenderClientChannel();

	// The server's PresentationEngine pulls the command batch for a frame: the client builds the
	// scene graph into `proxy` (local = direct on the server's FrameRequest; remote = serialized).
	// Returns true if a batch was produced and the frame may proceed. Maps onto
	// Director::acquireFrame().
	virtual bool acquireFrame(NotNull<FrameRequestProxy> proxy) = 0;

	// The server announces the active render graph the client must target (the shared contract).
	// Maps onto the runWithQueue handshake.
	virtual void handleRenderQueueAttached(const Rc<Queue> &) = 0;

	// The server pushes a presentation-constraints / swapchain change (size, density, transform,
	// frame interval). Maps onto handleSwapchainUpdated.
	virtual void handleConstraintsChanged(const FrameConstraints &) = 0;

	// Input + window-state events from the platform. WindowState changes arrive as
	// InputEventName::WindowState entries within the batch, as today.
	virtual void handleInputEvents(Vector<InputEventData> &&) = 0;
	virtual void handleTextInput(const TextInputState &) = 0;

	// Frame-lifecycle feedback for client-side pacing/stats (a frame finished presenting).
	virtual void handleFramePresented(uint64_t frameOrder) = 0;
};

// Implemented by the SERVER (PresentationEngine/window/loop). The CLIENT calls into it.
class SP_PUBLIC RenderServerChannel {
public:
	virtual ~RenderServerChannel();

	// --- render graph + GPU resource compilation (the server owns the gapi backend) ---
	virtual void compileRenderQueue(const Rc<Queue> &, Function<void(bool)> && = nullptr) = 0;
	virtual void compileResource(Rc<Resource> &&, Function<void(bool)> && = nullptr,
			bool preload = false) = 0;
	virtual void compileMaterials(Rc<MaterialInputData> &&,
			const Vector<Rc<DependencyEvent>> & = Vector<Rc<DependencyEvent>>()) = 0;
	virtual void compileImage(const Rc<DynamicImage> &, Function<void(bool)> && = nullptr) = 0;

	// Make `queue` the active render graph and begin presentation with it.
	// Maps onto AppWindow::runWithQueue().
	virtual void attachRenderQueue(const Rc<Queue> &) = 0;

	// --- frame flow / pacing ---
	virtual void setReadyForNextFrame() = 0;
	virtual void setPreferredFrameInterval(uint64_t intervalUs) = 0;
	virtual FrameTimingInfo getFrameTiming() const = 0;

	// --- window control ---
	virtual void acquireScreenInfo(Function<void(NotNull<ScreenInfo>)> &&, Ref * = nullptr) = 0;
	virtual void acquireTextInput(TextInputRequest &&) = 0;
	virtual void releaseTextInput() = 0;
	virtual void close(bool graceful = true) = 0;

	virtual void handleBackButton() = 0;

	virtual const sprt::window::WindowInfo *getInfo() const = 0;

	virtual bool enableState(WindowState) = 0;
	virtual bool disableState(WindowState) = 0;

	virtual bool setFullscreen(FullscreenInfo &&, Function<void(Status)> &&, Ref * = nullptr) = 0;

	// Try to set preferred framerate for OS WM.
	// WindowCapabilities::PreferredFrameRate should be available
	virtual bool setPreferredFrameRate(float, Function<void(Status)> && = nullptr) = 0;

	// Capture current window contents as an image buffer
	// (makes screenshot of the window's content without OS decorations)
	//
	// This call actually performs frame rendering into offscreen buffer
	// (via PresentationEngine::scheduleSwapchainImage with PresentationFrame::OffscreenTarget),
	// that then will be returned as info + data
	virtual void captureScreenshot(
			Function<void(const core::ImageInfoData &info, BytesView view)> &&cb) = 0;

	// pos - Location, on which window menu should be opened in presentation (Scene) coords;
	// Use Vec2::INVALID to open window menu in current pointer location;
	// WindowState::AlloedWindowMenu should be enabled
	virtual bool openWindowMenu(Vec2 pos) = 0;

	virtual void handleInputEvents(Vector<InputEventData> &&events) = 0;

	virtual void updateLayers(sprt::window::Vector<sprt::window::WindowLayer> &&) = 0;

	// Client-side endpoint of the render session (server -> client calls). Set by Director::init
	// at director-creation time so it is valid before the initial scene runs (and its queue is
	// announced). Cleared on teardown.
	virtual void setRenderClient(core::RenderClientChannel *c);

	core::WindowState getWindowState() const { return _state; }

	sprt::window::WindowCapabilities getCapabilities() const { return _capabilities; }

	StringView getId() const { return _windowId; }

	// It's not safe to ask PresentationEngine about current config, use this instead
	const core::SwapchainConfig &getAppSwapchainConfig() const { return _appSwapchainConfig; }

	const core::FrameConstraints &getConstraints() const { return _appFrameConstraints; }

protected:
	RenderClientChannel *_client = nullptr;
	core::WindowState _state = core::WindowState::None;
	core::FrameConstraints _appFrameConstraints; // read-only mirrior
	core::SwapchainConfig _appSwapchainConfig; // read-only mirrior
	sprt::window::WindowCapabilities _capabilities = sprt::window::WindowCapabilities::None;
	String _windowId; // should be constant
};

} // namespace stappler::xenolith::core

#endif /* XENOLITH_CORE_XLCORERENDERSESSION_H_ */
