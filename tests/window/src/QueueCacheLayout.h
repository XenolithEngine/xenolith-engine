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

#ifndef TESTS_WINDOW_SRC_QUEUECACHELAYOUT_H_
#define TESTS_WINDOW_SRC_QUEUECACHELAYOUT_H_

#include "TestLayout.h"
#include "XLWindowSceneInfo.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// One prewarmed render graph, several windows on it.
//
// The graph is built and compiled from THIS window, before any of the windows that will use it
// exist - which is the property the whole feature rests on: a compiled Queue holds nothing
// window-specific, so it can be made ready in advance and handed to a window that opens later.
//
// Phase 1 (t=0.6s): prewarm the queue named kQueueName through the AppThread's QueueCache.
// Phase 2 (t=1.4s): open kWindowCount secondary Root windows, each adopting that same queue.
// Phase 3 (t=4.0s): every one of them must be running on the very same Queue object, and the cache
//   must still hold exactly one entry - i.e. it was compiled once and reused, not rebuilt per
//   window.
class QueueCacheLayout : public TestLayout {
public:
	static constexpr auto kQueueName = StringView("TestCachedQueue");
	static constexpr uint32_t kWindowCount = 3;

	virtual bool init() override;
	virtual void handleContentSizeDirty() override;
	virtual void handleExit() override;

protected:
	void prewarmQueue();
	void openWindows();
	void runChecks();

	void expect(bool cond, StringView what);

	basic2d::Label *_status = nullptr;

	Rc<core::Queue> _prewarmed;
	Vector<Rc<WindowSceneInfo>> _windows;

	bool _prewarmDone = false;
	bool _prewarmOk = false;

	uint32_t _checks = 0;
	uint32_t _failures = 0;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_QUEUECACHELAYOUT_H_
