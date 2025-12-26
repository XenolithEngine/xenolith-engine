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

#ifndef XENOLITH_APPLICATION_RESOURCES_XLTEMPORARYRESOURCE_H_
#define XENOLITH_APPLICATION_RESOURCES_XLTEMPORARYRESOURCE_H_

#include "XLResourceOwner.h"
#include "XLCoreResource.h"
#include "XLResourceCache.h"
#include "XLTexture.h"
#include "XLMeshIndex.h"
#include "XLEvent.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class ResourceObject;
class Texture;
class MeshIndex;

class SP_PUBLIC TemporaryResource : public Ref {
public:
	static EventHeader onLoaded; // bool - true если ресурс загружен, false если выгружен

	virtual ~TemporaryResource();

	bool init(Rc<core::Resource> &&, TimeInterval timeout, TemporaryResourceFlags flags);
	void invalidate();

	Rc<Texture> acquireTexture(StringView);
	Rc<MeshIndex> acquireMeshIndex(StringView);

	void setLoaded(bool);
	void setRequested(bool);
	void setTimeout(TimeInterval);

	// Загружает ресурс в память, вызывает функцию по завершению со значением true
	// Если ресурс уже загружен, вызывает функцию немедленно со значением false
	// Возвращает true если загрузка начата и false есть ресурс уже загружен
	bool load(Ref *, Function<void(Ref *, bool)> &&);

	void handleEnter(ResourceOwner *, ResourceObject *);
	void handleExit(ResourceOwner *, ResourceObject *);

	bool clear();

	StringView getName() const;

	bool isRequested() const { return _requested; }
	bool isLoaded() const { return _loaded; }

	TemporaryResourceFlags getFlags() const { return _flags; }

	Time getAccessTime() const { return _atime; }
	TimeInterval getTimeout() const { return _timeout; }
	size_t getUsersCount() const { return _users; }

	const Rc<core::Resource> &getResource() const { return _resource; }

	bool isDeprecated(const UpdateTime &) const;

protected:
	bool _requested = false;
	bool _loaded = false;
	TemporaryResourceFlags _flags = TemporaryResourceFlags::None;
	size_t _users = 0;
	uint64_t _atime;
	TimeInterval _timeout;
	String _name;
	Rc<core::Resource> _resource;
	Map<const core::ImageData *, Rc<Texture>> _textures;
	Map<const core::BufferData *, Rc<MeshIndex>> _meshIndexes;
	Set<Rc<ResourceOwner>> _owners;
	Vector<Pair<Rc<Ref>, Function<void(Ref *, bool)>>> _callbacks;
};

} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_RESOURCES_XLTEMPORARYRESOURCE_H_ */
