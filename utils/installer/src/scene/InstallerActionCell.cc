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

// The status a row of `page` reads right now. One place, because the action cell and the status
// badge must never disagree about what the row is.
static RowStatus getRowStatus(PageId page, Kind kind, StringView id) {
	auto controller = AppController::getInstance();
	if (!controller) {
		return RowStatus::Checking;
	}
	return page == PageId::Engines ? controller->getEngineRowStatus(id)
								   : controller->getToolStatus(kind, id);
}

// --- InstallerStatusCell ----------------------------------------------------

InstallerStatusCell::~InstallerStatusCell() { }

bool InstallerStatusCell::init(PageId page, const Value &row) {
	if (!ui::Panel::init()) {
		return false;
	}

	setType("table-cell");
	removeStyleClass("xl-ui-panel");
	addStyleClass("xl-ui-table-cell");

	_page = page;
	_kind = (page == PageId::Hosts) ? Kind::Host : Kind::Target;
	_id = row.getString(page == PageId::Engines ? "name" : "id");

	_badge = addChild(Rc<ui::Badge>::create());

	return true;
}

void InstallerStatusCell::handleEnter(Scene *scene) {
	ui::Panel::handleEnter(scene);

	auto listener = addSystem(Rc<EventListener>::create());
	// Unfiltered on purpose: the payload names one row only when exactly one moved, and re-reading
	// one status is a lookup, against a rebuild of every row in the table.
	listener->listenForEvent(AppController::onRowStatusChanged, [this](const Event &) { refresh(); });

	refresh();
}

void InstallerStatusCell::refresh() {
	if (!_badge) {
		return;
	}
	switch (getRowStatus(_page, _kind, _id)) {
	case RowStatus::Checking:
		// Nothing is claimed about this row yet, and its action cell is empty to match.
		_badge->setText(strings::statusChecking());
		_badge->setVariant("checking");
		break;
	case RowStatus::Installed:
		_badge->setText(strings::statusInstalled());
		_badge->setVariant("installed");
		break;
	case RowStatus::UpdateAvailable:
		_badge->setText(strings::statusUpdateAvailable());
		_badge->setVariant("update");
		break;
	case RowStatus::NotInstalled:
		_badge->setText(strings::statusNotInstalled());
		_badge->setVariant("not-installed");
		break;
	}
}

// --- InstallerActionCell ----------------------------------------------------

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
	_key = (page == PageId::Engines) ? _id : rowKey(_kind, _id);
	// NOT row["status"]: that is the status as of whenever this row was last materialized, and a
	// status change deliberately does not rebuild the row any more. refresh() reads the live one.

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
	// The status moved under a row that is already on screen: this is the whole update - the label
	// on the button and the style class that colours it.
	listener->listenForEvent(AppController::onRowStatusChanged, [this](const Event &) { refresh(); });

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

	_status = getRowStatus(_page, _kind, _id);

	/* An unchecked row offers nothing.

	Every action here is an answer to "what is the state of this component" - install it, update it,
	delete it - and until the actuality check has answered that, the row has no state to act on.
	Guessing one would be worse than waiting: the default guess is "not installed", and a row
	offering to download a toolchain that is already on disk is exactly the mistake this prevents.
	The auto-update switch goes with it - it is a per-tool setting, and a tool nothing is known about
	is not yet a tool to configure. */
	const bool checked = (_status != RowStatus::Checking);
	if (_action) {
		_action->setVisible(checked && !busy);
		// Both classes are removed before one is added: this node OUTLIVES its status now, and a
		// row that went from Installed to NotInstalled would otherwise keep `danger` under
		// `primary` and paint an Install button red.
		_action->removeStyleClass("danger");
		_action->removeStyleClass("primary");
		switch (_status) {
		case RowStatus::Checking: break;
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
	if (_autoUpdate) {
		_autoUpdate->setVisible(checked);
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
	if (!controller || _status == RowStatus::Checking) {
		// The button is hidden in that state; this is the guarantee that it cannot be reached by
		// anything else either (a hotkey, the inspector's command list).
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
