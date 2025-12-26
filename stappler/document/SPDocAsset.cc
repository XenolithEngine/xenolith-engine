/**
 Copyright (c) 2024-2025 Stappler LLC <admin@stappler.dev>
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

#include "SPDocAsset.h"

namespace STAPPLER_VERSIONIZED stappler::document {

bool DocumentAssetMeta::isImage() const {
	if ((StringView(type).starts_with("image/") || type.empty()) && imageWidth > 0
			&& imageHeight > 0) {
		return true;
	}
	return false;
}

DocumentAssetLock::~DocumentAssetLock() {
	if (_releaseFunction) {
		_releaseFunction(*_asset, _lock);
	}
}

int64_t DocumentAssetLock::getMTime() const { return _asset->getMTime(this); }

Rc<Document> DocumentAssetLock::openDocument() const { return _asset->openDocument(this); }

StringView DocumentAssetLock::getContentType() const { return _asset->getContentType(this); }

bool DocumentAssetLock::getImageSize(uint32_t &w, uint32_t &h) const {
	return _asset->getImageSize(this, w, h);
}

bool DocumentAssetLock::load(const Callback<void(BytesView)> &cb) const {
	return _asset->load(this, cb);
}

DocumentAssetLock::DocumentAssetLock(Rc<DocumentAsset> &&a,
		Function<void(const DocumentAsset &, Ref *)> &&fn, Rc<Ref> &&lock)
: _asset(sp::move(a)), _releaseFunction(sp::move(fn)), _lock(sp::move(lock)) { }

Rc<DocumentAssetLock> DocumentAsset::lock(int64_t mtime) {
	auto lock = doLockAsset(mtime);
	if (lock) {
		auto ret = new (std::nothrow) DocumentAssetLock(this,
				[this](const DocumentAsset &, Ref *lock) { doReleaseLock(lock); }, sp::move(lock));
		auto ref = Rc<DocumentAssetLock>(ret);
		ret->release(0);
		return ref;
	}
	return nullptr;
}

} // namespace stappler::document
