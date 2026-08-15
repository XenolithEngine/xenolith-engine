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

#ifndef UTILS_INSTALLER_SRC_APP_INSTALLERAPPCONTROLLER_H_
#define UTILS_INSTALLER_SRC_APP_INSTALLERAPPCONTROLLER_H_

#include "InstallerAppTypes.h"
#include "InstallerNativeDialogs.h"

#include "SPIDirs.h"
#include "SPICatalogue.h"
#include "SPISettings.h"
#include "SPIState.h"
#include "SPIInstall.h"
#include "SPITransport.h"
#include "SPITriple.h"
#include "SPIScaffold.h"
#include "SPIBuild.h"
#include "SPIProjects.h"

#include "SPDataModel.h"
#include "XLEvent.h"

#include <sprt/runtime/window/dialog.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith {
class AppThread;
class AppWindow;
} // namespace stappler::xenolith

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

/* The single point of control for the application.

Everything that is not a scene node lives here: the resolved install layout, the user's settings,
the catalogue and installed-state caches, the registry of running operations, and the async bridge
onto the synchronous installer core. Widgets do not own any of it and are not handed pointers to
each other - they read through getInstance() and subscribe to the events below, which is what lets
a status bar, a tree row and a table cell all react to one download without any of them knowing the
others exist.

LIFETIME. The instance is created once and DELIBERATELY NEVER DESTROYED, the same way
xenolith::HotkeyRegistry is: it outlives every scene and every window, and a static destructor
would race whatever is still tearing down. That is exactly why detach() is not optional - it is
what drops the scene-lifetime Rc's (a dialog request, a spawned prompt, the data models) that must
not survive into static destruction, long after the renderer is gone.

It derives from Ref for one concrete reason: AppThread::perform() takes a `Ref *` to pin a task's
captures, and every asynchronous operation here needs one.

THREADING. Every public getter, every event emission and every data::Model mutation happens on the
APP THREAD only. Blocking core calls run inside perform()'s execute lambda on the worker pool and
may touch only (a) values copied into the task and (b) `const Layout &`, which is immutable after
attach(). Settings are COPIED into a task when a worker needs a URL - never read from a worker
through getSettings(), which can be rewritten from the settings form at any moment.

EVENTS vs SOURCES. Two channels, on purpose:
  - Row DATA goes through the data::Models below. TreeView and TableView already watch their whole
    model through a DataListener, so a view bound to one subscribes to nothing and rebuilds by
    itself when the controller calls setDirty().
  - Everything a model cannot express goes through the EventHeaders below, consumed with an
    EventListener on the node that cares.
And the rule that follows from having both: onJobProgress must NEVER dirty a model. Dirtying
rebuilds row nodes, and a download would do that many times a second. Progress mutates the live
node it belongs to and nothing else. */
class AppController : public Ref {
public:
	// --- events -------------------------------------------------------------
	//
	// Payloads are minimal because EventHeader::send only carries scalars / String / Value / Ref*;
	// a subscriber re-reads whatever it needs through getInstance().

	static EventHeader onReady; // attach() finished: layout, settings and installed state are in
	static EventHeader onSettingsChanged; // Value: array of changed keys, null = everything
	static EventHeader onCatalogueChanged; // the catalogue was replaced
	static EventHeader onEngineRefsChanged; // listEngineRefs landed
	static EventHeader onEngineStatusChanged; // queryEngine landed
	static EventHeader onInstalledStateChanged; // String: rowKey, or "" for a wholesale change
	/* One or more rows changed STATUS, and nothing else did. String: the rowKey when exactly one
	row moved, "" when a check re-judged several.

	Separate from onInstalledStateChanged because the two ask for different things: that one says
	the row SET may have changed, which only a rebuilt view can show, while this one says a row
	still exists and now reads differently - which is a label and a style class on a node that is
	already on screen. Dirtying a model for it would rebuild every row node in the table, and a
	table that flashes on every status change is what that cost looks like. */
	static EventHeader onRowStatusChanged;
	static EventHeader onJobStarted; // uint64_t: JobId
	static EventHeader onJobProgress; // uint64_t: JobId
	static EventHeader onJobFinished; // uint64_t: JobId
	static EventHeader onReachabilityChanged; // String: "engine" | "release"
	static EventHeader onError; // String: a message worth showing the user

	// The instance is never destroyed; see the class documentation.
	static AppController *getInstance();

	virtual ~AppController();

	// Idempotent. Called from InstallerSceneContent::handleEnter.
	bool attach(NotNull<AppThread>);
	// Called from InstallerSceneContent::handleExit. Drops every scene-lifetime Rc and clears the
	// job registry; the instance itself stays alive and can be attached again.
	void detach();

	bool isAttached() const { return _app != nullptr; }
	AppThread *getAppThread() const { return _app; }

	// --- configuration and persisted data (app thread) ----------------------

	const Layout &getLayout() const { return _layout; }
	const Settings &getSettings() const { return _settings; }

	// Writes one field, persists the whole file, fires onSettingsChanged, and kicks a reachability
	// probe when the field was a source URL. `key` is one of the settings.json keys — the set is
	// Settings::getFields() (SPISettings.h), which is also what `xenolith-cli config` lists. An
	// unknown key is refused with ErrorInvalidArguemnt.
	//
	// "enginePath" / "toolchainsPath" also re-derive getLayout(), but ONLY while !isBusy(): a worker
	// holds a `const Layout &`. Changed during an install or a build, they are saved and take effect
	// on the next start.
	Status setSettingsField(StringView key, const Value &);

	bool getToolAutoUpdate(Kind kind, StringView id) const {
		return _settings.getToolAutoUpdate(kind, id);
	}
	void setToolAutoUpdate(Kind kind, StringView id, bool);

	const CatalogueData *getCatalogue() const { return _hasCatalogue ? &_catalogue : nullptr; }
	bool hasCatalogue() const { return _hasCatalogue; }
	StringView getNativeId() const { return _nativeId; }
	// The native toolchain is reached through emulation (Rosetta, a WOW layer) rather than natively.
	bool isNativeViaEmulation() const { return _nativeViaEmulation; }

	const EngineStatusInfo &getEngineStatus() const { return _engineStatus; }
	SpanView<EngineRef> getEngineRefs() const { return _engineRefs; }
	bool hasEngineRefs() const { return _hasEngineRefs; }

	const ReachabilityInfo &getReachability(SourceKind) const;

	// --- data sources for the views ----------------------------------------
	//
	// Owned here and dirtied here. A TreeView/TableView bound to one of these needs no event
	// subscription of its own.

	// The left navigation tree: Xenolith -> Engines / Hosts / Targets, plus a Projects leaf that is
	// present but disabled (project management is a separate piece of work; leaving the branch out
	// entirely would make its later return a structural change).
	data::Model *getNavModel() const { return _navModel; }
	data::Model *getToolsSource(Kind) const;
	data::Model *getEnginesSource() const { return _enginesSource; }

	// --- job registry (app thread) -----------------------------------------

	const Job *getJob(JobId) const;
	// "Is this row busy?" - the tables' and the tree's hot path.
	const Job *findJob(JobKind, StringView target) const;
	SpanView<Job> getJobs() const { return _jobs; }
	size_t getActiveJobCount() const;
	// Weighted by bytes over every active job that reports a total; nan() when none does, which is
	// the honest answer for a clone (git reports received bytes and no total).
	float getAggregateProgress() const;
	bool isBusy() const { return getActiveJobCount() > 0; }

	// --- operations ---------------------------------------------------------
	//
	// All of these are app-thread entry points that run their blocking work on the worker pool and
	// deliver completion back on the app thread. Each registers a Job, so a caller that only wants
	// to know "did something happen" can listen for onJob* instead of passing a callback.

	void loadCatalogue(Function<void(bool ok, String err)> &&onDone = nullptr);
	void loadEngineRefs(Function<void(bool ok, String err)> &&onDone = nullptr);
	void queryEngine(Function<void(const EngineStatusInfo &)> &&onDone = nullptr);

	/* Re-read what is actually on this machine and re-decide every row's status.

	This is the "actuality check": it loads installed.json, stat()s every component the manifest
	claims, lists the cloned engines, and then answers, per row, NotInstalled / Installed /
	UpdateAvailable (installed under a release other than the catalogue's). All of the I/O happens on
	the worker pool - a manifest parse and a directory walk are not things a page may do while it is
	opening, which is why nothing on the app thread ever touches the disk for this any more and reads
	the caches below instead.

	Cheap to call and safe to call often: a page calls it every time it is shown. A second request
	while one is in flight is COALESCED into one re-run afterwards rather than queued, because the
	answer is always "what is on disk now" and the in-flight one is about to be stale either way.

	Rows that have never been answered for stay RowStatus::Checking, and a Checking row shows no
	action controls. */
	void checkComponents(Function<void()> &&onDone = nullptr);

	// A check has been performed at least once, so the row statuses mean something.
	bool hasCheckedComponents() const { return _hasInstalledState; }

	/* The status of one row, live.

	This is what a row's own nodes read, INSTEAD of the "status" in the Value they were built from:
	a status change no longer dirties the model, so that Value is a snapshot of whenever the row
	was last materialized, while these two are the truth. Both answer Checking for a row nothing is
	known about yet, which is the state that hides the row's controls. */
	RowStatus getToolStatus(Kind, StringView id) const;
	RowStatus getEngineRowStatus(StringView ref) const;

	void installComponent(Kind, StringView id,
			Function<void(const InstallProgress &)> &&onProgress = nullptr,
			Function<void(bool ok, String err)> &&onDone = nullptr);
	void uninstallComponent(Kind, StringView id,
			Function<void(bool ok, String err)> &&onDone = nullptr);

	// Install every listed component, one at a time: installComponent writes installed.json and
	// relinks every engine, so concurrent installs would race the manifest.
	void installSelected(Vector<Pair<Kind, String>> items,
			Function<void(const InstallProgress &)> &&onProgress = nullptr,
			Function<void(bool ok, String err)> &&onDone = nullptr);

	// Reinstall every UpdateAvailable row, and every Installed row too when `allInstalled`.
	void refreshComponents(bool allInstalled,
			Function<void(const InstallProgress &)> &&onProgress = nullptr,
			Function<void(bool ok, String err)> &&onDone = nullptr);

	// One-click provisioning: engine + native host + native target (+sprt target when published).
	void installForSystem(
			Function<void(StringView step, const InstallProgress &)> &&onProgress = nullptr,
			Function<void(bool ok, String err)> &&onDone = nullptr);

	void prepareEngine(StringView ref, Function<void(int64_t bytes, int64_t total)> &&onProgress = nullptr,
			Function<void(bool ok, String err)> &&onDone = nullptr);

	// Probe a source URL for reachability. Probes run on their own lane, one at a time, so a check
	// from the settings form never queues behind a 400 MB toolchain download.
	void probeSource(SourceKind, StringView url);

	// Optimistic status flip so a table updates without a full catalogue reload.
	void setRowStatus(Kind, StringView id, RowStatus);

	// --- projects ------------------------------------------------------------
	//
	// Carried over intact and currently reached from nowhere in the UI: project management is a
	// separate piece of work, and these are the working shapes it will re-use.

	Vector<String> engines() const;
	Vector<String> targets() const;
	void loadProjects(Function<void(Vector<ProjectEntry>)> &&onDone = nullptr);
	void createProject(StringView name, StringView location, StringView engine, StringView target,
			Function<void(bool ok, String err, ProjectEntry entry)> &&onDone = nullptr);
	void removeProject(StringView path, Function<void(bool ok)> &&onDone = nullptr);
	void buildProject(StringView path, StringView target, bool run, bool release,
			Function<void(StringView line)> &&onOutput = nullptr,
			Function<void(bool ok, String message)> &&onDone = nullptr);

	// --- native OS dialogs / file manager ------------------------------------
	//
	// The first two go through the runtime's system-dialog API, which picks the best thing the
	// platform has. They need the owning window: the dialog is parented to it, blocks it while it is
	// up, and is dismissed (with the callback still answered) if it closes. `onDone` always runs,
	// exactly once, on the app thread; an empty path means the user declined or the platform had
	// nothing to offer, which callers treat the same way.

	void pickFolder(NotNull<AppWindow>, StringView prompt,
			Function<void(String path)> &&onDone = nullptr);
	void openFolder(NotNull<AppWindow>, StringView path);
	// Still a spawned helper: there is no system "ask for a line of text" dialog to wrap.
	void promptText(StringView title, StringView def, Function<void(String text)> &&onDone = nullptr);

	// A compact dump of the whole controller state, for the inspector: both views are virtualized,
	// so what is on screen is never the whole model and the model has to be inspectable directly.
	Value encodeDebugState() const;

protected:
	virtual bool init();

	// --- job registry internals ---
	JobId beginJob(JobKind, StringView target, StringView title, Kind = Kind::Target);
	void updateJob(JobId, JobPhase, int64_t bytes, int64_t total);
	void finishJob(JobId, Status, StringView error = StringView());
	Job *getMutableJob(JobId);

	// The row payloads the spans answer with, derived from the caches on demand. Kept here rather
	// than materialized into the model so there is only one place row data can go stale.
	Vector<Value> makeToolRows(Kind) const;
	Vector<Value> makeEngineRows() const;
	// The INSTALLED entries of one branch of the navigation tree, plus its trailing "add new" row.
	Vector<Value> makeNavRows(StringView node) const;

	// Push the current row COUNTS onto the spans and dirty them, so every bound view re-reads.
	// This is the WHOLESALE path: it rebuilds every row node of every bound view, so it belongs to a
	// changed row SET (a catalogue that landed, engine refs that landed) and never to a status.
	void rebuildSources();
	// Only the navigation branches. What installing or removing a component really changes: the tree
	// lists what is on the machine, while the tables list what the mirror offers and keep their rows.
	void rebuildNavSources();
	// Replace one branch's rows with what the caches now say. Refilling is a structural change to
	// the model, and a Model dirties as a whole, so the bound TreeView follows with no help.
	void fillNavBranch(data::Model::Node *branch, StringView node);
	void setReachability(SourceKind, Reachability, StringView message);

	// Decide every catalogue row's status from the checked local state. Pure and app-thread-only:
	// the I/O it decides from was done by checkComponents()'s worker. A no-op while either half of
	// the answer is missing, which is what leaves rows Checking until both have landed.
	void applyInstalledState();
	// The manifest claims this component but its files are gone.
	bool isComponentMissing(StringView key) const;

	AppThread *_app = nullptr;
	Layout _layout;
	Settings _settings;

	bool _hasCatalogue = false;
	CatalogueData _catalogue;

	String _nativeId;
	bool _nativeViaEmulation = false;

	EngineStatusInfo _engineStatus;
	Vector<EngineRef> _engineRefs;
	bool _hasEngineRefs = false;

	/* The CHECKED view of this machine, refreshed only by checkComponents().

	Everything that runs on the app thread - the row payloads, the navigation branches - answers from
	these and never from the filesystem. They used to read the disk inline instead: makeNavRows()
	parsed installed.json and makeEngineRows() stat()ed a directory PER ROW, inside a data::Source's
	batch callback, which is the app thread, in the middle of building a page. That is the work that
	made opening a page take time. */
	InstalledState _installedState;
	Vector<String> _installedEngines; // engine refs whose clone directory exists
	Vector<String> _missingComponents; // rowKey()s the manifest claims but the store does not have
	bool _hasInstalledState = false;
	bool _hasInstalledEngines = false;

	// One check at a time; a request arriving while one runs re-runs it once at the end (see
	// checkComponents).
	bool _checkRunning = false;
	bool _checkPending = false;

	ReachabilityInfo _engineReachability;
	ReachabilityInfo _releaseReachability;

	Vector<Job> _jobs;
	JobId _nextJobId = 1;

	// One span each: the rows are answered from the caches, never stored here.
	Rc<data::Model> _hostsSource;
	Rc<data::Model> _targetsSource;
	Rc<data::Model> _enginesSource;

	/* The navigation tree, as one data::Model.

	A Model is a single Subscription for the whole tree, so refilling a branch reaches the bound
	TreeView by itself — the pane needs no invalidateSource() and this class needs no re-publishing
	of counts. The three branch nodes are raw pointers because the MODEL owns them; they stay valid
	until _navModel is released. */
	Rc<data::Model> _navModel;
	data::Model::Node *_navEngines = nullptr;
	data::Model::Node *_navHosts = nullptr;
	data::Model::Node *_navTargets = nullptr;

	// The text prompt is still a spawned helper, and an Rc<ProcessHandle> IS the child - dropping
	// the last one kills it - so this slot is what keeps the prompt on screen.
	Rc<ProcessHandle> _promptHandle;

	// The live folder picker, kept because the request doubles as its cancellation token: opening a
	// second picker dismisses the first, and the callback of the dismissed one still fires.
	Rc<sprt::window::DialogRequest> _dialogRequest;

	// Probes run beside the mutating operations, one lane per source (see probeSource).
	bool _engineProbeRunning = false;
	bool _releaseProbeRunning = false;
};

} // namespace stappler::xenolith::installer

#endif // UTILS_INSTALLER_SRC_APP_INSTALLERAPPCONTROLLER_H_
