/**
 Copyright (c) 2024 Stappler LLC <admin@stappler.dev>

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

#include "XLRTSourceAsset.h"
#include "XLAssetLibrary.h"
#include "SPBitmap.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::richtext {

bool SourceMemoryAsset::init(const StringDocument &str, StringView type) {
	_data = Bytes((const uint8_t *)str.get().data(),
			(const uint8_t *)(str.get().data() + str.get().size()));
	_type = type.str<Interface>();
	return true;
}

bool SourceMemoryAsset::init(const StringView &str, StringView type) {
	_data = Bytes((const uint8_t *)str.data(), (const uint8_t *)(str.data() + str.size()));
	_type = type.str<Interface>();
	return true;
}

bool SourceMemoryAsset::init(const BytesView &data, StringView type) {
	_data = Bytes(data.data(), data.data() + data.size());
	_type = type.str<Interface>();
	return true;
}

Rc<Document> SourceMemoryAsset::openDocument(const SourceAssetLock *) {
	if (!_data.empty()) {
		Rc<Document> ret = Document::open(_data, _type);
		if (ret) {
			return ret;
		}
	}

	return Rc<Document>();
}

bool SourceMemoryAsset::load(const SourceAssetLock *l, const Callback<void(BytesView)> &cb) const {
	cb(_data);
	return true;
}

bool SourceMemoryAsset::getImageSize(const SourceAssetLock *l, uint32_t &w, uint32_t &h) const {
	bitmap::ImageInfo info;
	if (bitmap::getImageInfo(_data, info)) {
		w = info.width;
		h = info.height;
		return true;
	}
	return false;
}

bool SourceFileAsset::init(const FileInfo &file, StringView type) {
	if (!filesystem::exists(file)) {
		log::error("richtext::SourceFileAsset", "File not found on path: ", file);
		return false;
	}

	_file = filesystem::findPath<Interface>(FileInfo(file));
	_type = type.str<Interface>();
	if (_type.empty()) {
		type = filesystem::detectMimeType(_file);
		if (!type.empty()) {
			_type = type.str<Interface>();
		}
	}
	return true;
}

Rc<Document> SourceFileAsset::openDocument(const SourceAssetLock *) {
	if (!_file.empty()) {
		Rc<Document> ret = Document::open(FileInfo(_file), _type);
		if (ret) {
			return ret;
		}
	}

	return Rc<Document>();
}

int64_t SourceFileAsset::getMTime(const SourceAssetLock *) const {
	filesystem::Stat stat;
	filesystem::stat(FileInfo(_file), stat);

	return stat.mtime.toMicros();
}

bool SourceFileAsset::load(const SourceAssetLock *, const Callback<void(BytesView)> &cb) const {
	auto d = filesystem::readIntoMemory<Interface>(FileInfo(_file));
	cb(d);
	return true;
}

bool SourceFileAsset::getImageSize(const SourceAssetLock *l, uint32_t &w, uint32_t &h) const {
	return bitmap::getImageSize(FileInfo(_file), w, h);
}

bool SourceNetworkAsset::init(AssetLibrary *l, const StringView &url, TimeInterval ttl) {
	l->acquireAsset(url, [this](Asset *a) { onAsset(a); }, ttl, this);
	return true;
}

bool SourceNetworkAsset::init(Asset *a) {
	onAsset(a);
	return true;
}

bool SourceNetworkAsset::init(const AssetCallback &cb) {
	auto linkId = retain();
	cb([this, linkId](Asset *a) {
		onAsset(a);
		release(linkId);
	});
	return true;
}

void SourceNetworkAsset::setAsset(Asset *a) { onAsset(a); }

void SourceNetworkAsset::setAsset(const AssetCallback &cb) {
	auto linkId = retain();
	cb([this, linkId](Asset *a) {
		onAsset(a);
		release(linkId);
	});
}

Asset *SourceNetworkAsset::getAsset() const { return _asset.get(); }

Rc<Document> SourceNetworkAsset::openDocument(const SourceAssetLock *l) {
	if (auto lock = dynamic_cast<AssetLock *>(l->getLock())) {
		return Document::open(memory::app_root_pool, FileInfo(lock->getPath()),
				lock->getContentType());
	}
	return Rc<Document>();
}

int64_t SourceNetworkAsset::getMTime(const SourceAssetLock *l) const {
	if (auto lock = dynamic_cast<AssetLock *>(l->getLock())) {
		return int64_t(lock->getMTime().toMicros());
	}
	return 0;
}

bool SourceNetworkAsset::download() { return _asset->download(); }

bool SourceNetworkAsset::isDownloadAvailable() const { return _asset->isDownloadAvailable(); }
bool SourceNetworkAsset::isDownloadInProgress() const { return _asset->isDownloadInProgress(); }
bool SourceNetworkAsset::isReadAvailable() const { return _asset->getReadableVersionId() != 0; }

StringView SourceNetworkAsset::getContentType(const SourceAssetLock *l) const {
	if (auto lock = dynamic_cast<AssetLock *>(l->getLock())) {
		return lock->getContentType();
	}
	return StringView();
}

bool SourceNetworkAsset::load(const SourceAssetLock *l, const Callback<void(BytesView)> &cb) const {
	if (auto lock = dynamic_cast<AssetLock *>(l->getLock())) {
		auto d = filesystem::readIntoMemory<Interface>(FileInfo(lock->getPath()));
		cb(d);
		return true;
	}
	return false;
}

bool SourceNetworkAsset::getImageSize(const SourceAssetLock *l, uint32_t &w, uint32_t &h) const {
	if (auto lock = dynamic_cast<AssetLock *>(l->getLock())) {
		return bitmap::getImageSize(FileInfo(lock->getPath()), w, h);
	}
	return false;
}

void SourceNetworkAsset::setExtraData(Value &&data) {
	if (_asset) {
		_asset->setData(move(data));
	}
}

Value SourceNetworkAsset::getExtraData() const {
	if (_asset) {
		return _asset->getData();
	}
	return Value();
}

Rc<Ref> SourceNetworkAsset::doLockAsset(int64_t mtime) {
	if (!_asset) {
		return Rc<Ref>();
	}

	if (auto lock = _asset->lockReadableVersion(this)) {
		if ((lock->getMTime() > Time::microseconds(mtime) && !_asset->isDownloadInProgress())
				|| mtime == 0) {
			return lock;
		}
	}

	return Rc<Ref>();
}

void SourceNetworkAsset::onAsset(Asset *a) {
	a->setForwardedSubscription(this);
	_asset = a;
	if (_asset) {
		setDirty(Subscription::Flags(Asset::CacheDataUpdated), true);
	}
}

} // namespace stappler::xenolith::richtext
