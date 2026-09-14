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

#ifndef XENOLITH_APPLICATION_NODES_XLWINDOWDECORATIONS_H_
#define XENOLITH_APPLICATION_NODES_XLWINDOWDECORATIONS_H_

#include "XLNode.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

/* Window header for user-space window decorations.

Eight invisible resize grips over the content (edges and corners): plain Nodes whose InputListener
declares a WindowLayerFlags::Resize*Grip and cursor; the window system performs the resize.

Listeners use DecorationsInputPriority (post-scene band), so they are declared last. NativeWindow
resolves window layers top-first, so an application layer over a grip wins, and a layer with only
WindowLayerFlags::GripGuard suppresses the grip. The high z-order only makes handleLayoutInParent
run after the covered content; input order is set by priority. */
class SP_PUBLIC WindowDecorations : public Node {
public:
	// Below SceneContent's own -1, so the grips are asked about a pointer last.
	static constexpr int32_t DecorationsInputPriority = -1'000;

	virtual ~WindowDecorations() = default;

	virtual bool init() override;

	virtual bool shouldBePresentedOnScene(Scene *) const;

	virtual Padding getPadding() const { return Padding(); }

	virtual WindowCapabilities getCapabilities() const { return _capabilities; }

	virtual void handleEnter(Scene *) override;
	virtual void handleContentSizeDirty() override;
	virtual void handleLayoutInParent(Node *) override;

protected:
	virtual void updateWindowState(WindowState);
	virtual void updateWindowTheme(const ThemeInfo &);

	// virtual nodes for resize implementation
	Node *_resizeTopLeft = nullptr;
	Node *_resizeTop = nullptr;
	Node *_resizeTopRight = nullptr;
	Node *_resizeRight = nullptr;
	Node *_resizeBottomRight = nullptr;
	Node *_resizeBottom = nullptr;
	Node *_resizeBottomLeft = nullptr;
	Node *_resizeLeft = nullptr;

	WindowState _currentState = WindowState::None;
	WindowCapabilities _capabilities = WindowCapabilities::None;
};

} // namespace stappler::xenolith

#endif // XENOLITH_APPLICATION_NODES_XLWINDOWDECORATIONS_H_
