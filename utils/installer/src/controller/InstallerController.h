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

namespace STAPPLER_VERSIONIZED stappler::xenolith {
class AppThread;
}

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

enum class RowStatus {
	NotInstalled,
	Installed
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

	// --- app-thread data accessors ---
	const CatalogueData *catalog() const { return _hasCatalog ? &_catalog : nullptr; }
	bool hasCatalog() const { return _hasCatalog; }
	bool isBusy() const { return _busy; }
	void setBusy(bool b) { _busy = b; }

	// Optimistic status flip (app thread) — used after install/uninstall succeeds so the table
	// updates without waiting for a full catalogue reload.
	void setRowStatus(Kind kind, StringView id, RowStatus s);

	const Layout &layout() const { return _layout; }

protected:
	AppThread *_app = nullptr;
	Layout _layout;

	bool _hasCatalog = false;
	CatalogueData _catalog;

	bool _busy = false;
};

} // namespace stappler::xenolith::installer

#endif // UTILS_INSTALLER_SRC_CONTROLLER_INSTALLERCONTROLLER_H_
