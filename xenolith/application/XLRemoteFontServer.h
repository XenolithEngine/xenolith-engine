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

#ifndef XENOLITH_APPLICATION_XLREMOTEFONTSERVER_H_
#define XENOLITH_APPLICATION_XLREMOTEFONTSERVER_H_

#include "XLCommon.h"
#include "XLCoreAttachment.h" // core::DependencyEvent
#include "XLRemotePeer.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

namespace core {
struct DynamicImageInstance;
} // namespace core

// Server-side endpoint serving remote::Domain::Font for one connected client: owns a network-only
// FontController (its own FontLibrary and atlas, so FaceIds of different clients never meet) and the
// registry of dependency events that gate the client's frames. The content-hash font store is shared
// by all endpoints of a server.
//
// An endpoint outlives its session: the server keeps idle ones with a compiled atlas and binds one
// to each new session (setPeer), so a client never waits for an atlas to be created.
//
// Declared here so the server can drive it without depending on xenolith_font; the concrete
// RemoteFontServerEndpoint lives in xenolith_font and is created via a SharedModule factory.
class SP_PUBLIC RemoteFontServer : public Ref {
public:
	virtual ~RemoteFontServer() = default;

	// Bind the endpoint to the session it serves; replies and notifications go there.
	virtual void setPeer(RemotePeer *) = 0;

	// Route a Domain::Font request/notification (SourcesAnnounce, GlyphRequest, ...). Always
	// consumes.
	virtual bool dispatch(uint8_t code, uint32_t serial, BytesView payload) = 0;

	// A font blob assembled by block transfer (remote::DataType::Font), keyed by content hash.
	virtual void receiveFontData(uint64_t contentHash, BytesView bytes) = 0;

	// Map a client-minted dependency id (from a frame's remoteWaitDependencyIds) to the
	// server-local event the atlas update will signal. Returns nullptr for ids this server does not
	// rasterize for; such frames are not gated here.
	virtual Rc<core::DependencyEvent> reconcileDependency(uint32_t depId) = 0;

	// Pin the network atlas's current ImageObject to a stable wire id and return it; called before
	// a MaterialSet push, since the atlas image is replaced on each glyph update. Returns 0 if the
	// atlas is not compiled yet.
	virtual uint64_t pinAtlasImage() = 0;

	// Resolve a wire image id to the network atlas's current DynamicImageInstance, to rebuild a
	// forwarded font material as atlas-tracked. Returns null if the id is not the pinned atlas id.
	virtual Rc<core::DynamicImageInstance> resolveAtlasInstance(uint64_t imageId) = 0;

	// Unbind from the session and drop per-connection state (the dependency registry) on disconnect.
	virtual void reset() = 0;

	// Final teardown, unlike reset(): release the network atlas. The endpoint's faces carry the
	// FaceIds its client minted, so it never serves another session - the owner invalidates it when
	// the session ends, and must do so while the render device is still alive.
	virtual void invalidate() = 0;
};

} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_XLREMOTEFONTSERVER_H_ */
