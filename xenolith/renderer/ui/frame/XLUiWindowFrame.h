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

#ifndef XENOLITH_RENDERER_UI_FRAME_XLUIWINDOWFRAME_H_
#define XENOLITH_RENDERER_UI_FRAME_XLUIWINDOWFRAME_H_

#include "XLUiPanel.h"
#include "XLUiButton.h"
#include "XL2dLabel.h"
#include "XL2dSprite.h"
#include "XL2dIconSprite.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

/* The title bar of a window that draws its own decorations.

A window created with WindowCreationFlags::UserSpaceDecorations gets no title bar from the system,
only the eight invisible resize grips that xenolith::WindowDecorations provides. This is the other
half: the OS button cluster, the application icon, the draggable strip carrying the title, and two
slots an application fills with buttons or menus of its own.

The buttons are ui::Buttons of ButtonType::Os*, so minimize / maximize / close / the window menu
already work, the maximize glyph already swaps when the window state changes, and macOS already
gets the Apple traffic-light icon theme. Nothing here talks to the window directly.

ARRANGEMENT IS THE STYLESHEET'S. The children are created in a fixed order and this class writes no
`order` at all, because the arrangement is per-platform: the OS cluster sits at the right on
Linux/Windows and at the LEFT on macOS, where the title also centres over the whole bar. Doing that
in C++ would mean a platform #if in every application; in CSS it is one `@media (platform: macos)`
block. The contract this publishes, and which a sheet writes against:

  type      window-frame
  names     #os-minimize #os-maximize #os-close #os-menu #frame-icon #frame-title
            #frame-leading #frame-trailing
  classes   .os-button (on each OS button), .frame-item (on anything put in a slot),
            .frame-title-line (on the draggable strip)
  variable  --frame-h, read back by getFrameHeight()

A workable default, with the leading slot after the icon and the trailing slot before the buttons:

  :root         { --frame-h: 32px; }
  window-frame  { height: var(--frame-h); background-color:#1a1a1a;
                  display:flex; flex-direction:row; align-items:center; }
  window-frame > #frame-icon     { order: 0; }
  window-frame > #frame-leading  { order: 1; display:flex; flex-direction:row; }
  window-frame > .frame-title-line { order: 2; flex-grow: 1; }
  window-frame > #frame-trailing { order: 3; display:flex; flex-direction:row; }
  window-frame > #os-minimize    { order: 98; }
  window-frame > #os-maximize    { order: 99; }
  window-frame > #os-close       { order: 100; }
  @media (platform: macos) {
      window-frame > #os-close    { order: 0; }
      window-frame > #os-minimize { order: 1; }
      window-frame > #os-maximize { order: 2; }
      window-frame > #frame-icon  { display: none; }
  }

Note that this is a Panel, so a fill it does not declare is an opaque WHITE surface: the sheet MUST
give `window-frame` a background-color. The title strip is a Panel for the same reason, and is
transparent only because the default sheet above leaves it so.

The two SLOTS, on the other hand, are plain Nodes: they draw nothing at all, and a sheet must NOT
give them `background-color: transparent` to say so. A colour with alpha is written into the node's
OPACITY, which multiplies down the whole subtree - so that declaration hides whatever the
application put in the slot. */
class SP_PUBLIC WindowFrame : public Panel {
public:
	struct Config {
		StringView title;
		// Name of an image in the window's render queue, as basic2d::Sprite takes it. Empty and
		// with `icon` unset means no icon node is created at all.
		StringView iconImage;
		// A vector icon instead of an image. Ignored when `iconImage` is set.
		IconName icon = IconName::None;

		bool minimize = true;
		bool maximize = true;
		bool close = true;
		// The window menu button. Off by default: the icon already opens the menu on either
		// click, and most applications do not want a second affordance for it.
		bool menuButton = false;
	};

	virtual ~WindowFrame();

	virtual bool init() override;
	virtual bool init(Config &&);

	virtual void setTitle(StringView);
	StringView getTitle() const;

	/* Application controls.

	Leading sits next to the application icon, trailing next to the OS button cluster - "next to"
	rather than a side, because which side that is depends on the platform and is the sheet's to
	decide. Both append in call order and stamp `.frame-item` on the node, so a sheet can size and
	space whatever an application puts there without knowing what it is. */
	virtual Node *addLeading(Rc<Node> &&);
	virtual Node *addTrailing(Rc<Node> &&);
	virtual void clearLeading();
	virtual void clearTrailing();

	Node *getLeadingSlot() const { return _leading; }
	Node *getTrailingSlot() const { return _trailing; }

	// The draggable strip. It carries WindowLayerFlags::MoveGrip, which is what makes a drag on it
	// move the window; put something in a slot instead of adding children here.
	Node *getTitleLine() const { return _titleLine; }

	Node *getIcon() const { return _icon; }

	// Null for a button the Config switched off.
	Button *getOsButton(ButtonType) const;

	/* The height the frame occupies, so code that has to reason about it - an overlay covering
	everything below the bar - does not duplicate the number.

	This is the height the stylesheet gave it, read back from the resolved contentSize, and
	kDefaultFrameHeight only until the first layout has run. Reading it back rather than storing it
	is what keeps the value in one place: a title bar height that is also a constant in C++ is a
	number two files have to agree about, and they drift. */
	float getFrameHeight() const;

	static constexpr float kDefaultFrameHeight = 32.0f;

protected:
	using Panel::init;

	// Builds an OS button, names it and gives it the `.os-button` class.
	Button *makeOsButton(ButtonType, StringView name);

	Button *_osMinimize = nullptr;
	Button *_osMaximize = nullptr;
	Button *_osClose = nullptr;
	Button *_osMenu = nullptr;

	Node *_icon = nullptr;
	Node *_leading = nullptr;
	Node *_trailing = nullptr;
	Node *_titleLine = nullptr;
	basic2d::Label *_titleLabel = nullptr;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_FRAME_XLUIWINDOWFRAME_H_
