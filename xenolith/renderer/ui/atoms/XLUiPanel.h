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

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

class ScrollView;

} // namespace stappler::xenolith::basic2d

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

// The resolved paint of a Panel and of everything built on one (badge, checkbox, button, ...): a
// fill, an optional outline stroke and the four CSS corner radii. CmdReset rewinds it to the
// widget's own paint; a widget with no matching rule and no own paint has no component and draws
// the defaults below.
struct PanelStyleComponent {
	static ComponentId Id;

	Color4B backgroundColor = Color4B::WHITE;
	Color4B outlineColor = Color4B::BLACK;
	float outlineWidth = 0.0f;
	document::BorderStyle outlineStyle = document::BorderStyle::Solid;
	float borderRadiusTopLeft = 0.0f;
	float borderRadiusTopRight = 0.0f;
	float borderRadiusBottomRight = 0.0f;
	float borderRadiusBottomLeft = 0.0f;

	bool operator==(const PanelStyleComponent &) const = default;
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

	/* An opaque hard-edged rectangle (no radius, no translucency, no antialiasing) draws at
	`Solid`, which writes destination alpha; a blended draw would leave a transparent window (a
	popup) see-through. Everything else draws at `Surface`. An explicit `setRenderingLevel` wins. */
	virtual RenderingLevel getRealRenderingLevel() const override;

	/* Direct paint, for surfaces outside a stylesheet and for a widget's own defaults. It is the
	layer under the stylesheet: a matching rule overrides it, and CmdReset rewinds the component to
	it rather than removing it. */
	virtual void setPathColor(const Color4B &, bool withOpacity);
	virtual Color4B getPathColor() const;

	// single radius for all four corners; getter reports the top-left one
	virtual void setBorderRadius(float);
	virtual float getBorderRadius() const;

	virtual void setOutline(const Color4B &, float width);

	virtual bool setStyleValue(const ResolvedStyle &, document::ParameterName,
			const document::StyleValue &);

	/* Registers the shared surface appliers (background-color, outline-*, border-radius, CmdReset)
	for CSS type `type`, routing them all into Panel::setStyleValue. Repeated calls for the same
	type are ignored. Code that renames a panel's type (ui::openPopupSurface: `menu`) must call it
	too, or `background-color` falls through to the node tint. */
	static void registerStyleAppliers(StringView type);

protected:
	using VectorSprite::init;

	// (re)build the VectorImage: a (optionally rounded) rect filled with the resolved background
	// colour, plus an outline stroke when its width is > 0
	virtual void updateBackgroundImage();

	// mutate the style component, creating it on demand; when the callback reports a change the
	// background is rebuilt. The guard keeps an unchanged value from re-dirtying the cascade.
	void updateStyle(const Callback<bool(NotNull<PanelStyleComponent>)> &);

	// The widget's own paint (setPathColor / setBorderRadius / setOutline), which CmdReset rewinds
	// to. The flag decides whether a reset restores the component or drops it.
	PanelStyleComponent _ownStyle;
	bool _ownPainted = false;
};

/* Give a scroll view a bar a stylesheet can paint.

Swaps the view's LayerRounded track and thumb for Panels, so `outline-*` and per-corner radii apply,
and registers the surface appliers for their two types:

    scroll-indicator-track        { background-color: transparent; border-radius: 5px; }
    scroll-indicator-track:hover  { background-color: rgba(0,0,0,0.25); }
    scroll-indicator              { background-color: rgba(255,255,255,0.35); border-radius: 3px; }
    scroll-indicator.active       { outline: 1px solid rgba(0,0,0,0.4); }
    tree-view scroll-indicator-track { display: none; }

`.active` is on both nodes while the bar is grabbable. `display: none` removes the bar; its size is
not a style (setIndicatorThickness). Idempotent, and keeps the existing paint until a rule matches;
ui::TreeView and ui::TableView call it themselves. */
SP_PUBLIC void useStyledScrollIndicator(NotNull<basic2d::ScrollView>);

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_ATOMS_XLUIPANEL_H_
