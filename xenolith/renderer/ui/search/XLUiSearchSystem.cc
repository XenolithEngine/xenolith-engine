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


#include "XLUiSearchSystem.h"
#include "XLScene.h"
#include "XLSceneContent.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

// ---- SearchSource -----------------------------------------------------------------------------

bool SearchSource::init(StringView name) {
	if (name.empty()) {
		return false;
	}
	_name = name.str<Interface>();
	return true;
}

void SearchSource::handleAttached(SearchSystem *system) { _system = system; }

void SearchSource::handleDetached() { _system = nullptr; }

void SearchSource::cancel(uint64_t) { }

// ---- SearchSystem -----------------------------------------------------------------------------

SearchSystem *SearchSystem::findForNode(Node *node) {
	while (node) {
		if (auto *search = node->getSystemByType<SearchSystem>()) {
			return search;
		}
		node = node->getParent();
	}
	return nullptr;
}

SearchSystem *SearchSystem::acquireForNode(Node *node) {
	if (auto *search = findForNode(node)) {
		return search;
	}

	if (node) {
		if (auto scene = node->getScene()) {
			if (auto content = scene->getContent()) {
				return content->addSystem(Rc<SearchSystem>::create());
			}
		}
	}

	log::source().warn("ui::SearchSystem",
			"acquireForNode: the node is not in a scene with a content node");
	return nullptr;
}

SearchSystem::~SearchSystem() {
	if (_configuration) {
		// Destroyed before the pool it was built in: its own child pool is registered under this
		// one, and destroying the parent first would leave the destructor freeing memory twice.
		_configuration->~Configuration();
		_configuration = nullptr;
	}
	if (_pool) {
		memory::pool::destroy(_pool);
		_pool = nullptr;
	}
}

bool SearchSystem::init() {
	if (!System::init()) {
		return false;
	}

	_pool = memory::pool::create();
	if (!_pool) {
		return false;
	}

	memory::perform(
			[&] {
		_configuration = new (memory::pool::palloc(_pool, sizeof(search::Configuration)))
				search::Configuration(search::Language::Simple);
	},
			_pool);

	if (!_configuration) {
		return false;
	}

	// Owner and scene events for the lifetime, and the update tick for the debounce. No visit, no
	// node events: this system draws nothing and cares about no geometry.
	_systemFlags = SystemFlags::HandleOwnerEvents | SystemFlags::HandleSceneEvents;
	return true;
}

void SearchSystem::handleExit() {
	// Whatever was waiting out the debounce will never be asked for now, and an in-flight request
	// would call back into a scene that is gone.
	_pending.reset();
	for (auto &it : _inFlight) {
		if (it.source && it.handle) {
			it.source->cancel(it.handle);
		}
	}
	_inFlight.clear();
	unscheduleUpdate();

	System::handleExit();
}

void SearchSystem::update(const UpdateTime &time) {
	System::update(time);

	if (!_pending) {
		unscheduleUpdate();
		return;
	}

	if (_pendingSince == 0) {
		// First tick after the request was queued: this is where the wait starts being measured.
		_pendingSince = time.app;
		return;
	}

	if (time.app - _pendingSince < _debounce.toMicroseconds()) {
		return;
	}

	auto request = sp::move(*_pending);
	_pending.reset();
	unscheduleUpdate();

	dispatch(request);
}

void SearchSystem::setLanguage(search::Language language) {
	_configuration->setLanguage(language);
}

search::Language SearchSystem::getLanguage() const { return _configuration->getLanguage(); }

bool SearchSystem::addSource(Rc<SearchSource> &&source) {
	if (!source || source->getName().empty()) {
		return false;
	}
	if (getSource(source->getName())) {
		log::source().warn("ui::SearchSystem", "addSource: a source named '", source->getName(),
				"' is already registered");
		return false;
	}

	auto ref = source.get();
	_sources.emplace_back(sp::move(source));
	ref->handleAttached(this);
	return true;
}

bool SearchSystem::removeSource(StringView name) {
	for (auto it = _sources.begin(); it != _sources.end(); ++it) {
		if ((*it)->getName() == name) {
			(*it)->handleDetached();
			_sources.erase(it);
			return true;
		}
	}
	return false;
}

SearchSource *SearchSystem::getSource(StringView name) const {
	for (auto &it : _sources) {
		if (it->getName() == name) {
			return it.get();
		}
	}
	return nullptr;
}

void SearchSystem::setDebounce(TimeInterval value) { _debounce = value; }

uint64_t SearchSystem::query(StringView sourceName, StringView queryString,
		const SearchRequestParams &params, SearchCallback &&callback) {
	auto id = _nextId++;
	++_generation;

	auto source = getSource(sourceName);
	if (!source) {
		// Not a crash and not silence: a picker aimed at a source the application has not
		// registered yet shows "nothing found" and keeps working.
		if (callback) {
			SearchResult result;
			result.query = queryString.str<Interface>();
			result.generation = _generation;
			callback(sp::move(result));
		}
		return id;
	}

	Request request;
	request.id = id;
	request.generation = _generation;
	request.source = source;
	request.query = queryString.str<Interface>();
	request.params = params;
	request.callback = sp::move(callback);

	if (_debounce.toMicroseconds() == 0 || !isRunning()) {
		// Nothing to debounce against without an update tick, and a caller that asked for no delay
		// gets none.
		dispatch(request);
		return id;
	}

	// A keystroke replaces whatever was waiting. Its callback is dropped rather than invoked with
	// an empty result: it was overtaken, and an overtaken request has no answer, not a blank one.
	_pendingSince = 0;
	_pending = sp::move(request);
	scheduleUpdate();
	return id;
}

void SearchSystem::cancel(uint64_t requestId) {
	if (_pending && _pending->id == requestId) {
		_pending.reset();
		unscheduleUpdate();
		return;
	}

	for (auto it = _inFlight.begin(); it != _inFlight.end(); ++it) {
		if (it->id == requestId) {
			if (it->source && it->handle) {
				it->source->cancel(it->handle);
			}
			_inFlight.erase(it);
			return;
		}
	}
}

void SearchSystem::dispatch(Request &request) {
	auto id = request.id;
	auto generation = request.generation;

	_inFlight.emplace_back(sp::move(request));
	auto &stored = _inFlight.back();
	stored.dispatched = true;

	// `this` is safe to capture unqualified: handleExit cancels everything in flight, and a source
	// that ignores its cancel is a source that outlives its own system, which nothing here can fix.
	stored.handle = stored.source->query(stored.query, stored.params,
			[this, id, generation](SearchResult &&result) {
		result.generation = generation;
		handleCompletion(id, sp::move(result));
	});

	// A synchronous source has already called back and removed itself from the list by now.
}

void SearchSystem::handleCompletion(uint64_t requestId, SearchResult &&result) {
	SearchCallback callback;

	for (auto it = _inFlight.begin(); it != _inFlight.end(); ++it) {
		if (it->id == requestId) {
			callback = sp::move(it->callback);
			_inFlight.erase(it);
			break;
		}
	}

	if (!callback) {
		return;
	}

	// Late: a newer query has already been answered. Delivering this would move the list backwards
	// to what the user typed two keystrokes ago.
	if (result.generation < _delivered) {
		return;
	}
	_delivered = result.generation;

	callback(sp::move(result));
}

// ---- StaticSearchSource -----------------------------------------------------------------------

StaticSearchSource::~StaticSearchSource() {
	// The index first: its storage lives in the pool, and releasing it afterwards would be a read
	// of memory that is already gone.
	_index = nullptr;
	_vocabulary = nullptr;
	if (_pool) {
		memory::pool::destroy(_pool);
		_pool = nullptr;
	}
}

bool StaticSearchSource::init(StringView name) { return init(name, SearchMatchMode::Subsequence); }

bool StaticSearchSource::init(StringView name, SearchMatchMode mode) {
	if (!SearchSource::init(name)) {
		return false;
	}
	_mode = mode;
	_pool = memory::pool::create();
	return _pool != nullptr;
}

void StaticSearchSource::handleAttached(SearchSystem *system) {
	SearchSource::handleAttached(system);

	// The configuration lives on the system, so a Text index built before attaching would have been
	// built against nothing.
	_dirty = true;
}

void StaticSearchSource::setItems(Vector<SearchItem> &&items) {
	_items = sp::move(items);
	_dirty = true;
}

void StaticSearchSource::setMatchMode(SearchMatchMode mode) {
	if (_mode != mode) {
		_mode = mode;
		_dirty = true;
	}
}

void StaticSearchSource::setTagScore(Function<float(int64_t)> &&cb) { _tagScore = sp::move(cb); }

void StaticSearchSource::setTypoTolerance(bool value) {
	if (_typoTolerance != value) {
		_typoTolerance = value;
		_dirty = true;
	}
}

void StaticSearchSource::setFuzzyConfig(const search::FuzzyConfig &config) {
	_fuzzyConfig = config;
}

void StaticSearchSource::addLoweredRange(SearchHit &hit, StringView title, StringView lowered,
		size_t start, size_t length) {
	if (length == 0) {
		return;
	}

	/* The index lowercases what it is given, so its offsets address the lowered form. For almost
	every string the two have the same byte length and the offsets transfer unchanged; where a
	lowercase mapping changes the encoded length - the Turkish dotted capital I is the everyday
	example - they do not, and an untranslated offset would underline the wrong letters. */
	if (title.size() != lowered.size()) {
		search::Distance alignment(title, lowered);
		if (!alignment.empty()) {
			auto begin = int64_t(start) + alignment.diff_original(start);
			auto end = int64_t(start + length);
			end += alignment.diff_original(size_t(end), true);
			if (end < begin) {
				return;
			}
			start = size_t(begin);
			length = size_t(end - begin);
		}
	}

	if (start > title.size()) {
		return;
	}

	auto utf16Start = search::byteToUtf16Offset(title, start);
	auto utf16End = search::byteToUtf16Offset(title, sprt::min(start + length, title.size()));
	if (utf16End <= utf16Start) {
		return;
	}

	hit.ranges.emplace_back(uint32_t(utf16Start), uint32_t(utf16End - utf16Start));
}

void StaticSearchSource::rebuild() {
	_dirty = false;

	// Released before the pool is cleared: the index's vectors live in it.
	_index = nullptr;
	_vocabulary = nullptr;
	_lowered.clear();
	_vectors.clear();
	memory::pool::clear(_pool);

	if (_items.empty()) {
		return;
	}

	memory::perform([&] { doRebuild(); }, _pool);
}

void StaticSearchSource::doRebuild() {
	switch (_mode) {
	case SearchMatchMode::Subsequence:
		// Nothing to build: the matcher walks the strings themselves. The lowered forms are still
		// wanted for the typo fallback, which compares whole strings.
		if (_typoTolerance) {
			_lowered.reserve(_items.size());
			for (auto &it : _items) {
				_lowered.emplace_back(string::tolower<Interface>(it.title));
			}
		}
		break;

	case SearchMatchMode::Prefix: {
		_index = Rc<search::SearchIndex>::create();
		_index->reserve(_items.size());

		_lowered.reserve(_items.size());
		for (uint32_t i = 0; i < _items.size(); ++i) {
			auto &item = _items[i];
			// The node id is the ITEM INDEX, not the item's own id: the result has to find its way
			// back to the title and the payload, and the caller's id space is not required to be
			// dense or even unique.
			_index->add(item.title, int64_t(i), item.tag);
			_lowered.emplace_back(string::tolower<Interface>(item.title));
		}

		if (_typoTolerance) {
			_vocabulary = Rc<search::Vocabulary>::create();
			for (auto &it : _lowered) {
				StringView(it).split<search::SearchIndex::DefaultSep>(
						[&](StringView word) { _vocabulary->add(word); });
			}
			_vocabulary->build();
		}
		break;
	}

	case SearchMatchMode::Text: {
		if (!_system) {
			// No configuration to stem with. Left unbuilt rather than built with a default one:
			// an index whose language silently differs from the query's is worse than an empty one.
			log::source().warn("ui::StaticSearchSource",
					"Text mode needs the SearchSystem's configuration; add the source to a system "
					"before querying it");
			return;
		}

		auto &cfg = _system->getConfiguration();

		_vectors.reserve(_items.size());
		if (_typoTolerance) {
			_vocabulary = Rc<search::Vocabulary>::create();
		}

		for (auto &item : _items) {
			mem_std::Bytes encoded;
			memory::perform_temporary([&] {
				search::SearchVector vec;
				size_t counter = 0;
				// The title outranks the body: a hit in the name of the thing is a better answer
				// than a hit in a paragraph about it.
				counter = cfg.makeSearchVector(vec, item.title, search::SearchRank::A, counter);
				if (!item.subtitle.empty()) {
					counter = cfg.makeSearchVector(vec, item.subtitle, search::SearchRank::B,
							counter);
				}
				if (!item.text.empty()) {
					counter = cfg.makeSearchVector(vec, item.text, search::SearchRank::D, counter);
				}

				auto data = cfg.encodeSearchVectorData(vec);
				encoded = mem_std::Bytes(data.begin(), data.end());
			});
			_vectors.emplace_back(sp::move(encoded));

			if (_vocabulary) {
				_vocabulary->addPhrase(cfg, item.title);
				if (!item.subtitle.empty()) {
					_vocabulary->addPhrase(cfg, item.subtitle);
				}
				if (!item.text.empty()) {
					_vocabulary->addPhrase(cfg, item.text);
				}
			}
		}

		if (_vocabulary) {
			_vocabulary->build();
		}
		break;
	}
	}
}

uint64_t StaticSearchSource::query(StringView queryString, const SearchRequestParams &params,
		SearchCallback &&callback) {
	if (_dirty) {
		rebuild();
	}

	SearchResult result;
	result.query = queryString.str<Interface>();

	switch (_mode) {
	case SearchMatchMode::Subsequence: querySubsequence(queryString, params, result); break;
	case SearchMatchMode::Prefix: queryPrefix(queryString, params, result); break;
	case SearchMatchMode::Text: queryText(queryString, params, result); break;
	}

	// Descending by score, and by title where the scores tie, so the order of a query does not
	// depend on the order the items happened to be added in.
	sprt::sort(result.hits.begin(), result.hits.end(),
			[](const SearchHit &l, const SearchHit &r) {
		if (l.score != r.score) {
			return l.score > r.score;
		}
		return sprt::unicode::compareCodepoints(StringView(l.title), StringView(r.title)) < 0;
	});

	if (params.limit && result.hits.size() > params.limit) {
		result.hits.resize(params.limit);
		result.partial = true;
	}

	if (callback) {
		callback(sp::move(result));
	}
	return 0; // synchronous: there is nothing left to cancel
}

void StaticSearchSource::querySubsequence(StringView queryString, const SearchRequestParams &params,
		SearchResult &result) {
	search::FuzzyMatch match;

	for (uint32_t i = 0; i < _items.size(); ++i) {
		auto &item = _items[i];
		if (params.filter && !params.filter(item.id, item.tag)) {
			continue;
		}

		search::fuzzyMatch(queryString, item.title, match, _fuzzyConfig);

		if (match.matched) {
			if (float(match.score) < params.minScore) {
				continue;
			}

			SearchHit hit;
			hit.id = item.id;
			hit.tag = item.tag;
			hit.title = item.title;
			hit.subtitle = item.subtitle;
			hit.score = float(match.score);
			hit.data = item.data;

			search::makeHighlightRanges(item.title, match.indices,
					[&](size_t start, size_t length) {
				hit.ranges.emplace_back(uint32_t(start), uint32_t(length));
			});

			result.hits.emplace_back(sp::move(hit));
			continue;
		}

		if (!_typoTolerance || queryString.empty() || i >= _lowered.size()) {
			continue;
		}

		// The fallback: the whole typed string against the whole title. It reports no ranges - an
		// edit distance says how far apart two strings are, not which of their characters agreed.
		auto k = search::Vocabulary::distanceForQuery(queryString);
		if (k == 0) {
			continue;
		}

		auto lowered = string::tolower<Interface>(queryString);
		search::Distance distance(StringView(lowered), StringView(_lowered[i]), k);
		if (distance.distance() > k) {
			continue;
		}

		SearchHit hit;
		hit.id = item.id;
		hit.tag = item.tag;
		hit.title = item.title;
		hit.subtitle = item.subtitle;
		// Below anything the matcher accepted: a corrected guess is an answer of last resort, and
		// it must never outrank a name the user actually typed part of.
		hit.score = -float(distance.distance());
		hit.data = item.data;
		result.hits.emplace_back(sp::move(hit));
	}
}

void StaticSearchSource::queryPrefix(StringView queryString, const SearchRequestParams &params,
		SearchResult &result) {
	if (!_index) {
		return;
	}

	if (queryString.empty()) {
		// An empty query is "no filter", the same as it is in the matcher.
		for (auto &item : _items) {
			if (params.filter && !params.filter(item.id, item.tag)) {
				continue;
			}
			SearchHit hit;
			hit.id = item.id;
			hit.tag = item.tag;
			hit.title = item.title;
			hit.subtitle = item.subtitle;
			hit.data = item.data;
			result.hits.emplace_back(sp::move(hit));
		}
		return;
	}

	memory::perform_temporary([&] {
		// Built here, inside the query's temporary pool: Heuristic holds pool functions.
		search::SearchIndex::Heuristic heuristic;
		if (_tagScore) {
			heuristic.tagScore = [this](int64_t tag) { return _tagScore(tag); };
		}

		auto collect = [&](StringView request) {
			auto found = _index->performSearch(request, 1,
					[&heuristic](const search::SearchIndex &index,
							const search::SearchIndex::ResultNode &node) {
				return heuristic(index, node);
			},
					[&](const search::SearchIndex::Node *node) {
				auto index = size_t(node->id);
				if (index >= _items.size()) {
					return false;
				}
				auto &item = _items[index];
				return !params.filter || params.filter(item.id, item.tag);
			});

			for (auto &node : found.nodes) {
				if (node.score < params.minScore) {
					continue;
				}

				auto index = size_t(node.node->id);
				if (index >= _items.size()) {
					continue;
				}

				auto &item = _items[index];

				// A word may be reached twice once the query has been expanded; the item is one
				// answer either way, and the better score is the one it earned.
				bool merged = false;
				for (auto &existing : result.hits) {
					if (existing.id == item.id && existing.title == item.title) {
						existing.score = sprt::max(existing.score, node.score);
						merged = true;
						break;
					}
				}
				if (merged) {
					continue;
				}

				SearchHit hit;
				hit.id = item.id;
				hit.tag = item.tag;
				hit.title = item.title;
				hit.subtitle = item.subtitle;
				hit.score = node.score;
				hit.data = item.data;

				for (auto &token : node.matches) {
					auto slice = _index->convertToken(*node.node, token);
					addLoweredRange(hit, item.title, _lowered[index], slice.start, slice.size);
				}

				result.hits.emplace_back(sp::move(hit));
			}
		};

		collect(queryString);

		if (_vocabulary && result.hits.empty()) {
			/* Expansion only when the exact request found nothing. A query that already works must
			not have its results reshuffled by guesses about what else it could have been - the
			tolerance is for when a person mistyped, not for when they did not. */
			mem_std::String expanded;
			StringView(queryString).split<search::SearchIndex::DefaultSep>([&](StringView word) {
				auto k = search::Vocabulary::distanceForQuery(word);
				auto lowered = string::tolower<Interface>(word);
				bool first = true;
				_vocabulary->near(StringView(lowered), k, [&](StringView near, uint32_t) {
					if (!first) {
						return;
					}
					first = false;
					if (!expanded.empty()) {
						expanded.append(" ");
					}
					expanded.append(near.data(), near.size());
				});
			});

			if (!expanded.empty()) {
				collect(StringView(expanded));
			}
		}
	});
}

void StaticSearchSource::queryText(StringView queryString, const SearchRequestParams &params,
		SearchResult &result) {
	if (!_system || _vectors.size() != _items.size()) {
		return;
	}

	auto &cfg = _system->getConfiguration();

	memory::perform_temporary([&] {
		auto query = cfg.parseQuery(queryString);
		if (query.empty()) {
			return;
		}

		if (_vocabulary) {
			auto expanded = _vocabulary->expand(query);
			query = sp::move(expanded);
		}

		auto stems = cfg.stemQuery(query);

		for (uint32_t i = 0; i < _items.size(); ++i) {
			auto &item = _items[i];
			if (params.filter && !params.filter(item.id, item.tag)) {
				continue;
			}

			BytesView blob(_vectors[i].data(), _vectors[i].size());
			if (!query.isMatch(blob)) {
				continue;
			}

			auto rank = query.rankQuery(blob, search::Normalization::DocLengthLog);
			if (rank < params.minScore) {
				continue;
			}

			SearchHit hit;
			hit.id = item.id;
			hit.tag = item.tag;
			hit.title = item.title;
			hit.subtitle = item.subtitle;
			hit.score = rank;
			hit.data = item.data;

			/* The highlight comes from stemming the title again rather than from
			`Configuration::makeHeadline`: the headline builder returns a string with markers in it,
			and reading positions back out of a marked-up copy is guesswork. Stemming reports the
			ORIGINAL word as a view into the title, so the range is arithmetic on pointers the
			caller already owns. */
			cfg.stemPhrase(item.title, [&](StringView word, StringView stem, search::ParserToken) {
				for (auto &it : stems) {
					if (StringView(it) == stem) {
						auto start = size_t(word.data() - item.title.data());
						addLoweredRange(hit, item.title, item.title, start, word.size());
						break;
					}
				}
			});

			result.hits.emplace_back(sp::move(hit));
		}
	});
}

} // namespace stappler::xenolith::ui
