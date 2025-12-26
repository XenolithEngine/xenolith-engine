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

#ifndef XENOLITH_CORE_XLCOREIMAGESTORAGE_H_
#define XENOLITH_CORE_XLCOREIMAGESTORAGE_H_

#include "XLCoreObject.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

class SP_PUBLIC ImageStorage : public Ref {
public:
	virtual ~ImageStorage();

	virtual bool init(Rc<ImageObject> &&);

	bool isStatic() const;
	bool isCacheable() const;
	bool isSwapchainImage() const;

	virtual void cleanup();
	virtual void rearmSemaphores(Loop &);
	virtual void releaseSemaphore(Semaphore *);

	void invalidate();

	bool isReady() const { return isStatic() || (_ready && !_invalid); }
	void setReady(bool);
	void waitReady(Function<void(bool)> &&);

	virtual bool isSemaphorePersistent() const { return true; }

	const Rc<Semaphore> &getWaitSem() const;
	const Rc<Semaphore> &getSignalSem() const;
	uint64_t getImageIndex() const;

	virtual ImageInfoData getInfo() const;
	Rc<ImageObject> getImage() const;

	void addView(const ImageViewInfo &, Rc<ImageView> &&);
	Rc<ImageView> getView(const ImageViewInfo &) const;
	virtual Rc<ImageView> makeView(const ImageViewInfo &);

	void setLayout(AttachmentLayout);
	AttachmentLayout getLayout() const;

	const Map<ImageViewInfo, Rc<ImageView>> &getViews() const { return _views; }

	void setAcquisitionTime(uint64_t t) { _acquisitionTime = t; }
	uint64_t getAcquisitionTime() const { return _acquisitionTime; }

	void setFrameIndex(uint64_t idx) { _frameIndex = idx; }
	uint64_t getFrameIndex() const { return _frameIndex; }

protected:
	void notifyReady();

	uint64_t _acquisitionTime = 0;
	uint64_t _frameIndex = 0;
	Rc<ImageObject> _image;
	Rc<Semaphore> _waitSem;
	Rc<Semaphore> _signalSem;
	Map<ImageViewInfo, Rc<ImageView>> _views;

	bool _ready = true;
	bool _invalid = false;
	bool _isSwapchainImage = false;
	AttachmentLayout _layout = AttachmentLayout::Undefined;

	Vector<Function<void(bool)>> _waitReady;
};

}


#endif /* XENOLITH_CORE_XLCOREIMAGESTORAGE_H_ */
