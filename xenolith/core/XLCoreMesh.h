/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

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

#ifndef XENOLITH_CORE_XLCOREMESH_H_
#define XENOLITH_CORE_XLCOREMESH_H_

#include "XLCoreResource.h"
#include "XLCoreAttachment.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

class MeshAttachment;
class MeshIndex;

struct SP_PUBLIC MeshInputData : AttachmentInputData {
	const MeshAttachment *attachment;
	Vector<Rc<MeshIndex>> meshesToAdd;
	Vector<Rc<MeshIndex>> meshesToRemove;
};

class SP_PUBLIC MeshSet final : public Ref {
public:
	struct Index {
		uint64_t indexOffset;
		uint64_t vertexOffset;
		Rc<MeshIndex> index;
	};

	virtual ~MeshSet();

	bool init(Vector<Index> &&, BufferObject *index, BufferObject *vertex);

	const Vector<Index> &getIndexes() const { return _indexes; }

	const Rc<BufferObject> &getVertexBuffer() const { return _vertexBuffer; }
	const Rc<BufferObject> &getIndexBuffer() const { return _indexBuffer; }

	void clear();

protected:
	Vector<Index> _indexes;
	Rc<BufferObject> _vertexBuffer;
	Rc<BufferObject> _indexBuffer;
};

class SP_PUBLIC MeshIndex final : public Resource {
public:
	using DataCallback = memory::callback<void(BytesView)>;
	using BufferCallback = memory::function<void(uint8_t *, uint64_t, const DataCallback &)>;

	struct MeshBufferInfo {
		uint64_t indexBufferSize;
		BufferCallback indexBufferCallback;

		uint64_t vertexBufferSize;
		BufferCallback vertexBufferCallback;
	};

	virtual ~MeshIndex();

	bool init(StringView, Rc<DataAtlas> &&, MeshBufferInfo &&);

	const BufferData *getVertexBufferData() const { return _vertexBuffer; }
	const BufferData *getIndexBufferData() const { return _indexBuffer; }

	const Rc<DataAtlas> &getAtlas() const { return _atlas; }

protected:
	Rc<DataAtlas> _atlas;
	const BufferData *_vertexBuffer = nullptr;
	const BufferData *_indexBuffer = nullptr;
};

// this attachment should provide material data buffer for rendering
class SP_PUBLIC MeshAttachment : public BufferAttachment {
public:
	virtual ~MeshAttachment();

	virtual bool init(AttachmentBuilder &, const BufferInfo &info, Vector<Rc<MeshIndex>> &&initials);

	const Rc<MeshSet> &getMeshes() const;
	void setMeshes(const Rc<MeshSet> &) const;

protected:
	using BufferAttachment::init;

	mutable Rc<MeshSet> _data;
	Vector<Rc<MeshIndex>> _initials;
};

}

#endif /* XENOLITH_CORE_XLCOREMESH_H_ */
