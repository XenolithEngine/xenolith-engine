/**
 Copyright (c) 2023-2023 Stappler LLC <admin@stappler.dev>
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

#ifndef EXTRA_DOCUMENT_DOCUMENT_SPDOCUMENT_H_
#define EXTRA_DOCUMENT_DOCUMENT_SPDOCUMENT_H_

#include "SPRef.h"
#include "SPDocStyle.h"
#include "SPMemForwardList.h"

namespace STAPPLER_VERSIONIZED stappler::document {

using NodeId = uint32_t;
constexpr NodeId NodeIdNone = maxOf<NodeId>();

class Node;
class StyleContainer;
class PageContainer;

using StringDocument = ValueWrapper<StringView, class StringDocumentTag>;

class Document;

struct SP_PUBLIC DocumentImage : public memory::AllocPool {
	enum Type {
		Embed, // The image is written inside the document
		Local, // Image on local file system
		Web, // Image available online
	};

	Type type = Local;

	uint32_t width = 0;
	uint32_t height = 0;

	StringView path;
	StringView ref;
	StringView ct;

	BytesView data; // if stored within document

	DocumentImage() = default;

	DocumentImage(uint32_t w, uint32_t h, StringView p, StringView r = StringView())
	: width(w), height(h), path(p.pdup()), ref(r.pdup()) { }
};

struct SP_PUBLIC DocumentFont {
	StringView path;
	StringView ref;
	StringView ct;

	BytesView data; // if stored within document

	DocumentFont(StringView p, StringView r = StringView()) : path(p.pdup()), ref(r.pdup()) { }
};

struct SpineFile {
	StringView file;
	bool linear = true;
};

struct SP_PUBLIC DocumentContentRecord {
	template <typename Value>
	using Vector = typename memory::PoolInterface::VectorType<Value>;

	StringView label;
	StringView href;
	Vector<DocumentContentRecord> childs;
};

struct SP_PUBLIC DocumentData : public memory::AllocPool,
								public InterfaceObject<memory::PoolInterface> {
	memory::pool_t *pool = nullptr;
	uint64_t id;
	StringView uid;
	StringView name;
	StringView type;
	Vector<StringView> strings;
	Vector<MediaQuery> queries;
	Vector<SpineFile> spine;
	Map<StringView, StyleContainer *> styles;
	Map<StringView, PageContainer *> pages;
	Map<StringView, DocumentImage> images;
	Map<StringView, DocumentFont> fonts;
	Map<StringView, StringView> meta;
	DocumentContentRecord tableOfContents;

	NodeId maxNodeId = NodeIdNone;

	virtual ~DocumentData();
	DocumentData(memory::pool_t *, StringView = StringView());

	StringId addString(const StringView &str);
	MediaQueryId addQuery(MediaQuery &&);
};

class SP_PUBLIC Document : public Ref {
public:
	static bool canOpen(FileInfo path, StringView ct = StringView());
	static bool canOpen(BytesView data, StringView ct = StringView());
	static bool canOpen(memory::pool_t *, FileInfo path, StringView ct = StringView());
	static bool canOpen(memory::pool_t *, BytesView data, StringView ct = StringView());

	static Rc<Document> open(FileInfo path, StringView ct = StringView());
	static Rc<Document> open(BytesView data, StringView ct = StringView());
	static Rc<Document> open(memory::pool_t *, FileInfo path, StringView ct = StringView());
	static Rc<Document> open(memory::pool_t *, BytesView data, StringView ct = StringView());

	virtual ~Document();

	virtual bool init();
	virtual bool init(memory::pool_t *);
	virtual bool init(memory::pool_t *, const Callback<DocumentData *(memory::pool_t *)> &);

	virtual StringView getType() const;
	virtual StringView getName() const;
	virtual SpanView<SpineFile> getSpine() const;
	virtual const DocumentContentRecord &getTableOfContents() const;

	virtual StringView getMeta(StringView) const;

	virtual bool isFileExists(StringView) const;
	virtual const DocumentImage *getImage(StringView) const;
	virtual const PageContainer *getContentPage(StringView) const;
	virtual const StyleContainer *getStyleDocument(StringView) const;

	virtual const PageContainer *getRoot() const;

	virtual const Node *getNodeById(StringView pagePath, StringView id) const;
	virtual Pair<const PageContainer *, const Node *> getNodeByIdGlobal(StringView id) const;

	virtual void foreachPage(const Callback<void(StringView, const PageContainer *)> &);

	NodeId getMaxNodeId() const;

	const DocumentData *getData() const { return _data; }

	// Default style, that can be redefined with css
	virtual void beginStyle(StyleList &, const Node &, SpanView<const Node *>,
			const MediaParameters &) const;

	// Default style, that can NOT be redefined with css
	virtual void endStyle(StyleList &, const Node &, SpanView<const Node *>,
			const MediaParameters &) const;

protected:
	virtual DocumentData *allocateData(memory::pool_t *);

	virtual void onStyleAttribute(StyleList &style, StringView tag, StringView name,
			StringView value, const MediaParameters &) const;

	memory::pool_t *_pool = nullptr;
	DocumentData *_data = nullptr;
};

} // namespace stappler::document

#endif /* EXTRA_DOCUMENT_DOCUMENT_SPDOCUMENT_H_ */
