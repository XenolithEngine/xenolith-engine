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

#ifndef XENOLITH_APPLICATION_XLREMOTERENDERCLIENT_H_
#define XENOLITH_APPLICATION_XLREMOTERENDERCLIENT_H_

#include "XLCoreRenderSession.h"
#include "XLRemoteListener.h" // remote::ServerConnection
#include "XLRemoteObject.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

// Server-side proxy for a connected remote client: implements core::RenderClientChannel by
// (eventually) serializing the calls over the QUIC connection to the real client, and is bound to
// an AppWindow via AppThread::openConnection.
class SP_PUBLIC RemoteRenderClient : public Ref, public core::RenderClientChannel {
public:
	virtual ~RemoteRenderClient();

	bool init(Rc<remote::ServerConnection> &&);

	// True once the underlying QUIC connection has begun terminating (client disconnected).
	bool isClosed();

	void closeConnection();

	// The accepted connection (for the host AppThread to drive the async message dispatch loop).
	remote::ServerConnection *getConnection() const { return _connection; }

	void announce(NotNull<remote::ObjectRegistry>);

	virtual bool acquireFrame(NotNull<core::FrameRequestProxy>) override;
	virtual void handleRenderQueueAttached(const Rc<core::Queue> &) override;
	virtual void handleConstraintsChanged(const core::FrameConstraints &) override;
	virtual void handleInputEvents(Vector<core::InputEventData> &&) override;
	virtual void handleTextInput(const core::TextInputState &) override;
	virtual void handleFramePresented(uint64_t frameOrder) override;

protected:
	Rc<remote::ServerConnection> _connection;
};

} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_XLREMOTERENDERCLIENT_H_ */
