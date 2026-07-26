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

#ifndef XENOLITH_RENDERER_UI_ATOMS_XLUIBUTTON_H_
#define XENOLITH_RENDERER_UI_ATOMS_XLUIBUTTON_H_

#include "XLUiInteractiveComponent.h"
#include "XL2dIconSprite.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

class Button : public basic2d::VectorSprite {
public:
	virtual ~Button();

	virtual bool init(Function<void()> && = nullptr);

	virtual void handleContentSizeDirty() override;

	virtual void setString(StringView);
	virtual StringView getString() const;

	virtual void setIcon(IconName);
	virtual IconName getIcon() const;

	//virtual void setBackgroundColor(const Color4B &);
	virtual Color4B getBackgroundColor() const { return _backgroundColor; }

	//virtual void setOutlineColor(const Color4B &);
	virtual Color4B getOutlineColor() const { return _outlineColor; }

	//virtual void setOutlineWidth(float);
	virtual float getOutlineWidth() const { return _outlineWidth; }

protected:
	Color4B _backgroundColor;
	Color4B _outlineColor;
	float _outlineWidth = 0.0f;

	Function<void()> _callback;

	InputListener *_listener = nullptr;

	basic2d::Label *_label = nullptr;
	basic2d::IconSprite *_icon = nullptr;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_ATOMS_XLUIBUTTON_H_
