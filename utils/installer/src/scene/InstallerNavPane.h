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

#ifndef UTILS_INSTALLER_SRC_SCENE_INSTALLERNAVPANE_H_
#define UTILS_INSTALLER_SRC_SCENE_INSTALLERNAVPANE_H_

#include "XLUiPanel.h"
#include "XLUiTreeView.h"

#include "InstallerAppController.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

// Which page the right-hand pane is showing. A nav row maps to one of these; there is no page per
// leaf, because design.md says the nested entries of a set have no page of their own — they open
// the set's master page.
enum class PageId {
	Welcome,
	Engines,
	Hosts,
	Targets,
};

/* The left navigation pane: Xenolith -> Engines / Hosts / Targets.

Owns a ui::TreeView rather than deriving from one — the model belongs to AppController
(getNavSource()), so this stays a pure view. The TreeView watches that Source through its own
DataListener, so nothing here subscribes to a catalogue event: the controller dirties the Source and
the rows follow.

A row's own progress fill is the exception. It cannot go through the Source, because dirtying would
rebuild the row node many times a second during a download, so the pane keeps the live row nodes by
key and pushes progress into them from onJobProgress. */
class InstallerNavPane;

/* The content of one navigation row: its label, and behind it the fill that shows a download.

design.md asks for a row "displayed as the row filled with colour" while its component is
downloading. Of the three ways to express that, only this one works here: a custom property plus
`calc()` cannot mix a unitless fraction with a percentage, and the `background` shorthand does not
exist in the CSS subset at all. So the fill is a real node whose WIDTH C++ writes and whose COLOUR
the stylesheet owns — the split the repository keeps everywhere else.

It is also why the fill is not part of the row's data: progress must never reach the Source, or
every byte would rebuild the row. */
class InstallerNavRow : public ui::Panel {
public:
	virtual ~InstallerNavRow();

	virtual bool init(StringView label);
	virtual void handleContentSizeDirty() override;

	// nan() clears the fill entirely (nothing is running for this row).
	void setProgress(float);

protected:
	using ui::Panel::init;

	basic2d::Layer *_fill = nullptr;
	basic2d::Label *_label = nullptr;
	float _progress = nan();
};

class InstallerNavPane : public ui::Panel {
public:
	using SelectCallback = Function<void(PageId, StringView id)>;

	virtual ~InstallerNavPane();

	virtual bool init() override;

	virtual void handleEnter(Scene *) override;
	virtual void handleContentSizeDirty() override;

	void setSelectCallback(SelectCallback &&cb) { _selectCallback = sp::move(cb); }

	// Highlight the row that stands for `page` without firing the callback back at the caller.
	void selectPage(PageId);

	ui::TreeView *getTree() const { return _tree; }

protected:
	using ui::Panel::init;

	void buildRow(ui::TreeView::RowBuilder &);
	void handleRowActivated(size_t index, const ui::TreeView::Row &);
	void confirmRemove(StringView node, StringView id);

	// Push a job's progress into the live row it belongs to, if that row is on screen.
	void applyJobProgress(JobId);

	// Open every branch of the tree, repeating until nothing more opens.
	void expandAll();

	static PageId pageForNode(StringView node);

	ui::TreeView *_tree = nullptr;
	// Live row content nodes by rowKey, so progress can reach one without a rebuild. Entries are
	// dropped when the row leaves the scroll window - a row rebuilt on the way back in is created
	// with the current progress anyway.
	Map<String, Rc<InstallerNavRow>> _rowNodes;
	SelectCallback _selectCallback;
	PageId _current = PageId::Welcome;
};

} // namespace stappler::xenolith::installer

#endif // UTILS_INSTALLER_SRC_SCENE_INSTALLERNAVPANE_H_
