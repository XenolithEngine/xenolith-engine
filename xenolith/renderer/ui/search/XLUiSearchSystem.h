/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

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


#ifndef XENOLITH_RENDERER_UI_SEARCH_XLUISEARCHSYSTEM_H_
#define XENOLITH_RENDERER_UI_SEARCH_XLUISEARCHSYSTEM_H_

#include "XLUiConfig.h"
#include "XLSystem.h"
#include "SPSearchConfiguration.h"
#include "SPSearchFuzzy.h"
#include "SPSearchIndex.h"
#include "SPSearchVocabulary.h"

#include <sprt/cxx/optional>

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

class SearchSystem;

// How a query matches; declared by the source, not the widget.
enum class SearchMatchMode {
	// Typed characters in order within the name (`vtxbuf` finds `VertexBufferPass`)
	Subsequence,

	// Whole words matched as prefixes, weighted by position and tag; for multi-word labels
	Prefix,

	// Full text: stemming, stop words and rank; for descriptions and documentation
	Text,
};

// One answer.
struct SP_PUBLIC SearchHit {
	int64_t id = 0;
	int64_t tag = 0;

	String title;
	String subtitle;

	float score = 0.0f;

	/* Matched fragments of `title` as (start, length) in UTF-16 code units, as used by
	`Label::setTextRangeStyle`. Empty for typo-tolerant matches, which have no position. */
	Vector<Pair<uint32_t, uint32_t>> ranges;

	Value data;
};

struct SP_PUBLIC SearchRequestParams {
	size_t limit = 64;
	float minScore = 0.0f;

	// Applied before scoring, so a rejected item costs nothing.
	Function<bool(int64_t id, int64_t tag)> filter;
};

struct SP_PUBLIC SearchResult {
	String query;

	// Which request this answers; the system already drops results overtaken by a later query.
	uint64_t generation = 0;

	Vector<SearchHit> hits;

	// The source had more to say than `limit` allowed.
	bool partial = false;
};

using SearchCallback = Function<void(SearchResult &&)>;

/** Where results come from. The interface is asynchronous (request handle, cancel, callback) so a
source may answer from another thread, e.g. a database; synchronous sources use the same callback.
A database-backed source needs `stappler_db` and must live outside `xenolith_renderer_ui`. */
class SP_PUBLIC SearchSource : public Ref {
public:
	virtual ~SearchSource() = default;

	virtual bool init(StringView name);

	StringView getName() const { return _name; }

	virtual void handleAttached(SearchSystem *);
	virtual void handleDetached();

	// Starts a request. The returned handle is this source's own and is only meaningful to its
	// `cancel`; zero means the request finished before returning and there is nothing to cancel.
	virtual uint64_t query(StringView, const SearchRequestParams &, SearchCallback &&) = 0;

	virtual void cancel(uint64_t handle);

	// Whether the callback can arrive after `query` returns.
	virtual bool isAsync() const { return false; }

protected:
	String _name;
	SearchSystem *_system = nullptr;
};

/** The shared search configuration (language, stemmers, stop words) and its sources, on the scene.
Each request gets a generation; results overtaken by a later request are dropped. Queries are
debounced on this system's update tick. */
class SP_PUBLIC SearchSystem : public System {
public:
	static constexpr TimeInterval DefaultDebounce = TimeInterval::milliseconds(120);

	// The nearest SearchSystem at or above `node`. Does not reach the opener from a popup
	// subwindow (a separate scene); pass the system down explicitly.
	static SearchSystem *findForNode(Node *);

	// findForNode, and if there is none, installs one on the scene's content node.
	static SearchSystem *acquireForNode(Node *);

	virtual ~SearchSystem();

	virtual bool init() override;

	virtual void handleExit() override;
	virtual void update(const UpdateTime &) override;

	search::Configuration &getConfiguration() { return *_configuration; }
	const search::Configuration &getConfiguration() const { return *_configuration; }

	virtual void setLanguage(search::Language);
	search::Language getLanguage() const;

	virtual bool addSource(Rc<SearchSource> &&);
	virtual bool removeSource(StringView name);
	SearchSource *getSource(StringView name) const;
	SpanView<Rc<SearchSource>> getSources() const { return _sources; }

	virtual void setDebounce(TimeInterval);
	TimeInterval getDebounce() const { return _debounce; }

	/* Asks `sourceName` for `query`; calls back once, or never if a later request overtakes it.
	Returns the request id for `cancel`. An unknown source answers at once with an empty result. */
	virtual uint64_t query(StringView sourceName, StringView, const SearchRequestParams &,
			SearchCallback &&);

	virtual void cancel(uint64_t requestId);

	uint64_t getGeneration() const { return _generation; }

protected:
	struct Request {
		uint64_t id = 0;
		uint64_t generation = 0;
		Rc<SearchSource> source;
		String query;
		SearchRequestParams params;
		SearchCallback callback;
		uint64_t handle = 0;
		bool dispatched = false;
	};

	void dispatch(Request &);
	void handleCompletion(uint64_t requestId, SearchResult &&);

	/* Own root pool for the configuration: it makes a child of the current pool, which during
	init() is a frame pool that dies at frame end. */
	memory::pool_t *_pool = nullptr;
	search::Configuration *_configuration = nullptr;

	Vector<Rc<SearchSource>> _sources;

	TimeInterval _debounce = DefaultDebounce;

	uint64_t _nextId = 1;
	uint64_t _generation = 0;

	// The newest generation a result has already been delivered for. Anything older is late.
	uint64_t _delivered = 0;

	// At most one request waits out the debounce; a new one replaces it.
	sprt::optional<Request> _pending;

	// Stamped on the first update tick after queueing, the only clock this system has.
	uint64_t _pendingSince = 0;

	Vector<Request> _inFlight;
};

// ---- the source that needs nothing but memory -------------------------------------------------

struct SP_PUBLIC SearchItem {
	int64_t id = 0;
	int64_t tag = 0;
	String title;
	String subtitle;

	// Body text searched in SearchMatchMode::Text only.
	String text;

	Value data;
};

/** A synchronous source over an in-memory list. The index is rebuilt whole on change, since
`search::SearchIndex::add` inserts into a sorted vector. */
class SP_PUBLIC StaticSearchSource : public SearchSource {
public:
	virtual ~StaticSearchSource();

	virtual bool init(StringView name) override;
	virtual bool init(StringView name, SearchMatchMode);

	virtual void handleAttached(SearchSystem *) override;

	virtual void setItems(Vector<SearchItem> &&);
	SpanView<SearchItem> getItems() const { return _items; }

	virtual void setMatchMode(SearchMatchMode);
	SearchMatchMode getMatchMode() const { return _mode; }

	/* Per-tag weighting for SearchMatchMode::Prefix; ignored by other modes. A plain callback,
	since `search::SearchIndex::Heuristic` holds pool functions and is built per query. */
	virtual void setTagScore(Function<float(int64_t tag)> &&);

	/* Retry a query with no exact match against words within a small edit distance. Prefix and Text
	expand the query to indexed words and keep highlights; Subsequence compares whole strings and
	reports no highlight. */
	virtual void setTypoTolerance(bool);
	bool isTypoTolerance() const { return _typoTolerance; }

	virtual void setFuzzyConfig(const search::FuzzyConfig &);
	const search::FuzzyConfig &getFuzzyConfig() const { return _fuzzyConfig; }

	virtual uint64_t query(StringView, const SearchRequestParams &, SearchCallback &&) override;

protected:
	void rebuild();
	void doRebuild();
	void querySubsequence(StringView, const SearchRequestParams &, SearchResult &);
	void queryPrefix(StringView, const SearchRequestParams &, SearchResult &);
	void queryText(StringView, const SearchRequestParams &, SearchResult &);

	// (start, length) in UTF-16 units of `title`, from a byte range of its lowercased form.
	static void addLoweredRange(SearchHit &, StringView title, StringView lowered, size_t start,
			size_t length);

	SearchMatchMode _mode = SearchMatchMode::Subsequence;
	bool _typoTolerance = false;
	bool _dirty = true;

	search::FuzzyConfig _fuzzyConfig;

	Vector<SearchItem> _items;

	// Lowercased titles, one per item, kept so a Prefix hit can be mapped back to the original.
	Vector<String> _lowered;

	// Encoded search vectors, one per item, in SearchMatchMode::Text.
	Vector<mem_std::Bytes> _vectors;

	/* Own root pool for the index, whose nodes are pool containers bound to the pool current at
	construction; cleared on each rebuild. */
	memory::pool_t *_pool = nullptr;

	Rc<search::SearchIndex> _index;
	Rc<search::Vocabulary> _vocabulary;
	Function<float(int64_t)> _tagScore;
};

} // namespace stappler::xenolith::ui

#endif /* XENOLITH_RENDERER_UI_SEARCH_XLUISEARCHSYSTEM_H_ */
