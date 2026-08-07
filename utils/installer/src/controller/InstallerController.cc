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

#include "InstallerController.h"

#include "XLAppThread.h"
#include "SPLog.h"

#include <cstdint>
#include <ctime>
#include <memory>

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

static constexpr auto kLogTag = StringView("installer");

// Progress is reported once per this many bytes; the core's raw callback fires far more often than
// a UI can use.
static constexpr uint64_t kProgressStep = 256 * 1'024;

InstallerController::~InstallerController() { }

bool InstallerController::init(AppThread *app) {
	_app = app;
	_layout = Layout::resolveFromEnv();
	log::info(kLogTag, "controller init: config=", _layout.config, " data=", _layout.data);
	return true;
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

void InstallerController::loadCatalog(Function<void(bool ok, String err)> &&onDone) {
	auto doneCb = std::make_shared<Function<void(bool, String)>>(sp::move(onDone));
	auto errStr = std::make_shared<String>();
	auto built = std::make_shared<CatalogueData>();

	log::info(kLogTag, "loadCatalog: discovering release under ", getFtpReleasesRoot());

	_app->perform([this, errStr, built](const AppThread::Task &) -> bool {
		String release = toString(getDefaultRelease());
		String releasesText;
		if (fetchText(getFtpReleasesRoot(), releasesText)) {
			release = resolveActiveRelease(releasesText);
		}

		auto base = getFtpReleaseBase(release);
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
		auto state = InstalledState::load(_layout.getInstalledManifest());

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
			if (auto *ic = state.get(c.id, c.kind)) {
				row.status = (ic->release != release) ? RowStatus::UpdateAvailable
													  : RowStatus::Installed;
			} else {
				row.status = RowStatus::NotInstalled;
			}
			row.isNative = (c.triple == built->nativeId);
			built->rows.push_back(sp::move(row));
		}
		return true;
	}, [this, doneCb, errStr, built](const AppThread::Task &, bool ok) {
		if (ok) {
			_catalog = sp::move(*built);
			_hasCatalog = true;

			size_t installed = 0;
			for (const auto &row : _catalog.rows) {
				if (row.status != RowStatus::NotInstalled) {
					++installed;
				}
			}
			log::info(kLogTag, "loadCatalog: ok, rows=", _catalog.rows.size(),
					" installed=", installed, " native=", _catalog.nativeId,
					" release=", _catalog.release);
		} else {
			log::error(kLogTag, "loadCatalog: FAILED: ", *errStr);
		}
		if (*doneCb) {
			(*doneCb)(ok, *errStr);
		}
	}, this);
}

void InstallerController::installComponent(Kind kind, StringView id,
		Function<void(const InstallProgress &)> &&onProgress,
		Function<void(bool ok, String err)> &&onDone) {
	String idStr = toString(id);
	const bool wantHost = (kind == Kind::Host);
	const bool wantTarget = (kind == Kind::Target);
	auto progCb = std::make_shared<Function<void(const InstallProgress &)>>(sp::move(onProgress));
	auto doneCb = std::make_shared<Function<void(bool, String)>>(sp::move(onDone));
	auto errStr = std::make_shared<String>();
	auto lastStep = std::make_shared<uint64_t>(UINT64_MAX);

	log::info(kLogTag, "installComponent: kind=", getKindName(kind), " id=", idStr);

	_app->perform(
			[this, idStr, kind, wantHost, wantTarget, progCb, errStr, lastStep](
					const AppThread::Task &) -> bool {
		auto result = installer::installComponent(idStr, _layout, wantHost, wantTarget,
				[this, kind, idStr, progCb, lastStep](int64_t bytes, int64_t) {
			marshalProgress(_app, this, *lastStep, bytes, [kind, idStr, bytes, progCb] {
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
			[idStr, doneCb, errStr](const AppThread::Task &, bool ok) {
		if (ok) {
			log::info(kLogTag, "installComponent: done id=", idStr);
		} else {
			log::error(kLogTag, "installComponent: FAILED id=", idStr, " err=", *errStr);
		}
		if (*doneCb) {
			(*doneCb)(ok, *errStr);
		}
	}, this);
}

void InstallerController::uninstallComponent(Kind kind, StringView id,
		Function<void(bool ok, String err)> &&onDone) {
	String idStr = toString(id);
	auto doneCb = std::make_shared<Function<void(bool, String)>>(sp::move(onDone));
	auto errStr = std::make_shared<String>();

	log::info(kLogTag, "uninstallComponent: kind=", getKindName(kind), " id=", idStr);

	_app->perform([this, idStr, kind, errStr](const AppThread::Task &) -> bool {
		// uninstall is idempotent (a missing dir is not an error); only a real removal failure
		// comes back false
		if (!removeComponent(_layout, kind, idStr)) {
			*errStr = toString("uninstall failed: ") + idStr;
			return false;
		}
		return true;
	}, [idStr, doneCb, errStr](const AppThread::Task &, bool ok) {
		if (ok) {
			log::info(kLogTag, "uninstallComponent: done id=", idStr);
		} else {
			log::error(kLogTag, "uninstallComponent: FAILED id=", idStr);
		}
		if (*doneCb) {
			(*doneCb)(ok, *errStr);
		}
	}, this);
}

void InstallerController::installForSystem(
		Function<void(StringView step, const InstallProgress &)> &&onProgress,
		Function<void(bool ok, String err)> &&onDone) {
	auto progCb = std::make_shared<Function<void(StringView, const InstallProgress &)>>(
			sp::move(onProgress));
	auto doneCb = std::make_shared<Function<void(bool, String)>>(sp::move(onDone));
	auto errStr = std::make_shared<String>();

	log::info(kLogTag, "installForSystem: starting (engine + host + target)");

	_app->perform([this, progCb, errStr](const AppThread::Task &) -> bool {
		// 1. Engine — clone the default ref if none resolves.
		bool ok = false;
		auto root = resolveEngineRoot(_layout, StringView(), &ok);
		if (!ok) {
			log::info(kLogTag, "installForSystem: cloning engine ", getEngineDefaultRef());
			auto lastStep = std::make_shared<uint64_t>(UINT64_MAX);
			auto cloneResult = cloneEngine(getEngineDefaultRef(), _layout,
					[this, progCb, lastStep](const git::CloneProgress &p) {
				if (p.stage != git::CloneStage::Downloading || p.bytesTotal == 0) {
					return;
				}
				auto received = static_cast<int64_t>(p.bytesReceived);
				marshalProgress(_app, this, *lastStep, received, [received, progCb] {
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
			root = _layout.getEngineDir(getEngineDefaultRef());
		}
		linkToolchainsIntoEnginePath(_layout, StringView(root));

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
			auto result = installer::installComponent(id, _layout, wantHost, wantTarget,
					[this, progCb, stepStr = toString(step), idStr = toString(id), progKind,
							lastStep](int64_t bytes, int64_t) {
				marshalProgress(_app, this, *lastStep, bytes,
						[stepStr, idStr, progKind, bytes, progCb] {
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
		if (!installWith(StringView("target"), Kind::Target, StringView(host.native), false,
					true)) {
			return false;
		}

		// best-effort: not every host publishes an +sprt target, and its absence is not an error
		installer::installComponent(host.native + "+sprt", _layout, false, true);
		return true;
	}, [doneCb, errStr](const AppThread::Task &, bool ok) {
		if (ok) {
			log::info(kLogTag, "installForSystem: done");
		} else {
			log::error(kLogTag, "installForSystem: FAILED: ", *errStr);
		}
		if (*doneCb) {
			(*doneCb)(ok, *errStr);
		}
	}, this);
}

void InstallerController::queryEngine(Function<void(const EngineStatusInfo &)> &&onDone) {
	auto doneCb = std::make_shared<Function<void(const EngineStatusInfo &)>>(sp::move(onDone));
	auto info = std::make_shared<EngineStatusInfo>();

	_app->perform([this, info](const AppThread::Task &) -> bool {
		bool ok = false;
		auto root = resolveEngineRoot(_layout, StringView(), &ok);
		info->ready = ok;
		info->path = root;
		if (ok) {
			// reference = the basename of the resolved engine dir (a ref name for cloned engines,
			// the checkout dir name for an external override)
			info->reference = toString(filepath::lastComponent(root));
		}
		return true;
	}, [info, doneCb](const AppThread::Task &, bool) {
		log::info(kLogTag, "queryEngine: ready=", info->ready, " ref=", info->reference,
				" path=", info->path);
		if (*doneCb) {
			(*doneCb)(*info);
		}
	}, this);
}

void InstallerController::prepareEngine(Function<void(int64_t bytes, int64_t total)> &&onProgress,
		Function<void(bool ok, String err)> &&onDone) {
	auto progCb = std::make_shared<Function<void(int64_t, int64_t)>>(sp::move(onProgress));
	auto doneCb = std::make_shared<Function<void(bool, String)>>(sp::move(onDone));
	auto errStr = std::make_shared<String>();

	log::info(kLogTag, "prepareEngine: cloning ", getEngineDefaultRef());

	_app->perform([this, progCb, errStr](const AppThread::Task &) -> bool {
		auto lastStep = std::make_shared<uint64_t>(UINT64_MAX);
		auto cloneResult = cloneEngine(getEngineDefaultRef(), _layout,
				[this, progCb, lastStep](const git::CloneProgress &p) {
			if (p.stage != git::CloneStage::Downloading || p.bytesTotal == 0) {
				return;
			}
			marshalProgress(_app, this, *lastStep, static_cast<int64_t>(p.bytesReceived),
					[bytes = p.bytesReceived, total = p.bytesTotal, progCb] {
				if (*progCb) {
					(*progCb)(static_cast<int64_t>(bytes), static_cast<int64_t>(total));
				}
			});
		});
		if (!cloneResult) {
			*errStr = toString("engine clone failed");
			return false;
		}
		linkToolchainsIntoEnginePath(_layout,
				StringView(_layout.getEngineDir(getEngineDefaultRef())));
		return true;
	}, [doneCb, errStr](const AppThread::Task &, bool ok) {
		if (ok) {
			log::info(kLogTag, "prepareEngine: done");
		} else {
			log::error(kLogTag, "prepareEngine: FAILED: ", *errStr);
		}
		if (*doneCb) {
			(*doneCb)(ok, *errStr);
		}
	}, this);
}

void InstallerController::setRowStatus(Kind kind, StringView id, RowStatus s) {
	for (auto &row : _catalog.rows) {
		if (row.kind == kind && StringView(row.id) == id) {
			row.status = s;
			return;
		}
	}
}

void InstallerController::installSelected(Vector<Pair<Kind, String>> items,
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
		}, [this, item, errStr, runNextStrong](bool ok, String err) {
			if (ok) {
				setRowStatus(item.first, item.second, RowStatus::Installed);
			} else if (errStr->empty()) {
				*errStr = err;
			}
			if (runNextStrong) {
				(*runNextStrong)();
			}
		});
	};
	(*runNext)();
}

void InstallerController::refreshComponents(bool allInstalled,
		Function<void(const InstallProgress &)> &&onProgress,
		Function<void(bool ok, String err)> &&onDone) {
	Vector<Pair<Kind, String>> items;
	for (const auto &row : _catalog.rows) {
		if (row.status == RowStatus::UpdateAvailable
				|| (allInstalled && row.status == RowStatus::Installed)) {
			items.emplace_back(row.kind, row.id);
		}
	}
	installSelected(sp::move(items), sp::move(onProgress), sp::move(onDone));
}

Vector<String> InstallerController::engines() const { return listInstalledEngines(_layout); }

Vector<String> InstallerController::targets() const { return listInstalledTargets(_layout); }

void InstallerController::loadProjects(Function<void(Vector<ProjectEntry>)> &&onDone) {
	auto doneCb = std::make_shared<Function<void(Vector<ProjectEntry>)>>(sp::move(onDone));
	auto list = std::make_shared<Vector<ProjectEntry>>();
	_app->perform([this, list](const AppThread::Task &) -> bool {
		*list = ProjectRegistry::load(_layout.getProjectsManifest()).projects;
		return true;
	}, [doneCb, list](const AppThread::Task &, bool) {
		if (*doneCb) {
			(*doneCb)(sp::move(*list));
		}
	}, this);
}

void InstallerController::createProject(StringView name, StringView location, StringView engine,
		StringView target, Function<void(bool ok, String err, ProjectEntry entry)> &&onDone) {
	auto doneCb = std::make_shared<Function<void(bool, String, ProjectEntry)>>(sp::move(onDone));
	auto errStr = std::make_shared<String>();
	auto entry = std::make_shared<ProjectEntry>();
	const auto nameStr = toString(name);
	const auto locStr = toString(location);
	const auto engStr = toString(engine);
	const auto tgtStr = toString(target);

	log::info(kLogTag, "createProject: name=", nameStr, " loc=", locStr);

	_app->perform(
			[this, nameStr, locStr, engStr, tgtStr, errStr, entry](
					const AppThread::Task &) -> bool {
		String engineOverride;
		if (!engStr.empty()) {
			auto engPath = _layout.getEngineDir(engStr);
			if (isDirectory(engPath)) {
				engineOverride = engPath;
			} else if (isDirectory(engStr)) {
				engineOverride = engStr;
			}
		}
		auto r = scaffoldProject(nameStr, locStr, _layout, engineOverride);
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

		auto reg = ProjectRegistry::load(_layout.getProjectsManifest());
		reg.upsert(*entry);
		if (!reg.save(_layout.getProjectsManifest())) {
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

void InstallerController::removeProject(StringView path, Function<void(bool ok)> &&onDone) {
	auto doneCb = std::make_shared<Function<void(bool)>>(sp::move(onDone));
	const auto pathStr = toString(path);
	_app->perform([this, pathStr](const AppThread::Task &) -> bool {
		auto reg = ProjectRegistry::load(_layout.getProjectsManifest());
		if (!reg.remove(pathStr)) {
			return false;
		}
		return reg.save(_layout.getProjectsManifest());
	}, [doneCb](const AppThread::Task &, bool ok) {
		if (*doneCb) {
			(*doneCb)(ok);
		}
	}, this);
}

void InstallerController::buildProject(StringView path, StringView target, bool run, bool release,
		Function<void(StringView line)> &&onOutput,
		Function<void(bool ok, String message)> &&onDone) {
	auto doneCb = std::make_shared<Function<void(bool, String)>>(sp::move(onDone));
	auto outCb = std::make_shared<Function<void(StringView)>>(sp::move(onOutput));
	auto msg = std::make_shared<String>();
	const auto pathStr = toString(path);
	const auto tgtStr = toString(target);

	log::info(kLogTag, "buildProject: path=", pathStr, " target=", tgtStr, " run=", run);

	_app->perform(
			[this, pathStr, tgtStr, run, release, outCb, msg](const AppThread::Task &) -> bool {
		BuildOptions opts;
		opts.target = tgtStr;
		opts.run = run;
		opts.release = release;

		String engineOverride;
		auto reg = ProjectRegistry::load(_layout.getProjectsManifest());
		if (auto *p = reg.find(pathStr)) {
			if (!p->engine.empty()) {
				auto engPath = _layout.getEngineDir(p->engine);
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

		auto r = installer::buildProject(pathStr, _layout, opts, engineOverride, outPtr);
		*msg = r.message.empty() ? r.error : r.message;
		return static_cast<bool>(r);
	}, [doneCb, msg](const AppThread::Task &, bool ok) {
		if (*doneCb) {
			(*doneCb)(ok, *msg);
		}
	}, this);
}

// No worker hop for any of the three: spawnProcess watches the child on the app looper and fires
// its callbacks there, so the answer arrives on the app thread without a thread ever blocking.
// The handles are kept because dropping the last Rc kills the child.

void InstallerController::pickFolder(StringView prompt, Function<void(String)> &&onDone) {
	// One picker at a time — a second request supersedes the first, and releasing that handle
	// dismisses the dialog still on screen.
	_dialogHandle = pickFolderAsync(_app, prompt, sp::move(onDone), this);
}

void InstallerController::promptText(StringView title, StringView def,
		Function<void(String)> &&onDone) {
	_dialogHandle = promptTextAsync(_app, title, def, sp::move(onDone), this);
}

void InstallerController::openFolder(StringView path) {
	// Fire-and-forget, but the helper still has to survive until it exits. There is no reference
	// to a handle from inside its own completion, so finished spawns are pruned on the way in
	// instead — the list only ever holds the few that are still running.
	auto out = _spawned.begin();
	for (auto &it : _spawned) {
		if (it && it->isRunning()) {
			*out++ = sp::move(it);
		}
	}
	_spawned.erase(out, _spawned.end());

	if (auto handle = openInFileManagerAsync(_app, path, this)) {
		_spawned.emplace_back(sp::move(handle));
	}
}

} // namespace stappler::xenolith::installer
