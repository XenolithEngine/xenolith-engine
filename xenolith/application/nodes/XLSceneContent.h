/**
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#ifndef XENOLITH_APPLICATION_NODES_XLSCENECONTENT_H_
#define XENOLITH_APPLICATION_NODES_XLSCENECONTENT_H_

#include "XLNode.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class WindowDecorations;
class CloseGuardWidget;

class SP_PUBLIC SceneContent : public Node {
public:
	using WindowDecorationsCallback = Function<Rc<WindowDecorations>(NotNull<SceneContent>)>;
	using CloseGuardWidgetCallback = Function<Rc<CloseGuardWidget>(NotNull<SceneContent>)>;

	virtual ~SceneContent();

	virtual bool init() override;

	virtual void handleEnter(Scene *) override;
	virtual void handleExit() override;

	virtual void handleContentSizeDirty() override;

	virtual bool handleBackButton();

	virtual void setHandlesViewDecoration(bool);
	virtual bool isHandlesViewDecoration() const { return _handlesViewDecoration; }

	virtual void showViewDecoration();
	virtual void hideViewDecoration();

	virtual void setCloseGuardEnabled(bool);
	virtual bool isCloseGuardEnabled() const;

	// Decoration padding is WM inset decorations + user window decorations
	virtual Padding getDecorationPadding() const;

	virtual void enableScissor();
	virtual void disableScissor();
	virtual bool isScissorEnabled() const;

	virtual void setWindowDecorationsContructor(WindowDecorationsCallback &&);
	virtual const WindowDecorationsCallback &getWindowDecorationsContructor() const {
		return _windowDecorationsConstructor;
	}

	virtual void setCloseGuardWidgetContructor(CloseGuardWidgetCallback &&);
	virtual const CloseGuardWidgetCallback &getCloseGuardWidgetContructor() const {
		return _closeGuardWidgetConstructor;
	}

protected:
	friend class Scene;

	virtual void handleBackgroundTransition(bool value);
	virtual void handleCloseRequest(bool value);
	virtual void handleWindowStateChanged(WindowState state, WindowState changes);

	InputListener *_inputListener = nullptr;
	DynamicStateSystem *_scissor = nullptr;

	bool _closeGuard = false;
	bool _closeGuardRetained = false;
	bool _handlesViewDecoration = true;
	bool _decorationVisible = true;

	WindowDecorations *_userDecorations = nullptr;
	WindowDecorationsCallback _windowDecorationsConstructor;

	CloseGuardWidget *_closeGuardWidget = nullptr;
	CloseGuardWidgetCallback _closeGuardWidgetConstructor;
};

} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_NODES_XLSCENECONTENT_H_ */
