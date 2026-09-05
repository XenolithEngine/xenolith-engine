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

#ifndef XENOLITH_APPLICATION_XLREMOTEWINDOW_H_
#define XENOLITH_APPLICATION_XLREMOTEWINDOW_H_

#include "XLCoreRenderSession.h"
#include "XLWindowSceneInfo.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class ClientAppThread;

class SP_PUBLIC RemoteWindow : public Ref, public core::RenderServerChannel {
public:
	// One shared queue as the server announced it. `api` and `typeTag` are what a scene matches
	// against to pick a queue it can drive (see Scene2d::selectServerQueue); `name` is left for
	// diagnostics and for a scene that really does want a queue by name.
	struct RemoteQueueInfo {
		uint64_t id;
		String name;
		core::InstanceApi api = core::InstanceApi::None;
		uint32_t typeTag = 0;
		core::QueueDamageFlags damage = core::QueueDamageFlags::None;
	};

	virtual ~RemoteWindow();

	virtual bool init(NotNull<ClientAppThread>, const Value &);

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

	virtual void setReadyForNextFrame() override;
	virtual void setPreferredFrameInterval(uint64_t intervalUs) override;
	virtual core::FrameTimingInfo getFrameTiming() const override;

	virtual void acquireScreenInfo(Function<void(NotNull<core::ScreenInfo>)> &&,
			Ref * = nullptr) override;
	virtual void acquireTextInput(core::TextInputRequest &&) override;
	virtual void releaseTextInput() override;
	virtual void performTextInput(core::TextInputCommand &&) override;
	virtual void close(bool graceful = true) override;

	virtual void handleBackButton() override;

	// Not overridden before: the base answered ErrorNotSupported, which is right for a channel with
	// no window and wrong for one whose window is simply elsewhere.
	virtual void setWindowExtent(Extent2, Function<void(Status)> && = nullptr,
			Ref * = nullptr) override;

	virtual const sprt::window::WindowInfo *getInfo() const override;

	// Mirror of AppWindow::getSceneInfo(). A remote window is announced by the server rather than
	// created locally, so the client sets this itself before the window takes a Director. App
	// thread.
	WindowSceneInfo *getSceneInfo() const { return _sceneInfo; }
	void setSceneInfo(Rc<WindowSceneInfo> &&s) { _sceneInfo = sp::move(s); }

	virtual bool enableState(core::WindowState) override;
	virtual bool disableState(core::WindowState) override;

	virtual bool setFullscreen(core::FullscreenInfo &&, Function<void(Status)> &&,
			Ref * = nullptr) override;

	virtual bool setPreferredFrameRate(float, Function<void(Status)> && = nullptr) override;

	virtual void captureScreenshot(
			Function<void(const core::ImageInfoData &info, BytesView view)> &&cb) override;

	// Deliver a screenshot that returned over Domain::Data: invoke the captureScreenshot() callback
	// registered for `serial` (the RequestScreenshot serial echoed in the transfer's announce reason).
	// Returns true iff a matching pending capture was found and fulfilled.
	bool deliverScreenshot(uint32_t serial, const core::ImageInfoData &info, BytesView pixels);

	virtual bool openWindowMenu(Vec2 pos) override;

	virtual void handleInputEvents(Vector<core::InputEventData> &&events) override;

	// Server-pushed text-input state (WindowCode::TextInputState) -- the echo from the window's
	// processor. Not an override, for the same reason handleWindowGeometryChanged is not: this is
	// the receiving end of a RenderClientChannel call the server made.
	void handleTextInput(const core::TextInputState &);

	// Server-pushed window geometry (WindowCode::WindowGeometryChanged). Updates the mirror
	// getWindowGeometry() serves and forwards to the local Director, so a remote scene hears about
	// a move on the same hook a local one does. Not an override: RenderServerChannel has no such
	// method -- this is the receiving end of a RenderClientChannel call made on the server.
	void handleWindowGeometryChanged(const sprt::window::WindowGeometry &);

	virtual void updateLayers(sprt::window::Vector<sprt::window::WindowLayer> &&) override;

	uint64_t getServerId() const { return _id; }

	SpanView<RemoteQueueInfo> getQueues() const { return _queues; }

	// Drive the local Director for a server frame request: the scene graph selects one of the shared
	// queues; `reply` is invoked with that queue's server id (0 if none could be selected). Per-frame
	// attachment input is NOT serialized at this stage.
	// `timing` and `stat` ride along with the request (see RemoteRenderClient::acquireFrame); pass
	// null for either when the server did not send it, which is how a version-1 server looks. Null
	// means "no update", NOT zeros -- overwriting the mirror with zeros every frame would make the
	// client's FPS overlay flicker to nothing against an older server.
	void acquireFrame(uint64_t frameId, const core::FrameConstraints &,
			const core::FrameTimingInfo *timing, const core::DrawStat *stat,
			Function<void(uint64_t queueId)> &&reply);

protected:
	uint64_t _id = 0;
	Rc<sprt::window::WindowInfo> _info;
	Vector<RemoteQueueInfo> _queues;
	Rc<ClientAppThread> _thread; // creates cyclic reference until windows is closed/detached

	// Set by the client; consulted by ClientAppThread::makeScene before the process-wide symbol.
	Rc<WindowSceneInfo> _sceneInfo;

	// captureScreenshot() callbacks awaiting their pixels, keyed by the RequestScreenshot serial.
	Map<uint32_t, Function<void(const core::ImageInfoData &, BytesView)>> _pendingScreenshots;

	/* Send one WindowCode::WindowControl request and route its Status back to `cb`.
	
	Every control op funnels through here. The reply is delivered by the ordinary reply machinery
	(AppThread::waitForReply keys by serial), so a dropped connection is already handled: the request
	watchdog synthesizes an error reply and `cb` gets an error Status instead of silence. That is
	precisely the defect this milestone fixes elsewhere in this class -- acquireScreenInfo and
	setFullscreen used to take a callback and never call it. */
	void sendWindowControl(remote::WindowControlOp, Value &&args, Function<void(Status)> &&cb);

	// Fire-and-forget counterpart for the text-input ops; the answer is the state echo, not a reply.
	void sendTextInputControl(Value &&args);

	// The server's frame telemetry, mirrored here because getFrameTiming() is a synchronous getter
	// the wire cannot answer. Fed by the AcquireFrame piggyback.
	core::FrameTimingInfo _frameTiming;

	// Last forwarded WindowLayer payload (the raw WindowLayer[] bytes, no window-id prefix), so identical
	// per-frame layer sets are not re-sent (updateLayers is driven every input commit).
	Bytes _lastLayersBlob;
};

} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_XLREMOTEWINDOW_H_ */
