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

#include "XLCore.h"
#include "XLCoreDeviceQueue.h"
#include "XLCoreInfo.h"
#include "XLCoreSwapchain.h"
#include "XLCorePresentationFrame.h"
#include "SPMovingAverage.h"
#include "SPEventHandle.h"
#include "XlCoreMonitorInfo.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

class PresentationFrame;

class SP_PUBLIC PresentationWindow {
public:
	virtual ~PresentationWindow() = default;

	virtual ImageInfo getSwapchainImageInfo(const SwapchainConfig &cfg) const = 0;
	virtual ImageViewInfo getSwapchainImageViewInfo(const ImageInfo &image) const = 0;
	virtual SurfaceInfo getSurfaceOptions(const Device &, NotNull<Surface>) const = 0;

	virtual SwapchainConfig selectConfig(const SurfaceInfo &, bool fastMode) = 0;

	virtual void acquireFrameData(NotNull<PresentationFrame>,
			Function<void(NotNull<PresentationFrame>)> &&) = 0;

	virtual void handleFramePresented(NotNull<PresentationFrame>) = 0;

	virtual Rc<Surface> makeSurface(NotNull<Instance>) = 0;
	virtual FrameConstraints exportConstraints() const = 0;

	virtual void setFrameOrder(uint64_t) = 0;
};

enum class UpdateConstraintsFlags : uint32_t {
	None,
	DeprecateSwapchain = 1 << 0,
	SwitchToFastMode = 1 << 1,
	SwitchToNext = 1 << 2,
	EndOfLife = 1 << 3,
	Finalized = 1 << 4,
	EnableLiveResize = 1 << 5,
	DisableLiveResize = 1 << 6,
	SyncUpdate = 1 << 7,

	// to be more clear what DeprecateSwapchain means
	WindowResized = DeprecateSwapchain,
};

SP_DEFINE_ENUM_AS_MASK(UpdateConstraintsFlags)

enum class PresentationUpdateFlags : uint32_t {
	None,
	DisplayLink = 1 << 0,
	FlushPending = 1 << 1,
};

SP_DEFINE_ENUM_AS_MASK(PresentationUpdateFlags)

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

	virtual bool handleFrameStarted(NotNull<PresentationFrame>);
	virtual void handleFrameInvalidated(NotNull<PresentationFrame>);
	virtual void handleFrameReady(NotNull<PresentationFrame>);
	virtual void handleFramePresented(NotNull<PresentationFrame>);
	virtual void handleFrameComplete(NotNull<PresentationFrame>);

	virtual void captureScreenshot(Function<void(const ImageInfoData &info, BytesView view)> &&cb);

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

	void scheduleImage(NotNull<PresentationFrame>);

	Status acquireScheduledImage();
	void scheduleImageAcquisition();

	void handleSwapchainImageReady(Rc<Swapchain::SwapchainAcquiredImage> &&image);

	void runScheduledPresent(NotNull<PresentationFrame> frame, ImageStorage *image,
			uint64_t presentWindow);
	void presentSwapchainImage(Rc<DeviceQueue> &&queue, NotNull<PresentationFrame> frame,
			ImageStorage *image, uint64_t presentWindow);

	void presentWithQueue(DeviceQueue &queue, NotNull<PresentationFrame> frame, ImageStorage *image,
			uint64_t presentWindow);

	bool canScheduleNextFrame() const;

	PresentationOptions _options;
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
	std::atomic<uint64_t> _lastPresentationInterval = 0;
	math::MovingAverage<FrameAverageCount, uint64_t> _avgPresentationInterval;
	std::atomic<uint64_t> _avgPresentationIntervalValue = 0;

	uint64_t _lastFrameTime = 0;
	math::MovingAverage<FrameAverageCount, uint64_t> _avgFrameTime;
	std::atomic<uint64_t> _avgFrameTimeValue = 0;

	uint64_t _lastFenceFrameTime = 0;
	math::MovingAverage<FrameAverageCount, uint64_t> _avgFenceInterval;
	std::atomic<uint64_t> _avgFenceIntervalValue = 0;

	uint64_t _lastTimestampFrameTime = 0;
	math::MovingAverage<FrameAverageCount, uint64_t> _avgTimestampInterval;
	std::atomic<uint64_t> _avgTimestampIntervalValue = 0;

	uint64_t _frameOrder = 0; // current scheduled frame order

	bool _running = false;
	bool _readyForNextFrame = false;
	bool _waitUntilFrame = false;
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
	std::deque<Rc<PresentationFrame>> _framesAwaitingImages;

	// Frames, waiting to be presented
	Vector< Pair<Rc<PresentationFrame>, Rc<ImageStorage>>> _scheduledForPresent;

	// Handles, waiting for their present windows
	Set<Rc<event::Handle>> _scheduledPresentHandles;

	// Async request for a swapchain images
	Set<Swapchain::SwapchainAcquiredImage *> _requestedSwapchainImage;

	// Already acquired swapchain images
	std::deque<Rc<Swapchain::SwapchainAcquiredImage>> _acquiredSwapchainImages;

	Set<PresentationFrame *> _activeFrames;
	Set<PresentationFrame *> _totalFrames;
	Set<PresentationFrame *> _detachedFrames;

	UpdateConstraintsFlags _deprecationFlags = UpdateConstraintsFlags::None;
	Vector<Function<void(bool)>> _deprecationCallbacks;
	Rc<event::TimerHandle> _acquisitionTimer;
};

} // namespace stappler::xenolith::core
#endif /* XENOLITH_CORE_XLCOREPRESENTATIONENGINE_H_ */
