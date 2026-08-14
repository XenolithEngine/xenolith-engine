/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
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

#include "XLFontController.h"

#include "XLAppThread.h"
#include "XLCoreAttachment.h"
#include "SPFilesystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::font {

XL_DECLARE_EVENT_CLASS(FontController, onLoaded)
XL_DECLARE_EVENT_CLASS(FontController, onFontSourceUpdated)

struct FontController::Builder::Data {
	String name;
	Rc<FontController> target;
	Map<String, FontSource> dataQueries;
	Map<String, FamilyQuery> familyQueries;
	Map<String, String> aliases;
};

FontController::Builder::~Builder() {
	if (_data) {
		sprt::__delete(_data);
		_data = nullptr;
	}
}

FontController::Builder::Builder(StringView name) {
	_data = new (sprt::nothrow) Data();
	_data->name = name.str<Interface>();
}

FontController::Builder::Builder(FontController *target) {
	_data = new (sprt::nothrow) Data();
	_data->target = target;
}

FontController::Builder::Builder(Builder &&other) {
	_data = other._data;
	other._data = nullptr;
}

FontController::Builder &FontController::Builder::operator=(Builder &&other) {
	if (_data) {
		sprt::__delete(_data);
		_data = nullptr;
	}

	_data = other._data;
	other._data = nullptr;
	return *this;
}

StringView FontController::Builder::getName() const { return _data->name; }
FontController *FontController::Builder::getTarget() const {
	return _data->target ? _data->target.get() : nullptr;
}

const FontController::FontSource *FontController::Builder::addFontSource(StringView name,
		BytesView data) {
	auto it = _data->dataQueries.find(name);
	if (it == _data->dataQueries.end()) {
		it = _data->dataQueries.emplace(name.str<Interface>(), FontSource()).first;
		it->second.fontExternalData = data;
		it->second.preconfiguredParams = false;
		return &it->second;
	}

	log::source().warn("FontController", "Duplicate font source: ", name);
	return nullptr;
}

const FontController::FontSource *FontController::Builder::addFontSource(StringView name,
		Bytes &&data) {
	auto it = _data->dataQueries.find(name);
	if (it == _data->dataQueries.end()) {
		it = _data->dataQueries.emplace(name.str<Interface>(), FontSource()).first;
		it->second.fontMemoryData = sp::move(data);
		it->second.preconfiguredParams = false;
		return &it->second;
	}

	log::source().warn("FontController", "Duplicate font source: ", name);
	return nullptr;
}

const FontController::FontSource *FontController::Builder::addFontSource(StringView name,
		const FileInfo &data) {
	auto it = _data->dataQueries.find(name);
	if (it == _data->dataQueries.end()) {
		it = _data->dataQueries.emplace(name.str<Interface>(), FontSource()).first;
		filesystem::enumeratePaths(data, filesystem::Access::Read,
				[&](const LocationInfo &, StringView path) {
			it->second.fontFilePath = path.str<Interface>();
			return false;
		});
		it->second.preconfiguredParams = false;
		return &it->second;
	}

	log::source().warn("FontController", "Duplicate font source: ", name);
	return nullptr;
}

const FontController::FontSource *FontController::Builder::addFontSource(StringView name,
		Function<Bytes()> &&cb) {
	auto it = _data->dataQueries.find(name);
	if (it == _data->dataQueries.end()) {
		it = _data->dataQueries.emplace(name.str<Interface>(), FontSource()).first;
		it->second.fontCallback = sp::move(cb);
		it->second.preconfiguredParams = false;
		return &it->second;
	}

	log::source().warn("FontController", "Duplicate font source: ", name);
	return nullptr;
}

const FontController::FontSource *FontController::Builder::addFontSource(StringView name,
		BytesView data, FontLayoutParameters params) {
	auto it = _data->dataQueries.find(name);
	if (it == _data->dataQueries.end()) {
		it = _data->dataQueries.emplace(name.str<Interface>(), FontSource()).first;
		it->second.fontExternalData = data;
		it->second.params = params;
		it->second.preconfiguredParams = true;
		return &it->second;
	}

	log::source().warn("FontController", "Duplicate font source: ", name);
	return nullptr;
}

const FontController::FontSource *FontController::Builder::addFontSource(StringView name,
		Bytes &&data, FontLayoutParameters params) {
	auto it = _data->dataQueries.find(name);
	if (it == _data->dataQueries.end()) {
		it = _data->dataQueries.emplace(name.str<Interface>(), FontSource()).first;
		it->second.fontMemoryData = sp::move(data);
		it->second.params = params;
		it->second.preconfiguredParams = true;
		return &it->second;
	}

	log::source().warn("FontController", "Duplicate font source: ", name);
	return nullptr;
}

const FontController::FontSource *FontController::Builder::addFontSource(StringView name,
		const FileInfo &data, FontLayoutParameters params) {
	auto it = _data->dataQueries.find(name);
	if (it == _data->dataQueries.end()) {
		it = _data->dataQueries.emplace(name.str<Interface>(), FontSource()).first;
		filesystem::enumeratePaths(data, filesystem::Access::Read,
				[&](const LocationInfo &, StringView path) {
			it->second.fontFilePath = path.str<Interface>();
			return false;
		});
		it->second.params = params;
		it->second.preconfiguredParams = true;
		return &it->second;
	}

	log::source().warn("FontController", "Duplicate font source: ", name);
	return nullptr;
}

const FontController::FontSource *FontController::Builder::addFontSource(StringView name,
		Function<Bytes()> &&cb, FontLayoutParameters params) {
	auto it = _data->dataQueries.find(name);
	if (it == _data->dataQueries.end()) {
		it = _data->dataQueries.emplace(name.str<Interface>(), FontSource()).first;
		it->second.fontCallback = sp::move(cb);
		it->second.params = params;
		it->second.preconfiguredParams = true;
		return &it->second;
	}

	log::source().warn("FontController", "Duplicate font source: ", name);
	return nullptr;
}

const FontController::FontSource *FontController::Builder::getFontSource(StringView name) const {
	auto it = _data->dataQueries.find(name);
	if (it != _data->dataQueries.end()) {
		return &it->second;
	}
	return nullptr;
}

const FontController::FamilyQuery *FontController::Builder::addFontFaceQuery(StringView family,
		const FontSource *source, bool front) {
	XL_ASSERT(source, "Source should not be nullptr");

	auto it = _data->familyQueries.find(family);
	if (it == _data->familyQueries.end()) {
		it = _data->familyQueries
					 .emplace(family.str<Interface>(), FamilyQuery{family.str<Interface>()})
					 .first;
	}

	addSources(&it->second, Vector<const FontSource *>{source}, front);
	return &it->second;
}

const FontController::FamilyQuery *FontController::Builder::addFontFaceQuery(StringView family,
		Vector<const FontSource *> &&sources, bool front) {
	auto it = _data->familyQueries.find(family);
	if (it == _data->familyQueries.end()) {
		it = _data->familyQueries
					 .emplace(family.str<Interface>(), FamilyQuery{family.str<Interface>()})
					 .first;
	}

	addSources(&it->second, sp::move(sources), front);
	return &it->second;
}

bool FontController::Builder::addAlias(StringView newAlias, StringView familyName) {
	auto iit = _data->aliases.find(familyName);
	if (iit != _data->aliases.end()) {
		_data->aliases.insert_or_assign(newAlias.str<Interface>(), iit->second);
		return true;
	} else {
		// check if family defined
		for (auto &it : _data->familyQueries) {
			if (it.second.family == familyName) {
				_data->aliases.insert_or_assign(newAlias.str<Interface>(), it.second.family);
				return true;
			}
		}
		return false;
	}
}

Vector<const FontController::FamilyQuery *> FontController::Builder::getFontFamily(
		StringView family) const {
	Vector<const FontController::FamilyQuery *> families;
	for (auto &it : _data->familyQueries) {
		if (it.second.family == family) {
			families.emplace_back(&it.second);
		}
	}
	return families;
}

Map<String, FontController::FontSource> &FontController::Builder::getDataQueries() {
	return _data->dataQueries;
}

Map<String, FontController::FamilyQuery> &FontController::Builder::getFamilyQueries() {
	return _data->familyQueries;
}

Map<String, String> &FontController::Builder::getAliases() { return _data->aliases; }

void FontController::Builder::addSources(FamilyQuery *query, Vector<const FontSource *> &&sources,
		bool front) {
	if (query->sources.empty() || !front) {
		query->sources.reserve(query->sources.size() + sources.size());
		for (auto &iit : sources) {
			XL_ASSERT(iit, "Source should not be nullptr");
			if (sprt::find(query->sources.begin(), query->sources.end(), iit)
					== query->sources.end()) {
				query->sources.emplace_back(iit);
			}
		}
	} else {
		auto iit = query->sources.begin();
		while (iit != query->sources.end()) {
			if (sprt::find(sources.begin(), sources.end(), *iit) != sources.end()) {
				iit = query->sources.erase(iit);
			} else {
				++iit;
			}
		}

		query->sources.reserve(query->sources.size() + sources.size());

		auto insertIt = query->sources.begin();
		for (auto &source : sources) {
			XL_ASSERT(source, "Source should not be nullptr");
			if (sprt::find(query->sources.begin(), query->sources.end(), source)
					== query->sources.end()) {
				query->sources.emplace(insertIt, source);
			}
		}
	}
	query->addInFront = front;
}

void FontController::extend(AppThread *app, const Callback<bool(FontController::Builder &)> &cb) {
	Builder builder(this);
	if (cb(builder)) {
		applyBuilder(app, move(builder));
	}
}

void FontController::addFont(StringView family, Rc<FontFaceData> &&data, bool front) {
	sprt::unique_lock lock(_layoutSharedMutex);
	auto familyIt = _families.find(family);
	if (familyIt == _families.end()) {
		familyIt = _families.emplace(family.str<Interface>(), FamilySpec()).first;
		_familiesNames.emplace_back(familyIt->first);
	}

	if (!familyIt->second.data.empty() && front) {
		familyIt->second.data.emplace(familyIt->second.data.begin(), move(data));
	} else {
		familyIt->second.data.emplace_back(move(data));
	}

	_dirty = true;
	lock.unlock();
}

void FontController::addFont(StringView family, Vector<Rc<FontFaceData>> &&data, bool front) {
	sprt::unique_lock lock(_layoutSharedMutex);
	auto familyIt = _families.find(family);
	if (familyIt == _families.end()) {
		familyIt = _families.emplace(family.str<Interface>(), FamilySpec()).first;
		_familiesNames.emplace_back(familyIt->first);
	}

	if (familyIt->second.data.empty()) {
		familyIt->second.data = sp::move(data);
	} else {
		if (front) {
			for (auto &it : data) {
				familyIt->second.data.emplace(familyIt->second.data.begin(), move(it));
			}
		} else {
			for (auto &it : data) { familyIt->second.data.emplace_back(move(it)); }
		}
	}

	_dirty = true;
	lock.unlock();
}

bool FontController::addAlias(StringView newAlias, StringView familyName) {
	sprt::unique_lock lock(_layoutSharedMutex);
	if (_aliases.find(newAlias) != _aliases.end()) {
		return false;
	}

	auto iit = _aliases.find(familyName);
	if (iit != _aliases.end()) {
		_aliases.emplace(newAlias.str<Interface>(), iit->second);
		return true;
	} else {
		auto f_it = _families.find(familyName);
		if (f_it != _families.end()) {
			_aliases.emplace(newAlias.str<Interface>(), familyName.str<Interface>());
			return true;
		}
		return false;
	}
}

Rc<FontFaceSet> FontController::getLayout(FontParameters style) {
	Rc<FontFaceSet> ret;

	FontFaceSet *face = nullptr;

	style.fontSize = style.fontSize * style.density;

	// check if layout already loaded
	sprt::shared_lock sharedLock(_layoutSharedMutex);
	if (!_loaded) {
		return nullptr;
	}

	if (style.fontFamily.empty()) {
		style.fontFamily = StringView(_defaultFontFamily);
	}

	auto a_it = _aliases.find(style.fontFamily);
	if (a_it != _aliases.end()) {
		style.fontFamily = a_it->second;
	}

	auto familyIt = _families.find(style.fontFamily);
	if (familyIt == _families.end()) {
		slog().error("FontController", "Font family is not defined: ", style.fontFamily);
		return nullptr;
	}

	// search for exact match
	auto cfgName = FontFaceSet::constructName(style.fontFamily, style);
	auto it = _layouts.find(cfgName);
	if (it != _layouts.end()) {
		face = it->second.get();
	}

	FontSpecializationVector spec;
	if (!face) {
		// find best possible config
		spec = findSpecialization(familyIt->second, style, nullptr);
		cfgName = FontFaceSet::constructName(style.fontFamily, spec);
		auto layoutIt = _layouts.find(cfgName);
		if (layoutIt != _layouts.end()) {
			face = layoutIt->second.get();
		}
	}

	if (face) {
		face->touch(_clock, style.persistent);
		return face;
	}

	// we need to create new layout
	sharedLock.unlock();
	sprt::unique_lock uniqueLock(_layoutSharedMutex);

	Vector<Rc<FontFaceData>> data;

	// update best match (if fonts was updated)
	spec = findSpecialization(familyIt->second, style, &data);
	cfgName = FontFaceSet::constructName(style.fontFamily, spec);

	// check if somebody already created layout for us in another thread
	it = _layouts.find(cfgName);
	if (it != _layouts.end()) {
		it->second->touch(_clock, style.persistent);
		return it->second.get();
	}

	// create layout
	ret = Rc<FontFaceSet>::create(sp::move(cfgName), style.fontFamily, sp::move(spec),
			sp::move(data), _library);
	_layouts.emplace(ret->getName(), ret);
	ret->touch(_clock, style.persistent);

	// some fonts contains preloaded chars - init dependency for renderer to actually preload them
	if (ret->getRequiredCharsCount()) {
		initDependency();
	}

	return ret;
}

Rc<FontFaceSet> FontController::getLayoutForString(const FontParameters &f, const CharVector &str) {
	if (auto l = getLayout(f)) {
		Vector<char32_t> failed;
		if (f.persistent) {
			l->addString(str, failed);
		} else {
			l->addString(str, failed);
		}
		return l;
	}
	return nullptr;
}

Rc<core::DependencyEvent> FontController::addTextureChars(const Rc<FontFaceSet> &l,
		SpanView<CharLayoutData> chars) {
	// The caller needs a gating dependency whenever the glyphs it just laid out may not be in the
	// atlas yet - and that is NOT the same question as "are they new".
	//
	// FontFaceObject::_required is permanent and process-wide, so it reports "new" only to the very
	// first requester of a glyph, ever. Every later caller used to get whatever _dependency happened
	// to be, which is null once the flush handed it over. With one window that is mostly harmless -
	// the same node usually is the first requester. With two windows it is the bug: window A lays
	// the string out, gets the dependency and waits for the atlas; window B lays the SAME string out
	// a tick later, is told nothing is new, waits for nothing, and draws point sprites whose CharId
	// is not in the atlas index yet - they collapse to zero size, i.e. invisible text. Each window
	// must wait for the glyphs it needs; only one of them did.
	//
	// So gate on the atlas being confirmed current instead. Once the atlas has caught up, a
	// known-glyph request opens nothing and returns null.
	//
	// "Caught up" needs both halves. A batch still running means glyphs that are laid out but not
	// rasterised, whatever the generation counter says - batches overlap, and a later one can land
	// first. So: gate while anything is in flight, and gate while new glyphs have appeared since the
	// last quiet moment.
	if (l->addTextureChars(chars)) {
		// Genuinely new characters: they are in _required but in no batch, so the caller must wait
		// for the batch that will carry them.
		++_glyphGeneration;
		initDependency();
		return _dependency;
	}

	if (_uploadsInFlight.load() > 0 || _uploadedGeneration.load() < _glyphGeneration) {
		// Nothing new from this caller, but the atlas is behind. acquireGatingDependency decides
		// whether that means "wait for the batch already running" or "wait for the next one".
		return acquireGatingDependency();
	}
	return nullptr;
}

void FontController::resetSubmittedGlyphs() {
	sprt::shared_lock lock(_layoutSharedMutex);
	for (auto &it : _layouts) {
		for (auto &iit : it.second->getFaces()) {
			if (iit) {
				iit->resetCharsSubmitted();
			}
		}
	}
}

bool FontController::hasPendingGlyphs() const {
	sprt::shared_lock lock(_layoutSharedMutex);
	for (auto &it : _layouts) {
		for (auto &iit : it.second->getFaces()) {
			if (iit && iit->hasPendingChars()) {
				return true;
			}
		}
	}
	return false;
}

Rc<core::DependencyEvent> FontController::acquireGatingDependency() {
	if (!hasPendingGlyphs()) {
		// Everything required has already been sent. Waiting for the batch that carries it is both
		// correct - a flush submits the WHOLE required set, so that batch covers these glyphs - and
		// the only way out of the feedback loop: opening another one here is what kept a successor
		// permanently queued behind the upload in flight.
		if (_submittedDependency) {
			if (!_submittedDependency->isSignaled()) {
				return _submittedDependency;
			}
			// It landed; from here on this controller has nothing outstanding to wait for.
			_submittedDependency = nullptr;
		}

		// A batch is still being accumulated for the next flush (something raised _dirty without
		// requiring a character - a font or an alias was added); it will be submitted, so it is a
		// valid thing to wait on.
		if (_dependency) {
			return _dependency;
		}

		// Nothing pending, nothing in flight: the atlas holds everything and the caller's own
		// generation check will agree on the next frame.
		return nullptr;
	}

	initDependency();
	return _dependency;
}

uint32_t FontController::getFamilyIndex(StringView name) const {
	sprt::shared_lock lock(_layoutSharedMutex);
	auto it = sprt::find(_familiesNames.begin(), _familiesNames.end(), name);
	if (it != _familiesNames.end()) {
		return uint32_t(it - _familiesNames.begin());
	}
	return maxOf<uint32_t>();
}

StringView FontController::getFamilyName(uint32_t idx) const {
	sprt::shared_lock lock(_layoutSharedMutex);
	if (idx < _familiesNames.size()) {
		return _familiesNames[idx];
	}
	return StringView();
}

void FontController::update(AppThread *app, const UpdateTime &clock, bool) {
	_clock = clock.global;
	removeUnusedLayouts();
	flushPendingGlyphs(app);
}

void FontController::flushPendingGlyphs(AppThread *app) {
	if (_uploadFailed.exchange(false)) {
		// A batch never reached the atlas. Its characters are still required and nothing else would
		// resend them - the flush below skips a set that has not grown - so drop the record of the
		// submission and let the next one carry everything again.
		resetSubmittedGlyphs();
		_dirty = true;
	}

	if (_dirty && _loaded) {
		Vector<FontUpdateRequest> objects;

		// Is there anything to send that has not been sent already?
		//
		// The batch below carries the WHOLE required set, because the atlas image is rebuilt from
		// scratch on every upload (FontRenderPassHandle::doPrepareCommands allocates a new image and
		// fills it from the request). So a flush whose set has not grown produces the same atlas
		// again - at the cost of a full rebuild plus a material recompile in every window that
		// samples it. _dirty alone cannot tell the two apart: it is raised by every node that gates
		// a frame while the atlas is behind, which is every node on screen, every frame.
		bool hasPending = false;

		sprt::shared_lock lock(_layoutSharedMutex);
		for (auto &it : _layouts) {
			for (auto &iit : it.second->getFaces()) {
				if (!iit) {
					continue;
				}

				if (iit->hasPendingChars()) {
					hasPending = true;
				}

				auto lb = sprt::lower_bound(objects.begin(), objects.end(), iit,
						[](const FontUpdateRequest &l, FontFaceObject *r) {
					return l.object.get() < r;
				});
				if (lb == objects.end()) {
					auto req = iit->getRequiredChars();
					if (!req.empty()) {
						objects.emplace_back(
								FontUpdateRequest{iit, sp::move(req), it.second->isPersistent()});
					}
				} else if (lb != objects.end() && lb->object != iit) {
					auto req = iit->getRequiredChars();
					if (!req.empty()) {
						objects.emplace(lb,
								FontUpdateRequest{iit, sp::move(req), it.second->isPersistent()});
					}
				}
			}
		}
		// `_dependency` forces the batch through even with nothing new: it has already been handed
		// to callers as the thing they are waiting for, and only a submission can signal it. It is
		// never minted for a set that has not grown (see acquireGatingDependency), so this does not
		// re-open the loop - it only keeps a promise that was made before the set stopped growing.
		if (!objects.empty() && (hasPending || _dependency)) {
			// EVERY flush replaces the atlas instance (FontRenderPassHandle::submitResult ->
			// DynamicImage::updateInstance), and every replacement makes each window recompile the
			// materials that sample it. So every flush needs a gating dependency - including the ones
			// _dirty was raised for by removeUnusedLayouts() or addFont() rather than by a glyph
			// request, which used to be submitted with none at all.
			//
			// An ungated flush is what actually breaks a second window: the atlas is swapped under it
			// and its material recompile is gated on nothing, so it keeps sampling the previous
			// instance and renders only the glyphs that instance happened to hold. Minting the
			// dependency here also puts the batch into _uploadsInFlight below, which is what makes
			// addTextureChars() gate everyone who lays glyphs out while it runs.
			if (!_dependency) {
				_dependency = makeDependency();
			}

			// Hand the batch (+ gating dependency) to the leaf's gAPI endpoint (local: FontComponent ->
			// gl Loop; remote: proxy -> server). The dependency gates the frame that uses these glyphs.
			auto dep = move(_dependency);
			_dependency = nullptr;

			// This batch carries every glyph required so far, so it is the generation the atlas will
			// hold once EVERY outstanding batch has landed - see the field comments.
			const uint64_t submitted = _glyphGeneration;
			auto prevSubmitted = _submittedGeneration.load();
			while (prevSubmitted < submitted
					&& !_submittedGeneration.compare_exchange_weak(prevSubmitted, submitted)) { }

			if (dep) {
				if (dep->isSignaled()) {
					// No queues to wait on: FontControllerRemote mints such an event because the
					// server gates on its own mirror event instead, and nothing will ever signal it
					// here. Waiting on it locally is a no-op, so treat the batch as confirmed at
					// submission - otherwise the generation would never advance and every request
					// would open another batch.
					_uploadedGeneration.store(submitted);
				} else {
					_uploadsInFlight.fetch_add(1);
					// Fires exactly once, on the signalling thread; the fields are atomic, so no
					// thread hop is needed. The Rc keeps the controller alive until the upload lands.
					// Only the batch that empties the queue publishes a generation: while others are
					// still running, some laid-out glyph is still missing from the atlas.
					// The event outlives its own callback, so holding it by raw pointer is safe here
					// and does not build a cycle through the Function it stores.
					dep->setSignalCallback([self = Rc<FontController>(this), e = dep.get()] {
						if (!e->isSuccessful()) {
							self->_uploadFailed.store(true);
						}
						if (self->_uploadsInFlight.fetch_sub(1) == 1) {
							self->_uploadedGeneration.store(self->_submittedGeneration.load());
						}
					});
				}
			}
			// dep == nullptr: _dirty was raised by something other than a glyph request (a font or
			// alias was added), so this batch is ungated and cannot confirm anything. Leaving the
			// generation behind is the safe answer - marking it current here is what let a second
			// window through while the previous upload was still running. The next flush carries a
			// real dependency, because addTextureChars() opens one for as long as the atlas is behind.

			// Record what this batch carries, BEFORE handing it over: from here on those characters
			// are on their way, and a flush that finds nothing beyond them has nothing to do. The
			// count is the snapshot's, not the face's current one - the layout path keeps adding to
			// _required while this runs, and those characters belong to the next batch.
			for (auto &it : objects) {
				if (it.object) {
					it.object->setCharsSubmitted(it.chars.size());
				}
			}

			// Keep the batch reachable for as long as it is in flight: a caller that lays out
			// nothing new waits on this instead of opening another one.
			_submittedDependency = dep;

			submitGlyphs(app, sp::move(objects), sp::move(dep));
		}
		_dirty = false;
	}
}

void FontController::setLoaded(bool value) {
	if (_loaded != value) {
		_loaded = value;
		onLoaded(this);
		UpdateTime t;
		t.global = sp::platform::clock(ClockType::Monotonic);
		update(nullptr, t, false);
	}
}

void FontController::sendFontUpdatedEvent() { onFontSourceUpdated(this); }

void FontController::setAliases(Map<String, String> &&aliases) {
	if (_aliases.empty()) {
		_aliases = sp::move(aliases);
	} else {
		for (auto &it : aliases) { _aliases.insert_or_assign(it.first, it.second); }
	}
}

FontSpecializationVector FontController::findSpecialization(const FamilySpec &family,
		const FontParameters &params, Vector<Rc<FontFaceData>> *dataList) {
	auto getFontFaceScore = [](const FontLayoutParameters &required,
									const FontLayoutParameters &existed) -> uint32_t {
		uint32_t ret = 0;
		// if no match - prefer normal variants
		if (existed.fontStyle == FontStyle::Normal) {
			ret += 50;
		}
		if (existed.fontWeight == FontWeight::Normal) {
			ret += 50;
		}
		if (existed.fontStretch == FontStretch::Normal) {
			ret += 50;
		}

		if ((required.fontStyle == FontStyle::Italic && existed.fontStyle == FontStyle::Italic)
				|| (required.fontStyle == FontStyle::Normal
						&& existed.fontStyle == FontStyle::Normal)) {
			ret += 100'000;
		} else if (existed.fontStyle == FontStyle::Italic) {
			if (required.fontStyle != FontStyle::Normal) {
				ret += ((360 << 6)
							   - sprt::abs(int(required.fontStyle.get())
									   - int(FontStyle::Oblique.get())))
						/ 2;
			}
		} else if (required.fontStyle == FontStyle::Italic) {
			if (existed.fontStyle != FontStyle::Normal) {
				ret += ((360 << 6)
							   - sprt::abs(int(FontStyle::Oblique.get())
									   - int(existed.fontStyle.get())))
						/ 2;
			}
		} else {
			ret += (360 << 6)
					- sprt::abs(int(required.fontStyle.get()) - int(existed.fontStyle.get()));
		}

		if (existed.fontStyle == required.fontStyle
				&& (existed.fontStyle == FontStyle::Oblique
						|| existed.fontStyle == FontStyle::Italic)) {

		} else if ((existed.fontStyle == FontStyle::Oblique
						   || existed.fontStyle == FontStyle::Italic)
				&& (required.fontStyle == FontStyle::Oblique
						|| required.fontStyle == FontStyle::Italic)) {
			ret += 75'000; // Oblique-Italic replacement
		} else if (existed.fontStyle == required.fontStyle
				&& existed.fontStyle == FontStyle::Normal) {
			ret += 50'000;
		}

		if (existed.fontGrade == required.fontGrade) {
			ret += (400 - sprt::abs(int(required.fontGrade.get()) - int(existed.fontGrade.get())))
					* 50;
		}

		ret += (1'000 - sprt::abs(int(required.fontWeight.get()) - int(existed.fontWeight.get())))
				* 100;
		ret += ((250 << 1)
					   - sprt::abs(
							   int(required.fontStretch.get()) - int(existed.fontStretch.get())))
				* 100;
		return ret;
	};

	uint32_t score = 0;
	FontSpecializationVector ret;

	Vector<Pair<FontFaceData *, uint32_t>> scores;

	uint32_t offset = uint32_t(family.data.size());
	for (auto &it : family.data) {
		auto spec = it->getSpecialization(params);
		auto specScore = getFontFaceScore(params, spec) + offset;
		if (dataList) {
			scores.emplace_back(pair(it.get(), specScore));
		}
		if (specScore >= score) {
			score = specScore;
			ret = spec;
		}
		--offset;
	}

	if (dataList) {
		sprt::sort(scores.begin(), scores.end(),
				[](const Pair<FontFaceData *, uint32_t> &l,
						const Pair<FontFaceData *, uint32_t> &r) {
			if (l.second == r.second) {
				return l.first < r.first;
			} else {
				return l.second > r.second;
			}
		});

		dataList->reserve(scores.size());
		for (auto &it : scores) { dataList->emplace_back(it.first); }
	}

	return ret;
}

void FontController::removeUnusedLayouts() {
	sprt::unique_lock lock(_layoutSharedMutex);
	auto it = _layouts.begin();
	while (it != _layouts.end()) {
		if (it->second->isPersistent()) {
			++it;
			continue;
		}

		if (/*_clock - it->second->getAccessTime                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           () > _unusedInterval.toMicros() &&*/ it
						->second->getReferenceCount()
				== 1) {
			auto c = it->second->getRequiredCharsCount();
			log::source().debug("FontController", "Removed: ", it->first);
			it = _layouts.erase(it);
			if (c > 0) {
				_dirty = true;
			}
		} else {
			++it;
		}
	}
}

void FontController::initDependency() {
	if (!_dependency) {
		_dependency = makeDependency();
	}
	_dirty = true;
}

} // namespace stappler::xenolith::font
