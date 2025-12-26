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

#ifndef XENOLITH_RESOURCES_ASSETS_XLASSET_H_
#define XENOLITH_RESOURCES_ASSETS_XLASSET_H_

#include "XLNetworkRequest.h"
#include "XLStorageServer.h"
#include "SPSubscription.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::storage {

class Asset;
class AssetLibrary;
struct AssetDownloadData;

struct SP_PUBLIC AssetVersionData {
	bool complete = false;
	bool download = false; // is download active for file
	uint32_t locked = 0;
	int64_t id = 0;
	Time ctime; // creation time
	Time mtime; // last modification time
	size_t size = 0; // file size
	float progress = 0.0f; // download progress

	String path;
	String contentType;
	String etag;
};

class SP_PUBLIC AssetLock : public Ref {
public:
	virtual ~AssetLock();

	int64_t getId() const { return _lockedVersion.id; }
	Time getCTime() const { return _lockedVersion.ctime; }
	Time getMTime() const { return _lockedVersion.mtime; }
	size_t getSize() const { return _lockedVersion.size; }

	StringView getPath() const { return _lockedVersion.path; }
	StringView getContentType() const { return _lockedVersion.contentType; }
	StringView getEtag() const { return _lockedVersion.etag; }
	StringView getCachePath() const;

	const Rc<Asset> &getAsset() const { return _asset; }

	Ref *getOwner() const { return _owner; }

protected:
	friend class Asset;

	AssetLock(Rc<Asset> &&, const AssetVersionData &, Function<void(const AssetVersionData &)> &&,
			Ref *owner);

	AssetVersionData _lockedVersion;
	Function<void(const AssetVersionData &)> _releaseFunction;
	Rc<Asset> _asset;
	Rc<Ref> _owner;
};

class SP_PUBLIC Asset : public Subscription {
public:
	using VersionData = AssetVersionData;

	enum Update : uint8_t {
		CacheDataUpdated = 1 << 1,
		DownloadStarted = 1 << 2,
		DownloadProgress = 1 << 3,
		DownloadCompleted = 1 << 4,
		DownloadSuccessful = 1 << 5,
		DownloadFailed = 1 << 6,
	};

	Asset(AssetLibrary *, const db::Value &);
	virtual ~Asset();

	Rc<AssetLock> lockVersion(int64_t, Ref *owner = nullptr);
	Rc<AssetLock> lockReadableVersion(Ref *owner = nullptr);

	int64_t getId() const { return _id; }
	StringView getUrl() const { return _url; }
	StringView getCachePath() const { return _cache; }
	Time getTouch() const { return _touch; }
	TimeInterval getTtl() const { return _ttl; }

	AssetLibrary *getLibrary() const { return _library; }

	StringView getContentType() const;

	bool download();
	void touch(Time t = Time::now());
	void clear();

	bool isDownloadAvailable() const;
	bool isDownloadInProgress() const;
	float getProgress() const;

	// or 0 if none
	int64_t getReadableVersionId() const;

	bool isStorageDirty() const { return _dirty; }
	void setStorageDirty(bool value) { _dirty = value; }

	void setData(const Value &d);
	void setData(Value &&d);
	const Value &getData() const { return _data; }

	Value encode() const;

protected:
	friend class AssetLibrary;

	const VersionData *getReadableVersion() const;

	void parseVersions(const db::Value &);
	bool startNewDownload(Time ctime, StringView etag);
	bool resumeDownload(VersionData &);

	void setDownloadProgress(int64_t, float progress);
	void setDownloadComplete(VersionData &, bool success);
	void setFileValidated(bool success);
	void replaceVersion(VersionData &);

	// called from network thread
	void addVersion(AssetDownloadData *);
	void dropVersion(const VersionData &);

	void releaseLock(const VersionData &);

	String _path;
	String _cache;
	String _url;
	TimeInterval _ttl;
	Time _touch;
	Time _mtime;
	int64_t _id = 0;
	int64_t _downloadId = 0;

	Vector<VersionData> _versions;

	Value _data;
	AssetLibrary *_library = nullptr;
	bool _download = false;
	bool _dirty = true;

	mutable Mutex _mutex;
};

} // namespace stappler::xenolith::storage

#endif /* XENOLITH_RESOURCES_ASSETS_XLASSET_H_ */
