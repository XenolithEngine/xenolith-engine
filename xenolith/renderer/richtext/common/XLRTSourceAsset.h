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

#ifndef EXTRA_DOCUMENT_RICHTEXT_COMMON_XLRTSOURCEASSET_H_
#define EXTRA_DOCUMENT_RICHTEXT_COMMON_XLRTSOURCEASSET_H_

#include "XLRichText.h"
#include "XLEvent.h"
#include "SPDocAsset.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::richtext {

using SourceAsset = stappler::document::DocumentAsset;
using SourceAssetLock = stappler::document::DocumentAssetLock;

class SP_PUBLIC SourceMemoryAsset : public SourceAsset {
public:
	virtual ~SourceMemoryAsset() = default;

	virtual bool init(const StringDocument &str, StringView type = StringView());
	virtual bool init(const StringView &str, StringView type = StringView());
	virtual bool init(const BytesView &data, StringView type = StringView());

	virtual Rc<Document> openDocument(const SourceAssetLock *) override;

	virtual StringView getContentType(const SourceAssetLock *) const override { return _type; }

	virtual bool load(const SourceAssetLock *, const Callback<void(BytesView)> &cb) const override;

	virtual bool getImageSize(const SourceAssetLock *, uint32_t &w, uint32_t &h) const override;

protected:
	String _type;
	Bytes _data;
};

class SP_PUBLIC SourceFileAsset : public SourceAsset {
public:
	virtual ~SourceFileAsset() = default;

	virtual bool init(const FileInfo &file, StringView type = StringView());

	virtual Rc<Document> openDocument(const SourceAssetLock *) override;

	virtual int64_t getMTime(const SourceAssetLock *) const override;

	virtual StringView getContentType(const SourceAssetLock *) const override { return _type; }

	virtual bool load(const SourceAssetLock *, const Callback<void(BytesView)> &cb) const override;

	virtual bool getImageSize(const SourceAssetLock *, uint32_t &w, uint32_t &h) const override;

protected:
	String _type;
	String _file;
};

class SP_PUBLIC SourceNetworkAsset : public SourceAsset {
public:
	using AssetCallback = Function<void(const Function<void(Asset *)> &)>;

	virtual ~SourceNetworkAsset() = default;

	virtual bool init(AssetLibrary *, const StringView &url, TimeInterval ttl = TimeInterval());
	virtual bool init(Asset *a);
	virtual bool init(const AssetCallback &cb);

	virtual void setAsset(Asset *a);
	virtual void setAsset(const AssetCallback &cb);

	virtual Asset *getAsset() const;

	virtual Rc<Document> openDocument(const SourceAssetLock *) override;

	virtual int64_t getMTime(const SourceAssetLock *) const override;

	virtual bool download() override;

	virtual bool isDownloadAvailable() const override;
	virtual bool isDownloadInProgress() const override;
	virtual bool isReadAvailable() const override;

	virtual StringView getContentType(const SourceAssetLock *) const override;

	virtual bool load(const SourceAssetLock *, const Callback<void(BytesView)> &cb) const override;

	virtual bool getImageSize(const SourceAssetLock *, uint32_t &w, uint32_t &h) const override;

	virtual void setExtraData(Value &&) override;
	virtual Value getExtraData() const override;

protected:
	virtual Rc<Ref> doLockAsset(int64_t mtime) override;

	virtual void onAsset(Asset *);

	Rc<Asset> _asset;
};

} // namespace stappler::xenolith::richtext

#endif /* EXTRA_DOCUMENT_RICHTEXT_COMMON_XLRTSOURCEASSET_H_ */
