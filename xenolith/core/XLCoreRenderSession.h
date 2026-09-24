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

#ifndef XENOLITH_CORE_XLCORERENDERSESSION_H_
#define XENOLITH_CORE_XLCORERENDERSESSION_H_

#include "XLCoreLoop.h"
#include "XLCoreFrameRequest.h"
#include "XLCoreFrameRequestProxy.h"
#include "XLCorePresentationEngine.h"
#include "XLCoreTextInput.h"
#include "XLCoreFrameCapture.h"

#include <sprt/runtime/window/dialog.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

// Render session boundary between the scene-graph client (Director/Scene/2D renderer) and the
// windowing + gapi-backend server (NativeWindow/AppWindow/PresentationEngine/Loop).
//
// The server owns the OS window, input, GPU backend and presentation; the client owns the scene
// graph and produces per-frame command batches. The server's PresentationEngine pulls frame data
// from the client. In-process, payloads are core objects passed by Rc and `LocalRenderSession`
// forwards calls directly; a remote pair serializes the same calls over the wire.
//
//   RenderClientChannel - implemented by the client, called by the server (server -> client)
//   RenderServerChannel - implemented by the server, called by the client (client -> server)

// Read-only frame timing/stats mirrored from the server's PresentationEngine for the client
// (FPS counters, frame-time overlays, etc.).
struct FrameTimingInfo {
	uint64_t lastFrameInterval = 0;
	uint64_t avgFrameInterval = 0;
	uint64_t lastFrameTime = 0;
	uint64_t lastFenceFrameTime = 0;
	uint64_t lastTimestampFrameTime = 0;

	// Which frame `lastFrameTime` is about; non-zero means a frame has been drawn at all
	// (see PresentationEngine::getLastFrameOrder).
	uint64_t lastFrameOrder = 0;
};

#if XL_FRAME_ACCOUNT
/* The clock every account site reads. The source is chosen once, at first use, by measuring each
clock's actual step (clock_getres is unreliable on RTOS targets like NuttX, where CLOCK_MONOTONIC
may advance once per tick). One clock for every site, so numbers can be compared across modules.
A value below `getAccountClockResolution` is not a measurement. */
SP_PUBLIC uint64_t getAccountClock();
SP_PUBLIC uint64_t getAccountClockResolution(); // nanoseconds, measured
SP_PUBLIC StringView getAccountClockName();

/* The frame timeline (XL_FRAME_TIMELINE=N): a closed account of the whole frame. Each bucket is
the interval ending at its mark, so the six sum to the period:

	render        VertexStart -> Presented. The render half; the backend's budget splits it further.
	postPresent   Presented -> Scheduled. Engine work after a present, including the wait for the
	              present window when a target frame interval is set.
	toApp         Scheduled -> AcquireStart. Hand-off from the loop thread to the app thread.
	update        AcquireStart -> VisitStart. Scheduler, actions and input, plus the hop that
	              posts the visit.
	visit         VisitStart -> VisitEnd. The scene graph walk.
	toLoop        VisitEnd -> VertexStart. Back to the loop thread, frame graph setup included.

Marks must be serial: only valid when frames never overlap (preStartFrame = false). */
enum class FrameMark : uint32_t {
	Presented,
	Scheduled,
	AcquireStart,
	VisitStart,
	VisitEnd,
	VertexStart,
	Count
};

// Whether XL_FRAME_TIMELINE named a non-zero interval. Check before taking a clock.
SP_PUBLIC bool isFrameTimelineEnabled();

// Record one mark. Charges the interval since the previous mark to this mark's bucket, and reports
// every Nth time the timeline closes (that is, on every Nth Presented).
SP_PUBLIC void markFrame(FrameMark);
#endif

struct SP_PUBLIC DrawStat {
	uint32_t vertexes;
	uint32_t triangles;
	uint32_t zPaths;
	uint32_t drawCalls;

	uint32_t cachedImages;
	uint32_t cachedFramebuffers;
	uint32_t cachedImageViews;
	uint32_t materials;

	uint32_t solidCmds;
	uint32_t surfaceCmds;
	uint32_t transparentCmds;
	uint32_t shadowsCmds;

	/* Surface commands the 2d vertex plan drew in painter's order instead of its surface pass,
	because transparent geometry behind them covered where they draw. Not carried by the remote
	wire (its positions are fixed). */
	uint32_t surfacePromotedCmds = 0;

	uint32_t vertexInputTime;

	/* CPU rasterizer output: `pixelsTotal` is the target, `pixelsFilled` what the kernels wrote
	this frame (a pixel covered twice counts twice), so the ratio is the overdraw. Zero on GPU
	backends, meaning "not measured". */
	uint64_t pixelsTotal = 0;
	uint64_t pixelsFilled = 0;

#if XL_FRAME_ACCOUNT
	/* Deferred account, carried back to the app thread via Director::pushDrawStat.
	`deferredWorkTime` and `deferredWaitTime` must never be added: work is summed across worker
	threads and may exceed the frame, wait is one thread stalling inside it. */
	uint64_t deferredWorkTime; // ns, summed across workers
	uint64_t deferredWaitTime; // ns, on the consuming thread
	uint32_t deferredCount; // results consumed
	uint32_t deferredWaited; // of those, how many were not finished when we got there

	/* FrameHandle::getDependencyWaitTime and its counts: the frame stalling for another queue's
	work (glyph atlas, materials) before an attachment takes its input. Inside the frame, before
	the vertex stage; not additive with the deferred times above. */
	uint64_t dependencyWaitTime;
	uint32_t dependencyCount;
	uint32_t dependencyWaited;

	/* Which frame this describes: pushDrawStat reaches the app thread asynchronously, so the stat
	may not belong to the frame that just ended. */
	uint64_t frameOrder;

	/* Phases of the vertex stage (`vertexInputTime`), in nanoseconds; one clock read per boundary,
	so they sum to the stage:

	  walk    - the command list, once, into per-material write plans
	  buffer  - spawning the three device buffers, whose sizes the walk decided
	  write   - copying vertexes, indexes and transforms into them
	  span    - turning the plans into draw spans, painter order included
	  upload  - flushing or setting the buffer data afterwards

	`damage` and `plan` are nested inside `walk`, never summed with it. */
	uint64_t walkTime;
	uint64_t bufferTime;
	uint64_t writeTime;
	uint64_t spanTime;
	uint64_t uploadTime;

	uint64_t damageTime; // inside walkTime
	uint64_t planTime; // inside walkTime

	/* Gaps the phases above do not cover. `vertexInputTime` starts in the processor's constructor
	(when input is submitted), so it includes time queued before the body starts. `fillTime` is
	the whole fill step; beyond `writeTime + spanTime` it holds buffer mapping or host resizing. */
	uint64_t queueWaitTime; // construction -> the body actually starting
	uint64_t fillTime; // the whole fill step; writeTime + spanTime is its inner part
#endif
};

// Implemented by the client (Director/scene). The server calls into it.
class SP_PUBLIC RenderClientChannel : public Ref {
public:
	// Out-of-line in the .cc: anchors the vtable (key function) and suppresses the
	// freestanding-delete warning there via __SPRT_*_ALLOW_CXXABI_ALLOC.
	virtual ~RenderClientChannel();

	// The server's PresentationEngine pulls the command batch for a frame: the client builds the
	// scene graph into `proxy` (local = direct on the server's FrameRequest; remote = serialized).
	// Callback called with true if frame processing was started and input will be produced in future;
	// false if client rejects the request
	//
	// In wire protocol, client should respond with queue selection.
	// windowId = 0 - self-request, local-only.
	virtual void acquireFrame(uint64_t windowId, NotNull<FrameRequestProxy> proxy,
			Function<void(bool)> &&) = 0;

	/* `windowId` in the calls below is the id the server's ObjectRegistry gave the window, or 0 for
	a local self-request. One channel serves all of a server's shared windows, so the id is the only
	way to route a call; a local Director has one window and ignores it. */

	// The server announces the active render graph the client must target (the shared contract).
	// Maps onto the runWithQueue handshake.
	virtual void handleRenderQueueAttached(const Rc<Queue> &) = 0;

	// The server pushes a presentation-constraints / swapchain change (size, density, transform,
	// frame interval). Maps onto handleSwapchainUpdated.
	virtual void handleConstraintsChanged(const FrameConstraints &) = 0;

	/* The window moved or changed size: where it now is, in the logical space WindowInfo::rect
	uses, plus the surface extent and density that go with it. Separate from
	handleConstraintsChanged so that moving a window does not relayout the scene. */
	virtual void handleWindowGeometryChanged(uint64_t windowId, const sprt::window::WindowGeometry &) {
	}

	// Input + window-state events from the platform. WindowState changes arrive as
	// InputEventName::WindowState entries within the batch.
	virtual void handleInputEvents(uint64_t windowId, Vector<InputEventData> &&) = 0;
	virtual void handleTextInput(uint64_t windowId, const TextInputState &) = 0;

	// A drag from another application over the window. The default refuses it, which is the
	// answer of a client that cannot take one (a remote client)
	virtual void handleDropEvent(uint64_t windowId, DropEvent &&);

	// Frame-lifecycle feedback for client-side pacing/stats (a frame finished presenting).
	virtual void handleFramePresented(uint64_t frameOrder) = 0;

	virtual void pushDrawStat(uint64_t windowId, const DrawStat &) = 0;

	// True for a client that serves frames over the wire (remote transport). The server tags such a
	// client's frames PresentationFrame::Remote so they can be force-invalidated if the connection drops
	// (a non-responding remote client must not wedge the window's presentation). Local clients return
	// false (the default).
	virtual bool isRemote() const { return false; }
};

// Implemented by the server (PresentationEngine/window/loop). The client calls into it.
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

	// See Loop::updateImage. The default does nothing: a channel with no loop has nothing to swap.
	virtual void updateImage(const Rc<DynamicImage> &, BytesView,
			Function<void(bool)> && = nullptr) { }

	// See Loop::updateImageStable. Falls back to updateImage.
	virtual void updateImageStable(const Rc<DynamicImage> &img, BytesView data,
			Function<void(bool)> &&cb = nullptr) {
		updateImage(img, data, sp::move(cb));
	}

	/* What this window wants copied out of the frame being built, or null. Called once per frame
	while inputs are assembled; it takes the request, so two frames never carry the same capture.
	The default (null) suits implementations with no local window, like the remote proxy. */
	virtual Rc<FrameCaptureInput> takeFrameCaptureInput() { return nullptr; }

	/* Render one frame offscreen, presenting nothing, so that a pass can do work inside it. Used by
	frame capture where the presented image cannot be read. False means no frame was scheduled and
	the callback will never run. */
	virtual bool scheduleOffscreenFrame(Function<void(bool)> && = nullptr) { return false; }

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

	// Drive the window's text-input processor as the platform IME would: composition (marked text),
	// insertion at an explicit range, deletion. These edits arrive without a keystroke, so they
	// cannot be expressed as input events; a test harness reproduces them through here.
	// Non-pure: a channel with no native window has no processor to drive.
	virtual void performTextInput(TextInputCommand &&);
	virtual void close(bool graceful = true) = 0;

	virtual void handleBackButton() = 0;

	virtual const sprt::window::WindowInfo *getInfo() const = 0;

	virtual bool enableState(WindowState) = 0;
	virtual bool disableState(WindowState) = 0;

	virtual bool setFullscreen(FullscreenInfo &&, Function<void(Status)> &&, Ref * = nullptr) = 0;

	// Resize the window from the application side. Only a window that owns its extent outright
	// (the headless pseudo-window) can honour this; anything backed by a window manager reports
	// ErrorNotSupported, because the size is the WM's to decide.
	virtual void setWindowExtent(Extent2, Function<void(Status)> && = nullptr, Ref * = nullptr);

	// Try to set preferred framerate for OS WM.
	// WindowCapabilities::PreferredFrameRate should be available
	virtual bool setPreferredFrameRate(float, Function<void(Status)> && = nullptr) = 0;

	// Capture the window contents (without OS decorations) as an image buffer. Renders a frame
	// into an offscreen target (PresentationFrame::OffscreenTarget) and returns it as info + data.
	virtual void captureScreenshot(
			Function<void(const core::ImageInfoData &info, BytesView view)> &&cb) = 0;

	// pos - Location, on which window menu should be opened in presentation (Scene) coords;
	// Use Vec2::INVALID to open window menu in current pointer location;
	// WindowState::AlloedWindowMenu should be enabled
	virtual bool openWindowMenu(Vec2 pos) = 0;

	// Open an OS dialog (file picker, colour, font, reveal, trash) owned by this window. The
	// implementation fills `parentWindowId`; the completion runs on the app thread. Keep the
	// Rc<DialogRequest>: it is the cancellation token. Any status other than Ok means the
	// completion is already scheduled with that status.
	//
	// Gate on WindowCapabilities::FileDialogs / ColorDialog / FontDialog / SystemFileActions.
	// Non-pure: RemoteWindow has no OS to ask.
	virtual Status openDialog(NotNull<sprt::window::DialogRequest>);

	// Dismiss a dialog opened with `req`; its completion still runs, with Status::ErrorCancelled.
	virtual Status cancelDialog(NotNull<sprt::window::DialogRequest>);

	virtual void handleInputEvents(Vector<InputEventData> &&events) = 0;

	// Inject events at the native-window level, taking the platform path: NativeWindow's text-input
	// processor claims printable keys, Backspace, Delete and Escape before the scene sees them
	// (handleInputEvents() bypasses it). Non-pure: without a native window it falls back to that.
	virtual void handleNativeInputEvents(Vector<InputEventData> &&events);

	// Inject one step of an OS drag at the native-window level, as a backend reports it. Non-pure:
	// without a native window the drag is refused
	virtual void handleNativeDropEvent(DropEvent &&);

	virtual void updateLayers(sprt::window::Vector<sprt::window::WindowLayer> &&) = 0;

	// Client-side endpoint of the render session (server -> client calls). Set by Director::init
	// at director-creation time so it is valid before the initial scene runs (and its queue is
	// announced). Cleared on teardown.
	virtual void setRenderClient(core::RenderClientChannel *c);

	// The counterpart of setRenderClient. Non-owning: the client is kept alive by _clientRef when
	// it is a Ref, and by its owner otherwise.
	core::RenderClientChannel *getRenderClient() const { return _client; }

	core::WindowState getWindowState() const { return _state; }

	sprt::window::WindowCapabilities getCapabilities() const { return _capabilities; }

	StringView getId() const { return _windowId; }

	// It's not safe to ask PresentationEngine about current config, use this instead
	const core::SwapchainConfig &getAppSwapchainConfig() const { return _appSwapchainConfig; }

	const core::FrameConstraints &getConstraints() const { return _appFrameConstraints; }

	/* Window position and size as of the last server update; app-thread-safe, unlike WindowInfo.
	`rect` is in logical units, suitable for Context::createWindow with UsePosition. Check
	`hasPosition` first: Wayland and windowless backends never report an origin. */
	const sprt::window::WindowGeometry &getWindowGeometry() const { return _appWindowGeometry; }

	/* What this window will accept, decided from the mirrored `_state` and `_capabilities` alone.
	enableState/disableState/openWindowMenu/setFullscreen answer synchronously, so the rules live
	on the base and the remote proxy refuses exactly what the real window would. */

	// Flags enableState/disableState will act on. Anything outside this mask is refused without
	// reaching the window system.
	core::WindowState getUpdatableStateFlags() const;

	// One flag per call (Maximized is the documented exception - it is two), and it must be
	// updatable. Logs the reason, because a silent `false` here is very hard to trace.
	bool validateStateChange(core::WindowState, StringView op) const;

	bool canOpenWindowMenu() const { return hasFlag(_state, core::WindowState::AllowedWindowMenu); }

	bool canSetFullscreen() const {
		return hasFlag(_capabilities, sprt::window::WindowCapabilities::Fullscreen);
	}

protected:
	Rc<Ref> _clientRef = nullptr;
	RenderClientChannel *_client = nullptr;
	core::WindowState _state = core::WindowState::None;
	core::FrameConstraints _appFrameConstraints; // read-only mirrior
	sprt::window::WindowGeometry _appWindowGeometry; // read-only mirrior
	core::SwapchainConfig _appSwapchainConfig; // read-only mirrior
	sprt::window::WindowCapabilities _capabilities = sprt::window::WindowCapabilities::None;
	String _windowId; // should be constant
};

} // namespace stappler::xenolith::core

#endif /* XENOLITH_CORE_XLCORERENDERSESSION_H_ */
