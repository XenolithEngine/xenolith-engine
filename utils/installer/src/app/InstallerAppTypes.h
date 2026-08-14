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

#ifndef UTILS_INSTALLER_SRC_APP_INSTALLERAPPTYPES_H_
#define UTILS_INSTALLER_SRC_APP_INSTALLERAPPTYPES_H_

#include "SPICommon.h"
#include "SPIManifest.h"
#include "SPIEngineSource.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

// --- catalogue view models --------------------------------------------------

/* What a row says about itself.

`Checking` is the state every row STARTS in, and it is not cosmetic: whether a component is
installed, and whether what is installed is still the current release, is answered by reading
installed.json and stat()ing the store — file I/O that may not run on the app thread while a page is
being built. Until AppController::checkComponents() has answered for a row, the row does not claim
anything and offers no action; see InstallerActionCell::refresh(). */
enum class RowStatus {
	Checking,
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
	// Unchecked until checkComponents() says otherwise: a freshly listed catalogue knows what the
	// mirror offers and nothing about this machine.
	RowStatus status = RowStatus::Checking;
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

// Identity of a component row, and the key everything per-row is looked up by: a running job, a
// live action cell, a tree row's progress fill. A triple can be both a host and a target, so the
// kind is part of the key.
inline String rowKey(Kind k, StringView id) {
	return toString(getKindName(k), ":", id);
}

// --- jobs -------------------------------------------------------------------

using JobId = uint64_t;
static constexpr JobId kInvalidJobId = 0;

enum class JobKind {
	CatalogueLoad,
	EngineRefs,
	EngineStatus,
	EngineClone,
	ComponentInstall,
	ComponentRemove,
	SystemProvision,
	Probe,
};

enum class JobPhase {
	Queued,
	Running,
	Downloading,
	Verifying,
	Extracting,
	Placing,
	Cloning,
	Done,
	Failed,
};

/* One tracked operation.

Every asynchronous thing the application does becomes one of these, which is what lets a status bar
show aggregate progress and a table row ask "am I busy?" without either of them knowing how the
work is started.

There is deliberately NO cancellation and no `cancellable` flag: the core operations are
straight-line blocking calls whose progress callbacks return void, and NetworkHandle is given no
abort token, so there is nothing a cancel button could do. Adding one would mean threading a
`bool keepGoing` return through SPITransport - a core change, not a UI one. Until then, do not draw
a cancel affordance. */
struct Job {
	JobId id = kInvalidJobId;
	JobKind kind = JobKind::ComponentInstall;

	Kind toolKind = Kind::Target; // Component* jobs only
	String target; // component id, engine ref name, or "engine"/"release" for a probe
	String title; // human-readable label for the status bar

	JobPhase phase = JobPhase::Queued;
	int64_t bytes = 0;
	int64_t total = 0; // 0 means the size is unknown - a clone never reports one

	Status status = Status::Ok;
	String error;

	bool isActive() const { return phase != JobPhase::Done && phase != JobPhase::Failed; }
};

// --- reachability -----------------------------------------------------------

enum class SourceKind {
	EngineRepo,
	Releases,
};

enum class Reachability {
	Unknown, // never probed
	Checking,
	Ok,
	Failed,
};

struct ReachabilityInfo {
	Reachability state = Reachability::Unknown;
	String message; // the failure reason, or a short summary of what answered
};

inline StringView getSourceKindName(SourceKind k) {
	return k == SourceKind::EngineRepo ? StringView("engine") : StringView("release");
}

} // namespace stappler::xenolith::installer

#endif // UTILS_INSTALLER_SRC_APP_INSTALLERAPPTYPES_H_
