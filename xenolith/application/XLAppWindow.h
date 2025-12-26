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

#ifndef XENOLITH_APPLICATION_XLBASICWINDOW_H_
#define XENOLITH_APPLICATION_XLBASICWINDOW_H_

#include "XLContext.h"
#include "XLEvent.h"
#include "XLCoreTextInput.h"
#include "XLCorePresentationEngine.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class Director;

enum class AppWindowConfigFlags {
	None = 0,
};

class SP_PUBLIC AppWindow : public Ref, core::PresentationWindow {
public:
	using InputEventData = core::InputEventData;
	using InputEventName = core::InputEventName;
	using TextInputRequest = core::TextInputRequest;
	using TextInputState = core::TextInputState;

	// In most cases, this can be received via InputListener,
	// but for objects without scene binding you can use this event
	static EventHeader onWindowState;

	virtual ~AppWindow();

	virtual bool init(NotNull<Context>, NotNull<AppThread>, NotNull<NativeWindow>);

	virtual void runWithQueue(const Rc<core::Queue> &); // from view thread

	virtual void run(); // from view thread

	virtual void update(core::PresentationUpdateFlags); // from view thread
	virtual void end(); // from view thread

	virtual void close(bool graceful = true);

	virtual void handleInputEvents(Vector<InputEventData> &&);
	virtual void handleTextInput(const TextInputState &);

	Context *getContext() const { return _context; }
	AppThread *getApplication() const { return _application; }

	core::WindowState getWindowState() const { return _state; } // from any thread

	// Note that WindowInfo describes window state in main thread,
	// you should not use it in app thread, except for constant fields (like flags)
	//
	// To access WindowState in app thread, use getWindowState
	const WindowInfo *getInfo() const;

	StringView getId() const { return _windowId; }

	WindowCapabilities getCapabilities() const { return _capabilities; }

	core::PresentationEngine *getPresentationEngine() const { return _presentationEngine; }

	Director *getDirector() const { return _director; }

	// It's not safe to ask PresentationEngine about current config, use this instead
	const core::SwapchainConfig &getAppSwapchainConfig() const { return _appSwapchainConfig; }

	// Run constraints update process
	void updateConstraints(core::UpdateConstraintsFlags); // from any thread

	void setReadyForNextFrame(); // from any thread

	// Block current thread until next frame
	bool waitUntilFrame();

	void setPresentationOnDemand(bool value); // from any thread
	bool isPresentationOnDemand() const; // from any thread

	// Set frame interval for Presentation engine
	// Can be used to limit frame rate on value, lower that current display mode
	// Can be called from any thread
	void setPresentationFrameInterval(uint64_t);

	// Get frame interval of presentation engine
	// 0 if no frame interval is set
	uint64_t getPresentationFrameInterval() const;

	// State flags you can enable or disable
	WindowState getUpdatableStateFlags() const;

	// try to change WindowState by adding new flag
	// Only one flag can be set per call
	//
	// WindowState::Fullscreen: acts like setFullscreen(FullscreenInfo::Current)
	// WindowState::CloseRequest: if this flag is NOT set in _state:  calls AppWindow::close() (so, ExitGuard can be triggered)
	// WindowState::CloseRequest: if this flag IS set in _state: forceы window to be closed by WM
	bool enableState(WindowState); // from app thread

	// try to change WindowState by removing flag
	// Only one flag can be removed per call
	//
	// WindowState::Fullscreen: acts like setFullscreen(FullscreenInfo::None)
	// WindowState::CloseRequest: if this flag IS set in _state: discards close request and re-enables ExitGuard if it is retained
	bool disableState(WindowState); // from app thread

	void acquireTextInput(TextInputRequest &&);
	void releaseTextInput();

	void updateLayers(Vector<WindowLayer> &&); // from app thread

	// Acquire data describing current monitor configuration
	void acquireScreenInfo(Function<void(NotNull<ScreenInfo>)> &&, Ref * = nullptr);

	// Try to enter or exit fullscreen mode with specific mode
	// Use FullscreenInfo::Current to use current monitor and mode for fullscreen
	// Use FullscreenInfo::None to exit fullscreen mode
	//
	// At least WindowCapabilities::Fullscreen should be available for successful call
	//
	// WindowCapabilities::FullscreenWithMode should be available for values,
	// other then FullscreenInfo::Current and FullscreenInfo::None
	//
	// WindowCapabilities::FullscreenExclusive required to use FullscreenFlags::Exclusive
	//
	// Without WindowCapabilities::FullscreenSeamlessModeSwitch, to set new display mode
	// for already-fullscreened window, engine will exit fullscreen mode, then re-enter
	// it with the new mode
	bool setFullscreen(FullscreenInfo &&, Function<void(Status)> &&, Ref * = nullptr);

	// Try to set preferred framerate for OS WM.
	// WindowCapabilities::PreferredFrameRate should be available
	bool setPreferredFrameRate(float, Function<void(Status)> && = nullptr);

	// Capture current window contents as an image buffer
	// (makes screenshot of the window's content without OS decorations)
	//
	// This call actually performs frame rendering into offscreen buffer
	// (via PresentationEngine::scheduleSwapchainImage with PresentationFrame::OffscreenTarget),
	// that then will be returned as info + data
	void captureScreenshot(Function<void(const core::ImageInfoData &info, BytesView view)> &&cb);

	// pos - Location, on which window menu should be opened in presentation (Scene) coords;
	// Use Vec2::INVALID to open window menu in current pointer location;
	// WindowState::AlloedWindowMenu should be enabled
	bool openWindowMenu(Vec2 pos);

	// Simulate back button press/gesture from app's thread (on Android)
	// It shouldn't be used on modern Android devices (above API 32), instead,
	//
	// use WindowLayerFlags::BackButtonHandler on listeners, that handles Back button,
	// which integrates with Predictive Back Gesture
	// (https://developer.android.com/guide/navigation/custom-back/predictive-back-gesture)
	//
	// Note, that on API 33+ it should be enabled in manifest with
	//  android:enableOnBackInvokedCallback="true"
	// in <application> or <activity>
	void handleBackButton();

protected:
	virtual core::ImageInfo getSwapchainImageInfo(const core::SwapchainConfig &cfg) const override;
	virtual core::ImageViewInfo getSwapchainImageViewInfo(
			const core::ImageInfo &image) const override;
	virtual core::SurfaceInfo getSurfaceOptions(const core::Device &,
			NotNull<core::Surface>) const override;

	virtual core::SwapchainConfig selectConfig(const core::SurfaceInfo &, bool fastMode) override;

	virtual void acquireFrameData(NotNull<core::PresentationFrame>,
			Function<void(NotNull<core::PresentationFrame>)> &&) override;

	virtual void handleFramePresented(NotNull<core::PresentationFrame>) override;

	virtual Rc<core::Surface> makeSurface(NotNull<core::Instance>) override;
	virtual core::FrameConstraints exportConstraints() const override;

	virtual void setFrameOrder(uint64_t) override;

	virtual void propagateInputEvent(InputEventData &); // from app thread
	virtual void propagateTextInput(TextInputState &); // from app thread

	virtual void handleContextStateUpdate(WindowState state);
	virtual void synchronizeClose();

	String _windowId; // should be constant
	Rc<Context> _context;
	Rc<AppThread> _application;
	Rc<Director> _director;
	NativeWindow *_window = nullptr;
	Rc<core::PresentationEngine> _presentationEngine;

	core::WindowState _state = core::WindowState::None; // for app thread
	core::WindowState _contextState = core::WindowState::None; // for context thread

	bool _inCloseRequest = false;
	bool _syncClose = false;

	core::SwapchainConfig _appSwapchainConfig;

	WindowCapabilities _capabilities = WindowCapabilities::None;
};

} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_XLBASICWINDOW_H_ */
