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

The invisible half of drawing your own frame: eight resize grips laid over the whole content, one
per edge and one per corner. Each is a plain Node with an InputListener declaring a
WindowLayerFlags::Resize*Grip and the matching cursor, so the window system - not this class - is
what turns a drag on one into a resize.

IT ANSWERS LAST, ON PURPOSE. The grips cover the entire window border, including whatever an
application put there: a title bar of its own, its window buttons, a tab strip. So every listener
here is registered at DecorationsInputPriority, which is negative and therefore lands in the
POST-scene bucket - after every ordinary listener in the graph, and after SceneContent's own. The
same order is what the declared WindowLayer array is built in, and NativeWindow resolves that array
TOP-FIRST: the first layer under the pointer decides the cursor and the grip, and layers below it
are not consulted. So an application layer over a grip simply wins, and a layer that declares
WindowLayerFlags::GripGuard and nothing else is how it says "no grip here" without having to know
which grip it is shadowing.

Z-ORDER IS NOT THAT MECHANISM, and this node keeps a very high one. It draws nothing at all, so its
z-order is not about painting; what it buys is that handleLayoutInParent runs after the content it
covers. Input order is the priority above, and the two are deliberately independent. */
class SP_PUBLIC WindowDecorations : public Node {
public:
	// Below SceneContent's own -1, so the grips are the very last thing asked about a pointer.
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
