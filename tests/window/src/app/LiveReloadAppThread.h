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

#ifndef TESTS_WINDOW_SRC_APP_LIVERELOADAPPTHREAD_H_
#define TESTS_WINDOW_SRC_APP_LIVERELOADAPPTHREAD_H_

#include "XLServerAppThread.h"
#include "SPMakefileObserver.h"

namespace sprt::dispatch {
class ProcessHandle;
}

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

class ProjectBuildThread;

// A ServerAppThread that, when the app is launched with --watch <dir>, watches that project's `all`
// source inputs from its own main loop (poll-driven makefile::SourceObserver, stappler_makefile) and,
// on a change, asks a dedicated ProjectBuildThread to rebuild the project — which on success stages
// the fresh executable under <watchDir>/stappler-build/live-reload/<N>/ for launching. The observer's
// cadence is owned here; the build runs off-thread. Launching the staged binary is a later step.
class LiveReloadAppThread : public ServerAppThread {
public:
	// Out-of-line so the destructor (and thus the base ServerAppThread's Rc<> members, whose types are
	// only forward-declared here) is emitted in the .cpp, not at every Rc<LiveReloadAppThread>::create
	// site.
	virtual ~LiveReloadAppThread();

	// Set before the thread starts (from the app's makeAppThread seam). Empty ⇒ the observer is never
	// created and this behaves exactly like a plain ServerAppThread.
	void setWatchDir(StringView dir) { _watchDir.assign(dir.data(), dir.size()); }

	// The per-session remote endpoint negotiated for live reload: a random "127.0.0.1:<port>" and a
	// bearer key = Sha512(random token). Empty when live reload is inactive (no --watch). The scene
	// reads these to open the matching server listener; the same address + token are handed to each
	// launched client on its command line, so client and server agree on a fresh key+port every run.
	StringView getServerAddress() const { return _serverAddress; }
	BytesView getBearerKey() const { return BytesView(_bearerKey.data(), _bearerKey.size()); }

protected:
	virtual void handleThreadInitialized() override;
	virtual void handleThreadDisposed() override;
	virtual void performAppUpdate(const UpdateTime &, bool wakeup) override;

	// Launch (or restart) the staged client executable on the app looper, passing it the server
	// address on its command line. Kills any previously launched client first. App-thread only.
	void launchClient(StringView stagedExe);

	mem_std::Interface::StringType _watchDir;
	// Per-session negotiated endpoint (generated in handleThreadInitialized when live reload is active).
	mem_std::Interface::StringType _serverAddress; // "127.0.0.1:<random port>"
	mem_std::Interface::StringType _bearerToken; // random token; passed to the client on argv
	mem_std::Interface::BytesType _bearerKey; // Sha512(token); the server's expected bearer key
	Rc<makefile::SourceObserver> _observer;
	Rc<ProjectBuildThread> _buildThread; // owns its own OS thread; built on demand
	Rc<sprt::dispatch::ProcessHandle> _clientProc; // the currently running client child, if any
	uint64_t _lastCheckClock = 0; // monotonic us of the last observer poll
};

// Factory used by the app's makeAppThread seam. Defined in LiveReloadAppThread.cpp so the
// Rc<LiveReloadAppThread> construction — which instantiates the teardown of the base ServerAppThread's
// Rc<> members, whose types are only forward-declared in the engine header — is emitted in a single
// TU that includes those types, rather than at the seam site.
Rc<AppThread> createLiveReloadAppThread(NotNull<Context> ctx, StringView watchDir);

} // namespace stappler::xenolith::app

#endif /* TESTS_WINDOW_SRC_APP_LIVERELOADAPPTHREAD_H_ */
