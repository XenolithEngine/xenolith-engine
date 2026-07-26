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

#include "InstallerLayout.h"
#include "InstallerTitleBar.h"
#include "XL2dLayer.h"
#include "XLUiStyleResolver.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

InstallerLayout::~InstallerLayout() { }

bool InstallerLayout::init() {
	if (!basic2d::SceneLayout2d::init()) {
		return false;
	}

	setName("installer-layout");

	// recursive: this one resolver styles the whole layout subtree
	// (#installer-layout + #title-bar / #promo-bar / .content-space children)
	addSystem(Rc<ui::StyleResolver>::create(true));

	_titleBar = addChild(Rc<TitleBar>::create());
	_titleBar->setName("title-bar");

	_promoBar = addChild(Rc<basic2d::Layer>::create(Color::Grey_600));
	_promoBar->setName("promo-bar");

	_content = addChild(Rc<basic2d::Layer>::create(Color::Grey_800));
	_content->addStyleClass("content-space");

	return true;
}

} // namespace stappler::xenolith::installer
