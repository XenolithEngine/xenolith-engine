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

For a window with WindowCreationFlags::UserSpaceDecorations (which gets only the resize grips of
xenolith::WindowDecorations): the OS button cluster, the application icon, the draggable title
strip, and two application slots. The buttons are ui::Buttons of ButtonType::Os*, which handle the
window actions themselves.

The arrangement is the stylesheet's: this class writes no `order`, so a per-platform layout is an
`@media (platform: macos)` block. The contract a sheet writes against:

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

The frame and the title strip are Panels: an undeclared fill is opaque white, so the sheet must
give `window-frame` a background-color. The slots are plain Nodes: do not give them
`background-color: transparent`, which becomes node opacity and hides the slot's contents. */
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
		// The window menu button. Off by default: the icon already opens the menu.
		bool menuButton = false;
	};

	virtual ~WindowFrame();

	virtual bool init() override;
	virtual bool init(Config &&);

	virtual void setTitle(StringView);
	StringView getTitle() const;

	/* Application controls. Leading sits next to the icon, trailing next to the OS buttons (the
	side is the sheet's). Both append in call order and stamp `.frame-item` on the node. */
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

	/* The height the stylesheet gave the frame, read back from the content size;
	kDefaultFrameHeight until the first layout. */
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
