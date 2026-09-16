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
		// Destroy before the pool: its child pool is registered under it
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

	// Owner and scene events only; the debounce uses the update tick. No visit
	_systemFlags = SystemFlags::HandleOwnerEvents | SystemFlags::HandleSceneEvents;
	return true;
}

void SearchSystem::handleExit() {
	// Drop the pending request and cancel in-flight ones before the scene goes away
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
		// First tick after queueing: start measuring the wait
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
		// Unknown source: answer at once with an empty result
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
		// No debounce requested, or no update tick to debounce with
		dispatch(request);
		return id;
	}

	// Replaces the waiting request; its callback is dropped, not called with an empty result
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

	// Capturing `this` relies on handleExit cancelling every in-flight request
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

	// Late: a newer query has already been answered
	if (result.generation < _delivered) {
		return;
	}
	_delivered = result.generation;

	callback(sp::move(result));
}

// ---- StaticSearchSource -----------------------------------------------------------------------

StaticSearchSource::~StaticSearchSource() {
	// Release the index before the pool that holds its storage
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

	// Text mode needs the system's configuration, so rebuild after attaching
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

	/* Offsets address the lowercased form; remap them when lowercasing changed the byte length
	(e.g. Turkish dotted capital I). */
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
		// No index; lowered titles only for the whole-string typo fallback
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
			// Node id is the item index, not item.id, which need not be dense or unique
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
			// No configuration to stem with; left unbuilt rather than using a default language
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
				// Title ranks above subtitle, subtitle above body
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

	// By score descending, then by title, so order does not depend on insertion order
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

		// Typo fallback: edit distance of the whole query to the whole title, without ranges
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
		// Negative score: ranks below every direct match
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

				// An expanded query may reach an item twice; keep one hit with the best score
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
			/* Expand only when the exact query found nothing; working queries keep their order. */
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

			/* Highlight by re-stemming the title: stemPhrase yields views into the title, giving
			exact offsets (makeHeadline returns only a marked-up copy). */
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
