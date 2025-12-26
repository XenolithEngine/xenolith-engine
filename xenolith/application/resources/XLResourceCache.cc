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

#include "XLResourceCache.h"
#include "XLTemporaryResource.h"
#include "XLCoreLoop.h"
#include "XLAppThread.h"
#include "XLContext.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

ResourceCache::~ResourceCache() { }

bool ResourceCache::init(AppThread *a) {
	_application = a;
	return true;
}

void ResourceCache::initialize(AppThread *a) { _loop = a->getContext()->getGlLoop(); }

void ResourceCache::invalidate(AppThread *a) {
	for (auto &it : _temporaries) { it.second->invalidate(); }
	_images.clear();
	_temporaries.clear();
	_resources.clear();
	_loop = nullptr;
}

void ResourceCache::update(AppThread *a, const UpdateTime &time, bool) {
	auto it = _temporaries.begin();
	while (it != _temporaries.end()) {
		if (it->second->getUsersCount() > 0 && !it->second->isRequested()) {
			compileResource(it->second);
			++it;
		} else if (it->second->isDeprecated(time)) {
			if (clearResource(it->second)) {
				it = _temporaries.erase(it);
			} else {
				++it;
			}
		} else {
			++it;
		}
	}
}

void ResourceCache::addImage(StringView name, const Rc<core::ImageObject> &img) {
	auto it = _images.emplace(name, core::ImageData()).first;
	static_cast<core::ImageInfoData &>(it->second) = img->getInfo();
	it->second.image = img;
}

void ResourceCache::addResource(const Rc<core::Resource> &req) {
	_resources.emplace(req->getName(), req);
}

void ResourceCache::removeResource(StringView requestName) { _resources.erase(requestName); }

Rc<Texture> ResourceCache::acquireTexture(StringView str) const {
	auto iit = _images.find(str);
	if (iit != _images.end()) {
		return Rc<Texture>::create(&iit->second);
	}

	for (auto &it : _temporaries) {
		if (auto tex = it.second->acquireTexture(str)) {
			return tex;
		}
	}

	for (auto &it : _resources) {
		if (auto v = it.second->getImage(str)) {
			return Rc<Texture>::create(v, it.second);
		}
	}

	log::source().error("ResourceCache", "Texture not found: ", str);
	return nullptr;
}

Rc<MeshIndex> ResourceCache::acquireMeshIndex(StringView str) const {
	for (auto &it : _temporaries) {
		if (auto mesh = it.second->acquireMeshIndex(str)) {
			return mesh;
		}
	}

	for (auto &it : _resources) {
		if (auto v = it.second->getBuffer(str)) {
			return Rc<MeshIndex>::create(v, it.second);
		}
	}

	log::source().error("ResourceCache", "MeshIndex not found: ", str);
	return nullptr;
}

const core::ImageData *ResourceCache::getEmptyImage() const {
	auto iit = _images.find(StringView(core::EmptyTextureName));
	if (iit != _images.end()) {
		return &iit->second;
	}
	return nullptr;
}

const core::ImageData *ResourceCache::getSolidImage() const {
	auto iit = _images.find(StringView(core::SolidTextureName));
	if (iit != _images.end()) {
		return &iit->second;
	}
	return nullptr;
}

Rc<Texture> ResourceCache::addExternalBitmapImageByRef(StringView key, core::ImageInfo &&info,
		BytesView data, TimeInterval ival, TemporaryResourceFlags flags) {
	auto it = _temporaries.find(key);
	if (it != _temporaries.end()) {
		if (auto tex = it->second->acquireTexture(key)) {
			return tex;
		}
		log::source().error("ResourceCache", "Resource '", key,
				"' already exists, but no texture '", key, "' found");
		return nullptr;
	}

	core::Resource::Builder builder(key);
	if (auto d = builder.addBitmapImageByRef(key, move(info), data)) {
		if (auto tmp = addTemporaryResource(Rc<core::Resource>::create(move(builder)), ival,
					flags)) {
			return Rc<Texture>::create(d, tmp);
		}
	}
	return nullptr;
}

Rc<Texture> ResourceCache::addExternalBitmapImage(StringView key, core::ImageInfo &&info,
		BytesView data, TimeInterval ival, TemporaryResourceFlags flags) {
	auto it = _temporaries.find(key);
	if (it != _temporaries.end()) {
		if (auto tex = it->second->acquireTexture(key)) {
			return tex;
		}
		log::source().error("ResourceCache", "Resource '", key,
				"' already exists, but no texture '", key, "' found");
		return nullptr;
	}

	core::Resource::Builder builder(key);
	if (auto d = builder.addBitmapImage(key, move(info), data)) {
		if (auto tmp = addTemporaryResource(Rc<core::Resource>::create(move(builder)), ival,
					flags)) {
			return Rc<Texture>::create(d, tmp);
		}
	}

	return nullptr;
}

Rc<Texture> ResourceCache::addExternalEncodedImageByRef(StringView key, core::ImageInfo &&info,
		BytesView data, TimeInterval ival, TemporaryResourceFlags flags) {
	auto it = _temporaries.find(key);
	if (it != _temporaries.end()) {
		if (auto tex = it->second->acquireTexture(key)) {
			return tex;
		}
		log::source().error("ResourceCache", "Resource '", key,
				"' already exists, but no texture '", key, "' found");
		return nullptr;
	}

	core::Resource::Builder builder(key);
	if (auto d = builder.addEncodedImageByRef(key, move(info), data)) {
		if (auto tmp = addTemporaryResource(Rc<core::Resource>::create(move(builder)), ival,
					flags)) {
			return Rc<Texture>::create(d, tmp);
		}
	}
	return nullptr;
}

Rc<Texture> ResourceCache::addExternalEncodedImage(StringView key, core::ImageInfo &&info,
		BytesView data, TimeInterval ival, TemporaryResourceFlags flags) {
	auto it = _temporaries.find(key);
	if (it != _temporaries.end()) {
		if (auto tex = it->second->acquireTexture(key)) {
			return tex;
		}
		log::source().error("ResourceCache", "Resource '", key,
				"' already exists, but no texture '", key, "' found");
		return nullptr;
	}

	core::Resource::Builder builder(key);
	if (auto d = builder.addEncodedImage(key, move(info), data)) {
		if (auto tmp = addTemporaryResource(Rc<core::Resource>::create(move(builder)), ival,
					flags)) {
			return Rc<Texture>::create(d, tmp);
		}
	}

	return nullptr;
}

Rc<Texture> ResourceCache::addExternalImage(StringView key, core::ImageInfo &&info,
		const FileInfo &data, TimeInterval ival, TemporaryResourceFlags flags) {
	auto it = _temporaries.find(key);
	if (it != _temporaries.end()) {
		if (auto tex = it->second->acquireTexture(key)) {
			return tex;
		}
		log::source().error("ResourceCache", "Resource '", key,
				"' already exists, but no texture '", key, "' found");
		return nullptr;
	}

	core::Resource::Builder builder(key);
	if (auto d = builder.addImage(key, move(info), data)) {
		if (auto tmp = addTemporaryResource(Rc<core::Resource>::create(move(builder)), ival,
					flags)) {
			return Rc<Texture>::create(d, tmp);
		}
	}
	return nullptr;
}

Rc<Texture> ResourceCache::addExternalImage(StringView key, core::ImageInfo &&info,
		const memory::function<void(uint8_t *, uint64_t, const core::ImageData::DataCallback &)>
				&cb,
		TimeInterval ival, TemporaryResourceFlags flags) {
	auto it = _temporaries.find(key);
	if (it != _temporaries.end()) {
		if (auto tex = it->second->acquireTexture(key)) {
			return tex;
		}
		log::source().error("ResourceCache", "Resource '", key,
				"' already exists, but no texture '", key, "' found");
		return nullptr;
	}

	core::Resource::Builder builder(key);
	if (auto d = builder.addImage(key, move(info), cb)) {
		if (auto tmp = addTemporaryResource(Rc<core::Resource>::create(move(builder)), ival,
					flags)) {
			return Rc<Texture>::create(d, tmp);
		}
	}
	return nullptr;
}

TemporaryResource *ResourceCache::addTemporaryResource(Rc<core::Resource> &&res, TimeInterval ival,
		TemporaryResourceFlags flags) {
	return addTemporaryResource(Rc<TemporaryResource>::create(move(res), ival, flags));
}

TemporaryResource *ResourceCache::addTemporaryResource(Rc<TemporaryResource> &&tmp) {
	auto it = _temporaries.find(tmp->getName());
	if (it != _temporaries.end()) {
		_temporaries.erase(it);
	}
	it = _temporaries.emplace(tmp->getName(), move(tmp)).first;

	if ((it->second->getFlags() & TemporaryResourceFlags::CompileWhenAdded)
			!= TemporaryResourceFlags::None) {
		compileResource(it->second);
	}
	return it->second;
}

Rc<TemporaryResource> ResourceCache::getTemporaryResource(StringView str) const {
	auto it = _temporaries.find(str);
	if (it != _temporaries.end()) {
		return it->second;
	}
	return nullptr;
}

bool ResourceCache::hasTemporaryResource(StringView str) const {
	auto it = _temporaries.find(str);
	if (it != _temporaries.end()) {
		return true;
	}
	return false;
}

void ResourceCache::removeTemporaryResource(StringView str) {
	auto it = _temporaries.find(str);
	if (it != _temporaries.end()) {
		it->second->clear();
		_temporaries.erase(it);
	}
}

void ResourceCache::compileResource(TemporaryResource *res) {
	if (_loop) {
		res->setRequested(true);
		_loop->compileResource(Rc<core::Resource>(res->getResource()),
				[res = Rc<TemporaryResource>(res), guard = Rc<ResourceCache>(this)](
						bool success) mutable {
			guard->getApplication()->performOnAppThread([guard, res = move(res), success] {
				res->setLoaded(success);
				guard->getApplication()->wakeup();
			}, nullptr, false);
		});
	}
}

bool ResourceCache::clearResource(TemporaryResource *res) { return res->clear(); }

} // namespace stappler::xenolith
