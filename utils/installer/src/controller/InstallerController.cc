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

#include <sprt/runtime/platform.h>

#include <climits>
#include <cstdint>
#include <memory>

using namespace stappler;        // makes git:: (stappler::git) usable
using namespace sprt::status;    // Status

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

static constexpr const char *kLogTag = "installer";

InstallerController::~InstallerController() { }

bool InstallerController::init(AppThread *app) {
	_app = app;
	_layout = Layout::resolve_from_env(nullptr);
	log::info(kLogTag, "controller init: config=", _layout.config, " data=", _layout.data);
	return true;
}

// Throttle a byte-progress callback to ~256 KiB steps and marshal it onto the app thread. The core
// install/clone callbacks fire on a worker thread; the scene graph may only be touched on the app
// thread, so we hop back through performOnAppThread. `emit` turns the (bytes,total) pair into the
// app-thread callback the caller supplied.
static void marshalProgress(AppThread *app, std::shared_ptr<uint64_t> lastStep, int64_t bytes,
		int64_t total, const Function<void()> &emit) {
	uint64_t step = static_cast<uint64_t>(bytes) / (256 * 1024);
	if (step == *lastStep) {
		return;
	}
	*lastStep = step;
	int64_t b = bytes;
	(void)total;
	app->performOnAppThread([emit]() {
		if (emit) { emit(); }
	});
}

void InstallerController::loadCatalog(Function<void(bool ok, String err)> &&onDone) {
	auto doneCb = std::make_shared<Function<void(bool, String)>>(sp::move(onDone));
	auto errStr = std::make_shared<String>();
	auto built = std::make_shared<CatalogueData>();

	log::info(kLogTag, "loadCatalog: fetching catalogue from ", ftp_release_base());

	_app->perform(
			[this, errStr, built](const AppThread::Task &) -> bool {
				auto base = ftp_release_base();
				String hostsText, targetsText;
				auto r1 = fetch_text(toString(base) + "/hosts/", hostsText);
				if (r1.status != Status::Ok) {
					*errStr = toString("hosts: ") + r1.error;
					return false;
				}
				auto r2 = fetch_text(toString(base) + "/targets/", targetsText);
				if (r2.status != Status::Ok) {
					*errStr = toString("targets: ") + r2.error;
					return false;
				}

				auto comps = build_catalogue(hostsText, targetsText);
				auto st = InstalledState::load(_layout.installed_manifest());

				auto h = resolve_host(native_arch(), native_os());
				built->nativeId = h.native;
				built->release = toString(default_release());
				for (const auto &c : comps) {
					CatalogRow row;
					row.kind = c.kind;
					row.id = c.id;
					row.triple = c.triple;
					row.variant = c.variant;
					row.size = c.size;
					bool installed = st.get(c.id, c.kind) != nullptr;
					row.status = installed ? RowStatus::Installed : RowStatus::NotInstalled;
					row.isNative = (c.triple == built->nativeId);
					built->rows.push_back(sp::move(row));
				}
				return true;
			},
			[this, doneCb, errStr, built](const AppThread::Task &, bool ok) {
				if (ok) {
					_catalog = sp::move(*built);
					_hasCatalog = true;
					log::info(kLogTag, "loadCatalog: ok, rows=", _catalog.rows.size(),
							" native=", _catalog.nativeId, " release=", _catalog.release);
					size_t installed = 0;
					for (const auto &r : _catalog.rows) {
						if (r.status == RowStatus::Installed) { ++installed; }
					}
					log::info(kLogTag, "loadCatalog: installed=", installed, " of ",
							_catalog.rows.size());
				} else {
					log::error(kLogTag, "loadCatalog: FAILED: ", *errStr);
				}
				if (*doneCb) {
					(*doneCb)(ok, *errStr);
				}
			},
			this);
}

void InstallerController::installComponent(Kind kind, StringView id,
		Function<void(const InstallProgress &)> &&onProgress,
		Function<void(bool ok, String err)> &&onDone) {
	String idStr = toString(id);
	bool wantHost = (kind == Kind::Host);
	bool wantTarget = (kind == Kind::Target);
	auto progCb =
			std::make_shared<Function<void(const InstallProgress &)>>(sp::move(onProgress));
	auto doneCb = std::make_shared<Function<void(bool, String)>>(sp::move(onDone));
	auto errStr = std::make_shared<String>();
	auto lastStep = std::make_shared<uint64_t>(UINT64_MAX);

	log::info(kLogTag, "installComponent: kind=", kind_to_string(kind), " id=", idStr);

	_app->perform(
			[this, idStr, kind, wantHost, wantTarget, progCb, errStr, lastStep](
					const AppThread::Task &) -> bool {
				auto r = install_component(idStr, _layout, wantHost, wantTarget,
						[this, kind, idStr, progCb, lastStep](int64_t bytes, int64_t total) {
							marshalProgress(_app, lastStep, bytes, total, [this, kind, idStr, bytes, progCb]() {
								if (*progCb) {
									(*progCb)(InstallProgress{kind, idStr,
											InstallPhase::Downloading, bytes});
								}
							});
						});
				if (r.status != Status::Ok || r.installed.empty()) {
					*errStr = r.error.empty() ? toString("install failed: ") + idStr : r.error;
					return false;
				}
				return true;
			},
			[this, idStr, kind, doneCb, errStr](const AppThread::Task &, bool ok) {
				if (ok) {
					log::info(kLogTag, "installComponent: done id=", idStr);
				} else {
					log::error(kLogTag, "installComponent: FAILED id=", idStr, " err=", *errStr);
				}
				if (*doneCb) {
					(*doneCb)(ok, *errStr);
				}
			},
			this);
}

void InstallerController::uninstallComponent(Kind kind, StringView id,
		Function<void(bool ok, String err)> &&onDone) {
	String idStr = toString(id);
	auto doneCb = std::make_shared<Function<void(bool, String)>>(sp::move(onDone));
	auto errStr = std::make_shared<String>();

	log::info(kLogTag, "uninstallComponent: kind=", kind_to_string(kind), " id=", idStr);

	_app->perform(
			[this, idStr, kind, errStr](const AppThread::Task &) -> bool {
				if (!uninstall(_layout, kind, idStr)) {
					*errStr = toString("uninstall failed: ") + idStr;
					// uninstall is idempotent (a missing dir is not an error); only surface a real
					// failure if the registry claims it installed but the dir removal failed.
					return false;
				}
				return true;
			},
			[this, idStr, kind, doneCb, errStr](const AppThread::Task &, bool ok) {
				if (ok) {
					log::info(kLogTag, "uninstallComponent: done id=", idStr);
				} else {
					log::error(kLogTag, "uninstallComponent: FAILED id=", idStr);
				}
				if (*doneCb) {
					(*doneCb)(ok, *errStr);
				}
			},
			this);
}

void InstallerController::installForSystem(
		Function<void(StringView step, const InstallProgress &)> &&onProgress,
		Function<void(bool ok, String err)> &&onDone) {
	auto progCb = std::make_shared<Function<void(StringView, const InstallProgress &)>>(
			sp::move(onProgress));
	auto doneCb = std::make_shared<Function<void(bool, String)>>(sp::move(onDone));
	auto errStr = std::make_shared<String>();

	log::info(kLogTag, "installForSystem: starting (engine + host + target)");

	_app->perform(
			[this, progCb, errStr](const AppThread::Task &) -> bool {
				// 1. Engine — clone the default ref if none resolves.
				bool ok = false;
				auto root = resolve_engine_root(_layout, nullptr, &ok);
				if (!ok) {
					log::info(kLogTag, "installForSystem: cloning engine ", engineDefaultRef());
					auto lastStep = std::make_shared<uint64_t>(UINT64_MAX);
					auto cr = clone_engine(engineDefaultRef(), _layout,
							[this, progCb, lastStep](const git::CloneProgress &p) {
								if (p.stage == git::CloneStage::Downloading && p.bytesTotal > 0) {
									auto recv = p.bytesReceived;
									marshalProgress(_app, lastStep, static_cast<int64_t>(recv),
											static_cast<int64_t>(p.bytesTotal),
											[this, recv, progCb]() {
												if (*progCb) {
													(*progCb)(StringView("engine"),
															InstallProgress{Kind::Host, String("engine"),
																	InstallPhase::Downloading,
																	static_cast<int64_t>(recv)});
												}
											});
								}
							});
					if (cr.status != Status::Ok) {
						*errStr = toString("engine clone failed");
						return false;
					}
					root = _layout.engine_dir(engineDefaultRef());
				}
				link_toolchains_into_engine_path(_layout, StringView(root));

				// 2 + 3. Native host + native target (+sprt target if present).
				auto h = resolve_host(native_arch(), native_os());
				if (h.native.empty()) {
					*errStr = toString("no SDK host for ") + toString(native_arch())
							+ toString("-") + toString(native_os());
					return false;
				}

				auto installWith = [&](StringView step, Kind progKind, StringView id, bool wantHost,
										bool wantTarget) -> bool {
					auto lastStep = std::make_shared<uint64_t>(UINT64_MAX);
					auto r = install_component(id, _layout, wantHost, wantTarget,
							[this, progCb, stepStr = toString(step), idStr = toString(id), progKind,
									lastStep](int64_t bytes, int64_t total) {
								marshalProgress(_app, lastStep, bytes, total,
										[this, stepStr, idStr, progKind, bytes, progCb]() {
											if (*progCb) {
												(*progCb)(StringView(stepStr),
														InstallProgress{progKind, idStr,
																InstallPhase::Downloading, bytes});
											}
										});
							});
					if (r.status != Status::Ok || r.installed.empty()) {
						*errStr = r.error.empty() ? (toString("install failed: ") + toString(id))
												  : r.error;
						return false;
					}
					return true;
				};

				if (!installWith(StringView("host"), Kind::Host, StringView(h.native), true, false)) {
					return false;
				}
				if (!installWith(StringView("target"), Kind::Target, StringView(h.native), false,
							true)) {
					return false;
				}

				String sprt = h.native + "+sprt";
				auto rs = install_component(sprt, _layout, false, true); // best-effort; absence is not an error
				(void)rs;
				return true;
			},
			[this, doneCb, errStr](const AppThread::Task &, bool ok) {
				if (ok) {
					log::info(kLogTag, "installForSystem: done");
				} else {
					log::error(kLogTag, "installForSystem: FAILED: ", *errStr);
				}
				if (*doneCb) {
					(*doneCb)(ok, *errStr);
				}
			},
			this);
}

void InstallerController::queryEngine(Function<void(const EngineStatusInfo &)> &&onDone) {
	auto doneCb = std::make_shared<Function<void(const EngineStatusInfo &)>>(sp::move(onDone));
	auto info = std::make_shared<EngineStatusInfo>();

	_app->perform(
			[this, info](const AppThread::Task &) -> bool {
				bool ok = false;
				auto root = resolve_engine_root(_layout, nullptr, &ok);
				info->ready = ok;
				info->path = root;
				if (ok) {
					// reference = the basename of the resolved engine dir (a ref name for cloned
					// engines, the checkout dir name for an external override).
					StringView p(root);
					size_t slash = p.rfind('/');
					info->reference = (slash < p.size()) ? toString(p.sub(slash + 1)) : toString(p);
					info->shortHash = String();
				}
				return true;
			},
			[this, info, doneCb](const AppThread::Task &, bool) {
				log::info(kLogTag, "queryEngine: ready=", info->ready, " ref=", info->reference,
						" path=", info->path);
				if (*doneCb) {
					(*doneCb)(*info);
				}
			},
			this);
}

void InstallerController::prepareEngine(Function<void(int64_t bytes, int64_t total)> &&onProgress,
		Function<void(bool ok, String err)> &&onDone) {
	auto progCb =
			std::make_shared<Function<void(int64_t, int64_t)>>(sp::move(onProgress));
	auto doneCb = std::make_shared<Function<void(bool, String)>>(sp::move(onDone));
	auto errStr = std::make_shared<String>();

	log::info(kLogTag, "prepareEngine: cloning ", engineDefaultRef());

	_app->perform(
			[this, progCb, errStr](const AppThread::Task &) -> bool {
				auto lastStep = std::make_shared<uint64_t>(UINT64_MAX);
				auto cr = clone_engine(engineDefaultRef(), _layout,
						[this, progCb, lastStep](const git::CloneProgress &p) {
							if (p.stage == git::CloneStage::Downloading && p.bytesTotal > 0) {
								marshalProgress(_app, lastStep,
										static_cast<int64_t>(p.bytesReceived),
										static_cast<int64_t>(p.bytesTotal),
										[this, bytes = p.bytesReceived, total = p.bytesTotal, progCb]() {
											if (*progCb) {
												(*progCb)(static_cast<int64_t>(bytes),
														static_cast<int64_t>(total));
											}
										});
							}
						});
				if (cr.status != Status::Ok) {
					*errStr = toString("engine clone failed");
					return false;
				}
				link_toolchains_into_engine_path(_layout, StringView(_layout.engine_dir(engineDefaultRef())));
				return true;
			},
			[this, doneCb, errStr](const AppThread::Task &, bool ok) {
				if (ok) {
					log::info(kLogTag, "prepareEngine: done");
				} else {
					log::error(kLogTag, "prepareEngine: FAILED: ", *errStr);
				}
				if (*doneCb) {
					(*doneCb)(ok, *errStr);
				}
			},
			this);
}

void InstallerController::setRowStatus(Kind kind, StringView id, RowStatus s) {
	for (auto &row : _catalog.rows) {
		if (row.kind == kind && StringView(row.id) == id) {
			row.status = s;
			return;
		}
	}
}

} // namespace stappler::xenolith::installer
