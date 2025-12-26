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

#include "XLCoreMesh.h"
#include "XLCoreObject.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

MeshSet::~MeshSet() { }

bool MeshSet::init(Vector<Index> &&idx, BufferObject *index, BufferObject *vertex) {
	_indexes = sp::move(idx);
	_indexBuffer = index;
	_vertexBuffer = vertex;
	return true;
}

void MeshSet::clear() {
	_indexes.clear();
	_vertexBuffer = nullptr;
	_indexBuffer = nullptr;
}

MeshIndex::~MeshIndex() { }

bool MeshIndex::init(StringView name, Rc<DataAtlas> &&atlas, MeshBufferInfo &&bufferInfo) {
	Resource::Builder builder(name);

	_atlas = move(atlas);
	_indexBuffer = builder.addBuffer(toString(name, ":index"),
			BufferInfo(bufferInfo.indexBufferSize, ForceBufferUsage(BufferUsage::TransferSrc)),
			bufferInfo.indexBufferCallback);

	_vertexBuffer = builder.addBuffer(toString(name, ":vertex"),
			BufferInfo(bufferInfo.vertexBufferSize, ForceBufferUsage(BufferUsage::TransferSrc)),
			bufferInfo.vertexBufferCallback);

	return Resource::init(move(builder));
}

MeshAttachment::~MeshAttachment() { }

bool MeshAttachment::init(AttachmentBuilder &builder, const BufferInfo &info, Vector<Rc<MeshIndex>> &&initials) {
	if (!BufferAttachment::init(builder, info)) {
		return false;
	}

	_initials = sp::move(initials);
	return true;
}

const Rc<MeshSet> &MeshAttachment::getMeshes() const {
	return _data;
}

void MeshAttachment::setMeshes(const Rc<MeshSet> &data) const {
	auto tmp = _data;
	_data = data;
	if (tmp) {
		tmp->clear();
	}
}

}
