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

#ifndef EXTRA_DOCUMENT_RICHTEXT_COMMON_XLRTCOMMONSOURCE_H_
#define EXTRA_DOCUMENT_RICHTEXT_COMMON_XLRTCOMMONSOURCE_H_

#include "XLRTSourceAsset.h"
#include "XLEvent.h"
#include "XLScheduler.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::richtext {

struct SP_PUBLIC RendererResource : public Ref {
	~RendererResource();

	Map<String, DocumentAssetMeta> externalAssets;
	Set<String> embeddedAssets;

	Rc<ResourceCache> cache;
	Set<String> textures;
	Map<String, String> svgs;
	Rc<TemporaryResource> resource;
};

class SP_PUBLIC CommonSource : public Ref {
public:
	using StringDocument = document::StringDocument;
	using AssetCallback = Function<void(const Function<void(SourceAsset *)> &)>;

	static EventHeader onError;
	static EventHeader onDocument;
	static EventHeader onUpdate;

	enum Error : int64_t /* event compatible */ {
		DocumentError,
		NetworkError,
	};

	struct NetworkAssetData {
		String originalUrl;
		Binding<SourceAsset> asset;
		DocumentAssetMeta meta;
	};

	struct LocalAssetData {
		String originalUrl;
		Rc<SourceAsset> asset;
		DocumentAssetMeta meta;
	};

	static String getPathForUrl(const String &url);

	virtual ~CommonSource();

	virtual bool init(AssetLibrary *);
	virtual bool init(AssetLibrary *, SourceAsset *, bool enabled = true);

	void setHyphens(font::HyphenMap *);
	font::HyphenMap *getHyphens() const;

	Document *getDocument() const;
	SourceAsset *getAsset() const;
	Asset *getNetworkAsset() const;

	virtual Map<String, DocumentAssetMeta> getExternalAssetMeta() const;
	const Map<String, NetworkAssetData> &getNetworkAssets() const;
	const Set<String> &getEmbeddedAssets() const;

	bool isDirty() const { return _dirty; }
	bool isReady() const;
	bool isActual() const;
	bool isDocumentLoading() const;

	void dropDirty() { _dirty = false; }

	void refresh();

	void setEnabled(bool val);
	bool isEnabled() const;

	virtual void update(const UpdateTime &);

	// Make resources for rendering, or return cached resource
	virtual Rc<RendererResource> prepareResource(ResourceCache *, Document *, Time ctime);

protected:
	virtual void onDocumentAsset(SourceAsset *);
	virtual void onDocumentAssetUpdated(Subscription::Flags);
	virtual void onDocumentLoaded(Document *);

	virtual void acquireNetworkAsset(Document *doc, const StringView &,
			const Function<void(SourceAsset *)> &);
	virtual Rc<SourceAsset> acquireLocalAsset(Document *doc, const StringView &);
	virtual bool isExternalAsset(Document *doc,
			const StringView &); // true is asset is external (not stored in document itself)

	virtual bool onDocumentAssets(Document *doc,
			const Set<String> &); // true if no asset requests is performed
	virtual void onExternalAssetUpdated(NetworkAssetData *, Subscription::Flags);

	virtual bool readExternalAsset(SourceAssetLock *,
			DocumentAssetMeta &) const; // true if asset meta was updated

	virtual void tryLoadDocument(SourceAssetLock *lock);
	virtual void updateDocument();

	bool hasAssetRequests() const;
	void addAssetRequest(NetworkAssetData *);
	void removeAssetRequest(NetworkAssetData *);
	void waitForAssets(Function<void()> &&);

	virtual bool isAssetsSame(const Map<String, DocumentAssetMeta> &,
			const Map<String, DocumentAssetMeta> &);

	Binding<SourceAsset> _documentAsset;
	Rc<Document> _document;

	Map<String, NetworkAssetData> _networkAssets;
	Map<String, LocalAssetData> _localAssets;
	Set<String> _embeddedAssets;

	int64_t _loadedAssetMTime = 0;
	bool _documentLoading = false;
	bool _enabled = true;

	Set<NetworkAssetData *> _assetRequests;
	Vector<Function<void()>> _assetWaiters;

	float _retryUpdate = nan();

	Rc<font::HyphenMap> _hyphens;

	bool _dirty = true;
	AssetLibrary *_assetLibrary = nullptr;

	Rc<RendererResource> _cachedResource;
};

} // namespace stappler::xenolith::richtext

#endif /* EXTRA_DOCUMENT_RICHTEXT_COMMON_XLRTCOMMONSOURCE_H_ */
