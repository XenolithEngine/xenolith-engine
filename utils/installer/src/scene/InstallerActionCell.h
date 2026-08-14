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

#ifndef UTILS_INSTALLER_SRC_SCENE_INSTALLERACTIONCELL_H_
#define UTILS_INSTALLER_SRC_SCENE_INSTALLERACTIONCELL_H_

#include "XLUiPanel.h"
#include "XLUiBadge.h"
#include "XLUiButton.h"
#include "XLUiCheckbox.h"
#include "XLUiProgressBar.h"

#include "InstallerAppController.h"
#include "InstallerNavPane.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

/* One row's status column: a badge that keeps up with its row.

The badge is INSIDE a cell rather than being one: a cell is stretched to its column, and a badge
stretched to 160px stops reading as a pill and starts reading as a filled band.

It re-reads the status from the controller instead of showing the one in the Value it was built
from, for the same reason the action cell does: a status change no longer dirties the row's Source,
so the row keeps the node it has and repaints it. The Value is the row's IDENTITY here, not its
state. */
class InstallerStatusCell : public ui::Panel {
public:
	virtual ~InstallerStatusCell();

	virtual bool init(PageId, const Value &row);
	virtual void handleEnter(Scene *) override;

protected:
	using ui::Panel::init;

	void refresh();

	PageId _page = PageId::Hosts;
	Kind _kind = Kind::Target;
	String _id;

	ui::Badge *_badge = nullptr;
};

/* The contents of a table row's "actions" column.

Three things live here, per design.md: the action that applies to the row's current status
(install / update / remove), a progress indicator that REPLACES that action while the row's job is
running, and a per-tool auto-update switch.

The swap is why this is a node with its own EventListener rather than something the cell callback
re-renders. A progress tick must never reach the row's data::Source: dirtying it would rebuild the
row node many times a second for the duration of a download. So the cell listens for the job events
itself, filters them by its own row key, and mutates what is already on screen.

There is no cancel button, and there deliberately is no place for one: the core install path is a
blocking call with no abort token, so a cancel affordance would be a lie. */
class InstallerActionCell : public ui::Panel {
public:
	virtual ~InstallerActionCell();

	virtual bool init(PageId, const Value &row);
	virtual void handleEnter(Scene *) override;

protected:
	using ui::Panel::init;

	void refresh();
	void handleJobEvent(JobId);
	void performAction();

	PageId _page = PageId::Hosts;
	Kind _kind = Kind::Target;
	String _id;
	String _key;
	// The last status this cell drew. Re-read from the controller on every refresh, and kept only so
	// that a repaint can tell whether anything actually moved.
	RowStatus _status = RowStatus::Checking;

	ui::Button *_action = nullptr;
	ui::ProgressBar *_progress = nullptr;
	ui::Checkbox *_autoUpdate = nullptr;
};

} // namespace stappler::xenolith::installer

#endif // UTILS_INSTALLER_SRC_SCENE_INSTALLERACTIONCELL_H_
