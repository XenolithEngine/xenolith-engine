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

void ClientContext::run() {
	if (!_appThread) {
		_appThread = Rc<ClientAppThread>::create(this);
	}
	if (_appThread) {
		_appThread->run();
	}
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

void ClientContext::setWindowConnectedCallback(Function<bool(NotNull<RemoteWindow>)> &&cb) {
	_onWindowConnected = sp::move(cb);
}

void ClientContext::setWindowDisconnectedCallback(Function<void(NotNull<RemoteWindow>)> &&cb) {
	_onWindowDisconnected = sp::move(cb);
}

} // namespace stappler::xenolith
