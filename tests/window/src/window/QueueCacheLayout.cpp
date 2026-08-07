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

#include "XLCommon.h"

#include "window/QueueCacheLayout.h"
#include "window/SecondaryWindow.h"

#include "XL2dLabel.h"
#include "XL2dSceneLayout.h"
#include "XLAction.h"
#include "XLAppThread.h"
#include "XLAppWindow.h"
#include "XLDirector.h"
#include "resources/XLQueueCache.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

namespace {

// Content of each cached-queue window: one label saying which window it is. Deliberately plain -
// what is under test is the graph they render through, not what they draw.
class CachedWindowContent : public basic2d::SceneLayout2d {
public:
	virtual bool init(StringView caption) {
		if (!SceneLayout2d::init()) {
			return false;
		}
		_label = addChild(Rc<basic2d::Label>::create(), ZOrder(1));
		_label->setFontSize(20);
		_label->setColor(Color::White, false);
		_label->setString(caption);
		_label->setAnchorPoint(Anchor::Middle);

		// Headless renders on demand and these windows have no FPS counter to keep them dirty.
		runAction(Rc<RenderContinuously>::create());
		return true;
	}

	virtual void handleContentSizeDirty() override {
		SceneLayout2d::handleContentSizeDirty();
		_label->setPosition(_contentSize / 2.0f);
	}

protected:
	basic2d::Label *_label = nullptr;
};

} // namespace

bool QueueCacheLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	_status = addChild(Rc<basic2d::Label>::create(), ZOrder(1));
	_status->setFontSize(18);
	_status->setColor(Color::White, false);
	_status->setString("queue cache: warming up");
	_status->setAnchorPoint(Anchor::Middle);

	runAction(Rc<Sequence>::create(Rc<DelayTime>::create(0.6f), [this] { prewarmQueue(); },
			Rc<DelayTime>::create(0.8f), [this] { openWindows(); },
			Rc<DelayTime>::create(2.6f), [this] { runChecks(); }));

	return true;
}

void QueueCacheLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();
	_status->setPosition(Vec2(_contentSize.width / 2.0f, getWorkTop() / 2.0f));
}

void QueueCacheLayout::handleExit() {
	for (auto &it : _windows) { SecondaryWindow::close(it); }
	_windows.clear();
	_prewarmed = nullptr;
	TestLayout::handleExit();
}

void QueueCacheLayout::prewarmQueue() {
	auto app = _director ? _director->getApplication() : nullptr;
	auto cache = app ? app->getExtension<QueueCache>() : nullptr;
	auto server = _director ? _director->getRenderServer() : nullptr;
	if (!cache || !server) {
		log::source().error("QueueCacheTest", "no QueueCache or no render server");
		_prewarmDone = true;
		return;
	}

	log::source().warn("QueueCacheTest", "prewarming '", kQueueName,
			"' with no window of its own yet");

	cache->acquire(kQueueName, server,
			[app](core::Queue::Builder &builder) {
		// Extent here is only a placeholder: FrameQueue::setup rewrites every image attachment's
		// extent from the frame's constraints, which is exactly why one graph can serve windows of
		// different sizes.
		basic2d::Scene2d::QueueInfo info{
			Extent2(1'024, 768),
			Color4F::BLACK,
		};
		return basic2d::Scene2d::buildQueue(app, info, builder);
	},
			[this](Rc<core::Queue> &&queue) {
		_prewarmDone = true;
		_prewarmOk = queue != nullptr;
		_prewarmed = sp::move(queue);
		log::source().warn("QueueCacheTest",
				"prewarm complete: ", _prewarmOk ? "compiled" : "FAILED");
	});
}

void QueueCacheLayout::openWindows() {
	auto server = _director ? _director->getRenderServer() : nullptr;
	if (!server || !_prewarmed) {
		log::source().error("QueueCacheTest", "nothing prewarmed, not opening windows");
		return;
	}

	for (uint32_t i = 0; i < kWindowCount; ++i) {
		auto id = toString("queue-cache-", i + 1);
		auto handle = SecondaryWindow::open(static_cast<AppWindow *>(server), id, Extent2(420, 160),
				[caption = id](StringView) -> Rc<basic2d::SceneLayout2d> {
			return Rc<CachedWindowContent>::create(caption);
		}, nullptr, Rc<core::Queue>(_prewarmed));
		if (handle) {
			_windows.emplace_back(sp::move(handle));
		}
	}

	log::source().warn("QueueCacheTest", "opened ", _windows.size(),
			" windows on the prewarmed queue");
}

void QueueCacheLayout::expect(bool cond, StringView what) {
	++_checks;
	if (!cond) {
		++_failures;
		log::source().error("QueueCacheTest", what);
	}
}

void QueueCacheLayout::runChecks() {
	auto app = _director ? _director->getApplication() : nullptr;
	auto cache = app ? app->getExtension<QueueCache>() : nullptr;

	expect(_prewarmDone, "prewarm never reported");
	expect(_prewarmOk && _prewarmed != nullptr, "prewarmed queue was not compiled");
	expect(cache != nullptr, "AppThread has no QueueCache extension");

	if (cache) {
		expect(cache->getState(kQueueName) == QueueCache::State::Ready,
				"cache entry is not Ready");
		// One entry, one compile. A per-window rebuild would have produced N.
		expect(cache->getSize() == 1, "cache holds more than the one prewarmed graph");
		expect(cache->get(kQueueName) == _prewarmed, "cache returns a different queue object");
	}

	if (_prewarmed) {
		expect(_prewarmed->isCompiled(), "prewarmed queue reports itself uncompiled");
	}

	expect(_windows.size() == kWindowCount, "not every window opened");

	uint32_t adopted = 0;
	for (auto &it : _windows) {
		auto scene = SecondaryWindow::getScene(it);
		if (scene && scene->getQueue() == _prewarmed) {
			++adopted;
		}
	}
	// The decisive check: every window renders through the SAME Queue object, not a copy of the
	// same shape.
	expect(adopted == kWindowCount,
			toString("only ", adopted, " of ", kWindowCount, " windows adopted the cached queue"));

	if (_status) {
		_status->setString(toString("queue cache: ", _checks, " checks, ", _failures, " failures"));
	}
	log::source().warn("QueueCacheTest", "SUMMARY: ", _checks, " checks, ", _failures, " failures");
}

} // namespace stappler::xenolith::app
