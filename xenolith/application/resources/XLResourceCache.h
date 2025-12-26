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

#ifndef XENOLITH_APPLICATION_RESOURCES_XLRESOURCECACHE_H_
#define XENOLITH_APPLICATION_RESOURCES_XLRESOURCECACHE_H_

#include "XLApplicationExtension.h"
#include "XLCoreResource.h"
#include "XLCoreLoop.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class TemporaryResource;
class Texture;
class MeshIndex;

enum class TemporaryResourceFlags : uint32_t {
	None = 0,
	Loaded = 1 << 0, // Resource is a wrapper around already loaded data
	RemoveOnClear = 1 << 1, // Remove whole resource from cache, when no one uses it
	CompileWhenAdded = 1 << 2, // Run resource compilation immediately after addTemporaryResource
};

SP_DEFINE_ENUM_AS_MASK(TemporaryResourceFlags)

class SP_PUBLIC ResourceCache : public ApplicationExtension {
public:
	using ImageData = core::ImageData;
	using ImageInfo = core::ImageInfo;

	virtual ~ResourceCache();

	bool init(AppThread *);

	virtual void initialize(AppThread *) override;
	virtual void invalidate(AppThread *) override;

	virtual void update(AppThread *, const UpdateTime &t, bool) override;

	void addImage(StringView, const Rc<core::ImageObject> &);
	void addResource(const Rc<core::Resource> &);
	void removeResource(StringView);

	AppThread *getApplication() const { return _application; }

	Rc<Texture> acquireTexture(StringView) const;
	Rc<MeshIndex> acquireMeshIndex(StringView) const;

	const ImageData *getEmptyImage() const;
	const ImageData *getSolidImage() const;

	/*const gl::BufferData * addExternalBufferByRef(StringView key, gl::BufferInfo &&, BytesView data);
	const gl::BufferData * addExternalBuffer(StringView key, gl::BufferInfo &&, FilePath data);
	const gl::BufferData * addExternalBuffer(StringView key, gl::BufferInfo &&, BytesView data);
	const gl::BufferData * addExternalBuffer(StringView key, gl::BufferInfo &&, size_t,
			const memory::function<void(const gl::BufferData::DataCallback &)> &cb);*/

	Rc<Texture> addExternalBitmapImageByRef(StringView key, ImageInfo &&, BytesView data,
			TimeInterval = TimeInterval(),
			TemporaryResourceFlags flags = TemporaryResourceFlags::None);
	Rc<Texture> addExternalBitmapImage(StringView key, ImageInfo &&, BytesView data,
			TimeInterval = TimeInterval(),
			TemporaryResourceFlags flags = TemporaryResourceFlags::None);
	Rc<Texture> addExternalEncodedImageByRef(StringView key, ImageInfo &&, BytesView data,
			TimeInterval = TimeInterval(),
			TemporaryResourceFlags flags = TemporaryResourceFlags::None);
	Rc<Texture> addExternalEncodedImage(StringView key, ImageInfo &&, BytesView data,
			TimeInterval = TimeInterval(),
			TemporaryResourceFlags flags = TemporaryResourceFlags::None);
	Rc<Texture> addExternalImage(StringView key, ImageInfo &&, const FileInfo &data,
			TimeInterval = TimeInterval(),
			TemporaryResourceFlags flags = TemporaryResourceFlags::None);
	Rc<Texture> addExternalImage(StringView key, ImageInfo &&,
			const memory::function<void(uint8_t *, uint64_t, const ImageData::DataCallback &)> &,
			TimeInterval = TimeInterval(),
			TemporaryResourceFlags flags = TemporaryResourceFlags::None);

	TemporaryResource *addTemporaryResource(Rc<core::Resource> &&, TimeInterval = TimeInterval(),
			TemporaryResourceFlags flags = TemporaryResourceFlags::None);

	TemporaryResource *addTemporaryResource(Rc<TemporaryResource> &&);

	Rc<TemporaryResource> getTemporaryResource(StringView str) const;
	bool hasTemporaryResource(StringView) const;
	void removeTemporaryResource(StringView);

protected:
	void compileResource(TemporaryResource *);
	bool clearResource(TemporaryResource *);

	AppThread *_application = nullptr;
	const core::Loop *_loop = nullptr;
	Map<StringView, core::ImageData> _images;
	Map<StringView, Rc<core::Resource>> _resources;
	Map<StringView, Rc<TemporaryResource>> _temporaries;
};

} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_RESOURCES_XLRESOURCECACHE_H_ */
