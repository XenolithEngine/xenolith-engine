/**
 Copyright (c) 2025 Stappler LLC <admin@stappler.dev>
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

#ifndef XENOLITH_CORE_XLCOREPRESENTATIONENGINE_H_
#define XENOLITH_CORE_XLCOREPRESENTATIONENGINE_H_

#include "XLCoreDeviceQueue.h"
#include "XLCoreInfo.h"
#include "XLCoreSwapchain.h"
#include "XLCorePresentationFrame.h"
#include "SPMovingAverage.h"
#include "XlCoreMonitorInfo.h"

#include <sprt/runtime/dispatch/handle.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

class PresentationFrame;

class SP_PUBLIC PresentationWindow {
public:
	__SPRT_PUSH_ALLOW_CXXABI_ALLOC
	virtual ~PresentationWindow() = default;
	__SPRT_POP_ALLOW_CXXABI_ALLOC

	virtual ImageInfo getSwapchainImageInfo(const SwapchainConfig &cfg) const = 0;
	virtual ImageViewInfo getSwapchainImageViewInfo(const ImageInfo &image) const = 0;
	virtual SurfaceInfo getSurfaceOptions(const Device &, NotNull<Surface>) const = 0;

	virtual SwapchainConfig selectConfig(const SurfaceInfo &, bool fastMode) = 0;

	virtual void acquireFrameData(NotNull<PresentationFrame>,
			Function<void(NotNull<PresentationFrame>)> &&) = 0;

	// called right before present call
	virtual void handleFrameReady(NotNull<core::PresentationFrame>) = 0;

	// called right after present call
	virtual void handleFramePresented(NotNull<PresentationFrame>) = 0;
	virtual void handleSwapchainUpdated(const FrameConstraints &) = 0;

	virtual Rc<Surface> makeSurface(NotNull<Instance>) = 0;
	virtual FrameConstraints exportConstraints(uint64_t &serial) const = 0;

	virtual void setFrameOrder(uint64_t) = 0;

	// Stable id for multi-window diagnostics (WindowInfo::id). Empty if unknown.
	virtual StringView getPresentationDebugId() const { return StringView(); }
};

using sprt::window::UpdateConstraintsFlags;
using sprt::window::PresentationUpdateFlags;

class SP_PUBLIC PresentationEngine : public Ref {
public:
	static constexpr size_t FrameAverageCount = 20;

	struct FrameTimeInfo {
		uint64_t dt;
		uint64_t avg;
		uint64_t clock;
	};

	virtual ~PresentationEngine();

	virtual bool init(NotNull<Loop>, NotNull<Device>, NotNull<PresentationWindow>,
			PresentationOptions);

	virtual bool run();
	virtual void end();

	virtual bool recreateSwapchain() = 0;
	virtual bool createSwapchain(const core::SurfaceInfo &, core::SwapchainConfig &&cfg,
			core::PresentMode presentMode, bool oldSwapchainValid) = 0;

	virtual Rc<ScreenInfo> getScreenInfo() const = 0;
	virtual Status setFullscreenSurface(const MonitorId &, const ModeInfo &) = 0;

	// Callback receives true for successful recreation and false for end-of-life
	virtual void updateConstraints(UpdateConstraintsFlags = UpdateConstraintsFlags::None,
			Function<void(bool)> && = nullptr);

	virtual bool present(PresentationFrame *frame, ImageStorage *image);
	virtual bool presentImmediate(PresentationFrame *frame) { return false; }

	virtual void update(PresentationUpdateFlags);

	// 0 - do not target any interval
	// In FIFO mode WM interval will ba the hard limit
	// In Mailbox or Immediate - no limit will be applied
	void setTargetFrameInterval(uint64_t);

	uint64_t getTargetFrameInterval() const { return _targetFrameInterval; }

	bool isFrameValid(const PresentationFrame *) const;

	Rc<FrameHandle> submitNextFrame(Rc<FrameRequest> &&);

	bool waitUntilFramePresentation();

	void scheduleNextImage(Function<void(PresentationFrame *, bool)> && = nullptr,
			PresentationFrame::Flags frameFlags = PresentationFrame::None);

	bool scheduleSwapchainImage(Rc<PresentationFrame> &&);

	Swapchain *getSwapchain() const { return _swapchain; }

	const FrameConstraints &getFrameConstraints() const { return _constraints; }

	const PresentationOptions &getOptions() const { return _options; }

	FrameTimeInfo updatePresentationInterval();

	uint64_t getFrameOrder() const;
	uint64_t getLastFrameInterval() const;
	uint64_t getAvgFrameInterval() const;
	uint64_t getLastFrameTime() const;
	uint64_t getLastFenceFrameTime() const;
	uint64_t getLastTimestampFrameTime() const;

	void setReadyForNextFrame();

	void setRenderOnDemand(bool value);
	bool isRenderOnDemand() const;

	bool isRunning() const;

	void enableExclusiveFullscreen();

	// Return an acquired-but-unused swapchain image to the reuse pool (a frame was discarded before
	// rendering started, see PresentationFrame::invalidate). The next frame picks it up via scheduleImage
	// instead of acquiring a fresh one, and it is accounted for (presented/released) exactly once.
	void reclaimAcquiredImage(Rc<Swapchain::SwapchainAcquiredImage> &&);

	virtual bool handleFrameStarted(NotNull<PresentationFrame>);
	virtual void handleFrameInvalidated(NotNull<PresentationFrame>);
	virtual void handleFrameReady(NotNull<PresentationFrame>);
	virtual void handleFramePresented(NotNull<PresentationFrame>);
	virtual void handleFrameComplete(NotNull<PresentationFrame>);

	virtual void handleSwapchainUpdated(const FrameConstraints &);

	// Force-invalidate every in-flight frame tagged PresentationFrame::Remote (data produced by a remote
	// render client). Called when the client connection is reset, so a frame stuck waiting on a dead
	// client cannot wedge the pipeline and the window can revert to its local Director.
	void invalidateRemoteFrames();

	// Kick presentation back into motion after the window's render client changed (remote takeover or
	// revert to the local Director): clear a possibly-stale display-link barrier and schedule one fresh
	// frame. Runs only on a client change, so normal frame pacing is untouched.
	void resetForRenderClientChange();

	virtual void captureScreenshot(Function<void(const ImageInfoData &info, BytesView view)> &&cb);

	/* Render one frame into an offscreen image and present nothing.

	The same frame captureScreenshot renders, minus the readback: what it is for is the work a pass
	does INSIDE that frame - a frame capture copying rectangles out of the image, where the
	presented image cannot be read. `cb` runs on the presentation thread when the frame ends. */
	virtual void scheduleOffscreenFrame(Function<void(bool)> &&cb);

	virtual void synchronizeClose();

protected:
	// Uncomment to track retain/release cycles
	//#if SP_REF_DEBUG
	//	virtual bool isRetainTrackerEnabled() const override { return true; }
	//#endif

	virtual void acquireFrameData(NotNull<PresentationFrame>,
			Function<void(NotNull<PresentationFrame>)> &&);

	void scheduleSwapchainRecreation();

	void resetFrames();

	// Start / stop a per-frame deadline timer that cancels the frame if it is still pending when the
	// deadline (FrameRequest::getDeadline(), or a seeded default) passes.
	void scheduleFrameDeadline(NotNull<PresentationFrame>);
	void cancelFrameDeadline(NotNull<PresentationFrame>);

	void scheduleImage(NotNull<PresentationFrame>);

	Status acquireScheduledImage();
	void scheduleImageAcquisition();

	void handleSwapchainImageReady(Rc<Swapchain::SwapchainAcquiredImage> &&image);

	void runScheduledPresent(NotNull<PresentationFrame> frame, ImageStorage *image,
			uint64_t presentWindow);
	void presentSwapchainImage(Rc<DeviceQueue> &&queue, NotNull<PresentationFrame> frame,
			ImageStorage *image, uint64_t presentWindow);

	// `queue` is null for a swapchain that presents without one (Swapchain::isPresentQueueRequired)
	void presentWithQueue(DeviceQueue *queue, NotNull<PresentationFrame> frame, ImageStorage *image,
			uint64_t presentWindow);

	bool canScheduleNextFrame() const;

	PresentationOptions _options;

	uint64_t _serial = 0;
	FrameConstraints _constraints;

	Device *_device = nullptr;

	Rc<Surface> _surface;
	Rc<Surface> _nextSurface;
	Rc<Surface> _originalSurface;
	Rc<Swapchain> _swapchain;
	Rc<Loop> _loop;

	PresentationWindow *_window = nullptr;

	// время, после которого нужно выпускать следующий кадрр
	// расчитывается как премя последней презентации + целевой кадроый интервал
	uint64_t _nextPresentWindow = 0;

	// Целевой кадроый интервал в режиме постоянной презентации (в микросекундах)
	// Может отличаться от кадрового интервала оконного менеджера (WM)
	// В режимах Mailbox и Immediate может быть больше интервала WM
	// Во всех режимах может быть меньше интервала WM
	uint64_t _targetFrameInterval = 0; // 1'000'000 / 60;

	// интервал обновления системы (приблизительная частота вызова update) (в микросекундах)
	uint64_t _engineUpdateInterval = 250;

	// Presentation interval is not the same, as frame interval, it's time between two present event
	uint64_t _lastPresentationTime = 0;
	sprt::atomic<uint64_t> _lastPresentationInterval = 0;
	MovingAverage<FrameAverageCount, uint64_t> _avgPresentationInterval;
	sprt::atomic<uint64_t> _avgPresentationIntervalValue = 0;

	uint64_t _lastFrameTime = 0;
	MovingAverage<FrameAverageCount, uint64_t> _avgFrameTime;
	sprt::atomic<uint64_t> _avgFrameTimeValue = 0;

	uint64_t _lastFenceFrameTime = 0;
	MovingAverage<FrameAverageCount, uint64_t> _avgFenceInterval;
	sprt::atomic<uint64_t> _avgFenceIntervalValue = 0;

	uint64_t _lastTimestampFrameTime = 0;
	MovingAverage<FrameAverageCount, uint64_t> _avgTimestampInterval;
	sprt::atomic<uint64_t> _avgTimestampIntervalValue = 0;

	uint64_t _frameOrder = 0; // current scheduled frame order

	bool _running = false;
	bool _readyForNextFrame = false;
	bool _waitUntilFrame = false;

	// XL_DAMAGE_DEBUG=1 - log the computed damage of every presented frame
	bool _damageDebug = false;
	bool _waitUntilSwapchainRecreation = false;
	bool _waitForDisplayLink = false;
	bool _swapchainRecreationScheduled = false;
	bool _liveResizeEnabled = false;
	bool _exclusiveFullscreenAvailable = false;

	// Platform supports presentation with window timing specification,
	// presentation engine should not implement scheduling by itself
	// (see VK_GOOGLE_display_timing)
	bool _presentWithWindowTiming = false;

	// New frames, that waits next swapchain image
	List<Rc<PresentationFrame>> _framesAwaitingImages;

	// Frames, waiting to be presented
	Vector< Pair<Rc<PresentationFrame>, Rc<ImageStorage>>> _scheduledForPresent;

	// Handles, waiting for their present windows
	Set<Rc<sprt::dispatch::Handle>> _scheduledPresentHandles;

	// Per-frame deadline timers (cancel a frame stuck waiting for input/dependencies)
	Map<PresentationFrame *, Rc<sprt::dispatch::Handle>> _frameDeadlines;

	// In-flight frames tagged Remote (served by a remote render client). Tracked from scheduling -- even
	// while still awaiting the client's reply, before they enter _activeFrames -- so they can be killed
	// on a connection reset (invalidateRemoteFrames). The value Rc keeps an awaiting frame alive after
	// the connection (its only other owner) drops. Erased at every terminal frame transition.
	Map<PresentationFrame *, Rc<PresentationFrame>> _remoteFrames;

	// Async request for a swapchain images
	Set<Swapchain::SwapchainAcquiredImage *> _requestedSwapchainImage;

	// Already acquired swapchain images
	List<Rc<Swapchain::SwapchainAcquiredImage>> _acquiredSwapchainImages;

	Set<PresentationFrame *> _activeFrames;
	Set<PresentationFrame *> _totalFrames;
	Set<PresentationFrame *> _detachedFrames;

	UpdateConstraintsFlags _deprecationFlags = UpdateConstraintsFlags::None;
	Vector<Function<void(bool)>> _deprecationCallbacks;
	Rc<sprt::dispatch::TimerHandle> _acquisitionTimer;
};

} // namespace stappler::xenolith::core
#endif /* XENOLITH_CORE_XLCOREPRESENTATIONENGINE_H_ */
