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

#include "InstallerMainScene.h"
#include "InstallerSceneContent.h"

#include "XLEntryPoint.h"
#include "XL2dSceneContent.h"

#include "XLWindowDecorations.h"
#include "XLUiStyleSystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

bool MainScene::init(NotNull<AppThread> app, NotNull<core::RenderServerChannel> window,
		const core::FrameConstraints &constraints) {

	if (!Scene2d::init(app, window, constraints)) {
		return false;
	}

	auto content = Rc<InstallerSceneContent>::create();

	content->setDefaultLights();

	setContent(content);

	setFpsVisible(true);

	return true;
}

void MainScene::handleContentSizeDirty() { Scene2d::handleContentSizeDirty(); }

void MainScene::handleEnter(Scene *scene) { Scene2d::handleEnter(scene); }

void MainScene::handlePresented(Director *dir) { Scene2d::handlePresented(dir); }

void MainScene::buildQueueResources(QueueInfo &, core::Queue::Builder &builder) {
	builder.addImage("app-icon.png",
			core::ImageInfo(core::ImageFormat::R8G8B8A8_UNORM, core::ImageUsage::Sampled),
			FileInfo("resources/app-icon.png", FileCategory::Bundled));
}

DEFINE_PRIMARY_SCENE_CLASS(MainScene)

} // namespace stappler::xenolith::installer
