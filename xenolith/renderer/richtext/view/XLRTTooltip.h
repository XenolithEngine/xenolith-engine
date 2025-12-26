/**
 Copyright (c) 2024 Stappler LLC <admin@stappler.dev>

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

#ifndef EXTRA_DOCUMENT_RICHTEXT_VIEW_XLRTTOOLTIP_H_
#define EXTRA_DOCUMENT_RICHTEXT_VIEW_XLRTTOOLTIP_H_

#include "XLRTListenerView.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::richtext {

/*
class SP_PUBLIC Tooltip : public material2d::FlexibleLayout {
public:
	using Result = document::LayoutResult;
	using CloseCallback = std::function<void()>;
	using CopyCallback = std::function<void(const String &)>;

	static EventHeader onCopy;

	virtual ~Tooltip();
	virtual bool init(RendererResult *, const Vector<String> &ids, WideStringView text);
	virtual void handleContentSizeDirty() override;
	virtual void handleEnter(Scene *) override;

	virtual void setMaxContentSize(const Size2 &);
	virtual void setOriginPosition(const Vec2 &pos, const Size2 &parentSize, const Vec2 &);

	virtual void setExpanded(bool value);

	virtual void pushToForeground(Scene *);
	virtual void close();

	virtual material2d::AppBar *getToolbar() const { return _toolbar; }
	virtual material2d::MenuSource *getActions() const { return _actions; }

	virtual Size2 getDefaultSize() const { return _defaultSize; }

	virtual void onDelayedFadeOut();
	virtual void onFadeIn();
	virtual void onFadeOut();

	virtual void setCloseCallback(const CloseCallback &);

	virtual Renderer *getRenderer() const;

	virtual String getSelectedString() const;

	virtual void onPush(material2d::SceneContent2d *l, bool replace) override;

protected:
	virtual void onResult(RendererResult *);

	virtual void copySelection();
	virtual void cancelSelection();
	virtual void onSelection();

	Vec2 _originPosition;
	Vec2 _worldPos;
	Size2 _parentSize;
	Size2 _maxContentSize;

	Size2 _defaultSize;

	InputListener *_listener = nullptr;
	InputListener *_closeListener = nullptr;

	Rc<material2d::MenuSource> _actions;
	Rc<material2d::MenuSource> _selectionActions;
	material2d::Surface *_surface = nullptr;
	material2d::AppBar *_toolbar = nullptr;
	ListenerView *_view = nullptr;

	CloseCallback _closeCallback;

	bool _expanded = true;
};
*/

}

#endif /* EXTRA_DOCUMENT_RICHTEXT_VIEW_XLRTTOOLTIP_H_ */
