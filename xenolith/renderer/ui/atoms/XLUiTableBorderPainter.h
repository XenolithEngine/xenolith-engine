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

#ifndef XENOLITH_RENDERER_UI_ATOMS_XLUITABLEBORDERPAINTER_H_
#define XENOLITH_RENDERER_UI_ATOMS_XLUITABLEBORDERPAINTER_H_

#include "XLUiConfig.h" // IWYU pragma: keep
#include "XLUiLayoutSystem.h" // TableBordersComponent + OutOfFlowComponent
#include "XL2dVectorSprite.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

/* Draws the collapsed table borders a layout pass published.

The LayoutSystem resolves the CSS border conflicts and leaves the answer as geometry - a flat list
of rects in a `TableBordersComponent`. It cannot draw them itself: a layout system must never create
nodes. This is the consumer that turns that list into one filled path per colour.

Add it as a child of the node carrying the component - the table container for a static
`display: table`, or the row for a virtualized one (`ui::TableView` does this per row):

    auto painter = table->addChild(Rc<TableBorderPainter>::create(), ZOrder(10));

It sizes itself to its parent and re-reads the component whenever the parent's geometry or
components change, so nothing has to be re-plumbed when the table re-lays-out. Give it a z-order
above the cells: the borders sit on the cell boundaries and would otherwise be painted over.

Nothing installs it automatically. A table with `border-collapse: collapse` and no painter is not a
bug - it is a table whose borders someone else is drawing, or nobody is. */
class SP_PUBLIC TableBorderPainter : public basic2d::VectorSprite {
public:
	virtual ~TableBorderPainter() = default;

	virtual bool init() override;

	virtual void handleContentSizeDirty() override;

	// Where to read the rects from. Defaults to the parent, which is what a child of the table (or
	// of the row) wants; point it elsewhere when the painter cannot be a child of the owner.
	void setSource(Node *);
	Node *getSource() const { return _source; }

protected:
	using VectorSprite::init;

	// rebuild the VectorImage from the source's TableBordersComponent, if its generation moved
	void updateBorders(bool force = false);

	Node *_source = nullptr; // not owned: it is this node's own ancestor
	uint64_t _generation = maxOf<uint64_t>(); // forces the first build

	// The size the current image was built for. A VectorImage is STRETCHED to the node's content
	// size, so an image built at one size and kept across a resize draws the borders scaled - which
	// looks like a layout bug and is not one. The border generation alone cannot catch this: a
	// resize that produces the same rects leaves it untouched.
	Size2 _sourceSize;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_ATOMS_XLUITABLEBORDERPAINTER_H_
