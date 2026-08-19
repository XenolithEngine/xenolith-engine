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

// How a source decides what a query matches. It is the SOURCE that declares this, not the widget:
// the right comparison is a property of what is being searched, and a picker over component names
// and a picker over documentation should not have to be two widgets to get two answers.
enum class SearchMatchMode {
	// The typed characters, read out of the name in order (`vtxbuf` finds `VertexBufferPass`).
	// For identifiers and short labels, where a person types an abbreviation rather than a word.
	Subsequence,

	// Whole words, each matched as a prefix, weighted by position and by the item's tag. For
	// multi-word labels where the words are words and their order carries meaning.
	Prefix,

	// Stemming, stop words and rank: the full-text path. For descriptions and documentation, where
	// "rendering" has to find "render" and "the" has to find nothing.
	Text,
};

// One answer.
struct SP_PUBLIC SearchHit {
	int64_t id = 0;
	int64_t tag = 0;

	String title;
	String subtitle;

	float score = 0.0f;

	/* The matched fragments of `title`, as (start, length) in UTF-16 CODE UNITS - the units
	`Label::setTextRangeStyle` counts in, and the reason this is computed here rather than at each
	call site. A range may be empty for a hit that matched without being able to say WHERE: an edit
	distance answers "how far apart", not "which characters", and inventing a highlight for it would
	underline the wrong letters. */
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

	// Which request this answers. A caller that only wants the newest can compare, but does not
	// have to: the system already drops results overtaken by a later query.
	uint64_t generation = 0;

	Vector<SearchHit> hits;

	// The source had more to say than `limit` allowed.
	bool partial = false;
};

using SearchCallback = Function<void(SearchResult &&)>;

/** Where results come from.

The abstraction exists for one concrete second implementation, not for symmetry: a source backed by
`xenolith::storage::Server` answers from a database on its own thread, and that is what dictates the
shape here - a request identity, a cancel, and a callback instead of a return value. A synchronous
source answers through the same callback rather than being given a shortcut, so that swapping one
for the other is a line in the application and nothing in the widget.

That source cannot live in this module: it needs `stappler_db`, and `xenolith_renderer_ui` must not.
It belongs in a module of its own on top of `xenolith_resources_storage`, defining a scheme with a
`db::Field::FullTextView` bound to this system's `search::Configuration`, and turning a query into
`Query::select(field, vocabulary.expand(cfg.parseQuery(input)))` ordered by the emitted rank column. */
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

	// Whether the callback can arrive after `query` returns. A synchronous source still calls back,
	// but a caller that has to keep something alive across the wait can tell the difference.
	virtual bool isAsync() const { return false; }

protected:
	String _name;
	SearchSystem *_system = nullptr;
};

/** The search configuration and the sources built on it, as one object on the scene.

WHAT IT OWNS THAT A SOURCE CANNOT.

The `search::Configuration` - language, stemmers, stop words - is shared. Two sources that disagree
about what a word IS would answer the same typing differently for no reason a user could see, and a
per-source configuration is the way that happens by accident.

Ordering. Every request gets a generation, and a result overtaken by a later one is dropped instead
of being handed to the widget. Without this, a slow answer to a short query lands after a fast answer
to a long one and the list flickers backwards - the standard failure of every typeahead, and not
something each widget should have to rediscover.

Timing. Typing produces a query per keystroke; the debounce collapses them. It runs off this system's
own update tick, so there is no timer to own and nothing to cancel when the scene goes away. */
class SP_PUBLIC SearchSystem : public System {
public:
	static constexpr TimeInterval DefaultDebounce = TimeInterval::milliseconds(120);

	// The nearest SearchSystem at or above `node`.
	//
	// NOT usable from inside a popup: a subwindow is a scene of its own with nothing of the opener
	// above it. A widget that opens one passes the system down by value.
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

	/* Asks `sourceName` for `query`, and calls back with the result once - unless a later request
	overtakes this one, in which case it never calls back at all.

	Returns the request id, which `cancel` takes. An unknown source is not an error worth a crash:
	it calls back at once with an empty result, because a picker pointed at a source that has not
	been registered yet should show "nothing" and not stop working. */
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

	/* The configuration lives in a pool this system OWNS, and is built inside it.

	`search::Configuration` creates its pool as a child of whatever pool is current when it is
	constructed. For an object on the scene that is the frame that happened to be running during
	init(), and a child pool cannot outlive its parent: the configuration's internals are freed the
	moment that frame ends, and the next query reads them. A pool with its own allocator has no
	parent to be outlived by. */
	memory::pool_t *_pool = nullptr;
	search::Configuration *_configuration = nullptr;

	Vector<Rc<SearchSource>> _sources;

	TimeInterval _debounce = DefaultDebounce;

	uint64_t _nextId = 1;
	uint64_t _generation = 0;

	// The newest generation a result has already been delivered for. Anything older is late.
	uint64_t _delivered = 0;

	// At most one request waits out the debounce: a keystroke replaces whatever was waiting, which
	// is the whole point of debouncing.
	sprt::optional<Request> _pending;

	// Stamped on the first update tick after the request was queued, not when it was queued: this
	// system has no clock of its own, and the tick is the only time it is handed one.
	uint64_t _pendingSince = 0;

	Vector<Request> _inFlight;
};

// ---- the source that needs nothing but memory -------------------------------------------------

struct SP_PUBLIC SearchItem {
	int64_t id = 0;
	int64_t tag = 0;
	String title;
	String subtitle;

	// Only read in SearchMatchMode::Text: the body a full-text query searches, as opposed to the
	// name it displays.
	String text;

	Value data;
};

/** A source over a list held in memory.

Answers synchronously - through the callback, like any other source. Rebuilt whole rather than
incrementally, because `search::SearchIndex::add` inserts into a sorted vector and an incremental
update of a large index costs more than rebuilding it. */
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

	/* Per-tag weighting for SearchMatchMode::Prefix, so a source can say "a component outranks a
	comment". Ignored by the other modes.

	A callback rather than a whole `search::SearchIndex::Heuristic`: that struct's members are
	POOL functions, and holding one in a refcounted object would bind it to whichever pool happened
	to be current when the source was created. The heuristic is assembled inside the query's own
	temporary pool instead, where it belongs. */
	virtual void setTagScore(Function<float(int64_t tag)> &&);

	/* Whether a query that matches nothing exactly is retried against words within an edit or two.

	What it does depends on the mode, because the modes have different things to be tolerant WITH:
	Prefix and Text expand the query into the words the index actually holds, and keep their
	highlights; Subsequence falls back to comparing the whole string and reports NO highlight,
	because an edit distance cannot say which characters matched. */
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

	/* The index is built in a pool this source OWNS.

	`search::SearchIndex` is refcounted but its nodes and tokens are POOL containers, bound to
	whatever pool was current when it was constructed. Built in the ambient pool, an index outlives
	its own storage: it survives as an object and reads freed memory as data. A pool with its own
	allocator has no parent frame to be outlived by, and clearing it between rebuilds is what keeps
	a source that is re-populated often from growing without bound. */
	memory::pool_t *_pool = nullptr;

	Rc<search::SearchIndex> _index;
	Rc<search::Vocabulary> _vocabulary;
	Function<float(int64_t)> _tagScore;
};

} // namespace stappler::xenolith::ui

#endif /* XENOLITH_RENDERER_UI_SEARCH_XLUISEARCHSYSTEM_H_ */
