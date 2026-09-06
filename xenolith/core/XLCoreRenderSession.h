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

#if XL_FRAME_ACCOUNT
	// Which frame `lastFrameTime` is about - see PresentationEngine::getLastFrameOrder.
	uint64_t lastFrameOrder = 0;
#endif
};

#if XL_FRAME_ACCOUNT
/* ---- the clock every account site reads ----------------------------------------------------------

Not `nanoclock(Monotonic)` directly, and the difference is not academic.

On a tickless desktop CLOCK_MONOTONIC is the right source and resolves to nanoseconds. On an RTOS
it need not be: NuttX with CONFIG_USEC_PER_TICK=1000 and no CONFIG_SCHED_TICKLESS advances
CLOCK_MONOTONIC once a millisecond, and every phase measured here is shorter than that. Measured on
raspberrypi-4b, 600 frames: `update`, `span`, `damage` and `plan` all reported exactly 0.0, and
every total was an exact multiple of 1000us - the signature of a quantized clock, not of free work.
The software backend's frame budget showed microsecond detail on the same run, because Time::now()
reads CLOCK_REALTIME and on that build it is the finer of the two.

So the source is chosen by MEASURING it, not by name. clock_getres cannot be trusted for this -
NuttX answers it with the tick period for both clocks even when one is finer - so the probe reads
each clock until it changes and takes the step. Once, at first use.

One clock for every site, because the account's numbers are compared and subtracted across modules;
two sources with different resolutions would produce differences that are neither.

`getAccountClockResolution` exists so a report can print it. A number below the clock's own step is
not a measurement, and a reader must be able to see that without knowing the board. */
SP_PUBLIC uint64_t getAccountClock();
SP_PUBLIC uint64_t getAccountClockResolution(); // nanoseconds, measured
SP_PUBLIC StringView getAccountClockName();

/* ---- the frame timeline (XL_FRAME_TIMELINE=N) ----------------------------------------------------

A CLOSED account of the whole frame, and the instrument that answers a gap.

The software backend's frame budget measures the render half stage by stage and lands the rest in
`wait` - the span from one present to the first thing the next frame's render half does. On
raspberrypi-4b that turned out to be 12.9ms of a 19.5ms frame, while the app thread's own account
said it worked for 1ms of it. Neither instrument could say what the other 11.9ms was, because it
belongs to neither: it is what happens BETWEEN them.

So the marks are placed on the frame's path itself, in order, and each bucket is the interval
ENDING at its mark. They close on themselves - the six sum to the period - which is what makes a
missing cost impossible to hide:

	render        VertexStart -> Presented. The render half, whatever it is made of; the backend's
	              own budget splits this one further.
	postPresent   Presented -> Scheduled. What the presentation engine does after a present before
	              it decides to start another frame - and, when a target frame interval is set,
	              the deliberate wait for the present window.
	toApp         Scheduled -> AcquireStart. Getting from the loop thread to the app thread.
	update        AcquireStart -> VisitStart. The scheduler, actions and input, plus the
	              deliberate "break current stack frame" hop that posts the visit.
	visit         VisitStart -> VisitEnd. The scene graph walk.
	toLoop        VisitEnd -> VertexStart. Back to the loop thread, frame graph setup included.

Three of the six are thread hand-offs, and on an RTOS a hand-off is not free: a looper that is
asleep wakes on a scheduler tick, so each one costs at least one tick and the account can be mostly
hops. That is a real finding rather than a measurement error, which is why they are named and
reported rather than summed into the stages around them.

Marks are recorded in sequence and the sequence is serial - one frame at a time on this path, and
the software presentation engine sets preStartFrame = false so two frames never overlap. Concurrent
frames would interleave marks and the buckets would be meaningless; a backend that starts a frame
early must not turn this on. */
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

	uint32_t vertexInputTime;

	/* ---- what the rasterizer wrote, for backends that rasterize on the CPU -----------------------

	`pixelsTotal` is the target; `pixelsFilled` is what the kernels actually wrote this frame,
	counted at their entry points, so a pixel covered by two commands counts twice. The ratio is
	the overdraw, and it is the number that says whether a frame is cheap because it drew little or
	expensive because it drew the same pixels repeatedly - the picture is identical either way.

	Zero on a GPU backend, which has no equivalent to report: a fragment count would come from a
	query pool and mean something else. Zero therefore means "not measured here", which is why the
	FPS overlay prints the line only when `pixelsTotal` is non-zero rather than printing 0/0.
	Default-initialized for the same reason - every producer that does not set them leaves them at
	the value that reads as "absent". */
	uint64_t pixelsTotal = 0;
	uint64_t pixelsFilled = 0;

#if XL_FRAME_ACCOUNT
	/* ---- the frame's deferred account -----------------------------------------------------------

	Carried on DrawStat because that is the one channel that already runs from the render half back
	to the app thread (Director::pushDrawStat), and adding a second would mean a second ordering to
	reason about.

	`deferredWorkTime` and `deferredWaitTime` ARE NOT PARTS OF ONE WHOLE and must never be added.
	The work is summed across worker threads and may exceed the frame; the wait is one thread
	standing still and is always inside it. A frame where work is large and wait is near zero is
	deferral doing its job; the two equal is deferral bought nothing. */
	uint64_t deferredWorkTime; // ns, summed across workers
	uint64_t deferredWaitTime; // ns, on the consuming thread
	uint32_t deferredCount; // results consumed
	uint32_t deferredWaited; // of those, how many were not finished when we got there

	/* WHICH FRAME this describes. pushDrawStat hops to the app thread asynchronously, so a reader
	there cannot assume the stat in hand belongs to the frame that just ended - and a measurement
	that attributes a number to the wrong frame is worse than one that reports nothing. */
	uint64_t frameOrder;

	/* ---- and WHERE inside the vertex stage the time went, in nanoseconds ------------------------

	`vertexInputTime` is the whole of the stage and has been reported for years; these are the parts
	it is made of, added because "the vertex stage is ten seconds" is not an answer to what to go
	and change. One clock read per boundary, so they sum to the stage rather than overlapping.

	  walk    - the command list, once, into per-material write plans
	  buffer  - spawning the three device buffers, whose sizes the walk decided
	  write   - copying vertexes, indexes and transforms into them
	  span    - turning the plans into draw spans, painter order included
	  upload  - flushing or setting the buffer data afterwards

	`damage` and `plan` are the walk split in two and are NESTED inside it: read as "of the walk,
	this much is that", never summed with it. */
	uint64_t walkTime;
	uint64_t bufferTime;
	uint64_t writeTime;
	uint64_t spanTime;
	uint64_t uploadTime;

	uint64_t damageTime; // inside walkTime
	uint64_t planTime; // inside walkTime

	/* THE TWO GAPS the five phases above do not cover, and one of them turned out to be the whole
	frame.

	`vertexInputTime` is stamped in the processor's CONSTRUCTOR, which runs when the attachment's
	input is submitted, and closed in `finalize`. Between the two the work is handed to a queue -
	so the stage's total includes however long it waited there before starting. Measured: the five
	phases summed to 10 ms of a 10 011 ms stage, and the difference was not in any of them.

	`fillTime` is the whole fill step, of which `writeTime + spanTime` is the part inside pushAll;
	the rest is the buffer mapping and, when there is no persistent mapping, resizing three host
	arrays the size of the frame. */
	uint64_t queueWaitTime; // construction -> the body actually starting
	uint64_t fillTime; // the whole fill step; writeTime + spanTime is its inner part
#endif
};

// Implemented by the CLIENT (Director/scene). The SERVER calls into it.
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

	/* WHICH WINDOW every call below is about, on the same terms as acquireFrame above: the id the
	server's ObjectRegistry gave the window, or 0 for a local self-request.

	One RenderClientChannel serves ALL of a server's shared windows -- setRenderClient installs the
	same object on each -- so a channel cannot tell from the call itself who is asking. The remote one
	used to answer that with "the window we most recently produced a frame for", which is right only
	while there is exactly one window and silently misroutes input the moment there are two. A local
	Director has exactly one window and ignores the id. */

	// The server announces the active render graph the client must target (the shared contract).
	// Maps onto the runWithQueue handshake.
	virtual void handleRenderQueueAttached(const Rc<Queue> &) = 0;

	// The server pushes a presentation-constraints / swapchain change (size, density, transform,
	// frame interval). Maps onto handleSwapchainUpdated.
	virtual void handleConstraintsChanged(const FrameConstraints &) = 0;

	/* The window moved or changed size: where it now is, in the logical space WindowInfo::rect
	uses, plus the surface extent and density that go with it.

	A SIBLING of handleConstraintsChanged rather than part of it, and deliberately so. Constraints
	describe what to render and are compared as a whole before a scene is resized; a window's
	position is not part of that and changes for entirely different reasons. Folding the two
	together would turn every drag of a title bar into a full scene relayout.

	Non-pure: a client that does not care where its window is - and most do not - should not have
	to say so. */
	virtual void handleWindowGeometryChanged(uint64_t windowId, const sprt::window::WindowGeometry &) {
	}

	// Input + window-state events from the platform. WindowState changes arrive as
	// InputEventName::WindowState entries within the batch, as today.
	virtual void handleInputEvents(uint64_t windowId, Vector<InputEventData> &&) = 0;
	virtual void handleTextInput(uint64_t windowId, const TextInputState &) = 0;

	// Frame-lifecycle feedback for client-side pacing/stats (a frame finished presenting).
	virtual void handleFramePresented(uint64_t frameOrder) = 0;

	virtual void pushDrawStat(uint64_t windowId, const DrawStat &) = 0;

	// True for a client that serves frames over the wire (remote transport). The server tags such a
	// client's frames PresentationFrame::Remote so they can be force-invalidated if the connection drops
	// (a non-responding remote client must not wedge the window's presentation). Local clients return
	// false (the default).
	virtual bool isRemote() const { return false; }
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

	/* What this window wants copied out of the frame that is being built, or null when nothing.

	Called once per frame while the frame's inputs are assembled, and it TAKES the request: two
	frames never carry the same capture. The default answers null, which is the right answer for
	every implementation that has no local window to capture from - the remote proxy included. */
	virtual Rc<FrameCaptureInput> takeFrameCaptureInput() { return nullptr; }

	/* Render one frame offscreen, presenting nothing, so that a pass can do work inside it.

	Only a frame capture uses this, and only where the presented image cannot be read: there the
	copy has to come out of an image this window owns rather than out of the swapchain. False means
	no such frame could be scheduled and the caller's work will never happen. */
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
	//
	// Non-pure on purpose: a channel with no native window behind it has no processor to drive.
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

	// Open an OS dialog (file picker, colour, font, reveal, trash) owned by this window. The
	// request's `parentWindowId` is filled in by the implementation and its completion runs on
	// the app thread. Keep the Rc<DialogRequest>: it is the cancellation token.
	//
	// Status::Ok means the dialog was accepted. Anything else means the completion has already
	// been scheduled with that status, so the caller never answers its own callback.
	//
	// Gate on WindowCapabilities::FileDialogs / ColorDialog / FontDialog / SystemFileActions and
	// fall back to an in-scene picker where the platform has none.
	//
	// Non-pure on purpose: RemoteWindow has no OS to ask, and must not be forced to implement it.
	virtual Status openDialog(NotNull<sprt::window::DialogRequest>);

	// Dismiss a dialog opened with `req`; its completion still runs, with Status::ErrorCancelled.
	virtual Status cancelDialog(NotNull<sprt::window::DialogRequest>);

	virtual void handleInputEvents(Vector<InputEventData> &&events) = 0;

	// Inject events at the native-window level instead of straight into the client, so they take
	// exactly the path a platform backend's events take: NativeWindow::handleInputEvents runs
	// first, which is where the text-input processor claims printable keys, Backspace, Delete and
	// Escape before the scene ever sees them. handleInputEvents() bypasses all of that.
	//
	// Non-pure on purpose: a channel with no native window behind it falls back to
	// handleInputEvents().
	virtual void handleNativeInputEvents(Vector<InputEventData> &&events);

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

	/* Where this window is and how big it is, as of the last update the server pushed.

	This is the app-thread-safe answer to a question WindowInfo cannot be asked from here (see
	AppWindow::getInfo). `rect` is in logical units - the same space WindowInfo::rect takes - so it
	can be saved and handed straight back to Context::createWindow with
	WindowCreationFlags::UsePosition to reopen the window where it was.

	Check `hasPosition` before trusting the origin: on Wayland and the windowless backends the
	platform never reports one, and the zeroes there mean "unknown", not "top-left corner". */
	const sprt::window::WindowGeometry &getWindowGeometry() const { return _appWindowGeometry; }

	/* ---- what this window will accept, decided from mirrored state alone ------------------------

	enableState/disableState/openWindowMenu/setFullscreen answer `bool` SYNCHRONOUSLY, so a channel
	that has to ask another process cannot produce that answer from the reply - it has to know it
	locally. Everything the decision needs is already here: `_state` and `_capabilities` are mirrors
	both implementations keep.

	So the rules live on the base rather than in whichever implementation happened to grow them
	first. The remote proxy then refuses exactly what the real window would have refused, and the
	two cannot drift apart - which is what makes "the client and the server answer identically" a
	property of the code rather than of a test. */

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
