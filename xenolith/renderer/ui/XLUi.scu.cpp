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
#include "atoms/XLUiBadge.cc"
#include "atoms/XLUiProgressBar.cc"
#include "atoms/XLUiTableBorderPainter.cc"

#include "input/XLUiInteractiveComponent.cc"
#include "input/XLUiButton.cc"
#include "atoms/XLUiCloseGuardWidget.cc"
#include "input/XLUiCheckbox.cc"
#include "input/XLUiTextInput.cc"

#include "view/XLUiTreeView.cc"
#include "view/XLUiTableView.cc"

#include "frame/XLUiWindowFrame.cc" // after Button: the frame is built out of them

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
