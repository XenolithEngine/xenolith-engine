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

#ifndef XENOLITH_CORE_XLCOREFRAMECACHE_H_
#define XENOLITH_CORE_XLCOREFRAMECACHE_H_

#include "XLCoreQueueData.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

struct SP_PUBLIC FrameCacheFramebuffer final {
	Vector<Rc<Framebuffer>> framebuffers;
	Extent3 extent;
};

struct SP_PUBLIC FrameCacheImageAttachment final {
	uint32_t refCount;
	Vector<Rc<ImageStorage>> images;
};

class SP_PUBLIC FrameCache final : public Ref {
public:
	virtual ~FrameCache();

	bool init(Loop &, Device &);
	void invalidate();

	Rc<Framebuffer> acquireFramebuffer(const QueuePassData *, SpanView<Rc<ImageView>>);
	void releaseFramebuffer(Rc<Framebuffer> &&);

	Rc<ImageStorage> acquireImage(uint64_t attachment, const ImageInfoData &, SpanView<ImageViewInfo> v);
	void releaseImage(Rc<ImageStorage> &&);

	void addImageView(uint64_t);
	void removeImageView(uint64_t);

	void addRenderPass(uint64_t);
	void removeRenderPass(uint64_t);

	void addAttachment(uint64_t);
	void removeAttachment(uint64_t);

	void removeUnreachableFramebuffers();

	size_t getFramebuffersCount() const;
	size_t getImagesCount() const;
	size_t getImageViewsCount() const;

	void clear();

	void freeze();
	void unfreeze();

protected:
	bool isReachable(SpanView<uint64_t> ids) const;
	bool isReachable(const ImageInfoData &) const;

	const ImageInfoData * addImage(const ImageInfoData &);
	void removeImage(const ImageInfoData &);

	void makeViews(const Rc<ImageStorage> &, SpanView<ImageViewInfo>);

	Loop *_loop = nullptr;
	Device *_device = nullptr;
	Map<ImageInfoData, FrameCacheImageAttachment> _images;
	Map<Vector<uint64_t>, FrameCacheFramebuffer> _framebuffers;
	Set<uint64_t> _imageViews;
	Set<uint64_t> _renderPasses;
	Map<uint64_t, const ImageInfoData *> _attachments;

	bool _freezed = false;
	Vector<Rc<Ref>> _autorelease;
};

}

#endif /* XENOLITH_CORE_XLCOREFRAMECACHE_H_ */
