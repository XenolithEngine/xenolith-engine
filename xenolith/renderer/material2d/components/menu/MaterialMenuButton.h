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

#ifndef XENOLITH_RENDERER_MATERIAL2D_COMPONENTS_MENU_MATERIALMENUBUTTON_H_
#define XENOLITH_RENDERER_MATERIAL2D_COMPONENTS_MENU_MATERIALMENUBUTTON_H_

#include "MaterialButton.h"
#include "MaterialMenu.h"
#include "XLFontController.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::material2d {

class SP_PUBLIC MenuButton : public Button, public MenuItemInterface {
public:
	static float getMaxWidthForButton(MenuSourceButton *, font::FontController *, float density);

	virtual ~MenuButton() { }

	virtual bool init(Menu *, MenuSourceButton *);

	virtual void setWrapName(bool);
	virtual bool isWrapName() const;

protected:
	virtual void handleButton();
	virtual void layoutContent();

	bool _wrapName = true;
};

}

#endif /* XENOLITH_RENDERER_MATERIAL2D_COMPONENTS_MENU_MATERIALMENUBUTTON_H_ */
