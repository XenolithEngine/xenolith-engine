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

The LayoutSystem resolves CSS border conflicts into a flat list of rects in a
`TableBordersComponent` (a layout system must not create nodes); this sprite draws them.

Add it as a child of the node carrying the component - the table container for a static
`display: table`, or the row for a virtualized one (`ui::TableView` does this per row):

    auto painter = table->addChild(Rc<TableBorderPainter>::create(), ZOrder(10));

It sizes itself to its parent and re-reads the component when the parent's geometry or components
change. Give it a z-order above the cells. Nothing installs it automatically. */
class SP_PUBLIC TableBorderPainter : public basic2d::VectorSprite {
public:
	virtual ~TableBorderPainter() = default;

	virtual bool init() override;

	virtual void handleContentSizeDirty() override;

	// Where to read the rects from. Defaults to the parent; set it when the painter cannot be a
	// child of the owner.
	void setSource(Node *);
	Node *getSource() const { return _source; }

protected:
	using VectorSprite::init;

	// rebuild the VectorImage from the source's TableBordersComponent, if its generation moved
	void updateBorders(bool force = false);

	Node *_source = nullptr; // not owned: it is this node's own ancestor
	uint64_t _generation = maxOf<uint64_t>(); // forces the first build

	// The size the current image was built for. A VectorImage is stretched to the content size, and
	// a resize may keep the same rects and generation, so the size is checked separately.
	Size2 _sourceSize;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_ATOMS_XLUITABLEBORDERPAINTER_H_
