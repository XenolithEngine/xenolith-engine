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

// Server-side endpoint serving remote::Domain::Font for a connected client: owns a network-only
// FontController (its own FontLibrary and atlas), a persistent content-hash font store, and the
// registry of dependency events that gate client frames.
//
// Declared here so the server can drive it without depending on xenolith_font; the concrete
// RemoteFontServerEndpoint lives in xenolith_font and is created via a SharedModule factory.
class SP_PUBLIC RemoteFontServer : public Ref {
public:
	virtual ~RemoteFontServer() = default;

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

	// Drop per-connection state (the dependency registry) on disconnect; the font store and network
	// atlas persist.
	virtual void reset() = 0;

	// Final teardown, unlike reset(): release the network atlas. Not a registered extension, so the
	// owner must call it while the render device is still alive.
	virtual void invalidate() = 0;
};

} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_XLREMOTEFONTSERVER_H_ */
