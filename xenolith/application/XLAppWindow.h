/**
 Copyright (c) 2025 Stappler Team <admin@stappler.org>
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

#ifndef XENOLITH_APPLICATION_XLBASICWINDOW_H_
#define XENOLITH_APPLICATION_XLBASICWINDOW_H_

#include "XLContext.h"
#include "XLEvent.h"
#include "XLWindowSceneInfo.h"
#include "XLCoreTextInput.h"
#include "XLCorePresentationEngine.h"
#include "XLCoreRenderSession.h"
#include "XLCoreFrameRequestProxy.h"

#include <sprt/runtime/window/interface.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class Director;
class ServerAppThread;

enum class AppWindowConfigFlags {
	None = 0,
};

class SP_PUBLIC AppWindow : public sprt::window::AppWindow,
							core::PresentationWindow,
							public core::RenderServerChannel {
public:
	using InputEventData = core::InputEventData;
	using InputEventName = core::InputEventName;
	using TextInputRequest = core::TextInputRequest;
	using TextInputState = core::TextInputState;

	// In most cases, this can be received via InputListener,
	// but for objects without scene binding you can use this event
	static EventHeader onWindowState;

	virtual ~AppWindow();

	virtual bool init(NotNull<Context>, NotNull<ServerAppThread>, NotNull<NativeWindow>);

	virtual void runWithQueue(const Rc<core::Queue> &); // from view thread

	virtual void run() override; // from view thread

	virtual void update(core::PresentationUpdateFlags) override; // from view thread
	virtual void end(); // from view thread

	virtual void close(bool graceful = true) override;

	// Popup/Tooltip dismiss — a graceful close, auxiliary windows are not reused.
	virtual void hide() override;

	// True from the moment close() is accepted until the window is destroyed. Refuses engine
	// start and non-teardown constraint updates on a window that is already winding down.
	bool isInCloseRequest() const { return _inCloseRequest; }

	// True once the first scheduled present has reported, success or not.
	bool hasCompletedFirstFrame() const { return _firstFrameCompleted; }

	// Resize the native content (points).
	void setContentExtent(Extent2);

	virtual void handleInputEvents(Vector<InputEventData> &&) override;
	virtual void handleTextInput(const TextInputState &);

	Context *getContext() const { return _context; }
	ServerAppThread *getApplication() const { return _application; }

	// Note that WindowInfo describes window state in main thread,
	// you should not use it in app thread, except for constant fields (like flags)
	//
	// To access WindowState in app thread, use getWindowState
	virtual const WindowInfo *getInfo() const override;

	// Native surface backend for this window (Display = direct KMS / embed).
	sprt::window::SurfaceBackend getSurfaceBackend() const;

	core::PresentationEngine *getPresentationEngine() const { return _presentationEngine; }

	Director *getDirector() const { return _director; }

	// The application payload this window was created with, or null for a window the application
	// did not create itself (above all the root one, whose WindowInfo is built from the command
	// line before the app thread exists). App thread.
	WindowSceneInfo *getSceneInfo() const { return _sceneInfo; }

	// Run constraints update process
	void updateConstraints(core::UpdateConstraintsFlags); // from any thread

	void setReadyForNextFrame() override; // from any thread

	// Force-invalidate all in-flight frames served by a remote render client (PresentationFrame::Remote).
	// Called when the remote connection is reset so a frame stuck on a dead client cannot wedge the
	// pipeline before the window reverts to its local Director. From any thread.
	void invalidateRemoteFrames();

	// Restart presentation after the window's render client changed (remote takeover or revert). Clears a
	// possibly-stale display-link barrier and pumps one fresh frame. From any thread.
	void resetForRenderClientChange();

	// Block current thread until next frame
	virtual bool waitUntilFrame() override;

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
	virtual bool enableState(WindowState) override; // from app thread

	// try to change WindowState by removing flag
	// Only one flag can be removed per call
	//
	// WindowState::Fullscreen: acts like setFullscreen(FullscreenInfo::None)
	// WindowState::CloseRequest: if this flag IS set in _state: discards close request and re-enables ExitGuard if it is retained
	virtual bool disableState(WindowState) override; // from app thread

	virtual void acquireTextInput(TextInputRequest &&) override;
	virtual void releaseTextInput() override;

	virtual void updateLayers(sprt::window::Vector<WindowLayer> &&) override; // from app thread

	// Acquire data describing current monitor configuration
	virtual void acquireScreenInfo(Function<void(NotNull<ScreenInfo>)> &&,
			Ref * = nullptr) override;

	// core::RenderServerChannel (client -> server) additions.
	// (setReadyForNextFrame / acquireScreenInfo / acquireTextInput / releaseTextInput / close are
	//  satisfied by the existing methods.)
	virtual void compileRenderQueue(const Rc<core::Queue> &,
			Function<void(bool)> && = nullptr) override;
	virtual void compileResource(Rc<core::Resource> &&, Function<void(bool)> && = nullptr,
			bool preload = false) override;
	virtual void compileMaterials(Rc<core::MaterialInputData> &&,
			const Vector<Rc<core::DependencyEvent>> & =
					Vector<Rc<core::DependencyEvent>>()) override;
	virtual void compileImage(const Rc<core::DynamicImage> &,
			Function<void(bool)> && = nullptr) override;
	virtual void attachRenderQueue(const Rc<core::Queue> &) override;
	virtual void setPreferredFrameInterval(uint64_t intervalUs) override;
	virtual core::FrameTimingInfo getFrameTiming() const override;

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
	virtual void setWindowExtent(Extent2, Function<void(Status)> && = nullptr,
			Ref * = nullptr) override;

	virtual bool setFullscreen(FullscreenInfo &&, Function<void(Status)> &&,
			Ref * = nullptr) override;

	// Open an OS dialog owned by this window; its completion runs on the app thread. The dialog is
	// cancelled if the window closes — the callback still fires, with Status::ErrorCancelled,
	// rather than being dropped.
	//
	// Keep the Rc<DialogRequest>: it is the cancellation token. The backend's DialogHandle never
	// crosses to this thread.
	virtual Status openDialog(NotNull<sprt::window::DialogRequest>) override;
	virtual Status cancelDialog(NotNull<sprt::window::DialogRequest>) override;

	// True while any dialog opened through this window is still on screen. App thread.
	bool hasPendingDialogs() const { return !_pendingDialogs.empty(); }

	// Try to set preferred framerate for OS WM.
	// WindowCapabilities::PreferredFrameRate should be available
	virtual bool setPreferredFrameRate(float, Function<void(Status)> && = nullptr) override;

	// Capture current window contents as an image buffer
	// (makes screenshot of the window's content without OS decorations)
	//
	// This call actually performs frame rendering into offscreen buffer
	// (via PresentationEngine::scheduleSwapchainImage with PresentationFrame::OffscreenTarget),
	// that then will be returned as info + data
	virtual void captureScreenshot(
			Function<void(const core::ImageInfoData &info, BytesView view)> &&cb) override;

	// pos - Location, on which window menu should be opened in presentation (Scene) coords;
	// Use Vec2::INVALID to open window menu in current pointer location;
	// WindowState::AlloedWindowMenu should be enabled
	virtual bool openWindowMenu(Vec2 pos) override;

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
	virtual void handleBackButton() override;

protected:
	virtual core::ImageInfo getSwapchainImageInfo(const core::SwapchainConfig &cfg) const override;
	virtual core::ImageViewInfo getSwapchainImageViewInfo(
			const core::ImageInfo &image) const override;
	virtual core::SurfaceInfo getSurfaceOptions(const core::Device &,
			NotNull<core::Surface>) const override;

	virtual core::SwapchainConfig selectConfig(const core::SurfaceInfo &, bool fastMode) override;

	virtual void acquireFrameData(NotNull<core::PresentationFrame>,
			Function<void(NotNull<core::PresentationFrame>)> &&) override;

	virtual void handleFrameReady(NotNull<core::PresentationFrame>) override;
	virtual void handleFramePresented(NotNull<core::PresentationFrame>) override;
	virtual void handleSwapchainUpdated(const core::FrameConstraints &) override;

	virtual Rc<core::Surface> makeSurface(NotNull<core::Instance>) override;
	virtual core::FrameConstraints exportConstraints(uint64_t &serial) const override;

	virtual void setFrameOrder(uint64_t) override;

	virtual StringView getPresentationDebugId() const override { return _windowId; }

	virtual void handleContextStateUpdate(WindowState state);
	virtual void synchronizeClose();

	// Hand _sceneInfo back to the app thread, where it is destroyed and its close callback fires.
	// Used by the teardown path that never gets there on its own.
	void releaseSceneInfo();

	Rc<Context> _context;

	Rc<ServerAppThread> _application;
	Rc<Director> _director;
	NativeWindow *_window = nullptr;
	Rc<core::PresentationEngine> _presentationEngine;

	// Taken off WindowInfo::appData in init() (context thread), handed to the app thread in end()
	// so it is destroyed there and its close callback fires there. See WindowSceneInfo.
	Rc<WindowSceneInfo> _sceneInfo;

	core::WindowState _contextState = core::WindowState::None; // for context thread

	// Dialogs opened through this window that have not answered yet. App thread only. Kept so a
	// teardown can answer whatever is still outstanding, and so a scene can ask whether a dialog
	// is up. Entries remove themselves from the completion wrapper installed in openDialog.
	Vector<Rc<sprt::window::DialogRequest>> _pendingDialogs;

	bool _inCloseRequest = false;
	bool _syncClose = false;
	bool _firstFrameCompleted = false;
	bool _mapOnFirstFrame = false;
};

} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_XLBASICWINDOW_H_ */
