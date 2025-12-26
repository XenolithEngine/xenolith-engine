/**
 Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>

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

#ifndef XENOLITH_CORE_XLCOREDYNAMICIMAGE_H_
#define XENOLITH_CORE_XLCOREDYNAMICIMAGE_H_

#include "XLCoreObject.h"
#include "XLCoreAttachment.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

class Loop;
class DynamicImage;
class MaterialAttachment;

struct SP_PUBLIC DynamicImageInstance : public Ref {
	virtual ~DynamicImageInstance() = default;

	ImageData data;
	ImageViewData view;
	Rc<Ref> userdata;
	Rc<DynamicImage> image = nullptr;
	uint32_t gen = 0;
};

class SP_PUBLIC DynamicImage : public Ref {
public:
	class Builder;

	virtual ~DynamicImage() = default;

	bool init(const Callback<bool(Builder &)> &);
	void finalize();

	Rc<DynamicImageInstance> getInstance();

	void updateInstance(Loop &, const Rc<ImageObject> &, Rc<DataAtlas> && = Rc<DataAtlas>(),
			Rc<Ref> &&userdata = Rc<Ref>(),
			const Vector<Rc<DependencyEvent>> & = Vector<Rc<DependencyEvent>>(),
			Rc<ImageView> && = Rc<ImageView>());

	void addTracker(const MaterialAttachment *);
	void removeTracker(const MaterialAttachment *);

	ImageInfo getInfo() const;
	Extent3 getExtent() const;

	// called when image compiled successfully
	void setImage(const Rc<ImageObject> &);
	void acquireData(const Callback<void(BytesView)> &);

protected:
	friend class Builder;

	mutable Mutex _mutex;
	String _keyData;
	Bytes _imageData;
	ImageData _data;
	Rc<DynamicImageInstance> _instance;
	Set<const MaterialAttachment *> _materialTrackers;
};

class SP_PUBLIC DynamicImage::Builder {
public:
	const ImageData *setImageByRef(StringView key, ImageInfo &&, BytesView data,
			Rc<DataAtlas> && = nullptr);
	const ImageData *setImage(StringView key, ImageInfo &&, const FileInfo &data,
			Rc<DataAtlas> && = nullptr);
	const ImageData *setImage(StringView key, ImageInfo &&, BytesView data,
			Rc<DataAtlas> && = nullptr);
	const ImageData *setImage(StringView key, ImageInfo &&,
			Function<void(uint8_t *, uint64_t, const ImageData::DataCallback &)> &&cb,
			Rc<DataAtlas> && = nullptr);

protected:
	friend class DynamicImage;

	Builder(DynamicImage *);

	DynamicImage *_data = nullptr;
};

} // namespace stappler::xenolith::core

#endif /* XENOLITH_CORE_XLCOREDYNAMICIMAGE_H_ */
