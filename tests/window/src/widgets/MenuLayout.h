/**
 Copyright (c) 2026 Stappler Team <admin@stappler.org>

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

#ifndef TESTS_WINDOW_SRC_WIDGETS_MENULAYOUT_H_
#define TESTS_WINDOW_SRC_WIDGETS_MENULAYOUT_H_

#include "app/TestLayout.h"
#include "XLUiMenuSource.h"
#include "XLUiMenuSystem.h"
#include "XLUiMenuPopup.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// ui::MenuSource / ui::MenuSystem: one model, shown twice - as an inline menu the test measures and
// reads back, and as a popup with a submenu chain.
//
// The inline menu is the assertion surface. Its metrics ARE the layout: the same numbers decide the
// popup's Extent2, the wrapped height of every row and where the columns sit, so a check that reads
// `menu.metrics` and `menu.state` is checking the thing that draws rather than a parallel model.
//
// The source deliberately contains one of everything a menu can hold: a plain command, one with a
// leading icon, one with a subtitle, one with a hotkey, a title long enough to wrap at the fixed
// width, a separator, a checkable KeepOpen toggle, a custom node with a factory, a disabled item, a
// hidden item, and a submenu.
class MenuLayout : public TestLayout {
public:
	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	virtual void registerCommands() override;

	void buildSource();

	// Re-measure the inline menu through the measurement protocol, which is also what a
	// `fit-content` ancestor would do.
	void updateInlineMenu();

	Value encodeMetrics() const;
	Value encodeState() const;

	AppWindow *getAppWindow() const;

	ui::MenuSourceItem *getItem(const Value &args) const;

	Rc<ui::MenuSource> _source;
	Rc<ui::MenuSource> _submenu;
	ui::Panel *_menuPanel = nullptr;
	ui::MenuSystem *_menu = nullptr;
	ui::Button *_openButton = nullptr;
	Rc<ui::SubWindow> _popup;

	// The width the inline menu is pinned to, so that wrapping is deterministic rather than a
	// function of whatever font the host happens to have.
	float _menuWidth = 320.0f;

	uint32_t _customBuilds = 0;
	uint32_t _activations = 0;
	String _lastActivated;
	Vector<String> _activationLog;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_WIDGETS_MENULAYOUT_H_
