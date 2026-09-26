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

#include "WmClientScene.h"

#include "XL2dSceneContent.h"
#include "XLDirector.h"
#include "XLServerAppThread.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::wm {

bool WmClientScene::init(NotNull<AppThread> app, NotNull<core::RenderServerChannel> window,
		const core::FrameConstraints &constraints, StringView label) {
	if (!Scene2d::init(app, window, constraints)) {
		return false;
	}

	_label = label.str<Interface>();
	setContent(Rc<basic2d::SceneContent2d>::create());
	setFpsVisible(false);
	return true;
}

void WmClientScene::handlePresented(Director *dir) {
	Scene2d::handlePresented(dir);

	// Once: a swapchain rebuild presents the scene again, and a second share would announce the
	// window twice.
	if (!_shared) {
		_shared = share();
	}
}

void WmClientScene::describeQueue(QueueInfo &info) { info.type = QueueType::Flat; }

bool WmClientScene::share() {
	auto dir = getDirector();
	auto app = dir ? dynamic_cast<ServerAppThread *>(dir->getApplication()) : nullptr;
	if (!app || !app->isListening()) {
		log::source().warn("WmClientScene", _label, ": no session to share the window with");
		return false;
	}

	core::Queue::Builder builder(toString("WmSurfaceQueue.", _label));

	QueueInfo queueInfo{
		Extent2(_constraints.extent.width, _constraints.extent.height),
		Color4F::WHITE,
	};

	describeQueue(queueInfo);
	buildQueueResources(queueInfo, builder);

	if (!buildQueue(dir->getApplication(), queueInfo, builder)) {
		log::source().error("WmClientScene", _label, ": fail to build the queue");
		return false;
	}

	dir->shareQueue(sp::move(builder));
	return true;
}

} // namespace stappler::xenolith::wm
