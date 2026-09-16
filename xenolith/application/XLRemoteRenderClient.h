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
class RemoteSession;

// Server-side proxy for a connected remote client: implements core::RenderClientChannel by
// serializing the calls over the connection to the real client.
class SP_PUBLIC RemoteRenderClient : public core::RenderClientChannel {
public:
	virtual ~RemoteRenderClient();

	// `host` provides the shared-object registry and the GPU loop; `session` (which owns this
	// client) the connection, the replies and the font endpoint. Both are raw back-refs.
	bool init(NotNull<ServerAppThread> host, NotNull<RemoteSession> session);

	// True once the underlying connection has begun terminating (client disconnected).
	bool isClosed();

	// The session is closing: forget it and every frame in flight. A window may still hold the
	// client until it is switched back; everything it asks is refused from here on.
	void detach();

	remote::ServerConnection *getConnection() const;

	// Send the windows this client's session can see.
	void announce(NotNull<remote::ObjectRegistry>);

	virtual void acquireFrame(uint64_t windowId, NotNull<core::FrameRequestProxy> proxy,
			Function<void(bool)> &&) override;

	virtual void handleRenderQueueAttached(const Rc<core::Queue> &) override;
	virtual void handleConstraintsChanged(const core::FrameConstraints &) override;
	virtual void handleWindowGeometryChanged(uint64_t windowId,
			const sprt::window::WindowGeometry &) override;
	virtual void handleInputEvents(uint64_t windowId, Vector<core::InputEventData> &&) override;
	virtual void handleTextInput(uint64_t windowId, const core::TextInputState &) override;
	virtual void handleFramePresented(uint64_t frameOrder) override;

	virtual void pushDrawStat(uint64_t windowId, const core::DrawStat &) override;

	// Frames served by this client are tagged remote, so the server's PresentationEngine can
	// invalidate them if the connection is reset.
	virtual bool isRemote() const override { return true; }

	// Push a MaterialSet update for an already-shared queue to the client. `registry` mints (and
	// keeps alive) ids for newly referenced gAPI objects.
	void handleMaterialsUpdated(uint64_t queue, NotNull<core::MaterialSet>,
			NotNull<remote::ObjectRegistry>);

	// One streamed input for one or more attachments of an in-flight frame: reconstruct it once and
	// feed it to each attachment of the armed FrameRequest on the gapi loop thread.
	void handleFrameInput(uint64_t frameId, SpanView<StringView> attachmentKeys, BytesView bytes);

	// All inputs for a frame were submitted; stop routing further input for it.
	void handleFrameCommit(uint64_t frameId);

	// Feed inputs the client can not produce because they are server state (FrameCapture); an unfed
	// input attachment wedges the frame.
	void submitServerOwnedInputs(uint64_t frameId, uint64_t windowId,
			NotNull<core::LocalFrameRequestProxy>);

	// A client-forwarded runtime material compile (WindowCode::CompileMaterials): resolve image
	// refs, reconstruct the materials, compile into the window's MaterialSet under the
	// client-assigned ids, and register gating deps so frames using them wait.
	void handleCompileMaterials(BytesView payload);

protected:
	// Map a client-minted dependency id to the server-local event gating the frame (material
	// compile or font atlas update). Used by handleFrameInput.
	Rc<core::DependencyEvent> reconcileDependency(uint32_t depId);

	ServerAppThread *_host = nullptr;
	RemoteSession *_session = nullptr;
	uint64_t _nextFrameId = 1; // monotonic wire token correlating an AcquireFrame request/reply

	// In-flight frames the client is still streaming input for, keyed by wire frame id; app thread
	// only. The window is stored because AcquireFrame replies are asynchronous and may interleave
	// between windows.
	struct PendingFrame {
		Rc<core::LocalFrameRequestProxy> proxy;
		uint64_t windowId = 0;
	};
	Map<uint64_t, PendingFrame> _pendingFrames;

	// Client-minted material dependency ids -> server-local events signalled by the forwarded
	// compile. Reconciled in handleFrameInput.
	Map<uint32_t, Rc<core::DependencyEvent>> _materialDeps;

	// The last DrawStat per window, waiting for that window's next frame request; `dirty` prevents
	// re-sending unchanged numbers. App thread only.
	struct WindowDrawStat {
		core::DrawStat stat{};
		bool dirty = false;
	};
	Map<uint64_t, WindowDrawStat> _drawStats;
};

} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_XLREMOTERENDERCLIENT_H_ */
