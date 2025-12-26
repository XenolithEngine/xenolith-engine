/**
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

#ifndef CORE_DOCUMENT_EPUB_SPDOCEPUB_H_
#define CORE_DOCUMENT_EPUB_SPDOCEPUB_H_

#include "SPDocument.h"
#include "SPDocPageContainer.h" // IWYU pragma: keep
#include "SPZip.h"
#include "SPMemForwardList.h"

namespace STAPPLER_VERSIONIZED stappler::document {

struct EpubRootNode {
	StringView id;

	EpubRootNode *refinedBy;
};

struct CollectionMeta {
	StringView title;
	StringView type;
	StringView position;
	StringView uid;

	mem_pool::Map<StringView, StringView> localizedTitle;
};

struct TitleMeta {
	enum Type {
		Main,
		Subtitle,
		Short,
		Collection,
		Edition,
		Expanded
	};

	StringView title;
	mem_pool::Map<StringView, StringView> localizedTitle;

	int64_t sequence = 0;
	Type type = Main;
};

struct AuthorMeta {
	enum Type {
		Creator,
		Contributor,
	};

	StringView name;
	Type type = Creator;
	mem_pool::Map<StringView, StringView> localizedName;

	StringView role;
	StringView roleScheme;
};

struct MetaData {
	mem_pool::Vector<TitleMeta> titles;
	mem_pool::Vector<AuthorMeta> authors;
	mem_pool::Vector<CollectionMeta> collections;
};

enum class EpubContentSection {
	None,
	Package,
	Metadata,
	Manifest,
	Spine,
	Other,
};

struct EpubContentNode {
	EpubContentSection section;
	StringView name;
	StringView id;
	StringView type;
	StringView content;
	StringView href;
	memory::forward_list<Pair<StringView, StringView>> attributes;
};

struct EpubArchiveFile {
	// ZIP data
	uint64_t index = 0;
	StringView path;
	size_t size = 0;

	// Manifest data
	StringView id;
	StringView type;
	mem_pool::Set<StringView> props;

	EpubContentNode *node = nullptr;
};

struct SP_PUBLIC EpubData : DocumentData {
	ZipArchive<memory::PoolInterface> archive;
	Map<StringView, EpubArchiveFile> archiveFiles;
	StringView rootPath;

	StringView version;
	StringView coverFile;
	StringView tocFile;

	memory::forward_list<EpubContentNode> epubContent;
	memory::forward_list<EpubContentNode *> epubMetadata;
	memory::forward_list<EpubContentNode *> epubManifest;
	memory::forward_list<EpubContentNode *> epubSpine;
	memory::map<StringView, EpubContentNode *> epubContentById;

	virtual ~EpubData();

	EpubData(memory::pool_t *, FileInfo, StringView = StringView());
	EpubData(memory::pool_t *, BytesView, StringView = StringView());

	bool init();
};

class SP_PUBLIC DocumentEpub : public Document {
public:
	static bool isEpub(FileInfo);
	static bool isEpub(BytesView);

	virtual ~DocumentEpub() = default;

	virtual bool init(FileInfo, StringView ct = StringView());
	virtual bool init(BytesView, StringView ct = StringView());
	virtual bool init(memory::pool_t *, FileInfo, StringView ct = StringView());
	virtual bool init(memory::pool_t *, BytesView, StringView ct = StringView());

protected:
	virtual bool processArchiveFiles(EpubData *);

	virtual void readContentFile(EpubData *, EpubArchiveFile *, StringView);
	virtual void readStyleFile(EpubData *, EpubArchiveFile *, StringView);
};

} // namespace stappler::document

#endif // CORE_DOCUMENT_EPUB_SPDOCEPUB_H_
