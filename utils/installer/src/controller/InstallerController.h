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

#ifndef UTILS_INSTALLER_SRC_CONTROLLER_INSTALLERCONTROLLER_H_
#define UTILS_INSTALLER_SRC_CONTROLLER_INSTALLERCONTROLLER_H_

#include "SPICommon.h"
#include "SPIDirs.h"
#include "SPIManifest.h"
#include "SPICatalogue.h"
#include "SPIState.h"
#include "SPIInstall.h"
#include "SPITransport.h"
#include "SPITriple.h"
#include "SPIEngineSource.h"
#include "SPIScaffold.h"
#include "SPIBuild.h"
#include "SPIProjects.h"
#include "InstallerNativeDialogs.h"

#include <sprt/runtime/window/dialog.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith {
class AppThread;
class AppWindow;
} // namespace stappler::xenolith

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

enum class RowStatus {
	NotInstalled,
	Installed,
	UpdateAvailable,
};
enum class InstallPhase {
	Downloading,
	Verifying,
	Extracting,
	Placing
};

struct CatalogRow {
	Kind kind = Kind::Target;
	String id; // full id incl. variant, e.g. "aarch64-apple-macosx+sprt"
	String triple; // triple without the +variant suffix
	String variant; // variant after "+", empty if none
	uint64_t size = 0;
	RowStatus status = RowStatus::NotInstalled;
	bool isNative = false;
};

struct CatalogueData {
	String release; // the FTP release path segment, e.g. "sdk-v0beta1"
	String nativeId; // the running machine's host triple, "" if unsupported
	Vector<CatalogRow> rows;
};

struct InstallProgress {
	Kind kind = Kind::Target;
	String id;
	InstallPhase phase = InstallPhase::Downloading;
	int64_t bytes = 0;
};

struct EngineStatusInfo {
	bool ready = false; // a usable engine root resolves
	String reference; // short ref name (master/stage/...) or the dir basename
	String shortHash; // commit oid prefix when known, else ""
	String path; // resolved engine root
};

inline String rowKey(Kind k, StringView id) {
	return toString(getKindName(k)) + ":" + toString(id);
}

// Async bridge between the GUI (app thread) and the synchronous installer core (run on the app
// looper's worker pool). Every operation runs its blocking work off-thread and delivers its
// completion callback (and progress) back onto the app thread, so the scene graph is never touched
// from a worker. The data accessors below are app-thread-only.
class InstallerController : public Ref {
public:
	virtual ~InstallerController();

	// `app` is the UI/app thread; its worker pool runs the blocking core calls.
	bool init(AppThread *app);

	// Load the remote catalogue and cross-reference the installed-state registry. On success the
	// in-memory catalogue is replaced and `onDone(ok, err)` fires on the app thread.
	void loadCatalog(Function<void(bool ok, String err)> &&onDone = nullptr);

	// Install one component by (id, kind). `onProgress` fires on the app thread during download.
	// On success the matching row is marked Installed optimistically (no full reload).
	void installComponent(Kind kind, StringView id,
			Function<void(const InstallProgress &)> &&onProgress = nullptr,
			Function<void(bool ok, String err)> &&onDone = nullptr);

	// Remove an installed component's files (idempotent). On success the row is marked NotInstalled.
	void uninstallComponent(Kind kind, StringView id,
			Function<void(bool ok, String err)> &&onDone = nullptr);

	// One-click provisioning: engine + native host + native target (+sprt target if present).
	// `step` names the phase ("engine"/"host"/"target"); one installComponent runs at a time.
	void installForSystem(
			Function<void(StringView step, const InstallProgress &)> &&onProgress = nullptr,
			Function<void(bool ok, String err)> &&onDone = nullptr);

	// Resolve the engine root (cloned ref / external override) into EngineStatusInfo. Fires onDone
	// on the app thread regardless of whether an engine resolves.
	void queryEngine(Function<void(const EngineStatusInfo &)> &&onDone = nullptr);

	// Ensure the default engine ref is cloned and linked (a re-download when one already exists).
	void prepareEngine(Function<void(int64_t bytes, int64_t total)> &&onProgress = nullptr,
			Function<void(bool ok, String err)> &&onDone = nullptr);

	// --- projects ---
	void loadProjects(Function<void(Vector<ProjectEntry>)> &&onDone = nullptr);
	void createProject(StringView name, StringView location, StringView engine, StringView target,
			Function<void(bool ok, String err, ProjectEntry entry)> &&onDone = nullptr);
	void removeProject(StringView path, Function<void(bool ok)> &&onDone = nullptr);
	void buildProject(StringView path, StringView target, bool run, bool release,
			Function<void(StringView line)> &&onOutput = nullptr,
			Function<void(bool ok, String message)> &&onDone = nullptr);

	// --- native OS dialogs / file manager ---
	//
	// The first two go through the runtime's system-dialog API (sprt/runtime/window/dialog.h),
	// which picks the best thing the platform has — the portal or IFileDialog where there is one, a
	// desktop helper where there is not. They need the owning window: the dialog is parented to it,
	// blocks it while it is up, and is dismissed (with the callback still answered) if it closes.
	//
	// `onDone` always runs, exactly once, on the app thread. An empty path means the user declined
	// or the platform had nothing to offer, which the callers treat the same way.
	void pickFolder(NotNull<AppWindow> window, StringView prompt,
			Function<void(String path)> &&onDone = nullptr);
	void openFolder(NotNull<AppWindow> window, StringView path);

	// Still a spawned helper: there is no system "ask for a line of text" dialog to wrap. Runs on
	// the app looper, so nothing blocks — see InstallerNativeDialogs.h.
	void promptText(StringView title, StringView def,
			Function<void(String text)> &&onDone = nullptr);

	Vector<String> engines() const;
	Vector<String> targets() const;

	// --- app-thread data accessors ---
	const CatalogueData *catalog() const { return _hasCatalog ? &_catalog : nullptr; }
	bool hasCatalog() const { return _hasCatalog; }
	bool isBusy() const { return _busy; }
	void setBusy(bool b) { _busy = b; }

	// Optimistic status flip (app thread) — used after install/uninstall succeeds so the table
	// updates without waiting for a full catalogue reload.
	void setRowStatus(Kind kind, StringView id, RowStatus s);

	// Install every NotInstalled/UpdateAvailable row currently selected in the UI.
	void installSelected(Vector<Pair<Kind, String>> items,
			Function<void(const InstallProgress &)> &&onProgress = nullptr,
			Function<void(bool ok, String err)> &&onDone = nullptr);

	// Reinstall every UpdateAvailable row (and optionally every Installed row when `all`).
	void refreshComponents(bool allInstalled,
			Function<void(const InstallProgress &)> &&onProgress = nullptr,
			Function<void(bool ok, String err)> &&onDone = nullptr);

	const Layout &layout() const { return _layout; }

protected:
	AppThread *_app = nullptr;
	Layout _layout;

	// The text prompt is still a spawned helper, and an Rc<ProcessHandle> IS the child — dropping
	// the last one kills it — so this slot is what keeps the prompt on screen.
	Rc<ProcessHandle> _promptHandle;

	// The live folder picker, kept because the request doubles as its cancellation token: opening a
	// second picker dismisses the first, and the callback of the dismissed one still fires.
	Rc<sprt::window::DialogRequest> _dialogRequest;

	bool _hasCatalog = false;
	CatalogueData _catalog;

	bool _busy = false;
};

} // namespace stappler::xenolith::installer

#endif // UTILS_INSTALLER_SRC_CONTROLLER_INSTALLERCONTROLLER_H_
