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

#ifndef TESTS_WINDOW_SRC_PROJECTBUILDTHREAD_H_
#define TESTS_WINDOW_SRC_PROJECTBUILDTHREAD_H_

#include "XLCommon.h"

#include <sprt/runtime/dispatch/thread.h>
#include <sprt/cxx/mutex>
#include <sprt/cxx/condition_variable>

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// A dedicated build thread for live reload. The app thread (LiveReloadAppThread) calls requestBuild()
// when the observer reports a source change; this thread then builds the watched project via the
// engine's executor (makefile::runBuild, moved out of xlmake). On success it copies the freshly built
// executable to a unique `<watchDir>/stappler-build/live-reload/<N>/<exe>` for launching; on failure
// it logs and waits for the next request. All build output goes to the log. Rapid changes coalesce
// into a single pending rebuild.
class ProjectBuildThread : public sprt::dispatch::Thread {
public:
	// Invoked (on THIS build thread) with the absolute path of the freshly staged executable after a
	// successful build+copy. The consumer marshals it to its own thread to launch the client.
	using BuiltCallback = mem_std::Function<void(StringView stagedExe)>;

	bool init(StringView watchDir);

	// Set the success callback (before run()).
	void setOnBuilt(BuiltCallback &&cb) { _onBuilt = sp::move(cb); }

	// Ask for a (re)build. Cheap; safe to call from another thread. Coalesces while a build runs.
	void requestBuild();

	virtual void stop() override; // base stop + wake the wait

protected:
	virtual bool worker() override;
	void doBuild();

	mem_std::Interface::StringType _watchDir;
	mem_std::Interface::StringType _reloadBase; // <watchDir>/stappler-build/live-reload
	uint64_t _counter = 0; // unique subdir per successful build
	BuiltCallback _onBuilt; // fired on this thread after a successful stage

	bool _pending = false;
	sprt::mutex _mutex;
	sprt::condition_variable _cond;
};

} // namespace stappler::xenolith::app

#endif /* TESTS_WINDOW_SRC_PROJECTBUILDTHREAD_H_ */
