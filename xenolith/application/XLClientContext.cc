/**
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#include "XLClientContext.h"
#include "XLClientAppThread.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

__SPRT_PUSH_ALLOW_CXXABI_ALLOC

ClientContext::~ClientContext() { }

__SPRT_POP_ALLOW_CXXABI_ALLOC

bool ClientContext::init() {
	_info = Rc<ContextInfo>::alloc();
	return _info != nullptr;
}

bool ClientContext::init(Rc<ContextInfo> &&info) {
	_info = info ? sp::move(info) : Rc<ContextInfo>::alloc();
	return _info != nullptr;
}

void ClientContext::run() {
	if (!_appThread) {
		_appThread = Rc<ClientAppThread>::create(this);
		if (_appThread && _appMessageHandler) {
			_appThread->setAppMessageHandler(sp::move(_appMessageHandler));
		}
	}
	if (_appThread) {
		_appThread->run();
	}
}

void ClientContext::setAppMessageHandler(ClientAppThread::AppMessageHandler &&handler) {
	if (_appThread) {
		_appThread->performOnAppThread(
				[thread = _appThread, handler = sp::move(handler)]() mutable {
			thread->setAppMessageHandler(sp::move(handler));
		}, this);
	} else {
		_appMessageHandler = sp::move(handler);
	}
}

void ClientContext::sendAppNotification(Value &&val) {
	if (!_appThread) {
		return;
	}
	_appThread->performOnAppThread([thread = _appThread, val = sp::move(val)]() {
		thread->sendAppNotification(val);
	}, this);
}

void ClientContext::sendAppRequest(Value &&val, Function<void(Status, Value &&)> &&cb,
		uint64_t timeoutUs) {
	if (!_appThread) {
		if (cb) {
			cb(Status::ErrorNotSupported, Value());
		}
		return;
	}
	_appThread->performOnAppThread(
			[thread = _appThread, val = sp::move(val), cb = sp::move(cb), timeoutUs]() mutable {
		// A copy goes with the request; this one answers when it could not be sent.
		auto callback = sp::move(cb);
		if (!thread->sendAppRequest(val, Function<void(Status, Value &&)>(callback), timeoutUs)
				&& callback) {
			callback(Status::ErrorNotSupported, Value());
		}
	}, this);
}

void ClientContext::stop() {
	if (_appThread) {
		_appThread->stop();
		_appThread->waitStopped();
		_appThread = nullptr;
	}
}

void ClientContext::handleAppThreadCreated(NotNull<ClientAppThread>) {
	log::source().info("ClientContext", "handleAppThreadCreated");
}

void ClientContext::handleAppThreadDestroyed(NotNull<ClientAppThread>) {
	log::source().info("ClientContext", "handleAppThreadDestroyed");
}

void ClientContext::handleAppThreadUpdate(NotNull<ClientAppThread>, const UpdateTime &) { }

bool ClientContext::handleWindowConnected(NotNull<ClientAppThread> thread,
		NotNull<RemoteWindow> w) {
	if (_onWindowConnected) {
		return _onWindowConnected(w);
	}
	return false;
}

void ClientContext::handleWindowDisconnected(NotNull<ClientAppThread> thread,
		NotNull<RemoteWindow> w) {
	if (_onWindowDisconnected) {
		_onWindowDisconnected(w);
	}
}

void ClientContext::createWindow(Rc<sprt::window::WindowInfo> &&info,
		Function<void(Status, StringView id)> &&complete) {
	if (!_appThread) {
		if (complete) {
			complete(Status::ErrorNotSupported, StringView());
		}
		return;
	}
	_appThread->performOnAppThread(
			[thread = _appThread, info = sp::move(info), complete = sp::move(complete)]() mutable {
		thread->createWindow(sp::move(info), sp::move(complete));
	}, this);
}

void ClientContext::handleServerInfo(NotNull<ClientAppThread> thread,
		const remote::PeerInfo &info) {
	if (_onServerInfo) {
		_onServerInfo(thread, info);
	}
}

void ClientContext::setServerInfoCallback(
		Function<void(NotNull<ClientAppThread>, const remote::PeerInfo &)> &&cb) {
	_onServerInfo = sp::move(cb);
}

void ClientContext::setWindowConnectedCallback(Function<bool(NotNull<RemoteWindow>)> &&cb) {
	_onWindowConnected = sp::move(cb);
}

void ClientContext::setWindowDisconnectedCallback(Function<void(NotNull<RemoteWindow>)> &&cb) {
	_onWindowDisconnected = sp::move(cb);
}

} // namespace stappler::xenolith
