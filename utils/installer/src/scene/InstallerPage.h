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

#ifndef UTILS_INSTALLER_SRC_SCENE_INSTALLERPAGE_H_
#define UTILS_INSTALLER_SRC_SCENE_INSTALLERPAGE_H_

#include "XLUiPanel.h"
#include "XLUiTableView.h"
#include "XL2dScrollView.h"
#include "XL2dLabel.h"

#include "InstallerAppController.h"
#include "InstallerNavPane.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

/* A right-hand page: a vertical ScrollView with a column of blocks inside it.

The table is NOT the page. A page is a heading, an explanation, a table and whatever notes belong
under it, and the whole column scrolls as one — which is why the table is embedded with
setAutoHeight(true) and its own scrolling switched off. Two nested scrollers would fight over the
same swipe, and a table sized to the viewport would scroll independently of the text that explains
it.

That is what ui::TableView::getIntrinsicHeight() and its measure system are for: the table reports
the height of its whole model before any of its rows exist, so the flex column above it can size
itself and the outer ScrollView gets a content height that is right on the first frame. */
class InstallerPage : public ui::Panel {
public:
	virtual ~InstallerPage();

	virtual bool init(StringView title, StringView note);
	virtual void handleContentSizeDirty() override;

	/* The page is about to become the visible one.

	Needed because the pages are built up front and kept hidden: a hidden node is not visited, so
	the row materialization its data::Source listener asks for never happens, and a table whose
	model changed while it was off screen would come back still showing the old one - or, at
	startup, still showing nothing. Re-reading here costs nothing when nothing changed. */
	virtual void handleShown() { }

protected:
	using ui::Panel::init;

	// Append a block to the page's column, after whatever is already there. Placement, spacing and
	// padding are the flex column's - that is, the stylesheet's; this only fixes the order.
	Node *addBlock(Rc<Node> &&);

	// Re-measure the column and hand the outer scroll its content height. Called whenever a block
	// changes size — most importantly when a nested table's model changes.
	void updateScrollArea();

	basic2d::ScrollView *_scroll = nullptr;
	Rc<basic2d::ScrollController> _scrollController;
	Node *_body = nullptr;
	// The last layout handed to the controller, so an unchanged one does not re-register.
	float _scrollHeight = nan();
	basic2d::Label *_title = nullptr;
	basic2d::Label *_note = nullptr;
	int16_t _blockCount = 0;
};

/* The tools page: one ui::TableView over the catalogue rows of one Kind, or over the engine refs.

Columns are exactly design.md's three — name, status, actions — and their WIDTHS come from CSS
(`grid-template-columns` on the table-view node), because a track list is what a stylesheet can
express and which key a column reads is what it cannot. */
class InstallerToolsPage : public InstallerPage {
public:
	virtual ~InstallerToolsPage();

	virtual bool init(PageId);
	virtual void handleEnter(Scene *) override;
	virtual void handleShown() override;

	PageId getPageId() const { return _page; }

protected:
	using InstallerPage::init;

	void buildCell(ui::TableView::CellBuilder &);
	void reload();

	PageId _page = PageId::Hosts;
	ui::TableView *_table = nullptr;
};

// The root "Xenolith" page: what this machine is, what the engine resolves to, and the one button
// that provisions everything for it.
class InstallerWelcomePage : public InstallerPage {
public:
	virtual ~InstallerWelcomePage();

	virtual bool init() override;
	virtual void handleEnter(Scene *) override;
	virtual void handleShown() override { refresh(); }

protected:
	using InstallerPage::init;

	void refresh();

	basic2d::Label *_summary = nullptr;
};

} // namespace stappler::xenolith::installer

#endif // UTILS_INSTALLER_SRC_SCENE_INSTALLERPAGE_H_
