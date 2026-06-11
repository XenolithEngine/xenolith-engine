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

#include "XLRemoteRenderClient.h"
#include "XLRemoteSerialize.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

__SPRT_PUSH_ALLOW_CXXABI_ALLOC

RemoteRenderClient::~RemoteRenderClient() = default;

__SPRT_POP_ALLOW_CXXABI_ALLOC

bool RemoteRenderClient::init(Rc<remote::ServerConnection> &&conn) {
	_connection = sp::move(conn);
	return _connection != nullptr;
}

bool RemoteRenderClient::isClosed() { return !_connection || _connection->isClosed(); }

void RemoteRenderClient::closeConnection() {
	if (_connection) {
		_connection->close(); // graceful QUIC shutdown (bounded); then drop it
		_connection = nullptr;
	}
}

void RemoteRenderClient::announce(NotNull<remote::ObjectRegistry> registry) {
	Value data;
	auto &windows = data.emplace("windows");
	for (auto &it : registry->getWindows()) {
		auto &v = windows.emplace();
		v.addInteger(it.first);
		v.addString(it.second.window->getId());
		v.addInteger(toInt(it.second.window->getWindowState()));
		v.addInteger(toInt(it.second.window->getCapabilities()));
		v.addValue(remote::serializeFrameConstraints(it.second.window->getConstraints()));
		v.addValue(remote::serializeSwapchainConfig(it.second.window->getAppSwapchainConfig()));

		Value &queues = v.emplace();
		for (auto &qIt : it.second.queues) {
			auto q = registry->resolveQueue(qIt);
			if (q) {
				auto &v = queues.emplace();
				v.addInteger(qIt);
				v.addString(q->getName());
			}
		}

		if (auto info = it.second.window->getInfo()) {
			v.addValue(remote::serializeWindowInfo(*info));
		}
	}

	_connection->sendCborMessage(remote::Domain::Global,
			toInt(remote::GlobalCode::SharedObjectsAnnounce), data);
}

bool RemoteRenderClient::acquireFrame(NotNull<core::FrameRequestProxy>) {
	// STUB (stage 5): the over-the-wire render protocol is not implemented yet, so the remote
	// client produces no frame. The window stays idle while attached until the protocol stage.
	return false;
}

void RemoteRenderClient::handleRenderQueueAttached(const Rc<core::Queue> &) { }
void RemoteRenderClient::handleConstraintsChanged(const core::FrameConstraints &) { }
void RemoteRenderClient::handleInputEvents(Vector<core::InputEventData> &&) { }
void RemoteRenderClient::handleTextInput(const core::TextInputState &) { }
void RemoteRenderClient::handleFramePresented(uint64_t) { }

} // namespace stappler::xenolith
