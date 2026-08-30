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

#include "XLCommon.h" // IWYU pragma: keep

#include "style/XLUiStyleSheet.cc"
#include "style/XLUiStyleSystem.cc"
#include "style/XLUiStyleResolver.cc"

#include "layout/XLUiLayoutSystem.cc"
#include "layout/XLUiLayoutFlex.cc"
#include "layout/XLUiLayoutGrid.cc"
#include "layout/XLUiLayoutTable.cc" // after Grid: shares its track sizing and parseGridTemplate

#include "atoms/XLUiPanel.cc"
#include "layout/XLUiScrollSystem.cc" // after Panel: the scroll indicator is one
#include "layout/XLUiDragScrollSystem.cc" // after ScrollSystem: it drives one of the two
#include "atoms/XLUiBadge.cc"
#include "atoms/XLUiProgressBar.cc"
#include "atoms/XLUiTableBorderPainter.cc"

#include "input/XLUiControlLock.cc" // the widget half of the lock; the state half is in application
                                 // with a reason attached, and every widget below calls in
                                 // here. It also builds a ui::TooltipTarget, whose
                                 // definition arrives much later in this unit - that is
                                 // fine (the header defines the class) and this include
                                 // cannot move down past its callers.
#include "input/XLUiButton.cc"
#include "atoms/XLUiCloseGuardWidget.cc"
#include "input/XLUiChip.cc" // after Badge and Button: a chip is one, with the other on it
#include "input/XLUiCheckbox.cc"
#include "input/XLUiSlider.cc" // a Panel and two more, like the progress bar it is not
#include "input/XLUiTextHistory.cc" // before TextInput and TextView: both hold one
#include "input/XLUiTextInput.cc"
#include "input/XLUiTextDocument.cc"
#include "input/XLUiTextView.cc" // after TextInput: the view replaces its container and cursor layer
#include "input/XLUiNumberField.cc" // after TextInput: it is one, filtered
#include "input/XLUiInlineEditor.cc" // after TextInput: the stock editor it opens is one
#include "input/XLUiCodeEditor.cc" // after TextView: both are it, configured
#include "input/XLUiVectorField.cc" // after NumberField: a row of them is one value
#include "input/XLUiConsole.cc"

#include "view/XLUiCanvasView.cc"
#include "view/XLUiFilesystemModel.cc"
#include "view/XLUiRowGeometry.cc" // before both views: each answers its geometry through it
#include "view/XLUiTreeView.cc"
#include "view/XLUiTableView.cc"

#include "frame/XLUiWindowFrame.cc" // after Button: the frame is built out of them

#include "menu/XLUiMenuSource.cc"
#include "menu/XLUiMenuItem.cc" // after Button: a menu row is one
#include "menu/XLUiMenuSystem.cc" // after MenuItem: it builds them

#include "forms/XLUiFormInputListener.cc"
#include "forms/XLUiFormSystem.cc"
#include "forms/XLUiFormAdapters.cc"

#include "dock/XLUiDockTree.cc"
#include "dock/XLUiDockTab.cc"
#include "dock/XLUiDockTabBar.cc"
#include "dock/XLUiDockSplitter.cc"
#include "dock/XLUiDockFrame.cc"
#include "dock/XLUiDockDragVisuals.cc"
#include "dock/XLUiDockSystem.cc"

#include "XLUiSubWindow.cc"
#include "XLUiSubWindowScene.cc"
#include "XLUiSubWindowSession.cc"
#include "XLUiPopupSurface.cc" // after SubWindowSession: every popup opens through its slot
#include "menu/XLUiMenuPopup.cc" // after PopupSurface: a menu is one, measured first
#include "menu/XLUiContextMenu.cc" // after MenuPopup: it opens through openMenu
#include "XLUiTooltipSystem.cc" // after SubWindowSession: the hint goes through its tip slot

#include "input/XLUiSelect.cc" // after MenuPopup: the list it opens is one
#include "input/XLUiChipRow.cc" // after MenuPopup: the "+" opens a menu
#include "input/XLUiColorField.cc" // after PopupSurface: its own picker is one
#include "search/XLUiSearchSystem.cc" // no order dependency: it draws nothing and opens nothing
#include "input/XLUiSearchPicker.cc" // after SearchSystem and MenuPopup: it queries one and opens through the other
