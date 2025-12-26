/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

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

#ifndef XENOLITH_RENDERER_MATERIAL2D_COMPONENTS_MENU_MATERIALMENU_H_
#define XENOLITH_RENDERER_MATERIAL2D_COMPONENTS_MENU_MATERIALMENU_H_

#include "MaterialMenuSource.h"
#include "MaterialSurface.h"
#include "XLSubscriptionListener.h"
#include "XL2dScrollView.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::material2d {

class Menu;
class MenuButton;
class MenuSeparator;

class SP_PUBLIC MenuItemInterface {
public:
	virtual ~MenuItemInterface() { }

	virtual void setMenu(Menu *m) { _menu = m; }
	virtual Menu *getMenu() const { return _menu; }

protected:
	Menu *_menu = nullptr;
};

class SP_PUBLIC Menu : public Surface {
public:
	constexpr static float MenuVerticalPadding = 16.0f;
	constexpr static float MenuItemHeight = 48.0f;
	constexpr static float MenuHorizontalIncrement = 56.0f;
	constexpr static float MenuLeadingHeight = 4.0f;
	constexpr static float MenuTrailingHeight = 8.0f;

	using ButtonCallback = Function<void(MenuButton *button)>;

	virtual ~Menu();

	virtual bool init() override;
	virtual bool init(MenuSource *source);

	virtual void setMenuSource(MenuSource *);
	virtual MenuSource *getMenuSource() const;

	virtual void setEnabled(bool value);
	virtual bool isEnabled() const;

	virtual void setMenuButtonCallback(const ButtonCallback &cb);
	virtual const ButtonCallback & getMenuButtonCallback();

	virtual void onMenuButtonPressed(MenuButton *button);
	virtual void layoutSubviews();

	virtual void handleContentSizeDirty() override;

	virtual ScrollView *getScroll() const;

	virtual DataListener<MenuSource> *getDataListener() const { return _menuListener; }

protected:
	virtual void rebuildMenu();
	virtual Rc<MenuButton> createButton(Menu *m, MenuSourceButton *btn);
	virtual Rc<MenuSeparator> createSeparator(Menu *m, MenuSourceItem *btn);

	virtual void handleSourceDirty();

	ScrollView *_scroll = nullptr;
	ScrollController *_controller = nullptr;
	DataListener<MenuSource> *_menuListener = nullptr;
	ButtonCallback _callback = nullptr;
};

}

#endif /* XENOLITH_RENDERER_MATERIAL2D_COMPONENTS_MENU_MATERIALMENU_H_ */
