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

#include "ProjectBuildThread.h"

#include "SPMakefile.h" // Makefile::getVariableValue, ErrorReporter, decodePathSpaces
#include "SPMakefileProject.h" // makefile::loadProject
#include "SPMakefileBuilder.h" // makefile::runBuild, makefile::BuildConfig
#include "SPFilesystem.h" // copy / mkdir_recursive / FileInfo
#include "SPFilepath.h" // merge / lastComponent

#if !SPRT_WINDOWS
#include <sys/stat.h> // chmod: the copied executable must keep its +x bit (filesystem::copy drops it)
#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

using StdString = memory::StandartInterface::StringType;

bool ProjectBuildThread::init(StringView watchDir) {
	_watchDir.assign(watchDir.data(), watchDir.size());
	_reloadBase = _watchDir + "/stappler-build/live-reload";

	// drop previous data
	filesystem::remove(FileInfo{_reloadBase}, true);

	return true;
}

void ProjectBuildThread::requestBuild() {
	sprt::unique_lock<sprt::mutex> lock(_mutex);
	_pending = true;
	_cond.notify_all();
}

void ProjectBuildThread::stop() {
	Thread::stop(); // clears _continueExecution
	sprt::unique_lock<sprt::mutex> lock(_mutex);
	_cond.notify_all();
}

bool ProjectBuildThread::worker() {
	if (!_continueExecution.test_and_set()) {
		return false;
	}

	bool doIt = false;
	{
		sprt::unique_lock<sprt::mutex> lock(_mutex);
		if (_pending) {
			_pending = false; // a change during the build re-sets it → we rebuild next iteration
			doIt = true;
		} else {
			_cond.wait_for(lock, 1'000'000'000); // 1s, woken by requestBuild()/stop()
		}
	}

	if (doIt) {
		doBuild();
	}
	return true;
}

void ProjectBuildThread::doBuild() {
	auto pool = memory::pool::create(static_cast<memory::pool_t *>(nullptr));
	memory::perform([&] {
		log::source().info("live-reload", "build: rebuilding '", StringView(_watchDir), "'");

		// Route ALL build output (progress + compiler stdout/stderr + errors) to the log, one line at
		// a time so the log stays readable.
		StdString lineBuf;
		Callback<void(StringView)> sink([&](StringView chunk) {
			lineBuf.append(chunk.data(), chunk.size());
			size_t nl;
			while ((nl = lineBuf.find('\n')) != StdString::npos) {
				log::source().info("build", StringView(lineBuf.data(), nl));
				lineBuf.erase(0, nl + 1);
			}
		});

		makefile::ErrorReporter err(nullptr);
		err.callback = [](void *, log::LogType, StringView msg) {
			log::source().info("build", msg);
		};

		auto mk = makefile::loadProject(StringView(_watchDir), err);
		if (!mk) {
			log::source().error("live-reload", "build: failed to load project '",
					StringView(_watchDir), "'");
			return;
		}

		//mk->assignSimpleVariable("verbose", makefile::Origin::CommandLine, "1");

		makefile::BuildConfig cfg;
		cfg.targets.emplace_back(StringView("all"));
		cfg.jobs = 0; // hardware concurrency
		cfg.output = &sink;

		int rc = makefile::runBuild(mk, cfg, err);
		if (!lineBuf.empty()) {
			log::source().info("build", StringView(lineBuf)); // flush any trailing partial line
		}

		if (rc != 0) {
			log::source().error("live-reload", "build FAILED (code ", rc,
					"); waiting for next change");
			return;
		}

		// The freshly built executable's path (make-visible, so decode any path-space placeholders).
		StdString exeRaw;
		mk->getVariableValue(StringView("BUILD_EXECUTABLE"),
				[&](StringView v) { exeRaw.append(v.data(), v.size()); }, err);
		StdString decoded;
		StringView exe = makefile::decodePathSpaces(StringView(exeRaw), decoded);
		if (exe.empty()) {
			log::source().error("live-reload", "build succeeded but BUILD_EXECUTABLE is empty");
			return;
		}

		// Copy to a unique <reloadBase>/<N>/<exe> for launching. (Inside memory::perform, so a plain
		// pool-backed toString has an active context.)
		auto dstDir = toString(StringView(_reloadBase), "/", _counter);
		auto dst = filepath::merge<memory::StandartInterface>(StringView(dstDir),
				filepath::lastComponent(exe));
		filesystem::mkdir_recursive(FileInfo{StringView(dstDir)});
		if (filesystem::copy(FileInfo{exe}, FileInfo{StringView(dst)})) {
#if !SPRT_WINDOWS
			::chmod(dst.c_str(),
					0755); // filesystem::copy drops the +x bit; the exe must be runnable
#endif
			log::source().info("live-reload", "built and staged: ", StringView(dst));
			++_counter;
			if (_onBuilt) {
				_onBuilt(StringView(dst)); // consumer copies + marshals to its own thread
			}
		} else {
			log::source().error("live-reload", "build succeeded but copy failed: ", exe, " -> ",
					StringView(dst));
		}
	}, pool);
	memory::pool::destroy(pool);
}

} // namespace stappler::xenolith::app
