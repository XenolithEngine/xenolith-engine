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

#ifndef XENOLITH_RENDERER_BASIC2D_XL2DINPUTFIELD_H_
#define XENOLITH_RENDERER_BASIC2D_XL2DINPUTFIELD_H_

#include "XL2dInputLabel.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

class SP_PUBLIC InputField : public Node, protected InputLabelDelegate {
public:
	using Handler = TextInputHandler;
	using Cursor = TextCursor;
	using PasswordMode = InputLabel::PasswordMode;
	using Error = InputLabel::Error;
	using Callback = Function<void()>;
	using CharFilter = Function<bool(char16_t)>;

	virtual bool init() override;
	virtual void handleContentSizeDirty() override;
	virtual bool visitGeometry(FrameInfo &, NodeFlags parentFlags) override;

	virtual void handleEnter(Scene *) override;
	virtual void handleExit() override;

	virtual void setInputCallback(const Callback &);
	virtual const Callback &getInputCallback() const;

	virtual void setMaxChars(size_t);
	virtual size_t getMaxChars() const;

	virtual void setInputType(TextInputType);
	virtual TextInputType getInputType() const;

	virtual void setPasswordMode(PasswordMode);
	virtual PasswordMode getPasswordMode();

	virtual void setAllowAutocorrect(bool);
	virtual bool isAllowAutocorrect() const;

	virtual void setEnabled(bool);
	virtual bool isEnabled() const;

	virtual void setNormalColor(const Color &);
	virtual const Color &getNormalColor() const;

	virtual void setErrorColor(const Color &);
	virtual const Color &getErrorColor() const;

	virtual bool empty() const;

	virtual void setPlaceholder(const WideStringView &);
	virtual void setPlaceholder(const StringView &);
	virtual WideStringView getPlaceholder() const;

	virtual void setString(const WideString &);
	virtual void setString(const String &);
	virtual WideStringView getString() const;

	virtual InputLabel *getLabel() const;

	virtual void setCharFilter(const CharFilter &);
	virtual const CharFilter &getCharFilter() const;

	virtual void acquireInput();
	virtual void releaseInput();

	virtual bool isInputActive() const;

public:
	virtual bool onPressBegin(const Vec2 &);
	virtual bool onLongPress(const Vec2 &, const TimeInterval &, int count);
	virtual bool onPressEnd(const Vec2 &);
	virtual bool onPressCancel(const Vec2 &);

	virtual bool onSwipeBegin(const Vec2 &, const Vec2 &);
	virtual bool onSwipe(const Vec2 &, const Vec2 &);
	virtual bool onSwipeEnd(const Vec2 &);

	virtual void onMenuCut();
	virtual void onMenuCopy();
	virtual void onMenuPaste();

protected:
	virtual bool handleInputChar(char16_t) override;
	virtual void handleActivated(bool) override;
	virtual void handlePointer(bool) override;
	virtual void handleCursor(const Cursor &) override;

	virtual void setMenuPosition(const Vec2 &);

	virtual void updateMenu();
	virtual void onMenuVisible();
	virtual void onMenuHidden();

	virtual Rc<InputLabel> makeLabel();

	bool _hasSwipe = false;
	Color _normalColor = Color::Blue_500;
	Color _errorColor = Color::Red_500;

	InputListener *_inputListener = nullptr;
	InputLabelContainer *_node = nullptr;
	InputLabel *_label = nullptr;
	Label *_placeholder = nullptr;
	//Rc<InputMenu> _menu;
	Callback _onInput;
	CharFilter _charFilter;

	Vec2 _menuPosition;
};

}

#endif /* XENOLITH_RENDERER_BASIC2D_XL2DINPUTFIELD_H_ */
