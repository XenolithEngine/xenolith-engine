/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
 SPDX-License-Identifier: MIT
**/

#include "XL3dMeshPack.h"
#include "XL3dMesh.h"
#include "XLTemporaryResource.h"
#include "XLAppThread.h"

#include <sprt/c/__sprt_stdlib.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic3d {

MeshPackLayout layoutPack(SpanView<Rc<Mesh3d>> meshes) {
	MeshPackLayout ret;
	size_t vertexes = 0, indexes = 0;
	for (auto &it : meshes) {
		vertexes += it->getVertexes().size();
		indexes += it->getIndexes().size();
	}
	ret.vertexes.reserve(vertexes * sizeof(Vertex3d));
	ret.indexes.reserve(indexes * sizeof(uint32_t));

	for (auto &it : meshes) {
		auto v = it->getVertexes();
		auto i = it->getIndexes();
		ret.ranges.emplace_back(
				MeshPackRange{uint32_t(ret.indexes.size() / sizeof(uint32_t)), uint32_t(i.size()),
					int32_t(ret.vertexes.size() / sizeof(Vertex3d)), uint32_t(v.size())});
		auto vb = reinterpret_cast<const uint8_t *>(v.data());
		auto ib = reinterpret_cast<const uint8_t *>(i.data());
		ret.vertexes.insert(ret.vertexes.end(), vb, vb + v.size() * sizeof(Vertex3d));
		ret.indexes.insert(ret.indexes.end(), ib, ib + i.size() * sizeof(uint32_t));
	}
	return ret;
}

static sprt::atomic<uint64_t> s_packCounter = 0;

static sprt::atomic<uint32_t> s_packsReleasedOffThread = 0;

MeshPack3d::~MeshPack3d() {
	// the resource and its index belong to the app thread: they are released there
	if (_app && !_app->isOnThisThread() && (_mesh || _temporary)) {
		s_packsReleasedOffThread.fetch_add(1);
		_app->performOnAppThread(
				[mesh = sp::move(_mesh), temporary = sp::move(_temporary)]() { }, nullptr, false,
				"basic3d::MeshPack3d::~MeshPack3d");
	}
}

bool MeshPack3d::init(ResourceCache *cache, SpanView<Rc<Mesh3d>> meshes) {
	if (!cache || meshes.empty()) {
		return false;
	}

	auto layout = layoutPack(meshes);
	if (layout.vertexes.empty() || layout.indexes.empty()) {
		return false;
	}

	_name = toString("basic3d.pack.", s_packCounter.fetch_add(1));
	_meshCount = uint32_t(meshes.size());
	_bytes = layout.vertexes.size() + layout.indexes.size();

	core::Resource::Builder builder(_name);
	auto vertexData = builder.addBuffer("vertexes",
			core::BufferInfo(core::BufferUsage::StorageBuffer,
					core::BufferUsage::ShaderDeviceAddress, uint64_t(layout.vertexes.size())),
			layout.vertexes);
	auto indexData = builder.addBuffer("indexes",
			core::BufferInfo(core::BufferUsage::IndexBuffer, uint64_t(layout.indexes.size())),
			layout.indexes);
	if (!vertexData || !indexData) {
		return false;
	}

	auto resource = Rc<core::Resource>::create(sp::move(builder));
	if (!resource) {
		return false;
	}

	// no timeout: cleared and removed as soon as it has no users
	auto temporary = cache->addTemporaryResource(sp::move(resource), TimeInterval(),
			TemporaryResourceFlags::RemoveOnClear);
	if (!temporary) {
		return false;
	}
	_app = cache->getApplication();
	_cache = cache;
	_temporary = temporary;
	_mesh = temporary->acquireMeshIndex("vertexes");
	if (!_mesh) {
		return false;
	}
	_mesh->setBuffers(indexData, vertexData);

	for (size_t i = 0; i < meshes.size(); ++i) { meshes[i]->setPack(this, layout.ranges[i]); }
	return true;
}

bool MeshPack3d::isLoaded() const {
	return _mesh && _mesh->isLoaded() && _mesh->getVertexData() && _mesh->getVertexData()->buffer
			&& _mesh->getIndexData() && _mesh->getIndexData()->buffer;
}

Rc<core::BufferObject> MeshPack3d::getVertexBuffer() const {
	return isLoaded() ? _mesh->getVertexData()->buffer : nullptr;
}

Rc<core::BufferObject> MeshPack3d::getIndexBuffer() const {
	return isLoaded() ? _mesh->getIndexData()->buffer : nullptr;
}

void MeshPack3d::addUser(ResourceOwner *owner) {
	if (_mesh && owner) {
		_mesh->handleEnter(owner);
		++_users;
	}
}

void MeshPack3d::removeUser(ResourceOwner *owner) {
	if (_mesh && owner && _users > 0) {
		_mesh->handleExit(owner);
		--_users;

		// a pack left before it was ever asked for would never load, so never be cleared: the
		// cache drops it here; a loaded one or one compiling goes by the cache's own update
		if (_users == 0 && _cache && _temporary && !_temporary->isRequested()
				&& !_temporary->isLoaded() && _temporary->getUsersCount() == 0) {
			_cache->removeTemporaryResource(_name);
		}
	}
}

static sprt::atomic<uint64_t> s_frameVertexBytes = 0;
static sprt::atomic<uint64_t> s_frameIndexBytes = 0;
static sprt::atomic<uint32_t> s_frameDraws = 0;
static sprt::atomic<uint32_t> s_packDraws = 0;
static sprt::atomic<uint64_t> s_recordBytes = 0;

Pass3dStats getLastPassStats() {
	return Pass3dStats{s_frameVertexBytes.load(), s_frameIndexBytes.load(), s_frameDraws.load(),
		s_packDraws.load(), s_recordBytes.load()};
}

void setLastPassStats(const Pass3dStats &stats) {
	s_frameVertexBytes.store(stats.frameVertexBytes);
	s_frameIndexBytes.store(stats.frameIndexBytes);
	s_frameDraws.store(stats.frameDraws);
	s_packDraws.store(stats.packDraws);
	s_recordBytes.store(stats.recordBytes);
}

uint32_t getPacksReleasedOffThread() { return s_packsReleasedOffThread.load(); }

bool isPackingDisabled() {
	static const bool disabled = [] {
		auto value = __sprt_getenv("BASIC3D_NOPACK");
		return value && StringView(value) != "0";
	}();
	return disabled;
}

} // namespace stappler::xenolith::basic3d
