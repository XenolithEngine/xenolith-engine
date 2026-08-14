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

#include "InstallerAppController.h"

#include "XLAppThread.h"
#include "XLAppWindow.h"
#include "SPLog.h"

#include <cstdint>
#include <ctime>
#include <memory>

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

static constexpr auto kLogTag = StringView("installer");

// Progress is reported once per this many bytes; the core's raw callback fires far more often than
// a UI can use.
static constexpr uint64_t kProgressStep = 256 * 1'024;

// How many finished jobs are kept for the status bar's "last action" line. A bound rather than a
// clear-on-finish so that a completed download still has something to say afterwards.
static constexpr size_t kMaxFinishedJobs = 32;

XL_DECLARE_EVENT_CLASS(AppController, onReady)
XL_DECLARE_EVENT_CLASS(AppController, onSettingsChanged)
XL_DECLARE_EVENT_CLASS(AppController, onCatalogueChanged)
XL_DECLARE_EVENT_CLASS(AppController, onEngineRefsChanged)
XL_DECLARE_EVENT_CLASS(AppController, onEngineStatusChanged)
XL_DECLARE_EVENT_CLASS(AppController, onInstalledStateChanged)
XL_DECLARE_EVENT_CLASS(AppController, onRowStatusChanged)
XL_DECLARE_EVENT_CLASS(AppController, onJobStarted)
XL_DECLARE_EVENT_CLASS(AppController, onJobProgress)
XL_DECLARE_EVENT_CLASS(AppController, onJobFinished)
XL_DECLARE_EVENT_CLASS(AppController, onReachabilityChanged)
XL_DECLARE_EVENT_CLASS(AppController, onError)

AppController *AppController::getInstance() {
	static sprt::qonce s_once;
	static AppController *s_instance = nullptr;
	s_once([] {
		// Deliberately never destroyed: it outlives every scene and every window, and a static
		// destructor would race whatever is still tearing down. The single retain leaked here is
		// what expresses that - detach() is the thing that releases scene-lifetime state.
		auto ref = Rc<AppController>::create();
		if (ref) {
			ref->retain();
			s_instance = ref.get();
		}
	});
	return s_instance;
}

AppController::~AppController() { }

bool AppController::init() { return true; }

bool AppController::attach(NotNull<AppThread> app) {
	if (_app == app) {
		return true; // idempotent: a second window must not reload everything
	}

	_app = app;
	_layout = Layout::resolveFromEnv();
	_settings = Settings::load(_layout.getSettingsManifest());

	auto host = resolveHost(getNativeArch(), getNativeOs());
	_nativeId = host.native;
	_nativeViaEmulation = host.viaEmulation;

	// Each Source answers from the caches, INLINE: the data is already in memory, so a view reading
	// one has every payload in place before it builds its first row and never draws a `loading`
	// placeholder. `this` is captured raw on purpose - the controller owns these Sources, so an Rc
	// back would be a cycle, and detach() releases them.
	auto makeSource = [](Function<Vector<Value>()> &&rows) {
		return Rc<data::Source>::create(data::Source::BatchSourceCallback(
				[rows = sp::move(rows)](const data::Source::BatchCallback &cb,
						data::Source::Id::Type first, size_t size) {
			auto all = rows();
			Map<data::Source::Id, Value> out;
			for (size_t i = 0; i < size; ++i) {
				const auto index = static_cast<size_t>(first) + i;
				if (index < all.size()) {
					out.emplace(data::Source::Id(first + i), sp::move(all[index]));
				}
			}
			cb(out);
		}));
	};

	_hostsSource = makeSource([this] { return makeToolRows(Kind::Host); });
	_targetsSource = makeSource([this] { return makeToolRows(Kind::Target); });
	_enginesSource = makeSource([this] { return makeEngineRows(); });

	// The navigation tree. A category's own record is what TreeView shows as its label, so each
	// branch carries a Value of its own; the leaves come from the branch's batch callback.
	auto makeBranch = [&](StringView node, StringView label) {
		auto source = makeSource([this, node = toString(node)] { return makeNavRows(node); });
		Value data;
		data.setString(label, "name");
		data.setString("group", "node");
		data.setString(node, "branch");
		source->setData(sp::move(data));
		return source;
	};

	_navEngines = makeBranch("engine", "Engines");
	_navHosts = makeBranch("host", "Hosts");
	_navTargets = makeBranch("target", "Targets");

	// The root's own items come AFTER its subcategories, which is exactly where the Projects leaf
	// belongs in design.md's tree.
	_navSource = makeSource([] {
		Vector<Value> rows;
		Value projects;
		projects.setString("Projects", "name");
		projects.setString("projects", "node");
		projects.setBool(false, "enabled");
		rows.emplace_back(sp::move(projects));
		return rows;
	});
	{
		Value data;
		data.setString("Xenolith", "name");
		data.setString("root", "node");
		_navSource->setData(sp::move(data));
	}
	// The root's one own item is the Projects leaf; without a count the batch callback is never
	// asked and the leaf would silently not exist.
	_navSource->setChildsCount(1);
	_navSource->addSubcategry(_navEngines);
	_navSource->addSubcategry(_navHosts);
	_navSource->addSubcategry(_navTargets);

	log::info(kLogTag, "attach: config=", _layout.config, " data=", _layout.data,
			" native=", _nativeId);

	onReady(this);

	/* Start fetching NOW, not when a page that needs the data is first shown.

	All three are network round trips of their own, they do not depend on each other, and none of
	them depends on a single node existing - so they run while the scene is still being built and
	are usually finished before anything asks. Each one dirties the Sources it fills when it lands,
	so a view that was already on screen picks the data up without being told.

	This is what the pages used to wait for: a table built on first show had to sit empty until the
	catalogue arrived, which is the delay that showed as a lag on opening a page.

	checkComponents() joins them for the same reason and answers first: it is local I/O, so the
	navigation tree and the engine rows have their answer long before either network round trip
	lands, and no page has to touch the disk to build itself. */
	checkComponents(nullptr);
	loadCatalogue(nullptr);
	loadEngineRefs(nullptr);
	queryEngine(nullptr);
	return true;
}

void AppController::detach() {
	// The instance survives, so everything with a scene's lifetime has to be released HERE. A
	// dialog request or a spawned prompt still held at static destruction would be torn down long
	// after the renderer, which is exactly the crash this avoids.
	_promptHandle = nullptr;
	_dialogRequest = nullptr;
	_hostsSource = nullptr;
	_targetsSource = nullptr;
	_enginesSource = nullptr;
	_navSource = nullptr;
	_navEngines = nullptr;
	_navHosts = nullptr;
	_navTargets = nullptr;
	_jobs.clear();
	_engineProbeRunning = false;
	_releaseProbeRunning = false;
	// The next attach() re-checks: the caches describe a disk that may have been changed by whoever
	// ran in between, and a stale "checked" flag would show the previous machine state as verified.
	_checkRunning = false;
	_checkPending = false;
	_hasInstalledState = false;
	_hasInstalledEngines = false;
	_app = nullptr;
}

// Throttle a byte-progress callback and marshal it onto the app thread. The core's install/clone
// callbacks fire on a worker thread and the scene graph may only be touched on the app thread, so
// `emit` — the caller's app-thread notification — hops back through performOnAppThread; `owner`
// keeps its captures alive until it runs.
static void marshalProgress(AppThread *app, Ref *owner, uint64_t &lastStep, int64_t bytes,
		Function<void()> &&emit) {
	const uint64_t step = static_cast<uint64_t>(bytes) / kProgressStep;
	if (step == lastStep) {
		return;
	}
	lastStep = step;
	app->performOnAppThread([emit = sp::move(emit)] { emit(); }, owner);
}

// --- job registry -----------------------------------------------------------

JobId AppController::beginJob(JobKind kind, StringView target, StringView title, Kind toolKind) {
	// Trim finished jobs before adding: the vector is the live set plus a bounded tail of history.
	size_t finished = 0;
	for (const auto &it : _jobs) {
		if (!it.isActive()) {
			++finished;
		}
	}
	while (finished > kMaxFinishedJobs) {
		for (auto it = _jobs.begin(); it != _jobs.end(); ++it) {
			if (!it->isActive()) {
				_jobs.erase(it);
				--finished;
				break;
			}
		}
	}

	Job job;
	job.id = _nextJobId++;
	job.kind = kind;
	job.toolKind = toolKind;
	job.target = target.str<mem_std::Interface>();
	job.title = title.str<mem_std::Interface>();
	job.phase = JobPhase::Running;
	_jobs.emplace_back(sp::move(job));

	const auto id = _jobs.back().id;
	onJobStarted(this, static_cast<uint64_t>(id));
	return id;
}

void AppController::updateJob(JobId id, JobPhase phase, int64_t bytes, int64_t total) {
	auto job = getMutableJob(id);
	if (!job) {
		return;
	}
	job->phase = phase;
	job->bytes = bytes;
	job->total = total;
	// Never dirties a Source: this fires many times a second and a rebuild would thrash every row.
	onJobProgress(this, static_cast<uint64_t>(id));
}

void AppController::finishJob(JobId id, Status result, StringView error) {
	auto job = getMutableJob(id);
	if (!job) {
		return;
	}
	// `result`, not `status`: a parameter of that name would shadow the sprt::status namespace that
	// isSuccessful() lives in.
	const bool ok = sprt::status::isSuccessful(result);
	job->status = result;
	job->error = error.str<mem_std::Interface>();
	job->phase = ok ? JobPhase::Done : JobPhase::Failed;
	onJobFinished(this, static_cast<uint64_t>(id));

	if (!ok && !error.empty()) {
		onError(this, job->error);
	}
}

Job *AppController::getMutableJob(JobId id) {
	for (auto &it : _jobs) {
		if (it.id == id) {
			return &it;
		}
	}
	return nullptr;
}

const Job *AppController::getJob(JobId id) const {
	for (const auto &it : _jobs) {
		if (it.id == id) {
			return &it;
		}
	}
	return nullptr;
}

const Job *AppController::findJob(JobKind kind, StringView target) const {
	// Newest first: a re-run of the same target must win over the finished record of the previous.
	for (auto it = _jobs.rbegin(); it != _jobs.rend(); ++it) {
		if (it->kind == kind && StringView(it->target) == target && it->isActive()) {
			return &(*it);
		}
	}
	return nullptr;
}

size_t AppController::getActiveJobCount() const {
	size_t count = 0;
	for (const auto &it : _jobs) {
		if (it.isActive()) {
			++count;
		}
	}
	return count;
}

float AppController::getAggregateProgress() const {
	int64_t bytes = 0, total = 0;
	for (const auto &it : _jobs) {
		if (it.isActive() && it.total > 0) {
			bytes += it.bytes;
			total += it.total;
		}
	}
	// nan, not 0: "no determinate work in flight" is not "nothing done yet", and a bar that showed
	// empty for a clone would be inventing a fraction git never reported.
	return total > 0 ? static_cast<float>(double(bytes) / double(total)) : nan();
}

// --- settings ---------------------------------------------------------------

Status AppController::setSettingsField(StringView key, const Value &value) {
	if (key == "engineRepoUrl") {
		_settings.sources.engineRepoUrl = value.getString();
	} else if (key == "releaseSourceUrl") {
		_settings.sources.releasesRoot = value.getString();
	} else if (key == "autoUpdateInstaller") {
		_settings.autoUpdateInstaller = value.getBool();
	} else if (key == "autoUpdateEngine") {
		_settings.autoUpdateEngine = value.getBool();
	} else if (key == "autoUpdateReleases") {
		_settings.autoUpdateReleases = value.getBool();
	} else if (key == "lang") {
		_settings.lang = value.getString();
	} else {
		log::error(kLogTag, "setSettingsField: unknown key '", key, "'");
		return Status::ErrorInvalidArguemnt;
	}

	if (!_settings.save(_layout.getSettingsManifest())) {
		log::error(kLogTag, "setSettingsField: failed to save ", _layout.getSettingsManifest());
		return Status::ErrorNotPermitted;
	}

	Value changed;
	changed.addString(key);
	onSettingsChanged(this, sp::move(changed));

	// A source URL is only useful if it answers, so saying so is part of accepting the change.
	if (key == "engineRepoUrl") {
		probeSource(SourceKind::EngineRepo, _settings.sources.getEngineRepoUrl());
	} else if (key == "releaseSourceUrl") {
		probeSource(SourceKind::Releases, _settings.sources.getReleasesRoot());
	}
	return Status::Ok;
}

void AppController::setToolAutoUpdate(Kind kind, StringView id, bool enabled) {
	if (_settings.getToolAutoUpdate(kind, id) == enabled) {
		return;
	}
	_settings.setToolAutoUpdate(kind, id, enabled);
	_settings.save(_layout.getSettingsManifest());

	Value changed;
	changed.addString("tools");
	onSettingsChanged(this, sp::move(changed));
}

const ReachabilityInfo &AppController::getReachability(SourceKind kind) const {
	return kind == SourceKind::EngineRepo ? _engineReachability : _releaseReachability;
}

void AppController::setReachability(SourceKind kind, Reachability state, StringView message) {
	auto &info = (kind == SourceKind::EngineRepo) ? _engineReachability : _releaseReachability;
	info.state = state;
	info.message = message.str<mem_std::Interface>();
	onReachabilityChanged(this, toString(getSourceKindName(kind)));
}

// --- data sources -----------------------------------------------------------

data::Source *AppController::getToolsSource(Kind kind) const {
	return kind == Kind::Host ? _hostsSource.get() : _targetsSource.get();
}

Vector<Value> AppController::makeToolRows(Kind kind) const {
	Vector<Value> rows;
	if (!_hasCatalogue) {
		return rows;
	}
	for (const auto &row : _catalogue.rows) {
		if (row.kind != kind) {
			continue;
		}
		Value v;
		v.setString(row.id, "id");
		v.setString(row.triple, "triple");
		v.setString(row.variant, "variant");
		v.setString(getKindName(kind), "kind");
		v.setString(rowKey(kind, row.id), "key");
		v.setInteger(static_cast<int64_t>(row.size), "size");
		v.setInteger(static_cast<int64_t>(row.status), "status");
		v.setBool(row.isNative, "native");
		v.setBool(_settings.getToolAutoUpdate(kind, row.id), "autoUpdate");
		rows.emplace_back(sp::move(v));
	}
	// The running machine's toolchain is ALWAYS the first data row (design.md). Sorted here rather
	// than in the view because both tool tables want it and neither should have to know the rule;
	// stable, so everything else keeps the catalogue's alphabetical order.
	sprt::stable_sort(rows.begin(), rows.end(),
			[](const Value &l, const Value &r) { return l.getBool("native") && !r.getBool("native"); });
	return rows;
}

Vector<Value> AppController::makeEngineRows() const {
	Vector<Value> rows;
	// Already ordered master / stage / branches / tags by loadEngineRefs().
	for (const auto &ref : _engineRefs) {
		Value v;
		v.setString(ref.name, "id");
		v.setString(ref.name, "name");
		v.setString(ref.oidHex, "commit");
		v.setBool(ref.isBranch, "branch");
		v.setBool(ref.isTag, "tag");
		/* "Installed" for an engine is simply that its clone directory exists. Per-ref staleness
		would need a commit recorded at clone time; until then a cloned ref reports Installed and
		never UpdateAvailable.

		Read from the CHECKED list, not from a stat() of its own: this runs inside a Source's batch
		callback, i.e. on the app thread while the table is being built, and it used to hit the
		filesystem once per row there. Before the first check has answered the row admits it does
		not know yet. */
		auto status = RowStatus::Checking;
		if (_hasInstalledEngines) {
			status = RowStatus::NotInstalled;
			for (const auto &name : _installedEngines) {
				if (StringView(name) == StringView(ref.name)) {
					status = RowStatus::Installed;
					break;
				}
			}
		}
		v.setInteger(static_cast<int64_t>(status), "status");
		v.setBool(StringView(ref.name) == StringView(_engineStatus.reference), "active");
		rows.emplace_back(sp::move(v));
	}
	return rows;
}

Vector<Value> AppController::makeNavRows(StringView node) const {
	Vector<Value> rows;

	auto add = [&](StringView name, bool removable) {
		Value v;
		v.setString(name, "name");
		v.setString(node, "node");
		v.setString(name, "id");
		v.setBool(removable, "removable");
		v.setBool(true, "enabled");
		if (node != "engine") {
			const auto kind = (node == "host") ? Kind::Host : Kind::Target;
			v.setString(rowKey(kind, name), "key");
		}
		rows.emplace_back(sp::move(v));
	};

	/* What is on disk, not what the remote offers: these branches list what you HAVE, and the page
	behind each is where the full set is chosen from.

	Both halves come from the checked caches. This is a Source batch callback - the app thread, while
	the tree is being built - and it used to walk the engines directory and parse installed.json
	right here, on every read. An unchecked branch simply has no entries yet; the check dirties the
	Sources when it lands and the tree fills in. */
	if (node == "engine") {
		for (const auto &name : _installedEngines) { add(name, true); }
	} else {
		const auto kind = (node == "host") ? Kind::Host : Kind::Target;
		for (const auto &c : _installedState.components) {
			if (c.kind == kind && !isComponentMissing(rowKey(c.kind, c.id))) {
				add(c.id, true);
			}
		}
	}

	// design.md ends every branch with "… add new"; it has no page of its own and simply opens the
	// branch's page, where installing is what "adding" means.
	Value more;
	more.setString("… add new", "name");
	more.setString(node, "node");
	more.setBool(false, "removable");
	more.setBool(true, "enabled");
	more.setBool(true, "add");
	rows.emplace_back(sp::move(more));
	return rows;
}

void AppController::rebuildSources() {
	// Only the COUNT is pushed: each Source answers its payload from the caches through the batch
	// callback installed in attach(), so there is nothing to copy in here and no second place where
	// row data could go stale. setDirty() is what makes every bound view re-read.
	if (_hostsSource) {
		_hostsSource->setChildsCount(makeToolRows(Kind::Host).size());
		_hostsSource->setDirty();
	}
	if (_targetsSource) {
		_targetsSource->setChildsCount(makeToolRows(Kind::Target).size());
		_targetsSource->setDirty();
	}
	if (_enginesSource) {
		_enginesSource->setChildsCount(_engineRefs.size());
		_enginesSource->setDirty();
	}

	rebuildNavSources();
}

void AppController::rebuildNavSources() {
	// A TreeView watches only the ROOT source, so each branch is re-counted here and the root is
	// dirtied last - dirtying a branch on its own would not reach the view.
	if (_navEngines) {
		_navEngines->setChildsCount(makeNavRows("engine").size());
	}
	if (_navHosts) {
		_navHosts->setChildsCount(makeNavRows("host").size());
	}
	if (_navTargets) {
		_navTargets->setChildsCount(makeNavRows("target").size());
	}
	if (_navSource) {
		_navSource->setDirty();
	}
}

bool AppController::isComponentMissing(StringView key) const {
	for (const auto &it : _missingComponents) {
		if (StringView(it) == key) {
			return true;
		}
	}
	return false;
}

RowStatus AppController::getToolStatus(Kind kind, StringView id) const {
	for (const auto &row : _catalogue.rows) {
		if (row.kind == kind && StringView(row.id) == id) {
			return row.status;
		}
	}
	return RowStatus::Checking;
}

RowStatus AppController::getEngineRowStatus(StringView ref) const {
	if (!_hasInstalledEngines) {
		return RowStatus::Checking;
	}
	for (const auto &name : _installedEngines) {
		if (StringView(name) == ref) {
			return RowStatus::Installed;
		}
	}
	return RowStatus::NotInstalled;
}

void AppController::applyInstalledState() {
	// Two independent answers are needed and either may land first: the catalogue says what exists
	// and under which release, the check says what this machine has. Until both are in, every row
	// keeps the Checking it was created with - which is exactly the state whose action controls are
	// hidden, so nothing offers to install what it has not looked for.
	if (!_hasCatalogue || !_hasInstalledState) {
		return;
	}

	// Which rows MOVED, not which rows exist: a check that confirms what was already on screen must
	// announce nothing at all, and the usual check confirms every row.
	String moved;
	size_t movedCount = 0;

	for (auto &row : _catalogue.rows) {
		const auto *ic = _installedState.get(row.id, row.kind);
		auto status = RowStatus::Installed;
		if (!ic || isComponentMissing(rowKey(row.kind, row.id))) {
			// Either never installed, or the manifest claims it and the store no longer has it -
			// which is the same offer to the user, and a truer one than the "Installed" a
			// manifest-only check used to report for a directory somebody deleted by hand.
			status = RowStatus::NotInstalled;
		} else if (StringView(ic->release) != StringView(_catalogue.release)) {
			status = RowStatus::UpdateAvailable;
		}

		if (row.status != status) {
			row.status = status;
			moved = rowKey(row.kind, row.id);
			++movedCount;
		}
	}

	if (movedCount > 0) {
		// The rows keep their nodes; each one re-reads its own status and repaints its own controls.
		onRowStatusChanged(this, movedCount == 1 ? moved : String());
	}
}

// --- operations -------------------------------------------------------------

void AppController::loadCatalogue(Function<void(bool ok, String err)> &&onDone) {
	auto doneCb = std::make_shared<Function<void(bool, String)>>(sp::move(onDone));
	auto errStr = std::make_shared<String>();
	auto built = std::make_shared<CatalogueData>();

	// Copied into the task: settings can be rewritten from the form while this runs, and a worker
	// must never read them back through getSettings(). The layout is not needed here at all any
	// more - this task no longer reads anything local.
	const auto sources = _settings.sources;

	log::info(kLogTag, "loadCatalogue: discovering release under ", sources.getReleasesRoot());

	const auto job = beginJob(JobKind::CatalogueLoad, StringView("catalogue"),
			StringView("Loading catalogue"));

	_app->perform([sources, errStr, built](const AppThread::Task &) -> bool {
		String release = toString(getDefaultRelease());
		String releasesText;
		if (fetchText(sources.getReleasesRoot(), releasesText)) {
			release = resolveActiveRelease(releasesText);
		}

		auto base = sources.getReleaseBase(release);
		String hostsText, targetsText;
		auto hostsResult = fetchText(toString(base) + "/hosts/", hostsText);
		if (!hostsResult) {
			*errStr = toString("hosts: ") + hostsResult.error;
			return false;
		}
		auto targetsResult = fetchText(toString(base) + "/targets/", targetsText);
		if (!targetsResult) {
			*errStr = toString("targets: ") + targetsResult.error;
			return false;
		}

		auto comps = buildCatalogue(hostsText, targetsText);

		auto host = resolveHost(getNativeArch(), getNativeOs());
		built->nativeId = host.native;
		built->release = release;
		for (const auto &c : comps) {
			CatalogRow row;
			row.kind = c.kind;
			row.id = c.id;
			row.triple = c.triple;
			row.variant = c.variant;
			row.size = c.size;
			// Status is deliberately NOT decided here. This task answers what the mirror offers;
			// what this machine has is checkComponents()'s answer, and mixing the two meant a row
			// could only ever be re-judged by fetching the whole catalogue again.
			row.isNative = (c.triple == built->nativeId);
			built->rows.push_back(sp::move(row));
		}
		return true;
	}, [this, job, doneCb, errStr, built](const AppThread::Task &, bool ok) {
		if (!_app) {
			return; // detached while the work was in flight
		}
		if (ok) {
			_catalogue = sp::move(*built);
			_hasCatalogue = true;

			// The rows arrive unchecked; this re-decides them from a check that has already landed
			// (the usual case at startup, where both run in parallel) and leaves them Checking when
			// none has. Either way the rows themselves are on screen now.
			applyInstalledState();

			size_t installed = 0;
			for (const auto &row : _catalogue.rows) {
				if (row.status == RowStatus::Installed
						|| row.status == RowStatus::UpdateAvailable) {
					++installed;
				}
			}
			log::info(kLogTag, "loadCatalogue: ok, rows=", _catalogue.rows.size(),
					" installed=", installed, " native=", _catalogue.nativeId,
					" release=", _catalogue.release);

			rebuildSources();
			onCatalogueChanged(this);
			// A new catalogue means a new release to judge against, so what was checked against the
			// previous one has to be re-judged.
			checkComponents(nullptr);
			// A catalogue that answered is a mirror that answered.
			setReachability(SourceKind::Releases, Reachability::Ok, _catalogue.release);
		} else {
			log::error(kLogTag, "loadCatalogue: FAILED: ", *errStr);
			setReachability(SourceKind::Releases, Reachability::Failed, *errStr);
		}
		finishJob(job, ok ? Status::Ok : Status::ErrorNotImplemented, *errStr);
		if (*doneCb) {
			(*doneCb)(ok, *errStr);
		}
	}, this);
}

void AppController::loadEngineRefs(Function<void(bool ok, String err)> &&onDone) {
	auto doneCb = std::make_shared<Function<void(bool, String)>>(sp::move(onDone));
	auto errStr = std::make_shared<String>();
	auto refs = std::make_shared<Vector<EngineRef>>();

	const auto sources = _settings.sources;
	const auto job =
			beginJob(JobKind::EngineRefs, StringView("engine-refs"), StringView("Reading branches"));

	_app->perform([sources, errStr, refs](const AppThread::Task &) -> bool {
		OperationResult status;
		*refs = listEngineRefs(sources, &status);
		if (!status) {
			*errStr = status.error;
			return false;
		}
		return true;
	}, [this, job, doneCb, errStr, refs](const AppThread::Task &, bool ok) {
		if (!_app) {
			return;
		}
		if (ok) {
			// master first, then stage, then the remaining branches alphabetically, then tags
			// (design.md). Done here rather than in the view: two views would otherwise each have
			// to know the rule.
			auto rank = [](const EngineRef &r) -> int {
				if (r.isBranch && r.name == getEngineDefaultRef()) {
					return 0;
				}
				if (r.isBranch && r.name == "stage") {
					return 1;
				}
				return r.isBranch ? 2 : 3;
			};
			sprt::stable_sort(refs->begin(), refs->end(),
					[&rank](const EngineRef &l, const EngineRef &r) {
				const auto lr = rank(l), rr = rank(r);
				return lr != rr ? lr < rr : l.name < r.name;
			});
			_engineRefs = sp::move(*refs);
			_hasEngineRefs = true;
			log::info(kLogTag, "loadEngineRefs: ok, refs=", _engineRefs.size());
			rebuildSources();
			onEngineRefsChanged(this);
			// ls-refs answering IS the reachability check for the repository URL.
			setReachability(SourceKind::EngineRepo, Reachability::Ok,
					toString(_engineRefs.size(), " refs"));
		} else {
			log::error(kLogTag, "loadEngineRefs: FAILED: ", *errStr);
			setReachability(SourceKind::EngineRepo, Reachability::Failed, *errStr);
		}
		finishJob(job, ok ? Status::Ok : Status::ErrorNotImplemented, *errStr);
		if (*doneCb) {
			(*doneCb)(ok, *errStr);
		}
	}, this);
}

void AppController::queryEngine(Function<void(const EngineStatusInfo &)> &&onDone) {
	auto doneCb = std::make_shared<Function<void(const EngineStatusInfo &)>>(sp::move(onDone));
	auto info = std::make_shared<EngineStatusInfo>();
	const auto layout = _layout;

	// Registered like every other operation, so the status bar accounts for it too: this is one of
	// the three that run at startup, and a start-up step missing from the bar reads as the app
	// having finished when it has not.
	const auto job = beginJob(JobKind::EngineStatus, StringView("engine"),
			StringView("Checking the engine"));

	_app->perform([layout, info](const AppThread::Task &) -> bool {
		bool ok = false;
		auto root = resolveEngineRoot(layout, StringView(), &ok);
		info->ready = ok;
		info->path = root;
		if (ok) {
			// reference = the basename of the resolved engine dir (a ref name for cloned engines,
			// the checkout dir name for an external override)
			info->reference = toString(filepath::lastComponent(root));
		}
		return true;
	}, [this, info, doneCb, job](const AppThread::Task &, bool) {
		if (!_app) {
			return;
		}
		log::info(kLogTag, "queryEngine: ready=", info->ready, " ref=", info->reference,
				" path=", info->path);
		_engineStatus = *info;
		finishJob(job, Status::Ok);
		onEngineStatusChanged(this);
		if (*doneCb) {
			(*doneCb)(*info);
		}
	}, this);
}

void AppController::checkComponents(Function<void()> &&onDone) {
	if (!_app) {
		if (onDone) {
			onDone();
		}
		return;
	}

	/* Coalesced, not queued. Every tools page asks for a check when it is shown and every install
	asks for one when it finishes, so requests arrive in bursts; what they all want is one fresh
	answer, and a queue would run the same directory walk three times to produce it. The in-flight
	one may have read the disk before the change that prompted the new request, so the request is not
	simply dropped either - it is remembered and re-run once. */
	if (_checkRunning) {
		_checkPending = true;
		if (onDone) {
			onDone();
		}
		return;
	}
	_checkRunning = true;

	auto doneCb = std::make_shared<Function<void()>>(sp::move(onDone));
	auto state = std::make_shared<InstalledState>();
	auto engines = std::make_shared<Vector<String>>();
	auto missing = std::make_shared<Vector<String>>();

	// Copied into the task: the layout is immutable after attach(), but the task must not reach back
	// into the controller for anything else.
	const auto layout = _layout;

	_app->perform([layout, state, engines, missing](const AppThread::Task &) -> bool {
		*state = InstalledState::load(layout.getInstalledManifest());
		// A manifest entry whose files are gone is not an installed component. getInvalid() takes
		// the existence predicate rather than assuming one, so the check is the only place that
		// touches the filesystem.
		for (const auto *c : state->getInvalid([](StringView path) { return isDirectory(path); })) {
			missing->emplace_back(rowKey(c->kind, c->id));
		}
		*engines = listInstalledEngines(layout);
		return true;
	}, [this, state, engines, missing, doneCb](const AppThread::Task &, bool) {
		_checkRunning = false;
		if (!_app) {
			return; // detached while the work was in flight
		}

		// What the tree lists and what the engine rows say both come from these, so "did anything
		// move" is asked before they are replaced: a check that confirms the previous answer - which
		// is what a check on every page open normally does - must not touch a single node.
		auto sameComponents = [](const InstalledState &l, const InstalledState &r) {
			if (l.components.size() != r.components.size()) {
				return false;
			}
			for (size_t i = 0; i < l.components.size(); ++i) {
				// Identity only: what the tree shows is the name, and everything else about an entry
				// is a status, which is announced separately.
				if (l.components[i].kind != r.components[i].kind
						|| l.components[i].id != r.components[i].id) {
					return false;
				}
			}
			return true;
		};
		const bool treeChanged = !_hasInstalledState || !sameComponents(_installedState, *state)
				|| _missingComponents != *missing;
		const bool enginesChanged = !_hasInstalledEngines || (_installedEngines != *engines);

		_installedState = sp::move(*state);
		_installedEngines = sp::move(*engines);
		_missingComponents = sp::move(*missing);
		_hasInstalledState = true;
		_hasInstalledEngines = true;

		log::info(kLogTag, "checkComponents: components=", _installedState.components.size(),
				" missing=", _missingComponents.size(), " engines=", _installedEngines.size());

		// Announces per-row status moves by itself, and only for the rows that moved.
		applyInstalledState();

		if (enginesChanged) {
			// The engines table reads its status from the list that has just been replaced, and its
			// rows are the refs, which did not change - so this is a repaint too, not a rebuild.
			onRowStatusChanged(this, String());
		}
		if (treeChanged || enginesChanged) {
			rebuildNavSources();
			// The wholesale form: the tree's row set changed and a listener has nothing to filter on.
			onInstalledStateChanged(this, String());
		}

		if (_checkPending) {
			_checkPending = false;
			checkComponents(nullptr);
		}
		if (*doneCb) {
			(*doneCb)();
		}
	}, this);
}

void AppController::installComponent(Kind kind, StringView id,
		Function<void(const InstallProgress &)> &&onProgress,
		Function<void(bool ok, String err)> &&onDone) {
	String idStr = toString(id);
	const bool wantHost = (kind == Kind::Host);
	const bool wantTarget = (kind == Kind::Target);
	auto progCb = std::make_shared<Function<void(const InstallProgress &)>>(sp::move(onProgress));
	auto doneCb = std::make_shared<Function<void(bool, String)>>(sp::move(onDone));
	auto errStr = std::make_shared<String>();
	auto lastStep = std::make_shared<uint64_t>(UINT64_MAX);

	const auto sources = _settings.sources;
	const auto layout = _layout;
	// Install from the release the catalogue was built against, not from whatever the default is:
	// otherwise a row shown as available under one release installs from another.
	const auto release = _hasCatalogue ? _catalogue.release : String();

	log::info(kLogTag, "installComponent: kind=", getKindName(kind), " id=", idStr);

	const auto job = beginJob(JobKind::ComponentInstall, rowKey(kind, idStr),
			toString("Installing ", idStr), kind);

	_app->perform(
			[this, sources, release, layout, idStr, kind, wantHost, wantTarget, progCb, errStr,
					lastStep, job](const AppThread::Task &) -> bool {
		auto result = installer::installComponent(sources, release, idStr, layout, wantHost,
				wantTarget, [this, kind, idStr, progCb, lastStep, job](int64_t bytes, int64_t total) {
			marshalProgress(_app, this, *lastStep, bytes,
					[this, kind, idStr, bytes, total, progCb, job] {
				updateJob(job, JobPhase::Downloading, bytes, total);
				if (*progCb) {
					(*progCb)(InstallProgress{kind, idStr, InstallPhase::Downloading, bytes});
				}
			});
		});
		if (!result || result.installed.empty()) {
			*errStr = result.error.empty() ? toString("install failed: ") + idStr : result.error;
			return false;
		}
		return true;
	},
			[this, job, kind, idStr, doneCb, errStr](const AppThread::Task &, bool ok) {
		if (!_app) {
			return;
		}
		if (ok) {
			log::info(kLogTag, "installComponent: done id=", idStr);
			// Optimistic first, verified second: the row must react on this frame, and the check
			// then re-reads the manifest the install has just rewritten - which is also what
			// refreshes the navigation branches and would otherwise leave them a component behind.
			setRowStatus(kind, idStr, RowStatus::Installed);
			onInstalledStateChanged(this, rowKey(kind, idStr));
			checkComponents(nullptr);
		} else {
			log::error(kLogTag, "installComponent: FAILED id=", idStr, " err=", *errStr);
		}
		finishJob(job, ok ? Status::Ok : Status::ErrorNotImplemented, *errStr);
		if (*doneCb) {
			(*doneCb)(ok, *errStr);
		}
	}, this);
}

void AppController::uninstallComponent(Kind kind, StringView id,
		Function<void(bool ok, String err)> &&onDone) {
	String idStr = toString(id);
	auto doneCb = std::make_shared<Function<void(bool, String)>>(sp::move(onDone));
	auto errStr = std::make_shared<String>();
	const auto layout = _layout;

	log::info(kLogTag, "uninstallComponent: kind=", getKindName(kind), " id=", idStr);

	const auto job = beginJob(JobKind::ComponentRemove, rowKey(kind, idStr),
			toString("Removing ", idStr), kind);

	_app->perform([layout, idStr, kind, errStr](const AppThread::Task &) -> bool {
		// uninstall is idempotent (a missing dir is not an error); only a real removal failure
		// comes back false
		if (!removeComponent(layout, kind, idStr)) {
			*errStr = toString("uninstall failed: ") + idStr;
			return false;
		}
		return true;
	}, [this, job, kind, idStr, doneCb, errStr](const AppThread::Task &, bool ok) {
		if (!_app) {
			return;
		}
		if (ok) {
			log::info(kLogTag, "uninstallComponent: done id=", idStr);
			setRowStatus(kind, idStr, RowStatus::NotInstalled);
			onInstalledStateChanged(this, rowKey(kind, idStr));
			checkComponents(nullptr);
		} else {
			log::error(kLogTag, "uninstallComponent: FAILED id=", idStr);
		}
		finishJob(job, ok ? Status::Ok : Status::ErrorNotImplemented, *errStr);
		if (*doneCb) {
			(*doneCb)(ok, *errStr);
		}
	}, this);
}

void AppController::installForSystem(
		Function<void(StringView step, const InstallProgress &)> &&onProgress,
		Function<void(bool ok, String err)> &&onDone) {
	auto progCb = std::make_shared<Function<void(StringView, const InstallProgress &)>>(
			sp::move(onProgress));
	auto doneCb = std::make_shared<Function<void(bool, String)>>(sp::move(onDone));
	auto errStr = std::make_shared<String>();

	const auto sources = _settings.sources;
	const auto layout = _layout;
	const auto release = _hasCatalogue ? _catalogue.release : String();

	log::info(kLogTag, "installForSystem: starting (engine + host + target)");

	const auto job = beginJob(JobKind::SystemProvision, StringView("system"),
			StringView("Preparing the SDK"));

	_app->perform([this, sources, release, layout, progCb, errStr, job](
						  const AppThread::Task &) -> bool {
		// 1. Engine — clone the default ref if none resolves.
		bool ok = false;
		auto root = resolveEngineRoot(layout, StringView(), &ok);
		if (!ok) {
			log::info(kLogTag, "installForSystem: cloning engine ", getEngineDefaultRef());
			auto lastStep = std::make_shared<uint64_t>(UINT64_MAX);
			auto cloneResult = cloneEngine(sources, getEngineDefaultRef(), layout,
					[this, progCb, lastStep, job](const git::CloneProgress &p) {
				if (p.stage != git::CloneStage::Downloading || p.bytesTotal == 0) {
					return;
				}
				auto received = static_cast<int64_t>(p.bytesReceived);
				auto total = static_cast<int64_t>(p.bytesTotal);
				marshalProgress(_app, this, *lastStep, received,
						[this, received, total, progCb, job] {
					updateJob(job, JobPhase::Cloning, received, total);
					if (*progCb) {
						(*progCb)(StringView("engine"),
								InstallProgress{Kind::Host, String("engine"),
									InstallPhase::Downloading, received});
					}
				});
			});
			if (!cloneResult) {
				*errStr = toString("engine clone failed");
				return false;
			}
			root = layout.getEngineDir(getEngineDefaultRef());
		}
		linkToolchainsIntoEnginePath(layout, StringView(root));

		// 2 + 3. Native host + native target (+sprt target if present).
		auto host = resolveHost(getNativeArch(), getNativeOs());
		if (host.native.empty()) {
			*errStr = toString("no SDK host for ") + toString(getNativeArch()) + toString("-")
					+ toString(getNativeOs());
			return false;
		}

		auto installWith = [&](StringView step, Kind progKind, StringView id, bool wantHost,
								   bool wantTarget) -> bool {
			auto lastStep = std::make_shared<uint64_t>(UINT64_MAX);
			auto result = installer::installComponent(sources, release, id, layout, wantHost,
					wantTarget,
					[this, progCb, stepStr = toString(step), idStr = toString(id), progKind,
							lastStep, job](int64_t bytes, int64_t total) {
				marshalProgress(_app, this, *lastStep, bytes,
						[this, stepStr, idStr, progKind, bytes, total, progCb, job] {
					updateJob(job, JobPhase::Downloading, bytes, total);
					if (*progCb) {
						(*progCb)(StringView(stepStr),
								InstallProgress{progKind, idStr, InstallPhase::Downloading, bytes});
					}
				});
			});
			if (!result || result.installed.empty()) {
				*errStr = result.error.empty() ? (toString("install failed: ") + toString(id))
											   : result.error;
				return false;
			}
			return true;
		};

		if (!installWith(StringView("host"), Kind::Host, StringView(host.native), true, false)) {
			return false;
		}
		if (!installWith(StringView("target"), Kind::Target, StringView(host.native), false, true)) {
			return false;
		}

		// best-effort: not every host publishes an +sprt target, and its absence is not an error
		installer::installComponent(sources, release, host.native + "+sprt", layout, false, true);
		return true;
	}, [this, job, doneCb, errStr](const AppThread::Task &, bool ok) {
		if (!_app) {
			return;
		}
		if (ok) {
			log::info(kLogTag, "installForSystem: done");
			onInstalledStateChanged(this, String());
			// Engine, host and target all landed at once: re-read the manifest and the engine list
			// rather than guessing which rows that touched.
			checkComponents(nullptr);
		} else {
			log::error(kLogTag, "installForSystem: FAILED: ", *errStr);
		}
		finishJob(job, ok ? Status::Ok : Status::ErrorNotImplemented, *errStr);
		if (*doneCb) {
			(*doneCb)(ok, *errStr);
		}
	}, this);
}

void AppController::prepareEngine(StringView ref,
		Function<void(int64_t bytes, int64_t total)> &&onProgress,
		Function<void(bool ok, String err)> &&onDone) {
	auto progCb = std::make_shared<Function<void(int64_t, int64_t)>>(sp::move(onProgress));
	auto doneCb = std::make_shared<Function<void(bool, String)>>(sp::move(onDone));
	auto errStr = std::make_shared<String>();

	const auto refStr = ref.empty() ? toString(getEngineDefaultRef()) : toString(ref);
	const auto sources = _settings.sources;
	const auto layout = _layout;

	log::info(kLogTag, "prepareEngine: cloning ", refStr);

	const auto job =
			beginJob(JobKind::EngineClone, refStr, toString("Cloning engine ", refStr), Kind::Host);

	_app->perform([this, sources, layout, refStr, progCb, errStr, job](
						  const AppThread::Task &) -> bool {
		auto lastStep = std::make_shared<uint64_t>(UINT64_MAX);
		auto cloneResult = cloneEngine(sources, refStr, layout,
				[this, progCb, lastStep, job](const git::CloneProgress &p) {
			if (p.stage != git::CloneStage::Downloading || p.bytesTotal == 0) {
				return;
			}
			marshalProgress(_app, this, *lastStep, static_cast<int64_t>(p.bytesReceived),
					[this, bytes = p.bytesReceived, total = p.bytesTotal, progCb, job] {
				updateJob(job, JobPhase::Cloning, static_cast<int64_t>(bytes),
						static_cast<int64_t>(total));
				if (*progCb) {
					(*progCb)(static_cast<int64_t>(bytes), static_cast<int64_t>(total));
				}
			});
		});
		if (!cloneResult) {
			*errStr = toString("engine clone failed");
			return false;
		}
		linkToolchainsIntoEnginePath(layout, StringView(layout.getEngineDir(refStr)));
		return true;
	}, [this, job, doneCb, errStr](const AppThread::Task &, bool ok) {
		if (!_app) {
			return;
		}
		if (ok) {
			log::info(kLogTag, "prepareEngine: done");
			/* The engine set changed, so both the status and the engines table are stale - and the
			list of cloned engines the table reads from is only refreshed by a check.

			The check is the whole notification: the REFS did not change, so the engines table keeps
			its rows and hears about the one that moved, and the tree is rebuilt from the new list.
			Announcing onEngineRefsChanged here would say the row set changed when it did not, and
			pay for a full rebuild of the table to show one changed button. */
			queryEngine();
			checkComponents(nullptr);
		} else {
			log::error(kLogTag, "prepareEngine: FAILED: ", *errStr);
		}
		finishJob(job, ok ? Status::Ok : Status::ErrorNotImplemented, *errStr);
		if (*doneCb) {
			(*doneCb)(ok, *errStr);
		}
	}, this);
}

void AppController::probeSource(SourceKind kind, StringView url) {
	// One lane PER SOURCE, not one overall: the engine repository and the release mirror are
	// independent, and a shared lane would silently drop the second of two probes started together
	// (which is exactly what "re-check both sources" does). Within a lane the newer request is
	// dropped rather than queued - a probe only ever answers "is what is on screen reachable
	// right now", and the in-flight answer is about to say so.
	auto &running = (kind == SourceKind::EngineRepo) ? _engineProbeRunning : _releaseProbeRunning;
	if (running) {
		return;
	}
	running = true;
	setReachability(kind, Reachability::Checking, StringView());

	auto errStr = std::make_shared<String>();
	auto summary = std::make_shared<String>();
	const auto urlStr = toString(url);

	_app->perform([kind, urlStr, errStr, summary](const AppThread::Task &) -> bool {
		if (kind == SourceKind::EngineRepo) {
			SourceConfig sources;
			sources.engineRepoUrl = urlStr;
			OperationResult status;
			auto refs = listEngineRefs(sources, &status);
			if (!status) {
				*errStr = status.error;
				return false;
			}
			*summary = toString(refs.size(), " refs");
			return true;
		}

		SourceConfig sources;
		sources.releasesRoot = urlStr;
		String text;
		auto result = fetchText(sources.getReleasesRoot(), text);
		if (!result) {
			*errStr = result.error;
			return false;
		}
		// A server that answers but carries no sdk-v* directories is not a usable mirror, and
		// saying so is more useful than reporting "reachable".
		auto release = resolveActiveRelease(text);
		if (release.empty()) {
			*errStr = toString("no releases found");
			return false;
		}
		*summary = release;
		return true;
	}, [this, kind, errStr, summary](const AppThread::Task &, bool ok) {
		((kind == SourceKind::EngineRepo) ? _engineProbeRunning : _releaseProbeRunning) = false;
		if (!_app) {
			return;
		}
		setReachability(kind, ok ? Reachability::Ok : Reachability::Failed,
				ok ? *summary : *errStr);
	}, this);
}

void AppController::setRowStatus(Kind kind, StringView id, RowStatus s) {
	for (auto &row : _catalogue.rows) {
		if (row.kind == kind && StringView(row.id) == id) {
			row.status = s;
			break;
		}
	}

	/* Patching the cached row is only half of it: nothing is bound to that row directly.

	But the two halves are told apart. The TABLE keeps its rows - the same components are still on
	offer, one of them just reads differently now - so it is not dirtied at all: the row's own nodes
	hear this and repaint themselves, which is a label and a style class rather than a rebuild of
	every row on the page. The TREE is a different question: it lists what is on this machine, so a
	component appearing or disappearing changes which rows exist there, and only a rebuild can show
	that.

	Both are unconditional and outside the loop: the row may not be in the catalogue at all (a
	component installed under an older release still shows in the tree), and the navigation branches
	are re-derived from the manifest rather than from `_catalogue`. */
	onRowStatusChanged(this, rowKey(kind, id));
	rebuildNavSources();
}

void AppController::installSelected(Vector<Pair<Kind, String>> items,
		Function<void(const InstallProgress &)> &&onProgress,
		Function<void(bool ok, String err)> &&onDone) {
	if (items.empty()) {
		if (onDone) {
			onDone(true, String());
		}
		return;
	}
	auto progCb = std::make_shared<Function<void(const InstallProgress &)>>(sp::move(onProgress));
	auto doneCb = std::make_shared<Function<void(bool, String)>>(sp::move(onDone));
	auto queue = std::make_shared<Vector<Pair<Kind, String>>>(sp::move(items));
	auto errStr = std::make_shared<String>();

	auto runNext = std::make_shared<Function<void()>>();
	std::weak_ptr<Function<void()>> runNextWeak(runNext);
	// The closure holds `runNext` only as a weak_ptr: a strong self-reference would be a
	// shared_ptr cycle leaking this whole closure on every call. The strong ref threads through
	// one installComponent() at a time — each per-step completion locks the weak ptr and holds the
	// result, installComponent retains that completion until it fires, and the last strong ref
	// drops when the queue drains (so the Function, and every captured shared_ptr, is freed).
	//
	// This serialization is not cosmetic: installComponent writes installed.json and relinks every
	// engine, so two of them at once would race the manifest.
	*runNext = [this, queue, progCb, doneCb, errStr, runNextWeak]() {
		if (queue->empty()) {
			if (*doneCb) {
				(*doneCb)(errStr->empty(), *errStr);
			}
			return;
		}
		auto item = sp::move(queue->front());
		queue->erase(queue->begin());
		auto runNextStrong = runNextWeak.lock();
		installComponent(item.first, item.second, [progCb](const InstallProgress &p) {
			if (*progCb) {
				(*progCb)(p);
			}
		}, [errStr, runNextStrong](bool ok, String err) {
			// installComponent already flipped the row status and announced it.
			if (!ok && errStr->empty()) {
				*errStr = err;
			}
			if (runNextStrong) {
				(*runNextStrong)();
			}
		});
	};
	(*runNext)();
}

void AppController::refreshComponents(bool allInstalled,
		Function<void(const InstallProgress &)> &&onProgress,
		Function<void(bool ok, String err)> &&onDone) {
	Vector<Pair<Kind, String>> items;
	for (const auto &row : _catalogue.rows) {
		if (row.status == RowStatus::UpdateAvailable
				|| (allInstalled && row.status == RowStatus::Installed)) {
			items.emplace_back(row.kind, row.id);
		}
	}
	installSelected(sp::move(items), sp::move(onProgress), sp::move(onDone));
}

// --- projects ---------------------------------------------------------------

Vector<String> AppController::engines() const { return listInstalledEngines(_layout); }

Vector<String> AppController::targets() const { return listInstalledTargets(_layout); }

void AppController::loadProjects(Function<void(Vector<ProjectEntry>)> &&onDone) {
	auto doneCb = std::make_shared<Function<void(Vector<ProjectEntry>)>>(sp::move(onDone));
	auto list = std::make_shared<Vector<ProjectEntry>>();
	const auto layout = _layout;
	_app->perform([layout, list](const AppThread::Task &) -> bool {
		*list = ProjectRegistry::load(layout.getProjectsManifest()).projects;
		return true;
	}, [doneCb, list](const AppThread::Task &, bool) {
		if (*doneCb) {
			(*doneCb)(sp::move(*list));
		}
	}, this);
}

void AppController::createProject(StringView name, StringView location, StringView engine,
		StringView target, Function<void(bool ok, String err, ProjectEntry entry)> &&onDone) {
	auto doneCb = std::make_shared<Function<void(bool, String, ProjectEntry)>>(sp::move(onDone));
	auto errStr = std::make_shared<String>();
	auto entry = std::make_shared<ProjectEntry>();
	const auto nameStr = toString(name);
	const auto locStr = toString(location);
	const auto engStr = toString(engine);
	const auto tgtStr = toString(target);
	const auto layout = _layout;

	log::info(kLogTag, "createProject: name=", nameStr, " loc=", locStr);

	_app->perform(
			[layout, nameStr, locStr, engStr, tgtStr, errStr, entry](
					const AppThread::Task &) -> bool {
		String engineOverride;
		if (!engStr.empty()) {
			auto engPath = layout.getEngineDir(engStr);
			if (isDirectory(engPath)) {
				engineOverride = engPath;
			} else if (isDirectory(engStr)) {
				engineOverride = engStr;
			}
		}
		auto r = scaffoldProject(nameStr, locStr, layout, engineOverride);
		if (!r) {
			*errStr = r.error.empty() ? String("scaffold failed") : r.error;
			return false;
		}
		entry->name = nameStr;
		entry->path = r.path;
		entry->engine = engStr;
		entry->target = tgtStr;
		entry->makeTool = "make";
		char buf[64];
		const auto t = std::time(nullptr);
		std::tm tm{};
		::gmtime_r(&t, &tm); // thread-safe; this runs on the worker job thread
		std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
		entry->createdAt = buf;

		auto reg = ProjectRegistry::load(layout.getProjectsManifest());
		reg.upsert(*entry);
		if (!reg.save(layout.getProjectsManifest())) {
			*errStr = "failed to save projects.json";
			return false;
		}
		return true;
	},
			[doneCb, errStr, entry](const AppThread::Task &, bool ok) {
		if (*doneCb) {
			(*doneCb)(ok, *errStr, ok ? *entry : ProjectEntry{});
		}
	}, this);
}

void AppController::removeProject(StringView path, Function<void(bool ok)> &&onDone) {
	auto doneCb = std::make_shared<Function<void(bool)>>(sp::move(onDone));
	const auto pathStr = toString(path);
	const auto layout = _layout;
	_app->perform([layout, pathStr](const AppThread::Task &) -> bool {
		auto reg = ProjectRegistry::load(layout.getProjectsManifest());
		if (!reg.remove(pathStr)) {
			return false;
		}
		return reg.save(layout.getProjectsManifest());
	}, [doneCb](const AppThread::Task &, bool ok) {
		if (*doneCb) {
			(*doneCb)(ok);
		}
	}, this);
}

void AppController::buildProject(StringView path, StringView target, bool run, bool release,
		Function<void(StringView line)> &&onOutput,
		Function<void(bool ok, String message)> &&onDone) {
	auto doneCb = std::make_shared<Function<void(bool, String)>>(sp::move(onDone));
	auto outCb = std::make_shared<Function<void(StringView)>>(sp::move(onOutput));
	auto msg = std::make_shared<String>();
	const auto pathStr = toString(path);
	const auto tgtStr = toString(target);
	const auto layout = _layout;

	log::info(kLogTag, "buildProject: path=", pathStr, " target=", tgtStr, " run=", run);

	_app->perform(
			[this, layout, pathStr, tgtStr, run, release, outCb, msg](
					const AppThread::Task &) -> bool {
		BuildOptions opts;
		opts.target = tgtStr;
		opts.run = run;
		opts.release = release;

		String engineOverride;
		auto reg = ProjectRegistry::load(layout.getProjectsManifest());
		if (auto *p = reg.find(pathStr)) {
			if (!p->engine.empty()) {
				auto engPath = layout.getEngineDir(p->engine);
				if (isDirectory(engPath)) {
					engineOverride = engPath;
				} else if (isDirectory(p->engine)) {
					engineOverride = p->engine;
				}
			}
		}

		// `sink` must be a NAMED local: Callback does not own its functor, and binding a temporary
		// lambda (which owns a copy of the outCb shared_ptr) would destroy it at the end of the
		// declaration, leaving `stream` — and every line routed through it — pointing at freed
		// storage.
		auto sink = [this, outCb](StringView line) {
			_app->performOnAppThread([outCb, s = toString(line)] {
				if (*outCb) {
					(*outCb)(s);
				}
			}, this);
		};
		const Callback<void(StringView)> stream(sink);
		const Callback<void(StringView)> *outPtr = *outCb ? &stream : nullptr;

		auto r = installer::buildProject(pathStr, layout, opts, engineOverride, outPtr);
		*msg = r.message.empty() ? r.error : r.message;
		return static_cast<bool>(r);
	}, [doneCb, msg](const AppThread::Task &, bool ok) {
		if (*doneCb) {
			(*doneCb)(ok, *msg);
		}
	}, this);
}

// --- native dialogs ---------------------------------------------------------
//
// Nothing here blocks and nothing hops to a worker: the runtime answers its dialogs on the looper
// it was handed (the app thread), and spawnProcess watches its child on that same looper.

void AppController::pickFolder(NotNull<AppWindow> window, StringView prompt,
		Function<void(String)> &&onDone) {
	// One picker at a time. Cancelling the previous request dismisses the dialog still on screen;
	// its callback then fires with ErrorCancelled and clears the slot on its own, which is why the
	// slot is overwritten rather than relied upon to be empty.
	if (_dialogRequest) {
		window->cancelDialog(_dialogRequest);
	}

	auto req = Rc<sprt::window::DialogRequest>::create();
	req->type = sprt::window::DialogType::OpenDirectory;
	req->flags |= sprt::window::DialogFlags::Modal;
	req->title = sprt::window::String(prompt.data(), prompt.size());

	// `target` keeps the controller alive until the callback has run, so the capture can be raw.
	req->target = this;
	req->callback = [this, req = req.get(), onDone = sp::move(onDone)](
							const sprt::window::DialogResult &res) mutable {
		// Only clear the slot if it is still ours: a superseding pick has already replaced it.
		if (_dialogRequest == req) {
			_dialogRequest = nullptr;
		}
		if (onDone) {
			String path;
			if (res.status == Status::Ok && !res.paths.empty()) {
				auto &first = res.paths.front();
				path = String(first.data(), first.size());
			}
			onDone(sp::move(path));
		}
	};

	_dialogRequest = req;
	window->openDialog(req);
}

void AppController::openFolder(NotNull<AppWindow> window, StringView path) {
	if (path.empty()) {
		return;
	}

	auto req = Rc<sprt::window::DialogRequest>::create();
	req->type = sprt::window::DialogType::RevealInFileManager;
	req->paths.emplace_back(path.data(), path.size());

	// Not modal and nothing to collect, but a callback is still required — the runtime guarantees
	// exactly one, and that is the only place a failure can be reported.
	req->callback = [path = path.str<String>()](const sprt::window::DialogResult &res) {
		if (res.status != Status::Ok) {
			log::source().debug("installer", "openFolder failed for ", path, ": ",
					sprt::status::getStatusName(res.status));
		}
	};

	window->openDialog(req);
}

void AppController::promptText(StringView title, StringView def, Function<void(String)> &&onDone) {
	_promptHandle = promptTextAsync(_app, title, def, sp::move(onDone), this);
}

// --- inspector --------------------------------------------------------------

Value AppController::encodeDebugState() const {
	Value ret;
	ret.setString(_layout.config, "config");
	ret.setString(_layout.data, "data");
	ret.setString(_nativeId, "native");
	ret.setBool(_nativeViaEmulation, "nativeViaEmulation");

	Value settings;
	settings.setString(_settings.sources.getEngineRepoUrl(), "engineRepoUrl");
	settings.setString(_settings.sources.getReleasesRoot(), "releaseSourceUrl");
	settings.setBool(_settings.autoUpdateInstaller, "autoUpdateInstaller");
	settings.setBool(_settings.autoUpdateEngine, "autoUpdateEngine");
	settings.setBool(_settings.autoUpdateReleases, "autoUpdateReleases");
	settings.setString(_settings.lang, "lang");
	ret.setValue(sp::move(settings), "settings");

	Value reach;
	reach.setInteger(static_cast<int64_t>(_engineReachability.state), "engine");
	reach.setString(_engineReachability.message, "engineMessage");
	reach.setInteger(static_cast<int64_t>(_releaseReachability.state), "release");
	reach.setString(_releaseReachability.message, "releaseMessage");
	ret.setValue(sp::move(reach), "reachability");

	Value catalogue;
	catalogue.setBool(_hasCatalogue, "loaded");
	catalogue.setString(_catalogue.release, "release");
	size_t hosts = 0, targets = 0, installed = 0, updates = 0, checking = 0;
	for (const auto &row : _catalogue.rows) {
		(row.kind == Kind::Host ? hosts : targets)++;
		switch (row.status) {
		case RowStatus::Installed: ++installed; break;
		case RowStatus::UpdateAvailable: ++updates; break;
		case RowStatus::Checking: ++checking; break;
		case RowStatus::NotInstalled: break;
		}
	}
	catalogue.setInteger(static_cast<int64_t>(hosts), "hosts");
	catalogue.setInteger(static_cast<int64_t>(targets), "targets");
	catalogue.setInteger(static_cast<int64_t>(installed), "installed");
	catalogue.setInteger(static_cast<int64_t>(updates), "updates");
	// Unanswered rows are what an "empty" table and a row with no buttons mean, so they have to be
	// visible from the inspector - neither is distinguishable from a broken view otherwise.
	catalogue.setInteger(static_cast<int64_t>(checking), "checking");
	ret.setValue(sp::move(catalogue), "catalogue");

	Value check;
	check.setBool(_hasInstalledState, "done");
	check.setBool(_checkRunning, "running");
	check.setBool(_checkPending, "pending");
	check.setInteger(static_cast<int64_t>(_installedState.components.size()), "components");
	check.setInteger(static_cast<int64_t>(_missingComponents.size()), "missing");
	check.setInteger(static_cast<int64_t>(_installedEngines.size()), "engines");
	ret.setValue(sp::move(check), "check");

	Value engine;
	engine.setBool(_engineStatus.ready, "ready");
	engine.setString(_engineStatus.reference, "reference");
	engine.setString(_engineStatus.path, "path");
	engine.setInteger(static_cast<int64_t>(_engineRefs.size()), "refs");
	ret.setValue(sp::move(engine), "engine");

	Value jobs;
	for (const auto &job : _jobs) {
		Value item;
		item.setInteger(static_cast<int64_t>(job.id), "id");
		item.setInteger(static_cast<int64_t>(job.kind), "kind");
		item.setString(job.target, "target");
		item.setString(job.title, "title");
		item.setInteger(static_cast<int64_t>(job.phase), "phase");
		item.setInteger(job.bytes, "bytes");
		item.setInteger(job.total, "total");
		if (!job.error.empty()) {
			item.setString(job.error, "error");
		}
		jobs.addValue(sp::move(item));
	}
	ret.setValue(sp::move(jobs), "jobs");
	return ret;
}

} // namespace stappler::xenolith::installer
