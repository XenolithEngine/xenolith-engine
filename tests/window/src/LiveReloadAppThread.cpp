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

#include "LiveReloadAppThread.h"
#include "ProjectBuildThread.h" // the off-thread builder
#include "XLContextInfo.h" // UpdateTime

// Complete definitions of the base ServerAppThread's Rc<> members, needed here so the
// Rc<LiveReloadAppThread> construction/teardown can be emitted in this one TU (the engine header only
// forward-declares them).
#include "XLRemoteListener.h" // remote::Listener, remote::ServerConnection
#include "XLRemoteFontServer.h" // RemoteFontServer
#include "XLRemoteRenderClient.h" // RemoteRenderClient
#include "XLRemoteBlockTransfer.h" // BlockTransferManager (AppThread member)
#include "XLDirector.h" // Director (HashMap<String, Rc<Director>> member)
#include "resources/XLResourceCache.h" // ResourceCache (AppThread member)
#include "SPCoreCrypto.h" // crypto::Sha512 — bearer key = Sha512(token)
#include "SPValid.h" // valid::makeRandomBytes — random token + port
#include "SPString.h" // base16::encode — token as hex
#include <sprt/runtime/dispatch/handle.h> // sprt::dispatch::PollHandle / TimerHandle / ProcessHandle
#include <sprt/runtime/dispatch/looper.h> // _appLooper->spawnProcess / performOnThread
#include <stdio.h> // snprintf for "127.0.0.1:<port>"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// How often to poll the observer, regardless of the app-update tick rate.
static constexpr uint64_t kWatchPollIntervalUs = 1'000'000; // 1s

// Out-of-line: emits the destructor here so the base ServerAppThread's Rc<> members are torn down by
// the engine-compiled ~ServerAppThread rather than at each create site.
LiveReloadAppThread::~LiveReloadAppThread() = default;

Rc<AppThread> createLiveReloadAppThread(NotNull<Context> ctx, StringView watchDir) {
	auto thread = Rc<LiveReloadAppThread>::create(ctx);
	if (thread && !watchDir.empty()) {
		thread->setWatchDir(watchDir);
	}
	return thread;
}

void LiveReloadAppThread::handleThreadInitialized() {
	ServerAppThread::handleThreadInitialized();

	if (_watchDir.empty()) {
		return; // no --watch: behave exactly like a plain ServerAppThread
	}

	// Negotiate a fresh secret + port for THIS session: a random token (bearer key = Sha512(token))
	// and a random loopback port. The scene opens the server listener on these (getServerAddress /
	// getBearerKey), and launchClient hands the same address + token to each client on its command
	// line — so client and server agree on a unique key+port every run, replacing the fixed dev
	// key/4480.
	auto tokenBytes = valid::makeRandomBytes<memory::StandartInterface>(16);
	_bearerToken = base16::encode<memory::StandartInterface>(
			BytesView(tokenBytes.data(), tokenBytes.size()));
	auto keyBuf = crypto::Sha512::perform(StringView(_bearerToken));
	_bearerKey.assign(keyBuf.data(), keyBuf.data() + keyBuf.size());

	uint16_t portRaw = 0;
	valid::makeRandomBytes(reinterpret_cast<uint8_t *>(&portRaw), sizeof(portRaw));
	uint16_t port = uint16_t(20'000 + (portRaw % 40'000)); // ephemeral-ish range; localhost only
	char addrBuf[32];
	int addrLen = ::snprintf(addrBuf, sizeof(addrBuf), "127.0.0.1:%u", unsigned(port));
	_serverAddress.assign(addrBuf, size_t(addrLen > 0 ? addrLen : 0));
	log::source().info("live-reload", "session endpoint: ", _serverAddress, " (unique key+port)");

	// Load the project and collect its `all` source inputs. This is a one-time synchronous load
	// (a few seconds of $(shell) build configuration on a cold project) done here on the app thread.
	makefile::ErrorReporter err(nullptr);
	_observer = makefile::SourceObserver::createForProject(StringView(_watchDir), StringView("all"),
			err);
	if (_observer) {
		log::source().info("live-reload", "watching ", _observer->getWatchedCount(),
				" source files of '", _watchDir, "' (fingerprint=", _observer->getFingerprint(),
				")");
	} else {
		log::source().error("live-reload", "failed to load project for watching: ", _watchDir);
		return;
	}

	// Start the dedicated build thread; the app thread only signals it (requestBuild) on a change.
	// When a build succeeds (on the build thread), marshal the staged path back to THIS app thread and
	// (re)launch the client there — spawnProcess must run on the looper's own thread.
	_buildThread = Rc<ProjectBuildThread>::create(StringView(_watchDir));
	if (_buildThread) {
		_buildThread->setOnBuilt([this](StringView stagedExe) {
			memory::StandartInterface::StringType exe(stagedExe.data(), stagedExe.size());
			_appLooper->performOnThread([this, exe]() { launchClient(StringView(exe)); }, this);
		});
		_buildThread->run();
	}
}

void LiveReloadAppThread::launchClient(StringView stagedExe) {
	// Restart: kill the previously launched client (SIGKILL + reap via cancel()).
	if (_clientProc) {
		if (_clientProc->isRunning()) {
			_clientProc->cancel();
		}
		_clientProc = nullptr;
	}

	// The client we just killed was SIGKILLed (uncatchable), so it never sent a QUIC CONNECTION_CLOSE.
	// The server (this ServerAppThread, whose listener runs on this same thread) therefore still holds
	// that now-dead client in its single-connection slot and would REJECT the replacement we are about
	// to launch ("remote client already connected"), freeing the slot only after the ~5s keepalive
	// timeout — by which point the one-shot client has already failed its handshake and exited. So drop
	// the stale connection ourselves now (reverts shared windows to their local Directors and sends a
	// server-side CONNECTION_CLOSE); the fresh client is then accepted immediately.
	if (_remoteClient) {
		resetRemoteClient();
	}

	// `'<exe>' <address> <token>` run via the app looper (/bin/sh -c). The client reads argv[1] as the
	// server address to dial and argv[2] as the token (key = Sha512(token)) — this is how the server
	// "прокидывает" the negotiated address + secret on the command line.
	memory::StandartInterface::StringType cmd;
	cmd.append("'").append(stagedExe.data(), stagedExe.size()).append("' ").append(_serverAddress);
	cmd.append(" ").append(_bearerToken);

	_clientProc = _appLooper->spawnProcess(StringView(cmd), [](StringView out) {
		out.trimChars<StringView::WhiteSpace>();
		log::source(sprt::source_location()).info("client", out);
	}, [](int code, sprt::Status) {
		log::source().info("live-reload", "client exited (code=", code, ")");
	});
	if (_clientProc) {
		log::source().info("live-reload", "launched client: ", StringView(cmd));
	} else {
		log::source().error("live-reload", "failed to launch client: ", StringView(cmd));
	}
}

void LiveReloadAppThread::handleThreadDisposed() {
	if (_clientProc) {
		if (_clientProc->isRunning()) {
			_clientProc->cancel();
		}
		_clientProc = nullptr;
	}
	if (_buildThread) {
		_buildThread->stop();
		_buildThread->waitStopped();
		_buildThread = nullptr;
	}
	_observer = nullptr;
	ServerAppThread::handleThreadDisposed();
}

void LiveReloadAppThread::performAppUpdate(const UpdateTime &time, bool wakeup) {
	ServerAppThread::performAppUpdate(time, wakeup);

	if (!_observer) {
		return;
	}

	// Poll on the app-update cadence, throttled to kWatchPollIntervalUs. The observer owns no thread;
	// this app thread drives check() and reacts to a changed fingerprint.
	auto now = sp::platform::clock(ClockType::Monotonic);
	if (now - _lastCheckClock < kWatchPollIntervalUs) {
		return;
	}
	_lastCheckClock = now;

	if (_observer->check()) {
		log::source().info("live-reload",
				"change detected (fingerprint=", _observer->getFingerprint(),
				"); requesting rebuild");
		if (_buildThread) {
			_buildThread->requestBuild();
		}
	}
}

} // namespace stappler::xenolith::app
