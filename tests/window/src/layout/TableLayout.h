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

#ifndef TESTS_WINDOW_SRC_LAYOUT_TABLELAYOUT_H_
#define TESTS_WINDOW_SRC_LAYOUT_TABLELAYOUT_H_

#include "app/TestLayout.h"
#include "XL2dLayer.h"
#include "XLUiLayoutSystem.h"
#include "XLUiTableBorderPainter.h"
#include "XLUiButton.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// Demonstration layout for the table backend of ui::LayoutSystem.
//
// The point of a table, as opposed to a grid, is that the columns are resolved ONCE and imposed on
// every row: the rows are separate nodes, each with its own LayoutSystem in TableRow mode, all
// reading the same TableColumnsComponent the container stamped on them. Both the colspan cell and
// the rowspan cell exist to show that placement is a per-row cursor over shared columns.
//
// `getLayoutState()` reports the resolved column geometry and the emitted border rects, so a
// headless run can assert the numbers instead of comparing pixels.
class TableLayout : public TestLayout {
public:
	virtual ~TableLayout() = default;

	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	virtual void registerCommands() override;

	ui::Button *addControlButton(StringView, Function<void()> &&);
	void updateControlLabels();

	// Everything the table resolved, as data: column widths and x offsets read back off a row's
	// stamped component (not recomputed here - the assertion has to see what the rows saw), the row
	// boxes, and the collapsed border rects.
	Value getLayoutState() const;

	void cycleTemplate();
	void cycleAlgorithm();
	void cycleCollapse();

	void rebuildTable();

	// container built in code: components assigned directly
	basic2d::Layer *_table = nullptr;
	ui::LayoutSystem *_tableSystem = nullptr;

	// the same table declared entirely in CSS - the control that says whether
	// StyleResolver::applyLayout maps display:table / table-row / table-cell correctly
	basic2d::Layer *_cssTable = nullptr;

	basic2d::Layer *_controls = nullptr;
	ui::LayoutSystem *_controlsFlex = nullptr;

	ui::Button *_btnTemplate = nullptr;
	ui::Button *_btnAlgorithm = nullptr;
	ui::Button *_btnCollapse = nullptr;

	uint32_t _templateIndex = 0;
	bool _fixedAlgorithm = false;
	bool _collapse = true;

	float _controlsHeight = 44.0f;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_LAYOUT_TABLELAYOUT_H_
