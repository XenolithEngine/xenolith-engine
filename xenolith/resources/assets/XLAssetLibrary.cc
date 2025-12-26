/**
 Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>
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

#include "XLAssetLibrary.h"
#include "SPFilepath.h"
#include "SPFilesystem.h"
#include "SPLog.h"
#include "XLAppThread.h" // IWYU pragma: keep
#include "XLAsset.h"
#include "XLStorageComponent.h"
#include "XLStorageServer.h"
#include "XLNetworkController.h"
#include "SPSqlHandle.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::storage {

class AssetComponent : public Component {
public:
	static constexpr auto DtKey = "XL.AssetLibrary.dt";

	virtual ~AssetComponent() { }

	AssetComponent(AssetComponentContainer *, ComponentLoader &, StringView name);

	const db::Scheme &getAssets() const { return _assets; }
	const db::Scheme &getVersions() const { return _versions; }

	virtual void handleChildInit(const Server &, const db::Transaction &) override;

	void cleanup(const db::Transaction &t);

	db::Value getAsset(const db::Transaction &, StringView) const;
	db::Value createAsset(const db::Transaction &, StringView, TimeInterval) const;

	void updateAssetTtl(const db::Transaction &, int64_t, TimeInterval) const;

protected:
	AssetComponentContainer *_container = nullptr;
	db::Scheme _assets = db::Scheme("assets");
	db::Scheme _versions = db::Scheme("versions");
};

XL_DECLARE_EVENT_CLASS(AssetLibrary, onLoaded)

bool AssetComponentContainer::init(StringView name, AssetLibrary *l) {
	if (!ComponentContainer::init(name)) {
		return false;
	}

	_library = l;
	return true;
}

void AssetComponentContainer::handleStorageInit(storage::ComponentLoader &loader) {
	ComponentContainer::handleStorageInit(loader);
	_component = new (std::nothrow) AssetComponent(this, loader, "AssetComponent");
}

void AssetComponentContainer::handleStorageDisposed(const db::Transaction &t) {
	_component = nullptr;
	ComponentContainer::handleStorageDisposed(t);
}


AssetComponent::AssetComponent(AssetComponentContainer *c, ComponentLoader &loader, StringView name)
: Component(loader, name), _container(c) {
	using namespace db;

	loader.exportScheme(_assets.define({
		Field::Integer("mtime", Flags::AutoMTime),
		Field::Integer("touch", Flags::AutoCTime),
		Field::Integer("ttl"),
		Field::Text("local"),
		Field::Text("url", MaxLength(2_KiB), Transform::Url, Flags::Unique | Flags::Indexed),
		Field::Set("versions", _versions),
		Field::Boolean("download", db::Value(false), Flags::Indexed),
		Field::Data("data"),
	}));

	loader.exportScheme(_versions.define({
		Field::Text("etag", MaxLength(2_KiB)),
		Field::Integer("ctime", Flags::AutoCTime),
		Field::Integer("mtime", Flags::AutoMTime),
		Field::Integer("size"),
		Field::Text("type"),
		Field::Boolean("complete", db::Value(false)),
		Field::Object("asset", _assets, RemovePolicy::Cascade),
	}));
}

void AssetComponent::handleChildInit(const Server &serv, const db::Transaction &t) {
	Component::handleChildInit(serv, t);

	filesystem::mkdir(FileInfo("assets", FileCategory::AppState));

	Time time = Time::now();
	Vector<Rc<Asset>> assetsVec;

	auto assets = _assets.select(t, db::Query().select("download", db::Value(true)));
	for (auto &it : assets.asArray()) {
		auto versions = _versions.select(t, db::Query().select("asset", it.getValue("__oid")));
		it.setValue(move(versions), "versions");

		auto &asset = assetsVec.emplace_back(Rc<Asset>::alloc(_container->getLibrary(), it));
		asset->touch(time);

		_assets.update(t, it, db::Value({pair("touch", db::Value(asset->getTouch().toMicros()))}));
	}

	cleanup(t);

	_container->getLibrary()->getApplication()->performOnAppThread(
			[assetsVec = sp::move(assetsVec), lib = _container->getLibrary()]() mutable {
		lib->handleLibraryLoaded(sp::move(assetsVec));
	}, _container->getLibrary());
}

void AssetComponent::cleanup(const db::Transaction &t) {
	Time time = Time::now();
	if (auto iface = dynamic_cast<db::sql::SqlHandle *>(t.getAdapter().getBackendInterface())) {
		auto query = toString("SELECT __oid, url FROM ", _assets.getName(),
				" WHERE download == 0 AND ttl != 0 AND (touch + ttl) < ", time.toMicros(), ";");
		iface->performSimpleSelect(query, [&](db::Result &res) {
			for (auto it : res) {
				auto path = _container->getLibrary()->getAssetPath(it.toInteger(0));
				filesystem::remove(FileInfo{path}, true, true);
			}
		});

		iface->performSimpleQuery(toString("DELETE FROM ", _assets.getName(),
				" WHERE download == 0 AND ttl != 0 AND touch + ttl * 2 < ", time.toMicros(), ";"));
	}
}

db::Value AssetComponent::getAsset(const db::Transaction &t, StringView url) const {
	if (auto v = _assets.select(t, db::Query().select("url", db::Value(url))).getValue(0)) {
		if (auto versions = _versions.select(t, db::Query().select("asset", v.getValue("__oid")))) {
			v.setValue(move(versions), "versions");
		}
		return db::Value(move(v));
	}
	return db::Value();
}

db::Value AssetComponent::createAsset(const db::Transaction &t, StringView url,
		TimeInterval ttl) const {
	return _assets.create(t,
			db::Value({
				pair("url", db::Value(url)),
				pair("ttl", db::Value(ttl)),
			}));
}

void AssetComponent::updateAssetTtl(const db::Transaction &t, int64_t id, TimeInterval ttl) const {
	_assets.update(t, id,
			db::Value({
				pair("ttl", db::Value(ttl)),
			}),
			db::UpdateFlags::NoReturn);
}

String AssetLibrary::getAssetUrl(StringView url) {
	if (url.starts_with("%") || url.starts_with("app://") || url.starts_with("http://")
			|| url.starts_with("https://") || url.starts_with("ftp://")
			|| url.starts_with("ftps://")) {
		return url.str<Interface>();
	} else if (url.starts_with("/")) {
		return filepath::canonical<Interface>(url);
	} else {
		return toString("app://", url);
	}
}

Rc<ApplicationExtension> AssetLibrary::createLibrary(AppThread *app, network::Controller *c,
		StringView name, const FileInfo &root, const Value &dbParams) {
	return Rc<storage::AssetLibrary>::create(app, c, name, root, dbParams);
}

AssetLibrary::~AssetLibrary() { _server = nullptr; }

bool AssetLibrary::init(AppThread *app, network::Controller *c, StringView name,
		const FileInfo &root, const Value &dbParams) {
	_rootPath = filesystem::findWritablePath<Interface>(root);

	db::Value targetDbParams = dbParams;
	if (!dbParams.hasValue("driver")) {
		targetDbParams.setString("sqlite", "driver");
	}
	if (!dbParams.hasValue("dbname")) {
		targetDbParams.setString(filepath::merge<Interface>(_rootPath, "assets.sqlite"), "dbname");
	}

	targetDbParams.setString(name, "serverName");

	_server = Rc<Server>::create(app, targetDbParams);
	if (!_server) {
		log::source().error("AssetLibrary",
				"Fail to create DB server connection for asset library");
		return false;
	}

	_application = app; // always before server initialization
	_controller = c;
	_container = Rc<AssetComponentContainer>::create("AssetLibrary", this);

	_server->addComponentContainer(_container);
	return true;
}

void AssetLibrary::initialize(AppThread *) { }

void AssetLibrary::invalidate(AppThread *app) {
	UpdateTime t;
	update(app, t, false);
	_liveAssets.clear();
	_assetsByUrl.clear();
	_assetsById.clear();
	_callbacks.clear();

	_server->removeComponentContainer(_container);
	_server->invalidate(app);
	_server = nullptr;
}

void AssetLibrary::update(AppThread *, const UpdateTime &t, bool) {
	auto it = _liveAssets.begin();
	while (it != _liveAssets.end()) {
		if ((*it)->isStorageDirty()) {
			_server->perform(
					[this, value = (*it)->encode(), id = (*it)->getId()](const Server &,
							const db::Transaction &t) {
				_container->getComponent()->getAssets().update(t, id, db::Value(value),
						db::UpdateFlags::NoReturn);
				return true;
			},
					this);
			(*it)->setStorageDirty(false);
		}
		if ((*it)->getReferenceCount() == 1) {
			it = _liveAssets.erase(it);
		} else {
			++it;
		}
	}
}

String AssetLibrary::getAssetPath(int64_t id) { return toString(_rootPath, "/", id); }

bool AssetLibrary::acquireAsset(StringView iurl, AssetCallback &&cb, TimeInterval ttl,
		Rc<Ref> &&ref) {
	if (!_loaded) {
		_tmpRequests.push_back(AssetRequest(iurl, sp::move(cb), ttl, sp::move(ref)));
		return true;
	}

	auto url = getAssetUrl(iurl);
	if (auto a = getLiveAsset(url)) {
		if (cb) {
			cb(a);
		}
		return true;
	}

	auto it = _callbacks.find(url);
	if (it != _callbacks.end()) {
		it->second.emplace_back(sp::move(cb), sp::move(ref));
	} else {
		_callbacks.emplace(url,
				Vector<Pair<AssetCallback, Rc<Ref>>>({pair(sp::move(cb), sp::move(ref))}));

		_server->perform(
				[this, url = sp::move(url), ttl](const Server &, const db::Transaction &t) {
			Rc<Asset> asset;
			if (auto data = _container->getComponent()->getAsset(t, url)) {
				if (data.getInteger("ttl") != int64_t(ttl.toMicros())) {
					_container->getComponent()->updateAssetTtl(t, data.getInteger("__oid"), ttl);
					data.setInteger(ttl.toMicros(), "ttl");
				}

				handleAssetLoaded(Rc<Asset>::alloc(this, data));
			} else {
				if (auto data = _container->getComponent()->createAsset(t, url, ttl)) {
					handleAssetLoaded(Rc<Asset>::alloc(this, data));
				}
			}
			return true;
		});
	}

	return true;
}

bool AssetLibrary::acquireAssets(SpanView<AssetRequest> vec, AssetVecCallback &&icb,
		Rc<Ref> &&ref) {
	if (!_loaded) {
		if (!icb && !ref) {
			for (auto &it : vec) { _tmpRequests.emplace_back(sp::move(it)); }
		} else {
			_tmpMultiRequest.emplace_back(
					AssetMultiRequest(vec.vec<Interface>(), sp::move(icb), sp::move(ref)));
		}
		return true;
	}

	size_t assetCount = vec.size();
	auto requests = new Vector<AssetRequest>;

	Vector<Rc<Asset>> *retVec = nullptr;
	AssetVecCallback *cb = nullptr;
	if (cb) {
		retVec = new Vector<Rc<Asset>>;
		cb = new AssetVecCallback(sp::move(icb));
	}

	for (auto &it : vec) {
		if (auto a = getLiveAsset(it.url)) {
			if (it.callback) {
				it.callback(a);
			}
			if (retVec) {
				retVec->emplace_back(a);
			}
		} else {
			auto cbit = _callbacks.find(it.url);
			if (cbit != _callbacks.end()) {
				cbit->second.emplace_back(it.callback, ref);
				if (cb && retVec) {
					cbit->second.emplace_back([cb, retVec, assetCount](Asset *a) {
						retVec->emplace_back(a);
						if (retVec->size() == assetCount) {
							(*cb)(*retVec);
							delete retVec;
							delete cb;
						}
					}, nullptr);
				}
			} else {
				Vector<Pair<AssetCallback, Rc<Ref>>> vec;
				vec.emplace_back(it.callback, ref);
				if (cb && retVec) {
					vec.emplace_back([cb, retVec, assetCount](Asset *a) {
						retVec->emplace_back(a);
						if (retVec->size() == assetCount) {
							(*cb)(*retVec);
							delete retVec;
							delete cb;
						}
					}, nullptr);
				}
				_callbacks.emplace(it.url, sp::move(vec));
				requests->push_back(it);
			}
		}
	}

	if (requests->empty()) {
		if (cb && retVec) {
			if (retVec->size() == assetCount) {
				(*cb)(*retVec);
				delete retVec;
				delete cb;
			}
		}
		delete requests;
		return true;
	}

	_server->perform([this, requests](const Server &, const db::Transaction &t) {
		db::Vector<int64_t> ids;
		for (auto &it : *requests) {
			Rc<Asset> asset;
			if (auto data = _container->getComponent()->getAsset(t, it.url)) {
				if (!db::emplace_ordered(ids, data.getInteger("__oid"))) {
					continue;
				}

				if (data.getInteger("ttl") != int64_t(it.ttl.toMicros())) {
					_container->getComponent()->updateAssetTtl(t, data.getInteger("__oid"), it.ttl);
					data.setInteger(it.ttl.toMicros(), "ttl");
				}

				handleAssetLoaded(Rc<Asset>::alloc(this, data));
			} else {
				if (auto data = _container->getComponent()->createAsset(t, it.url, it.ttl)) {
					handleAssetLoaded(Rc<Asset>::alloc(this, data));
				}
			}
		}
		return true;
	});
	return true;
}

int64_t AssetLibrary::addVersion(const db::Transaction &t, int64_t assetId,
		const Asset::VersionData &data) {
	auto version = _container->getComponent()->getVersions().create(t,
			db::Value({
				pair("asset", db::Value(assetId)),
				pair("etag", db::Value(data.etag)),
				pair("ctime", db::Value(data.ctime)),
				pair("size", db::Value(data.size)),
				pair("type", db::Value(data.contentType)),
			}));
	return version.getInteger("__oid");
}

void AssetLibrary::eraseVersion(int64_t id) {
	_server->perform([this, id](const Server &serv, const db::Transaction &t) {
		return _container->getComponent()->getVersions().remove(t, id);
	});
}

void AssetLibrary::setAssetDownload(int64_t id, bool value) {
	_server->perform([this, id, value](const Server &serv, const db::Transaction &t) -> bool {
		return _container->getComponent()->getAssets().update(t, id,
					   db::Value({pair("download", db::Value(value))}))
				? true
				: false;
	});
}

void AssetLibrary::setVersionComplete(int64_t id, bool value) {
	_server->perform([this, id, value](const Server &serv, const db::Transaction &t) -> bool {
		return _container->getComponent()->getVersions().update(t, id,
					   db::Value({pair("complete", db::Value(value))}))
				? true
				: false;
	});
}

void AssetLibrary::cleanup() {
	if (!_controller->isNetworkOnline()) {
		// if we had no network connection to restore assets - do not cleanup them
		return;
	}

	_server->perform([this](const storage::Server &serv, const db::Transaction &t) {
		_container->getComponent()->cleanup(t);
		return true;
	}, this);
}

Asset *AssetLibrary::getLiveAsset(StringView url) const {
	auto it = _assetsByUrl.find(url);
	if (it != _assetsByUrl.end()) {
		return it->second;
	}
	return nullptr;
}

Asset *AssetLibrary::getLiveAsset(int64_t id) const {
	auto it = _assetsById.find(id);
	if (it != _assetsById.end()) {
		return it->second;
	}
	return nullptr;
}

bool AssetLibrary::perform(TaskCallback &&cb, Ref *ref) const {
	return _container->perform(sp::move(cb), ref);
}

void AssetLibrary::removeAsset(Asset *asset) {
	_assetsById.erase(asset->getId());
	_assetsByUrl.erase(asset->getUrl());
}

void AssetLibrary::handleLibraryLoaded(Vector<Rc<Asset>> &&assets) {
	for (auto &it : assets) {
		StringView url = it->getUrl();

		_assetsByUrl.emplace(it->getUrl(), it.get());
		_assetsById.emplace(it->getId(), it.get());

		auto iit = _callbacks.find(url);
		if (iit != _callbacks.end()) {
			for (auto &cb : iit->second) { cb.first(it); }
		}
	}

	assets.clear();

	_loaded = true;

	for (auto &it : _tmpRequests) {
		acquireAsset(it.url, sp::move(it.callback), it.ttl, sp::move(it.ref));
	}

	_tmpRequests.clear();

	for (auto &it : _tmpMultiRequest) {
		acquireAssets(it.vec, sp::move(it.callback), sp::move(it.ref));
	}

	_tmpMultiRequest.clear();
}

void AssetLibrary::handleAssetLoaded(Rc<Asset> &&asset) {
	_application->performOnAppThread([this, asset = move(asset)] {
		_assetsById.emplace(asset->getId(), asset);
		_assetsByUrl.emplace(asset->getUrl(), asset);

		auto it = _callbacks.find(asset->getUrl());
		if (it != _callbacks.end()) {
			for (auto &cb : it->second) {
				if (cb.first) {
					cb.first(asset);
				}
			}
			_callbacks.erase(it);
		}
	}, this);
}

} // namespace stappler::xenolith::storage
