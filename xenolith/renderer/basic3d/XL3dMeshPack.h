/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
 SPDX-License-Identifier: MIT
**/

#ifndef XENOLITH_RENDERER_BASIC3D_XL3DMESHPACK_H_
#define XENOLITH_RENDERER_BASIC3D_XL3DMESHPACK_H_

#include "XL3d.h"
#include "XLResourceCache.h"
#include "XLMeshIndex.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic3d {

// Where a mesh is in a pack: its indexes from `firstIndex`, its vertexes from `vertexOffset`
struct SP_PUBLIC MeshPackRange {
	uint32_t firstIndex = 0;
	uint32_t indexCount = 0;
	int32_t vertexOffset = 0;
	uint32_t vertexCount = 0;
};

// The data of a pack: the meshes' vertexes one after another, their indexes likewise, unchanged -
// a draw adds its range's vertexOffset
struct SP_PUBLIC MeshPackLayout {
	Bytes vertexes;
	Bytes indexes;
	Vector<MeshPackRange> ranges;
};

SP_PUBLIC MeshPackLayout layoutPack(SpanView<Rc<Mesh3d>>);

/* Meshes in GPU memory, a batch at once (plan.md §5.1, stage 7): one vertex and one index buffer
for all of them, in one temporary resource of the ResourceCache, so a few hundred meshes take a
few dozen allocations, far below Vulkan's usual limit of 4096. A mesh of the pack then draws from
its range of those buffers, with nothing copied into the frame.

The pack is loaded while a view retains it (View3d::retainPack, once for every mesh that draws from
it), and goes - cleared and removed from the cache - once nobody does. Making a pack sets it on
its meshes (Mesh3d::getPack); a later pack takes a mesh over, which is how an app compacts packs
whose meshes mostly left: the frames still drawing from the old one hold its buffers until they
end. App thread only: a pack destroyed elsewhere sends its resource back to the app thread. */
class SP_PUBLIC MeshPack3d : public Ref {
public:
	virtual ~MeshPack3d();

	bool init(ResourceCache *, SpanView<Rc<Mesh3d>>);

	StringView getName() const { return _name; }
	bool isLoaded() const;

	uint32_t getMeshCount() const { return _meshCount; }
	uint64_t getBytes() const { return _bytes; }

	// the buffers, once loaded; null before
	Rc<core::BufferObject> getVertexBuffer() const;
	Rc<core::BufferObject> getIndexBuffer() const;

	// a user more or less, through a resource owner (the frame context): the pack loads while
	// it has users
	void addUser(ResourceOwner *);
	void removeUser(ResourceOwner *);
	uint32_t getUsers() const { return _users; }

protected:
	String _name;
	Rc<AppThread> _app;
	ResourceCache *_cache = nullptr;
	Rc<TemporaryResource> _temporary;
	Rc<MeshIndex> _mesh;
	uint32_t _meshCount = 0;
	uint32_t _users = 0;
	uint64_t _bytes = 0;
};

// What the last frame of the 3d pass drew, for checks and overlays: the vertex and index bytes
// it copied into the frame, its draws from the frame's buffers and from packs, the bytes of its
// records (the materials' among them), how many of the draws were translucent (either kind), and
// the materials of a schema it wrote. Written by the pass's
// worker, read from any thread
struct SP_PUBLIC Pass3dStats {
	uint64_t frameVertexBytes = 0;
	uint64_t frameIndexBytes = 0;
	uint32_t frameDraws = 0;
	uint32_t packDraws = 0;
	uint64_t recordBytes = 0;
	uint32_t translucentDraws = 0;
	uint32_t materials = 0; // the materials of a schema it wrote, each once
	uint64_t materialBytes = 0;
};

SP_PUBLIC Pass3dStats getLastPassStats();
SP_PUBLIC void setLastPassStats(const Pass3dStats &);

// Packs destroyed off the app thread, which sent their resource back to it. A frame releases what
// it holds of the app's on the app thread, so a pack's last reference stays there (for checks)
SP_PUBLIC uint32_t getPacksReleasedOffThread();

// BASIC3D_NOPACK=1: views draw every mesh from the frame's buffers, packs or not (for checks)
SP_PUBLIC bool isPackingDisabled();

} // namespace stappler::xenolith::basic3d

#endif /* XENOLITH_RENDERER_BASIC3D_XL3DMESHPACK_H_ */
