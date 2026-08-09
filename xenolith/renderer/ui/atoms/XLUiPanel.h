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

#ifndef XENOLITH_RENDERER_UI_ATOMS_XLUIPANEL_H_
#define XENOLITH_RENDERER_UI_ATOMS_XLUIPANEL_H_

#include "XLUiConfig.h"
#include "XLUiStyleResolver.h" // ResolvedStyle + document::ParameterName of setStyleValue below

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

// The resolved paint of a Panel and of everything built on one (badge, checkbox, button, ...): a
// fill, an optional outline stroke and the four CSS corner radii. Created on the first styled
// attribute and dropped by CmdReset, so a widget that no rule matches carries NO component at all
// and paints the defaults below.
struct PanelStyleComponent {
	static ComponentId Id;

	Color4B backgroundColor = Color4B::WHITE;
	Color4B outlineColor = Color4B::BLACK;
	float outlineWidth = 0.0f;
	float borderRadiusTopLeft = 0.0f;
	float borderRadiusTopRight = 0.0f;
	float borderRadiusBottomRight = 0.0f;
	float borderRadiusBottomLeft = 0.0f;
};

// Passive rounded container: background-color, outline-color/-width and border-radius driven by
// CSS (type "panel"). A flex-capable surface for cards, working panels, table backings - and the
// painted base of every atom in this directory. CSS:
//   panel { background-color:#232323; border-radius:20px; padding:25px; display:flex; ... }
class SP_PUBLIC Panel : public basic2d::VectorSprite {
public:
	virtual ~Panel();

	virtual bool init() override;

	virtual void handleContentSizeDirty() override;

	// Direct paint, for surfaces built outside a stylesheet (auxiliary windows that do not share
	// the main StyleSystem). CSS is the primary path; these write the same style component, so a
	// style pass that declares the attribute overrides them.
	virtual void setPathColor(const Color4B &, bool withOpacity);
	virtual Color4B getPathColor() const;

	// single radius for all four corners; getter reports the top-left one
	virtual void setBorderRadius(float);
	virtual float getBorderRadius() const;

	virtual void setOutline(const Color4B &, float width);

	virtual bool setStyleValue(const ResolvedStyle &, document::ParameterName,
			const document::StyleValue &);

protected:
	using VectorSprite::init;

	// Registers the shared surface appliers (background-color, outline-*, border-radius, CmdReset)
	// for CSS type `type`, routing them all into Panel::setStyleValue. Every Panel-derived atom
	// calls it with its own type from init(); repeated calls for the same type are ignored.
	static void registerStyleAppliers(StringView type);

	// (re)build the VectorImage: a (optionally rounded) rect filled with the resolved background
	// colour, plus an outline stroke when its width is > 0
	virtual void updateBackgroundImage();

	// mutate the style component, creating it on demand; when the callback reports a change the
	// background is rebuilt. The guard keeps an unchanged value from re-dirtying the cascade.
	void updateStyle(const Callback<bool(NotNull<PanelStyleComponent>)> &);
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_ATOMS_XLUIPANEL_H_
