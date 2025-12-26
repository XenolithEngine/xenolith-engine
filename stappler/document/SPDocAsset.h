/**
 Copyright (c) 2024 Stappler LLC <admin@stappler.dev>
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

#ifndef CORE_DOCUMENT_SPDOCASSET_H_
#define CORE_DOCUMENT_SPDOCASSET_H_

#include "SPDocument.h"
#include "SPSubscription.h"

namespace STAPPLER_VERSIONIZED stappler::document {

class DocumentAsset;
class DocumentAssetLock;

struct SP_PUBLIC DocumentAssetMeta {
	using String = memory::StandartInterface::StringType;

	int64_t mtime;
	String type;
	Rc<Document> css;
	uint16_t imageWidth;
	uint16_t imageHeight;

	// to prevent asset from removal
	Rc<DocumentAssetLock> lock;

	bool isImage() const;
};

class SP_PUBLIC DocumentAssetLock : public Ref {
public:
	using Bytes = memory::StandartInterface::BytesType;

	template <typename T>
	using Function = memory::StandartInterface::FunctionType<T>;

	virtual ~DocumentAssetLock();

	DocumentAsset *getAsset() const { return _asset; }
	Ref *getLock() const { return _lock; }

	int64_t getMTime() const;
	Rc<Document> openDocument() const;
	StringView getContentType() const;
	bool getImageSize(uint32_t &w, uint32_t &h) const;

	// xenolith-compatible loader
	bool load(const Callback<void(BytesView)> &cb) const;

protected:
	friend class DocumentAsset;

	DocumentAssetLock(Rc<DocumentAsset> &&, Function<void(const DocumentAsset &, Ref *)> &&,
			Rc<Ref> &&lock);

	Rc<DocumentAsset> _asset;
	Function<void(const DocumentAsset &, Ref *)> _releaseFunction;
	Rc<Ref> _lock;
};

class SP_PUBLIC DocumentAsset : public SubscriptionTemplate<memory::StandartInterface> {
public:
	using Value = data::ValueTemplate<memory::StandartInterface>;
	using Bytes = memory::StandartInterface::BytesType;

	using StringDocument = document::StringDocument;
	using Document = document::Document;

	virtual Rc<Document> openDocument(const DocumentAssetLock *) { return nullptr; }

	virtual Rc<DocumentAssetLock> lock(int64_t mtime = 0);

	virtual bool download() { return false; }

	virtual bool isDownloadAvailable() const { return false; }
	virtual bool isDownloadInProgress() const { return false; }
	virtual bool isReadAvailable() const { return true; }

	virtual int64_t getMTime(const DocumentAssetLock *) const { return 0; }

	virtual StringView getContentType(const DocumentAssetLock *) const { return StringView(); }

	virtual bool getImageSize(const DocumentAssetLock *, uint32_t &w, uint32_t &h) const {
		return false;
	}

	virtual bool load(const DocumentAssetLock *, const Callback<void(BytesView)> &cb) const {
		return false;
	}

	virtual void setExtraData(Value &&) { }
	virtual Value getExtraData() const { return Value(); }

protected:
	virtual Rc<Ref> doLockAsset(int64_t mtime) { return this; }
	virtual void doReleaseLock(Ref *) { }
};

} // namespace stappler::document

#endif /* CORE_DOCUMENT_SPDOCASSET_H_ */
