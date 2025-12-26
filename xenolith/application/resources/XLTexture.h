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

#ifndef XENOLITH_APPLICATION_RESOURCES_XLTEXTURE_H_
#define XENOLITH_APPLICATION_RESOURCES_XLTEXTURE_H_

#include "XLResourceObject.h"
#include "XLCoreDynamicImage.h"
#include "XLCoreMaterial.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class SP_PUBLIC Texture : public ResourceObject {
public:
	virtual ~Texture();

	virtual bool init(const core::ImageData *);
	virtual bool init(const core::ImageData *, const Rc<core::Resource> &);
	virtual bool init(const core::ImageData *, const Rc<TemporaryResource> &);
	virtual bool init(const Rc<core::DynamicImage> &);

	virtual StringView getName() const;

	uint64_t getIndex() const;

	bool hasAlpha() const;

	Extent3 getExtent() const;

	virtual bool isLoaded() const;

	core::MaterialImage getMaterialImage() const;

	// returns nullptr for dynamic images
	const core::ImageData *getImageData() const { return _data; }

	core::ImageInfoData getImageInfo() const;

protected:
	const core::ImageData *_data = nullptr;
	Rc<core::DynamicImage> _dynamic;
};

} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_RESOURCES_XLTEXTURE_H_ */
