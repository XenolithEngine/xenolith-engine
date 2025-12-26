/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
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

#ifndef XENOLITH_RESOURCES_ASSETS_XLASSETLIBRARY_H_
#define XENOLITH_RESOURCES_ASSETS_XLASSETLIBRARY_H_

#include "XLAsset.h"
#include "XLEvent.h"
#include "XLStorageServer.h"
#include "XLStorageComponent.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::storage {

class AssetComponent;

class SP_PUBLIC AssetComponentContainer : public ComponentContainer {
public:
	virtual ~AssetComponentContainer() { }

	virtual bool init(StringView, AssetLibrary *);

	virtual void handleStorageInit(storage::ComponentLoader &loader) override;
	virtual void handleStorageDisposed(const db::Transaction &t) override;

	AssetLibrary *getLibrary() const { return _library; }
	AssetComponent *getComponent() const { return _component; }

protected:
	AssetLibrary *_library = nullptr;
	AssetComponent *_component = nullptr;
};


class SP_PUBLIC AssetLibrary : public ApplicationExtension {
public:
	static EventHeader onLoaded;

	using AssetCallback = Function<void(const Rc<Asset> &)>;
	using AssetVecCallback = Function<void(const SpanView<Rc<Asset>> &)>;
	using TaskCallback = Function<bool(const Server &, const db::Transaction &)>;

	struct AssetRequest {
		String url;
		AssetCallback callback;
		TimeInterval ttl;
		Rc<Ref> ref;

		AssetRequest(StringView url, AssetCallback &&cb, TimeInterval ttl, Rc<Ref> &&ref)
		: url(AssetLibrary::getAssetUrl(url))
		, callback(sp::move(cb))
		, ttl(ttl)
		, ref(sp::move(ref)) { }
	};

	struct AssetMultiRequest {
		Vector<AssetRequest> vec;
		AssetVecCallback callback;
		Rc<Ref> ref;

		AssetMultiRequest(Vector<AssetRequest> &&vec, AssetVecCallback &&cb, Rc<Ref> &&ref)
		: vec(sp::move(vec)), callback(sp::move(cb)), ref(sp::move(ref)) { }
	};

	static String getAssetUrl(StringView);

	static Rc<ApplicationExtension> createLibrary(AppThread *, network::Controller *,
			StringView name, const FileInfo &root, const Value & = Value());

	virtual ~AssetLibrary();

	bool init(AppThread *, network::Controller *, StringView name, const FileInfo &root,
			const Value & = Value());

	virtual void initialize(AppThread *) override;
	virtual void invalidate(AppThread *) override;

	virtual void update(AppThread *, const UpdateTime &t, bool) override;

	StringView getRootPath() const { return _rootPath; }

	String getAssetPath(int64_t);

	bool acquireAsset(StringView url, AssetCallback &&cb, TimeInterval ttl = TimeInterval(),
			Rc<Ref> && = nullptr);
	bool acquireAssets(SpanView<AssetRequest>, AssetVecCallback && = nullptr, Rc<Ref> && = nullptr);

	Asset *getLiveAsset(StringView) const;
	Asset *getLiveAsset(int64_t) const;

	bool perform(TaskCallback &&, Ref * = nullptr) const;

	AppThread *getApplication() const { return _application; }
	network::Controller *getController() const { return _controller; }

protected:
	friend class Asset;
	friend class AssetComponent;

	int64_t addVersion(const db::Transaction &t, int64_t assetId, const Asset::VersionData &);
	void eraseVersion(int64_t);

	void setAssetDownload(int64_t id, bool value);
	void setVersionComplete(int64_t id, bool value);

	void removeAsset(Asset *);

	network::Handle *downloadAsset(Asset *);

	void cleanup();

	void handleLibraryLoaded(Vector<Rc<Asset>> &&assets);
	void handleAssetLoaded(Rc<Asset> &&);

	Rc<AssetComponentContainer> _container;

	bool _loaded = false;
	String _rootPath;
	Map<String, Vector<Pair<AssetCallback, Rc<Ref>>>> _callbacks;

	Vector<Rc<Asset>> _liveAssets;
	Map<StringView, Asset *> _assetsByUrl;
	Map<uint64_t, Asset *> _assetsById;

	AppThread *_application = nullptr;
	network::Controller *_controller = nullptr;
	Rc<Server> _server;

	Vector<AssetRequest> _tmpRequests;
	Vector<AssetMultiRequest> _tmpMultiRequest;
};

} // namespace stappler::xenolith::storage

#endif /* XENOLITH_RESOURCES_ASSETS_XLASSETLIBRARY_H_ */
