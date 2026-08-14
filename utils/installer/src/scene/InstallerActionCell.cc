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

#include "InstallerActionCell.h"
#include "InstallerDialogs.h"
#include "InstallerStrings.h"

#include "XLEventListener.h"
#include "XLAppWindow.h"
#include "XLScene.h"
#include "XLDirector.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

InstallerActionCell::~InstallerActionCell() { }

bool InstallerActionCell::init(PageId page, const Value &row) {
	if (!ui::Panel::init()) {
		return false;
	}

	setName("action-cell");
	removeStyleClass("xl-ui-panel");
	registerStyleAppliers("action-cell");

	_page = page;
	_kind = (page == PageId::Hosts) ? Kind::Host : Kind::Target;
	_id = row.getString(page == PageId::Engines ? "name" : "id");
	_status = static_cast<RowStatus>(row.getInteger("status"));
	_key = (page == PageId::Engines) ? _id : rowKey(_kind, _id);

	_action = addChild(Rc<ui::Button>::create([this] { performAction(); }), ZOrder(1));
	_action->addStyleClass("row-action");

	_progress = addChild(Rc<ui::ProgressBar>::create(), ZOrder(2));
	_progress->addStyleClass("row-progress");
	_progress->setVisible(false);

	if (page != PageId::Engines) {
		// design.md wants auto-update switchable per tool, not only globally.
		_autoUpdate = addChild(Rc<ui::Checkbox>::create(), ZOrder(3));
		_autoUpdate->addStyleClass("autoupdate");
		_autoUpdate->setChecked(row.getBool("autoUpdate"));
		_autoUpdate->setCallback([this](bool value) {
			if (auto controller = AppController::getInstance()) {
				controller->setToolAutoUpdate(_kind, _id, value);
			}
		});
	}

	return true;
}

void InstallerActionCell::handleEnter(Scene *scene) {
	ui::Panel::handleEnter(scene);

	auto listener = addSystem(Rc<EventListener>::create());
	// Filtered by this cell's own key inside handleJobEvent: every cell hears every job, which is
	// far cheaper than any registry of live cells and cannot go stale when a row is recycled.
	listener->listenForEvent(AppController::onJobStarted, [this](const Event &event) {
		handleJobEvent(static_cast<JobId>(event.getDataValue().getInteger()));
	});
	listener->listenForEvent(AppController::onJobProgress, [this](const Event &event) {
		handleJobEvent(static_cast<JobId>(event.getDataValue().getInteger()));
	});
	listener->listenForEvent(AppController::onJobFinished, [this](const Event &event) {
		handleJobEvent(static_cast<JobId>(event.getDataValue().getInteger()));
	});

	refresh();
}

void InstallerActionCell::handleJobEvent(JobId id) {
	auto controller = AppController::getInstance();
	if (!controller) {
		return;
	}
	const auto *job = controller->getJob(id);
	if (!job || StringView(job->target) != _key) {
		return;
	}
	refresh();
}

void InstallerActionCell::refresh() {
	auto controller = AppController::getInstance();
	if (!controller) {
		return;
	}

	const auto *job = controller->findJob(_page == PageId::Engines ? JobKind::EngineClone
																   : JobKind::ComponentInstall,
			_key);
	if (!job) {
		// Removal is a job too, and while it runs the row must not offer to remove again.
		job = controller->findJob(JobKind::ComponentRemove, _key);
	}

	const bool busy = job != nullptr;
	if (_action) {
		_action->setVisible(!busy);
		switch (_status) {
		case RowStatus::Installed:
			_action->setString(strings::actionDelete());
			_action->addStyleClass("danger");
			break;
		case RowStatus::UpdateAvailable:
			_action->setString(strings::actionUpdate());
			_action->addStyleClass("primary");
			break;
		case RowStatus::NotInstalled:
			_action->setString(strings::actionInstall());
			_action->addStyleClass("primary");
			break;
		}
	}
	if (_progress) {
		_progress->setVisible(busy);
		if (busy) {
			// nan when the total is unknown: the bar then shows its indeterminate state rather
			// than a fraction nobody measured.
			_progress->setProgress(job->total > 0
							? static_cast<float>(double(job->bytes) / double(job->total))
							: nan());
		}
	}
}

void InstallerActionCell::performAction() {
	auto controller = AppController::getInstance();
	if (!controller) {
		return;
	}

	if (_page == PageId::Engines) {
		// Cloning is the only engine action wired: the core has no removeEngine(), and deleting the
		// tree the current build root points at would be worse than not offering it.
		controller->prepareEngine(_id);
		return;
	}

	if (_status == RowStatus::Installed) {
		auto *scene = getScene();
		auto *director = scene ? scene->getDirector() : nullptr;
		auto *window = director ? static_cast<AppWindow *>(director->getRenderServer()) : nullptr;
		if (!window) {
			return;
		}
		const auto kind = _kind;
		const auto id = _id;
		showConfirmDialog(window, strings::confirmDeleteTitle(), strings::confirmDeleteMessage(id),
				strings::actionDelete(), ConfirmTone::Danger,
				[controller, kind, id] { controller->uninstallComponent(kind, id); });
		return;
	}

	// Install and update are the same operation: the core replaces an installed component in place
	// and has no separate update path.
	controller->installComponent(_kind, _id);
}

} // namespace stappler::xenolith::installer
