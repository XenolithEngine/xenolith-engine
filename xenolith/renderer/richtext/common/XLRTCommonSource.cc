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

#include "XLRTCommonSource.h"
#include "SPFilesystem.h"
#include "SPDocPageContainer.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::richtext {

XL_DECLARE_EVENT(CommonSource, "RichTextSource", onError);
XL_DECLARE_EVENT(CommonSource, "RichTextSource", onDocument);
XL_DECLARE_EVENT(CommonSource, "RichTextSource", onUpdate);

CommonSource::~CommonSource() { }

bool CommonSource::init(AssetLibrary *lib) {
	_assetLibrary = lib;
	return true;
}

bool CommonSource::init(AssetLibrary *lib, SourceAsset *asset, bool enabled) {
	if (!init(lib)) {
		return false;
	}

	_enabled = enabled;
	onDocumentAsset(asset);
	return true;
}

void CommonSource::setHyphens(font::HyphenMap *map) { _hyphens = map; }

font::HyphenMap *CommonSource::getHyphens() const { return _hyphens; }

Document *CommonSource::getDocument() const { return static_cast<Document *>(_document.get()); }
SourceAsset *CommonSource::getAsset() const { return _documentAsset; }

Map<String, DocumentAssetMeta> CommonSource::getExternalAssetMeta() const {
	Map<String, DocumentAssetMeta> ret;
	for (auto &it : _networkAssets) {
		auto &v = ret.emplace(it.first, it.second.meta).first->second;
		v.lock = it.second.asset.get()->lock();
	}
	for (auto &it : _localAssets) {
		auto &v = ret.emplace(it.first, it.second.meta).first->second;
		v.lock = it.second.asset->lock();
	}
	return ret;
}

const Map<String, CommonSource::NetworkAssetData> &CommonSource::getNetworkAssets() const {
	return _networkAssets;
}

const Set<String> &CommonSource::getEmbeddedAssets() const { return _embeddedAssets; }

bool CommonSource::isReady() const { return _document; }

bool CommonSource::isActual() const {
	if (!_document) {
		return false;
	}

	if (!_documentAsset) {
		return true;
	}

	if (_documentLoading) {
		return false;
	}

	auto l = _documentAsset.get()->lock(0);
	if (_loadedAssetMTime >= l->getMTime()) {
		return true;
	}

	return false;
}

bool CommonSource::isDocumentLoading() const { return _documentLoading; }

void CommonSource::refresh() { updateDocument(); }

void CommonSource::setEnabled(bool val) {
	if (_enabled != val) {
		_enabled = val;
		if (_enabled && _documentAsset) {
			onDocumentAssetUpdated(Subscription::Flag((uint8_t)Asset::CacheDataUpdated));
		}
	}
}
bool CommonSource::isEnabled() const { return _enabled; }

void CommonSource::onDocumentAsset(SourceAsset *a) {
	_documentAsset = a;
	if (_documentAsset) {
		_loadedAssetMTime = 0;
		if (_enabled) {
			_documentAsset->download();
		}
		onDocumentAssetUpdated(Subscription::Flag((uint8_t)Asset::CacheDataUpdated));
	}
}

void CommonSource::onDocumentAssetUpdated(Subscription::Flags f) {
	if (f.hasFlag(Asset::DownloadFailed)) {
		onError(this, Error::NetworkError);
	}

	if (_documentAsset->isDownloadAvailable() && !_documentAsset->isDownloadInProgress()) {
		if (f.hasFlag(Asset::DownloadFailed)) {
			if (isnan(_retryUpdate)) {
				_retryUpdate = 20.0f;
			}
		} else {
			if (_enabled) {
				_documentAsset->download();
			}
			onUpdate(this);
		}
	}

	auto lock = _documentAsset.get()->lock(0);
	if (lock) {
		if (_loadedAssetMTime < lock->getMTime()) {
			tryLoadDocument(lock);
		} else if ((f.initial() && _loadedAssetMTime == 0)
				|| f.hasFlag(Asset::Update::CacheDataUpdated)) {
			_loadedAssetMTime = 0;
			tryLoadDocument(lock);
		}
	}

	if (f.hasFlag(Asset::CacheDataUpdated) || f.hasFlag(Asset::DownloadSuccessful)
			|| f.hasFlag(Asset::DownloadFailed)) {
		onDocument(this, _documentAsset.get());
	}
}

struct CommonSourceDocumentLock : public Ref {
	Rc<Document> document;
	Rc<SourceAssetLock> lock;
	Set<String> assets;
};

void CommonSource::tryLoadDocument(SourceAssetLock *lock) {
	if (!_enabled || !lock) {
		return;
	}

	auto l = Rc<CommonSourceDocumentLock>::alloc();
	l->lock = lock;

	_loadedAssetMTime = l->lock->getMTime();
	_documentLoading = true;
	onUpdate(this);

	_assetLibrary->getApplication()->perform([l](const thread::Task &) -> bool {
		l->document = l->lock->openDocument();
		if (l->document) {
			l->document->foreachPage([&](StringView, const document::PageContainer *page) {
				for (auto &str : page->getAssets()) {
					l->assets.emplace(StringView(str).str<Interface>());
				}
			});
		}
		return true;
	}, [this, l](const thread::Task &, bool success) {
		if (success && l->document) {
			auto lcb = [this, l] {
				_documentLoading = false;
				onDocumentLoaded(l->document.get());
			};
			if (onDocumentAssets(l->document, l->assets)) {
				lcb();
			} else {
				waitForAssets(move(lcb));
			}
		}
	}, this);
}

void CommonSource::onDocumentLoaded(Document *doc) {
	if (_document != doc) {
		_document = doc;
		if (_document) {
			_dirty = true;
		}
		onDocument(this);
	}
}

void CommonSource::acquireNetworkAsset(Document *doc, const StringView &url,
		const Function<void(SourceAsset *)> &fn) {
	_assetLibrary->acquireAsset(url, [this, fn](const Rc<Asset> &a) {
		fn(Rc<SourceNetworkAsset>::create(a));
	}, config::getDocumentAssetTtl(), this);
}

Rc<SourceAsset> CommonSource::acquireLocalAsset(Document *doc, const StringView &url) {
	if (filepath::isAbsolute(url)) {
		if (filesystem::exists(FileInfo(url))) {
			return Rc<SourceFileAsset>::create(FileInfo(url));
		} else {
			log::error("richtext::CommonSource", "Fail to load asset on local path: ", url);
		}
	} else {
		auto path = filesystem::findPath<Interface>(FileInfo(url));
		if (filesystem::exists(FileInfo(path))) {
			return Rc<SourceFileAsset>::create(FileInfo(path));
		}

		path = filesystem::currentDir<Interface>(url);
		if (filesystem::exists(FileInfo(path))) {
			return Rc<SourceFileAsset>::create(FileInfo(path));
		}

		log::error("richtext::CommonSource", "Fail to load asset on local path: ", url);
	}
	return nullptr;
}

bool CommonSource::isExternalAsset(Document *doc, const StringView &asset) {
	bool isDocFile = doc->isFileExists(asset);
	if (!isDocFile) {
		if (asset.starts_with("http://") || asset.starts_with("https://")) {
			return true;
		}
	}
	return false;
}

bool CommonSource::onDocumentAssets(Document *doc, const Set<String> &assets) {
	for (const String &it : assets) {
		if (doc->isFileExists(it)) {
			_embeddedAssets.emplace(it);
			// embedded asset - no actions needed
		} else if (isExternalAsset(doc, it)) {
			auto n_it = _networkAssets.find(it);
			if (n_it == _networkAssets.end()) {
				auto a_it = _networkAssets.emplace(it, NetworkAssetData{it}).first;
				NetworkAssetData *data = &a_it->second;
				addAssetRequest(data);
				acquireNetworkAsset(doc, it, [this, data](SourceAsset *a) {
					if (a) {
						data->asset = a;
						if (data->asset->isReadAvailable()) {
							if (auto l = data->asset->lock()) {
								readExternalAsset(l, data->meta);
							}
						}
						if (data->asset->isDownloadAvailable()) {
							data->asset->download();
						}
					}
					removeAssetRequest(data);
				});
				log::debug("External asset", it);
			}
		} else {
			// local asset, try to load on local path

			auto n_it = _localAssets.find(it);
			if (n_it == _localAssets.end()) {
				if (auto asset = acquireLocalAsset(doc, it)) {
					auto a_it = _localAssets.emplace(it, LocalAssetData{it}).first;
					LocalAssetData *data = &a_it->second;
					data->asset = asset;
					if (auto l = data->asset.get()->lock()) {
						readExternalAsset(l, data->meta);
					}
				}
				log::debug("Local asset", it);
			}
		}
	}
	return !hasAssetRequests();
}

void CommonSource::onExternalAssetUpdated(NetworkAssetData *a, Subscription::Flags f) {
	if (f.hasFlag(Asset::CacheDataUpdated)) {
		bool updated = false;
		if (auto l = a->asset->lock()) {
			if (readExternalAsset(l, a->meta)) {
				updated = true;
			}
		}
		if (updated) {
			if (_document) {
				_dirty = true;
			}
			onDocument(this);
		}
	}
}

bool CommonSource::readExternalAsset(SourceAssetLock *asset, DocumentAssetMeta &meta) const {
	auto mtime = asset->getMTime();
	meta.type = asset->getContentType().str<Interface>();
	if (StringView(meta.type).starts_with("image/") || meta.type.empty()) {
		auto tmp = meta;
		uint32_t w = 0, h = 0;
		if (asset->getImageSize(w, h)) {
			meta.imageWidth = w;
			meta.imageHeight = h;
		}

		meta.mtime = mtime;
		if (tmp.imageWidth != meta.imageWidth || tmp.imageHeight != meta.imageHeight
				|| mtime != tmp.imageHeight) {
			return true;
		}
	} else if (StringView(meta.type).is("text/css")) {
		if (meta.mtime != mtime) {
			asset->load([&](BytesView d) { meta.css = Document::open(d, meta.type); });
			meta.mtime = mtime;
		}
		return true;
	}
	meta.mtime = mtime;
	return false;
}

void CommonSource::updateDocument() {
	if (_documentAsset && _loadedAssetMTime > 0) {
		_loadedAssetMTime = 0;
	}

	if (auto l = _documentAsset.get()->lock()) {
		tryLoadDocument(l);
	}
}

bool CommonSource::hasAssetRequests() const { return !_assetRequests.empty(); }
void CommonSource::addAssetRequest(NetworkAssetData *data) { _assetRequests.emplace(data); }
void CommonSource::removeAssetRequest(NetworkAssetData *data) {
	if (!_assetRequests.empty()) {
		_assetRequests.erase(data);
		if (_assetRequests.empty()) {
			if (!_assetWaiters.empty()) {
				auto w = sp::move(_assetWaiters);
				_assetWaiters.clear();
				for (auto &it : w) { it(); }
			}
		}
	}
}

void CommonSource::waitForAssets(Function<void()> &&fn) {
	_assetWaiters.emplace_back(sp::move(fn));
}

void CommonSource::update(const UpdateTime &t) {
	auto val = _documentAsset.check();
	if (!val.empty()) {
		onDocumentAssetUpdated(val);
	}

	for (auto &it : _networkAssets) {
		auto val = it.second.asset.check();
		if (!val.empty()) {
			onExternalAssetUpdated(&it.second, val);
		}
	}

	if (!isnan(_retryUpdate)) {
		_retryUpdate -= t.dt;
		if (_retryUpdate <= 0.0f) {
			_retryUpdate = nan();
			if (_enabled && _documentAsset && _documentAsset->isDownloadAvailable()
					&& !_documentAsset->isDownloadInProgress()) {
				_documentAsset->download();
			}
		}
	}
}

Rc<RendererResource> CommonSource::prepareResource(ResourceCache *cache, Document *doc,
		Time ctime) {
	if (_cachedResource) {
		if (_cachedResource->cache == cache
				&& _cachedResource->embeddedAssets == getEmbeddedAssets()
				&& isAssetsSame(getExternalAssetMeta(), _cachedResource->externalAssets)) {
			return _cachedResource;
		}
	}

	Rc<RendererResource> res = Rc<RendererResource>::alloc();
	res->cache = cache;
	res->externalAssets = getExternalAssetMeta();
	res->embeddedAssets = getEmbeddedAssets();

	core::Resource::Builder builder(toString(doc->getName(), ctime.toMicros()));

	bool empty = true;
	for (const Pair<const String, DocumentAssetMeta> &it : res->externalAssets) {
		if (it.second.isImage() && it.second.type != "image/svg") {
			res->textures.emplace(it.first);
			builder.addImage(it.first,
					core::ImageInfo(core::ImageFormat::R8G8B8A8_UNORM, core::ImageUsage::Sampled,
							Extent2(it.second.imageWidth, it.second.imageHeight)),
					[lock = it.second.lock](uint8_t *data, uint64_t size,
							const core::ImageData::DataCallback &cb) {
				lock->load([&](BytesView d) {
					core::Resource::loadImageMemoryData(data, size, d,
							core::ImageFormat::R8G8B8A8_UNORM, cb);
				});
			});
			empty = false;
		} else if (it.second.type == "image/svg") {
			it.second.lock->load([&](BytesView d) {
				res->svgs.emplace(it.first, d.toStringView().str<Interface>());
			});
		}
	}

	for (auto &it : res->embeddedAssets) {
		auto img = doc->getImage(it);
		if (img && !img->data.empty()) {
			if (img->ct != "image/svg") {
				res->textures.emplace(it);
				builder.addEncodedImageByRef(it,
						core::ImageInfo(core::ImageFormat::R8G8B8A8_UNORM,
								core::ImageUsage::Sampled, Extent2(img->width, img->height)),
						img->data);
				empty = false;
			} else if (img->ct == "image/svg") {
				res->svgs.emplace(it, img->data.toStringView().str<Interface>());
			}
		}
	}

	if (!empty) {
		res->resource = res->cache->addTemporaryResource(Rc<core::Resource>::create(move(builder)),
				TimeInterval::seconds(720), TemporaryResourceFlags::CompileWhenAdded);
	}

	_cachedResource = res;
	return res;
}

bool CommonSource::isAssetsSame(const Map<String, DocumentAssetMeta> &l,
		const Map<String, DocumentAssetMeta> &r) {
	return l.size() == r.size()
			&& std::equal(l.begin(), l.end(), r.begin(),
					[](const Pair<const String, DocumentAssetMeta> &l,
							const Pair<const String, DocumentAssetMeta> &r) {
		return l.first == r.first && l.second.mtime == r.second.mtime
				&& l.second.type == r.second.type;
	});
}

} // namespace stappler::xenolith::richtext
