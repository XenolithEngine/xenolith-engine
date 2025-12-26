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

#ifndef XENOLITH_RENDERER_MATERIAL2D_COMPONENTS_MENU_MATERIALFLOATINGMENU_H_
#define XENOLITH_RENDERER_MATERIAL2D_COMPONENTS_MENU_MATERIALFLOATINGMENU_H_

#include "MaterialMenu.h"
#include "MaterialOverlayLayout.h"
#include "XL2dSceneContent.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::material2d {

class SP_PUBLIC FloatingMenu : public Menu {
public:
	using CloseCallback = Function<void ()>;
	using Binding = OverlayLayout::Binding;

	static void push(SceneContent2d *, MenuSource *source, const Vec2 &, Binding = Binding::Relative, Menu *root = nullptr);

	virtual bool init(MenuSource *source, Menu *root = nullptr);

	virtual void setCloseCallback(const CloseCallback &);
	virtual const CloseCallback & getCloseCallback() const;

	virtual void close();
	virtual void closeRecursive();

	virtual void onMenuButton(MenuButton *btn);

	virtual float getMenuWidth(Node *root);
	virtual float getMenuHeight(Node *root, float width);

	virtual void setReady(bool);
	virtual bool isReady() const;

protected:
	virtual void onCapturedTap();

	bool _ready = false;
	Menu *_root = nullptr;
	CloseCallback _closeCallback = nullptr;
};

}

#endif /* XENOLITH_RENDERER_MATERIAL2D_COMPONENTS_MENU_MATERIALFLOATINGMENU_H_ */
