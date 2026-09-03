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

class ServerAppThread;

// Server-side proxy for a connected remote client: implements core::RenderClientChannel by
// (eventually) serializing the calls over the QUIC connection to the real client, and is bound to
// an AppWindow via AppThread::openConnection.
class SP_PUBLIC RemoteRenderClient : public core::RenderClientChannel {
public:
	virtual ~RemoteRenderClient();

	// `host` is the owning thread (raw back-ref: the host owns this client, so an Rc would cycle); it
	// provides the request/reply transport (sendMessageWithReply) and the shared-object registry.
	bool init(NotNull<ServerAppThread> host, Rc<remote::ServerConnection> &&);

	// True once the underlying QUIC connection has begun terminating (client disconnected).
	bool isClosed();

	void closeConnection();

	// The accepted connection (for the host AppThread to drive the async message dispatch loop).
	remote::ServerConnection *getConnection() const { return _connection; }

	void announce(NotNull<remote::ObjectRegistry>);

	virtual void acquireFrame(uint64_t windowId, NotNull<core::FrameRequestProxy> proxy,
			Function<void(bool)> &&) override;

	virtual void handleRenderQueueAttached(const Rc<core::Queue> &) override;
	virtual void handleConstraintsChanged(const core::FrameConstraints &) override;
	virtual void handleInputEvents(Vector<core::InputEventData> &&) override;
	virtual void handleTextInput(const core::TextInputState &) override;
	virtual void handleFramePresented(uint64_t frameOrder) override;

	virtual void pushDrawStat(const core::DrawStat &) override;

	// This client serves frames over the QUIC connection: its frames are tagged remote so the server's
	// PresentationEngine can force-invalidate them if the connection is reset.
	virtual bool isRemote() const override { return true; }

	// Push a MaterialSet update for an already-shared queue to the client. `registry` mints (and keeps
	// alive) ids for any newly-referenced gAPI objects.
	void handleMaterialsUpdated(uint64_t queue, NotNull<core::MaterialSet>,
			NotNull<remote::ObjectRegistry>);

	// One streamed input addressed to one or more attachments of an in-flight frame: reconstruct it
	// once (the first attachment mints the type, then deserialize) and feed it to each attachment of
	// the armed FrameRequest on the gapi loop thread.
	void handleFrameInput(uint64_t frameId, SpanView<StringView> attachmentKeys, BytesView bytes);

	// All inputs for a frame were submitted; stop routing further input for it.
	void handleFrameCommit(uint64_t frameId);

	// Feed the inputs the client cannot produce because they are server state (FrameCapture). An
	// input attachment left unfed wedges the frame and stalls the window -- see the .cc.
	void submitServerOwnedInputs(uint64_t frameId, NotNull<core::LocalFrameRequestProxy>);

	// A client-forwarded runtime material compile (WindowCode::CompileMaterials): resolve image refs (the
	// atlas image id -> the font server's DynamicImage), reconstruct the materials, compile into the
	// window's MaterialSet under the client-assigned ids, and register the gating deps so a frame using a
	// not-yet-compiled material waits.
	void handleCompileMaterials(BytesView payload);

protected:
	// Map a client-minted dependency id to the server-local event that gates the frame (material compile,
	// or a font atlas update via the font server). Used by handleFrameInput.
	Rc<core::DependencyEvent> reconcileDependency(uint32_t depId);

	ServerAppThread *_host = nullptr;
	Rc<remote::ServerConnection> _connection;
	uint64_t _nextFrameId = 1; // monotonic wire token correlating an AcquireFrame request/reply

	// The window this client most recently produced a frame for; the wire id used to route forwarded
	// input back to the client's matching RemoteWindow. Captured in acquireFrame (frames flow before any
	// input). TODO(multi-window): one client may serve several windows; carry the id per input batch.
	uint64_t _windowId = 0;

	// In-flight frames the client is still streaming input for, keyed by the wire frame id. Each
	// proxy wraps the armed server FrameRequest; touched only on the app (dispatch) thread.
	Map<uint64_t, Rc<core::LocalFrameRequestProxy>> _pendingFrames;

	// Client-minted material dependency ids -> the server-local events the forwarded compile signals, so a
	// frame using a not-yet-compiled material waits. Reconciled in handleFrameInput.
	Map<uint32_t, Rc<core::DependencyEvent>> _materialDeps;
};

} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_XLREMOTERENDERCLIENT_H_ */
