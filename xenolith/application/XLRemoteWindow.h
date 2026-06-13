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

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class ClientAppThread;

class SP_PUBLIC RemoteWindow : public Ref, public core::RenderServerChannel {
public:
	struct RemoteQueueInfo {
		uint64_t id;
		String name;
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
	virtual void close(bool graceful = true) override;

	virtual void handleBackButton() override;

	virtual const sprt::window::WindowInfo *getInfo() const override;

	virtual bool enableState(core::WindowState) override;
	virtual bool disableState(core::WindowState) override;

	virtual bool setFullscreen(core::FullscreenInfo &&, Function<void(Status)> &&,
			Ref * = nullptr) override;

	virtual bool setPreferredFrameRate(float, Function<void(Status)> && = nullptr) override;

	virtual void captureScreenshot(
			Function<void(const core::ImageInfoData &info, BytesView view)> &&cb) override;

	virtual bool openWindowMenu(Vec2 pos) override;

	virtual void handleInputEvents(Vector<core::InputEventData> &&events) override;

	virtual void updateLayers(sprt::window::Vector<sprt::window::WindowLayer> &&) override;

	uint64_t getServerId() const { return _id; }

	SpanView<RemoteQueueInfo> getQueues() const { return _queues; }

	// Drive the local Director for a server frame request: the scene graph selects one of the shared
	// queues; `reply` is invoked with that queue's server id (0 if none could be selected). Per-frame
	// attachment input is NOT serialized at this stage.
	void acquireFrame(uint64_t frameId, const core::FrameConstraints &,
			Function<void(uint64_t queueId)> &&reply);

protected:
	uint64_t _id = 0;
	Rc<sprt::window::WindowInfo> _info;
	Vector<RemoteQueueInfo> _queues;
	Rc<ClientAppThread> _thread; // creates cyclic reference until windows is closed/detached
};

} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_XLREMOTEWINDOW_H_ */
