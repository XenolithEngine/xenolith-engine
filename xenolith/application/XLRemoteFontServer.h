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

namespace STAPPLER_VERSIONIZED stappler::xenolith {

namespace core {
struct DynamicImageInstance;
} // namespace core

// Server-side endpoint that serves remote::Domain::Font for a connected client: it owns a dedicated,
// network-only FontController (a separate FontLibrary + atlas from the server's local-scene controller),
// a persistent content-hash font store, and a registry of the dependency events that gate client frames.
//
// Abstract interface declared here (xenolith_application) so the server can hold and drive it without a
// hard build-time dependency on xenolith_font; the concrete RemoteFontServerEndpoint lives in
// xenolith_font and is constructed via a SharedModule factory (mirroring createDefaultController).
class SP_PUBLIC RemoteFontServer : public Ref {
public:
	virtual ~RemoteFontServer() = default;

	// Route a Domain::Font request/notification (SourcesAnnounce / GlyphRequest / ...). Always consumes.
	virtual bool dispatch(uint8_t code, uint32_t serial, BytesView payload) = 0;

	// A font blob assembled by the block-transfer (remote::DataType::Font), keyed by its content hash.
	virtual void receiveFontData(uint64_t contentHash, BytesView bytes) = 0;

	// Map a client-minted dependency id (carried in a frame's remoteWaitDependencyIds) to the server-local
	// event the atlas update will signal, so the frame waits for it. Returns nullptr for an id the server
	// is not rasterizing for (e.g. a non-font dependency) -- such frames are simply not gated here.
	virtual Rc<core::DependencyEvent> reconcileDependency(uint32_t depId) = 0;

	// Pin the network atlas's current ImageObject to a stable wire id and return it. Called right before a
	// MaterialSet push (the atlas image is replaced on each glyph update, so its id must stay constant for
	// the client's mirror identity to hold). Returns 0 if the atlas is not compiled yet.
	virtual uint64_t pinAtlasImage() = 0;

	// Resolve a wire image id to the network atlas's current DynamicImageInstance (so a forwarded font
	// material can be rebuilt as a dynamic, atlas-tracked material on the server). Returns null if the id
	// is not the atlas's pinned id.
	virtual Rc<core::DynamicImageInstance> resolveAtlasInstance(uint64_t imageId) = 0;

	// Drop per-connection transient state (the dependency registry) on client disconnect; the persistent
	// font store and the network atlas survive for the next client.
	virtual void reset() = 0;

	// Final teardown, as opposed to reset(): release the network atlas and everything behind it. The
	// endpoint is not a registered ApplicationExtension, so nothing else gives it the invalidate() the
	// local-scene controller gets - it has to be called while the render device is still alive, or the
	// atlas image outlives the device it was allocated on.
	virtual void invalidate() = 0;
};

} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_XLREMOTEFONTSERVER_H_ */
