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

#include "SPDocEpub.h"
#include "SPDocFormat.h"
#include "SPDocStyleContainer.h"
#include "SPHtmlParser.h"
#include "SPBitmap.h"
#include "html/SPDocHtml.h"

namespace STAPPLER_VERSIONIZED stappler::document {

SP_USED static Format s_epubFormat([](memory::pool_t *, FileInfo str, StringView ct) -> bool {
	return DocumentEpub::isEpub(str);
}, [](memory::pool_t *p, FileInfo str, StringView ct) -> Rc<Document> {
	return Rc<DocumentEpub>::create(p, str, ct);
}, [](memory::pool_t *, BytesView str, StringView ct) -> bool {
	return DocumentEpub::isEpub(str);
}, [](memory::pool_t *p, BytesView str, StringView ct) -> Rc<Document> {
	return Rc<DocumentEpub>::create(p, str, ct);
}, 0);

bool DocumentEpub::isEpub(BytesView data) {
	ZipArchive<memory::StandartInterface> zip(data, true);
	if (!zip) {
		return false;
	}

	bool success = false;
	zip.readFile("mimetype", [&](BytesView data) {
		auto str = data.toStringView();
		if (str.equals(StringView("application/epub+zip"))) {
			success = true;
		}
	});
	return success;
}

bool DocumentEpub::isEpub(FileInfo path) {
	ZipArchive<memory::StandartInterface> zip(path);
	if (!zip) {
		return false;
	}

	bool success = false;
	zip.readFile("mimetype", [&](BytesView data) {
		auto str = data.toStringView();
		if (str.equals(StringView("application/epub+zip"))) {
			success = true;
		}
	});
	return success;
}

bool DocumentEpub::init(FileInfo info, StringView ct) {
	if (!Document::init(memory::app_root_pool, [&](memory::pool_t *pool) -> DocumentData * {
		return new (pool) EpubData(pool, info, ct);
	})) {
		return false;
	}

	if (_data) {
		return memory::perform([&] {
			auto epub = static_cast<EpubData *>(_data);
			if (epub->init()) {
				return processArchiveFiles(epub);
			}
			return false;
		}, _pool);
	}
	return false;
}

bool DocumentEpub::init(BytesView data, StringView ct) {
	if (!Document::init(memory::app_root_pool, [&](memory::pool_t *pool) -> DocumentData * {
		return new (pool) EpubData(pool, data, ct);
	})) {
		return false;
	}

	if (_data) {
		return memory::perform([&] {
			auto epub = static_cast<EpubData *>(_data);
			if (epub->init()) {
				return processArchiveFiles(epub);
			}
			return false;
		}, _pool);
	}
	return false;
}

bool DocumentEpub::init(memory::pool_t *pool, FileInfo info, StringView ct) {
	if (!Document::init(pool, [&](memory::pool_t *pool) -> DocumentData * {
		return new (pool) EpubData(pool, info, ct);
	})) {
		return false;
	}

	if (_data) {
		return memory::perform([&] {
			auto epub = static_cast<EpubData *>(_data);
			if (epub->init()) {
				return processArchiveFiles(epub);
			}
			return false;
		}, _pool);
	}
	return false;
}

bool DocumentEpub::init(memory::pool_t *pool, BytesView data, StringView ct) {
	if (!Document::init(pool, [&](memory::pool_t *pool) -> DocumentData * {
		return new (pool) EpubData(pool, data, ct);
	})) {
		return false;
	}

	if (_data) {
		return memory::perform([&] {
			auto epub = static_cast<EpubData *>(_data);
			if (epub->init()) {
				return processArchiveFiles(epub);
			}
			return false;
		}, _pool);
	}
	return false;
}

bool DocumentEpub::processArchiveFiles(EpubData *epubData) {
	for (auto &it : epubData->archiveFiles) {
		if (it.second.type.starts_with("image/")) {
			epubData->archive.readFile(it.second.index, [&](BytesView data) {
				uint32_t width = 0;
				uint32_t height = 0;
				CoderSource fileSource(data);
				if (bitmap::getImageSize(fileSource, width, height)) {
					auto imgIt = epubData->images
										 .emplace(it.second.path,
												 DocumentImage(width, height, it.second.path))
										 .first;
					imgIt->second.type = DocumentImage::Embed;
					imgIt->second.ct = it.second.type;

					// preserve extracted file in memory
					imgIt->second.data = data.pdup();
				}
			});
		} else if (it.second.type.starts_with("font/")
				|| it.second.type.starts_with("application/font-")
				|| it.second.type.starts_with("application/x-font")) {
			epubData->archive.readFile(it.second.index, [&](BytesView data) {
				auto fontIt =
						epubData->fonts.emplace(it.second.path, DocumentFont(it.second.path)).first;
				fontIt->second.ct = it.second.type;
				fontIt->second.data = data;
			});
		} else if (it.second.type.starts_with("text/html")
				|| it.second.type.starts_with("application/xhtml+xml")) {
			epubData->archive.readFile(it.second.index, [&](BytesView data) {
				readContentFile(epubData, &it.second, data.toStringView());
			});
		} else if (it.second.type.starts_with("text/css")) {
			epubData->archive.readFile(it.second.index, [&](BytesView data) {
				readStyleFile(epubData, &it.second, data.toStringView());
			});
		}
	}

	return true;
}

void DocumentEpub::readContentFile(EpubData *epubData, EpubArchiveFile *file, StringView content) {
	auto page = new (epubData->pool) PageContainer(_data, file->path);

	HtmlReader reader(page);

	html::parse<HtmlReader, StringView, HtmlTag>(reader, content, html::ParserFlags::None);

	page->finalize();

	epubData->pages.emplace(file->path, page);
}

void DocumentEpub::readStyleFile(EpubData *epubData, EpubArchiveFile *file, StringView content) {
	auto page = new (epubData->pool) StyleContainer(_data, StyleContainer::StyleType::Css);

	auto ucontent = StringViewUtf8(content);
	page->readStyle(ucontent);

	epubData->styles.emplace(file->path, page);
}

EpubData::~EpubData() { }

EpubData::EpubData(memory::pool_t *p, FileInfo info, StringView ct)
: DocumentData(p), archive(info) {
	type = ct.pdup(p);
}

EpubData::EpubData(memory::pool_t *p, BytesView data, StringView ct)
: DocumentData(p), archive(data, true) {
	type = ct.pdup(p);
}

static StringView _readEpubRootPath(StringView container) {
	struct EpubXmlContentReader {
		using Parser = html::Parser<EpubXmlContentReader>;
		using Tag = Parser::Tag;
		using StringReader = Parser::StringReader;

		inline void onTagAttribute(Parser &p, Tag &tag, StringReader &name, StringReader &value) {
			if (tag.name.equals("rootfile") && name.equals("full-path")) {
				result = StringView(value);
				p.cancel();
			}
		}

		StringView result;
	} r;

	html::parse(r, StringViewUtf8((const char *)container.data(), container.size()));
	return r.result.pdup();
};

struct EpubContentTag : html::Tag<StringView> {
	EpubContentSection section = EpubContentSection::None;
	EpubContentNode *content = nullptr;
};

static StringView _resolveEpubPath(StringView path, StringView root) {
	if (root.empty()) {
		return path.pdup();
	} else {
		path.skipChars<StringView::Chars<'/'>>();
		root.backwardSkipChars<StringView::Chars<'/'>>();
		return string::pdupString(root, "/", path);
	}
}

struct EpubContentReader {
	using Parser = html::Parser<EpubContentReader, StringView, EpubContentTag>;
	using Tag = EpubContentTag;
	using StringReader = Parser::StringReader;

	EpubContentReader(EpubData *d) : data(d) { }

	void onBeginTag(Parser &p, Tag &tag) {
		EpubContentSection sect = EpubContentSection::None;
		if (!p.tagStack.empty()) {
			sect = p.tagStack.back().section;
		}

		if (tag.name.starts_with<StringCaseComparator>("opf:")) {
			tag.name = tag.name.sub(4);
		}

		switch (sect) {
		case EpubContentSection::None:
			if (tag.name.equals<StringCaseComparator>("package")) {
				tag.section = EpubContentSection::Package;
			}
			return;
			break;
		case EpubContentSection::Package:
			if (tag.name.equals<StringCaseComparator>("metadata")) {
				tag.section = EpubContentSection::Metadata;
			} else if (tag.name.equals<StringCaseComparator>("manifest")) {
				tag.section = EpubContentSection::Manifest;
			} else if (tag.name.equals<StringCaseComparator>("spine")) {
				tag.section = EpubContentSection::Spine;
			}
			return;
			break;
		default: tag.section = sect; break;
		}

		switch (tag.section) {
		case EpubContentSection::Metadata:
			tag.content = &(data->epubContent.emplace_back());
			tag.content->section = tag.section;
			if (tag.name.starts_with<StringCaseComparator>("dc:")) {
				tag.content->name = StringView(tag.name.sub(3)).pdup();
			} else if (tag.name.equals<StringCaseComparator>("meta")) {
				tag.content->name = tag.name.pdup();
			}
			break;
		case EpubContentSection::Manifest:
			tag.content = &data->epubContent.emplace_back();
			tag.content->section = tag.section;
			if (tag.name.equals("item")) {
				tag.content->name = tag.name.pdup();
			}
			break;
		case EpubContentSection::Spine:
			tag.content = &data->epubContent.emplace_back();
			tag.content->section = tag.section;
			if (tag.name.equals("itemref")) {
				tag.content->name = tag.name.pdup();
			}
			break;
		default: break;
		}
	}

	void onTagAttribute(Parser &p, Tag &tag, StringReader &name, StringReader &value) {
		if (name.starts_with<StringCaseComparator>("opf:")) {
			name = name.sub(4);
		}
		if (tag.content && name.equals<StringCaseComparator>("id")) {
			tag.content->id = value.pdup();
		} else if (tag.content && name.equals<StringCaseComparator>("media-type")) {
			tag.content->type = value.pdup();
		} else if (tag.content && name.equals<StringCaseComparator>("href")) {
			tag.content->href = value.pdup();
		} else {
			switch (tag.section) {
			case EpubContentSection::Package:
				if (tag.name.equals<StringCaseComparator>("package")
						|| tag.name.equals<StringCaseComparator>("opf:package")) {
					if (name.equals<StringCaseComparator>("version")) {
						version = StringView(value).pdup();
					} else if (name.equals<StringCaseComparator>("unique-identifier")) {
						uid = StringView(value).pdup();
					}
				} else if (tag.content) {
					tag.content->attributes.emplace_back(name.pdup(), value.pdup());
				}
				break;
			case EpubContentSection::Metadata:
				if (tag.content) {
					tag.content->attributes.emplace_back(name.pdup(), value.pdup());
				}
				break;
			case EpubContentSection::Manifest:
				if (tag.content) {
					tag.content->attributes.emplace_back(name.pdup(), value.pdup());
				}
				break;
			case EpubContentSection::Spine:
				if ((tag.name.equals<StringCaseComparator>("spine"))
						&& (name.equals<StringCaseComparator>("toc"))) {
					tocFile = value.pdup();
				} else if (tag.content) {
					tag.content->attributes.emplace_back(name.pdup(), value.pdup());
				}
				break;
			default: break;
			}
		}
	}

	void onPushTag(Parser &p, Tag &tag) { }

	void onPopTag(Parser &p, Tag &tag) {
		if (tag.content) {
			if (!tag.content->id.empty()) {
				data->epubContentById.emplace(tag.content->id, tag.content);
			}
			switch (tag.content->section) {
			case EpubContentSection::Metadata: data->epubMetadata.emplace_back(tag.content); break;
			case EpubContentSection::Manifest: data->epubManifest.emplace_back(tag.content); break;
			case EpubContentSection::Spine: data->epubSpine.emplace_back(tag.content); break;
			default: break;
			}

			for (auto &it : tag.content->attributes) {
				if (it.first.equals<StringCaseComparator>("content")) {
					if (tag.content->content.empty()) {
						tag.content->content = it.second;
					} else {
						tag.content->content =
								string::pdupString(tag.content->content, " ", it.second);
					}
				}
			}
		}
	}

	inline void onInlineTag(Parser &p, Tag &tag) {
		if (tag.content) {
			if (!tag.content->id.empty()) {
				data->epubContentById.emplace(tag.content->id, tag.content);
			}
			switch (tag.content->section) {
			case EpubContentSection::Metadata: data->epubMetadata.emplace_back(tag.content); break;
			case EpubContentSection::Manifest: data->epubManifest.emplace_back(tag.content); break;
			case EpubContentSection::Spine: data->epubSpine.emplace_back(tag.content); break;
			default: break;
			}

			for (auto &it : tag.content->attributes) {
				if (it.first.equals<StringCaseComparator>("content")) {
					if (tag.content->content.empty()) {
						tag.content->content = it.second;
					} else {
						tag.content->content =
								string::pdupString(tag.content->content, " ", it.second);
					}
				}
			}
		}
	}

	inline void onTagContent(Parser &p, Tag &tag, StringReader &s) {
		if (tag.content) {
			if (tag.content->content.empty()) {
				tag.content->content = s.pdup();
			} else {
				tag.content->content = string::pdupString(tag.content->content, " ", s);
			}
		}
	}

	EpubData *data = nullptr;
	StringView version;
	StringView uid;
	StringView tocFile;
};

static void _epubReadNcxNav(EpubData *data, StringView content, StringView filePath) {
	struct NcxReader {
		using Parser = html::Parser<NcxReader, StringView>;
		using Tag = Parser::Tag;
		using StringReader = Parser::StringReader;

		enum Section {
			None,
			Ncx,
			Head,
			DocTitle,
			NavMap,
			NavPoint,
			NavPointLabel,
		} section = None;

		EpubData *data;
		StringView path;
		mem_pool::Vector<DocumentContentRecord *> contents;

		NcxReader(EpubData *d, StringView path, DocumentContentRecord *c)
		: data(d), path(filepath::root(path)) {
			contents.push_back(c);
		}

		inline void onTagAttribute(Parser &p, Tag &tag, StringReader &name, StringReader &value) {
			switch (section) {
			case NavPoint:
				if (tag.name.equals<StringCaseComparator>("content")
						&& name.equals<StringCaseComparator>("src")) {
					contents.back()->href = _resolveEpubPath(value, path);
				}
				break;
			default: break;
			}
		}

		inline void onPushTag(Parser &p, Tag &tag) {
			switch (section) {
			case None:
				if (tag.name.equals<StringCaseComparator>("ncx")) {
					section = Ncx;
				}
				break;
			case Ncx:
				if (tag.name.equals<StringCaseComparator>("head")) {
					section = Head;
				} else if (tag.name.equals<StringCaseComparator>("doctitle")) {
					section = DocTitle;
				} else if (tag.name.equals<StringCaseComparator>("navmap")) {
					section = NavMap;
				}
				break;
			case Head: break;
			case DocTitle: break;
			case NavMap:
				if (tag.name.equals<StringCaseComparator>("navpoint")) {
					section = NavPoint;
					contents.back()->childs.emplace_back(DocumentContentRecord());
					contents.emplace_back(&contents.back()->childs.back());
				}
				break;
			case NavPoint:
				if (tag.name.equals<StringCaseComparator>("navpoint")) {
					section = NavPoint;
					contents.back()->childs.emplace_back(DocumentContentRecord());
					contents.emplace_back(&contents.back()->childs.back());
				} else if (tag.name.equals<StringCaseComparator>("navlabel")) {
					section = NavPointLabel;
				}
				break;
			default: break;
			}
		}

		inline void onPopTag(Parser &p, Tag &tag) {
			switch (section) {
			case None: break;
			case Ncx:
				if (tag.name.equals<StringCaseComparator>("ncx")) {
					section = None;
				}
				break;
			case Head:
				if (tag.name.equals<StringCaseComparator>("head")) {
					section = Ncx;
				}
				break;
			case DocTitle:
				if (tag.name.equals<StringCaseComparator>("doctitle")) {
					section = Ncx;
				}
				break;
			case NavMap:
				if (tag.name.equals<StringCaseComparator>("navmap")) {
					section = Ncx;
				}
				break;
			case NavPoint:
				if (tag.name.equals<StringCaseComparator>("navpoint")) {
					contents.pop_back();
					if (p.tagStack.at(p.tagStack.size() - 2)
									.name.equals<StringCaseComparator>("navmap")) {
						section = NavMap;
					}
				}
				break;
			case NavPointLabel:
				if (tag.name.equals<StringCaseComparator>("navlabel")) {
					section = NavPoint;
				}
				break;
			default: break;
			}
		}

		inline void onTagContent(Parser &p, Tag &tag, StringReader &s) {
			switch (section) {
			case DocTitle:
			case NavPointLabel:
				if (tag.name.equals<StringCaseComparator>("text")) {
					StringView value(s);
					value.trimChars<StringView::WhiteSpace>();
					if (!value.empty()) {
						contents.back()->label = value.pdup();
					}
				}
				break;
			default: break;
			}
		}
	} r(data, filePath, &data->tableOfContents);

	html::parse<NcxReader, StringView>(r, content);

	if (data->tableOfContents.label.empty()) {
		data->tableOfContents.label = data->name;
	}
}

static void _epubReadXmlNav(EpubData *data, StringView content, StringView filePath) {
	struct TocReader {
		using Parser = html::Parser<TocReader, StringView>;
		using Tag = Parser::Tag;
		using StringReader = Parser::StringReader;

		enum Section {
			None,
			PreNav,
			Nav,
			Heading,
			Ol,
			Li,
		} section = None;

		EpubData *data;
		StringView path;
		mem_pool::Vector<DocumentContentRecord *> contents;

		TocReader(EpubData *d, StringView path, DocumentContentRecord *c)
		: data(d), path(filepath::root(path)) {
			contents.push_back(c);
		}

		inline void onTagAttribute(Parser &p, Tag &tag, StringReader &name, StringReader &value) {
			switch (section) {
			case None:
				if (tag.name.equals<StringCaseComparator>("nav")
						&& name.equals<StringCaseComparator>("epub:type")
						&& value.equals<StringCaseComparator>("toc")) {
					section = PreNav;
				}
				break;
			case Li:
				if (tag.name.equals<StringCaseComparator>("a")
						&& name.equals<StringCaseComparator>("href")) {
					contents.back()->href = _resolveEpubPath(value, path);
				}
				break;
			case Heading:
				if (name.equals<StringCaseComparator>("title")
						|| name.equals<StringCaseComparator>("alt")) {
					value.trimChars<StringView::WhiteSpace>();
					if (contents.back()->label.empty()) {
						contents.back()->label = value.pdup();
					} else {
						contents.back()->label =
								string::pdupString(contents.back()->label, " ", value);
					}
				}
				break;
			default: break;
			}
		}

		inline void onPushTag(Parser &p, Tag &tag) {
			switch (section) {
			case PreNav:
				if (tag.name.equals<StringCaseComparator>("nav")) {
					section = Nav;
				}
				break;
			case Nav:
				if (tag.name.equals<StringCaseComparator>("h1")
						|| tag.name.equals<StringCaseComparator>("h2")
						|| tag.name.equals<StringCaseComparator>("h3")
						|| tag.name.equals<StringCaseComparator>("h4")
						|| tag.name.equals<StringCaseComparator>("h5")
						|| tag.name.equals<StringCaseComparator>("h6")) {
					section = Heading;
				} else if (tag.name.equals<StringCaseComparator>("ol")) {
					section = Ol;
				}
				break;
			case Ol:
				if (tag.name.equals<StringCaseComparator>("li")) {
					section = Li;
					contents.back()->childs.emplace_back(DocumentContentRecord());
					contents.emplace_back(&contents.back()->childs.back());
				}
			case Li:
				if (tag.name.equals<StringCaseComparator>("a")
						|| tag.name.equals<StringCaseComparator>("span")) {
					section = Heading;
				} else if (tag.name.equals<StringCaseComparator>("ol")) {
					section = Ol;
				}
				break;
			default: break;
			}
		}

		inline void onPopTag(Parser &p, Tag &tag) {
			switch (section) {
			case Nav:
				if (tag.name.equals<StringCaseComparator>("nav")) {
					section = None;
				}
				break;
			case Heading:
			case Ol:
				if (tag.name.equals<StringCaseComparator>("h1")
						|| tag.name.equals<StringCaseComparator>("h2")
						|| tag.name.equals<StringCaseComparator>("h3")
						|| tag.name.equals<StringCaseComparator>("h4")
						|| tag.name.equals<StringCaseComparator>("h5")
						|| tag.name.equals<StringCaseComparator>("h6")
						|| tag.name.equals<StringCaseComparator>("ol")
						|| tag.name.equals<StringCaseComparator>("a")
						|| tag.name.equals<StringCaseComparator>("span")) {
					auto &last = p.tagStack.at(p.tagStack.size() - 2);
					if (last.name.equals<StringCaseComparator>("nav")) {
						section = Nav;
					} else if (last.name.equals<StringCaseComparator>("li")) {
						section = Li;
					}
				}
				break;
			case Li:
				if (tag.name.equals<StringCaseComparator>("li")) {
					contents.pop_back();
					section = Ol;
				}
				break;
			default: break;
			}
		}

		inline void onTagContent(Parser &p, Tag &tag, StringReader &s) {
			switch (section) {
			case Heading:
				s.trimChars<StringView::WhiteSpace>();
				if (contents.back()->label.empty()) {
					contents.back()->label = s.pdup();
				} else {
					contents.back()->label = string::pdupString(contents.back()->label, " ", s);
				}
				break;
			default: break;
			}
		}
	} r(data, filePath, &data->tableOfContents);

	html::parse(r, content);

	if (data->tableOfContents.label.empty()) {
		data->tableOfContents.label = data->name;
	}
}

static void _processRootEpubPublication(EpubData *data, StringView content, StringView rootPath) {
	EpubContentReader r(data);

	html::parse(r, content);

	data->version = r.version;

	StringView cover;

	for (auto &it : data->epubMetadata) {
		StringView metaName = it->name;
		for (auto aIt : it->attributes) {
			if (aIt.first.equals<StringCaseComparator>("name")) {
				metaName = aIt.second;
			}
		}

		if (!it->content.empty()) {
			if (metaName.equals<StringCaseComparator>("title")) {
				data->name = it->content;
			} else if (metaName.equals<StringCaseComparator>("identifier") && it->id == r.uid) {
				data->uid = it->content;
			} else if (metaName.equals<StringCaseComparator>("cover")) {
				cover = it->content;
			}

			auto mIt = data->meta.find(metaName);
			if (mIt == data->meta.end()) {
				data->meta.emplace(metaName, it->content);
			} else {
				mIt->second = string::pdupString(mIt->second, " ", it->content);
			}
		}
	}

	for (auto &it : data->epubManifest) {
		if (it->href.empty()) {
			continue;
		}

		auto filename = _resolveEpubPath(it->href, rootPath);
		auto fileIt = data->archiveFiles.find(filename);
		if (fileIt == data->archiveFiles.end()) {
			slog().error("EpubDocument", "Fail to locate file: ", filename);
			continue;
		}

		it->href = filename;

		if (!it->type.empty()) {
			fileIt->second.type = it->type;
		}
		if (!it->id.empty()) {
			fileIt->second.id = it->id;
		}
	}

	data->spine.reserve(data->epubSpine.size());
	for (auto &it : data->epubSpine) {
		StringView idref;
		bool linear = true;

		for (auto &aIt : it->attributes) {
			if (aIt.first.equals<StringCaseComparator>("idref")) {
				idref = aIt.second;
			} else if (aIt.first.equals<StringCaseComparator>("linear")) {
				if (aIt.second.equals<StringCaseComparator>("no")) {
					linear = false;
				}
			}
		}

		if (!idref.empty()) {
			auto vIt = data->epubContentById.find(idref);
			if (vIt == data->epubContentById.end()) {
				slog().error("EpubDocument", "Fail to locate itemref: ", idref);
				continue;
			}

			auto fileIt = data->archiveFiles.find(vIt->second->href);
			if (fileIt == data->archiveFiles.end()) {
				slog().error("EpubDocument", "Fail to locate file: ", vIt->second->href);
				continue;
			}

			data->spine.emplace_back(SpineFile(fileIt->first, linear));
		}
	}

	auto coverIt = data->epubContentById.find(cover);
	if (coverIt != data->epubContentById.end()) {
		data->coverFile = coverIt->second->href;
	} else {
		slog().error("EpubDocument", "Fail to locate cover file: ", cover);
	}

	auto tocIt = data->epubContentById.find(r.tocFile);
	if (tocIt != data->epubContentById.end()) {
		data->tocFile = tocIt->second->href;
	} else {
		slog().error("EpubDocument", "Fail to locate toc file: ", r.tocFile);
	}
}

bool EpubData::init() {
	if (!archive) {
		return false;
	}

	archive.ftw([&](uint64_t index, StringView path, size_t size, Time time) {
		path = path.pdup();
		if (path == "META-INF/container.xml") {
			archive.readFile(index, [&](BytesView data) {
				rootPath = _readEpubRootPath(data.readString());
				rootPath = rootPath.pdup(pool);
			});
		}

		archiveFiles.emplace(path,
				EpubArchiveFile{
					index,
					path,
					size,
				});
	});

	if (rootPath.empty()) {
		return false;
	}

	auto fIt = archiveFiles.find(rootPath);
	if (fIt == archiveFiles.end()) {
		return false;
	}

	archive.readFile(fIt->second.index, [&](BytesView data) {
		_processRootEpubPublication(this, data.toStringView(), filepath::root(rootPath));
	});

	if (!tocFile.empty()) {
		auto tocFileIt = archiveFiles.find(tocFile);
		if (tocFileIt != archiveFiles.end()) {
			auto fileType = tocFileIt->second.type;

			if (fileType.equals("application/x-dtbncx+xml")) {
				archive.readFile(tocFileIt->second.index, [&](BytesView data) {
					_epubReadNcxNav(this, data.toStringView(), tocFile);
				});
			} else if (fileType.equals("application/xhtml+xml")
					|| fileType.equals("application/xhtml") || fileType.equals("text/html")) {
				archive.readFile(tocFileIt->second.index, [&](BytesView data) {
					_epubReadXmlNav(this, data.toStringView(), tocFile);
				});
			} else {
				slog().error("EpubData", "Unknown table-of-contents file type: ", fileType);
			}
		}
	}

	return true;
}

} // namespace stappler::document
