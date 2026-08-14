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

#include "XLUiTableBorderPainter.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

bool TableBorderPainter::init() {
	if (!VectorSprite::init()) {
		return false;
	}

	setType("table-borders");
	// The borders are hairlines over the cells; drawing them in the transparent pass keeps them
	// from being depth-rejected behind the opaque cell surfaces they sit on.
	setRenderingLevel(RenderingLevel::Transparent);

	// The painter lives INSIDE the node whose borders it draws, so without this the table would
	// collect it as one more row - an empty one, which then stretches every vertical line down to
	// the bottom of the table box. Out of flow is exactly the right claim: it is an overlay, it
	// takes no space, and it sizes itself.
	setComponent<OutOfFlowComponent>();

	// A node's own handleComponentsDirty fires for ITS components, never for its parent's - and the
	// component we live off belongs to the parent. So check the generation at the start of each
	// visit instead: it is an integer compare when nothing changed, and it cannot miss an update
	// the way a one-shot notification can.
	makeDefaultCallbackSystem()->setVisitBeginCallback(
			[this](CallbackSystem *, FrameInfo &) { updateBorders(); });
	return true;
}

void TableBorderPainter::setSource(Node *source) {
	if (_source == source) {
		return;
	}
	_source = source;
	updateBorders(true);
}

void TableBorderPainter::handleContentSizeDirty() {
	VectorSprite::handleContentSizeDirty();
	// the image is built in the owner's content-box space, so a resize invalidates it wholesale
	updateBorders(true);
}

void TableBorderPainter::updateBorders(bool force) {
	auto source = _source ? _source : getParent();
	if (!source) {
		return;
	}

	auto borders = source->getComponent<TableBordersComponent>();
	if (!borders) {
		if (_generation != maxOf<uint64_t>()) {
			_generation = maxOf<uint64_t>();
			setImage(Rc<VectorImage>::create(Size2(1.0f, 1.0f)));
		}
		return;
	}
	// Match the owner exactly: the rects are in ITS content-box space, and the painter is the same
	// box drawn on top. Doing this here rather than making the caller do it is what lets a painter
	// be added with a bare addChild().
	const Size2 size = source->getContentSize();
	if (size.width <= 0.0f || size.height <= 0.0f) {
		return;
	}
	if (!force && borders->generation == _generation && size == _sourceSize) {
		return;
	}
	_generation = borders->generation;
	_sourceSize = size;

	if (getContentSize() != size) {
		setContentSize(size);
	}
	setAnchorPoint(Anchor::BottomLeft);
	setPosition(Vec2::ZERO);

	auto image = Rc<VectorImage>::create(size);

	// One path per rect. Batching several rects into one path per colour looks cheaper, but the
	// tessellator does not treat the closed subpaths as independent shapes - it fills across them,
	// which shows up as triangles spanning the whole table. The rect list is short by construction
	// (collapseTableBorders merges each grid line into runs), so a path each is the honest cost.
	for (auto &r : borders->rects) {
		// The rects go in as they are: a VectorImage built for a node's content size shares that
		// node's axes, so no y mirroring is needed (what Panel's addBox corner comment describes is
		// addBox's own corner ORDER, not a flipped image space).
		auto path = image->addPath();
		path->openForWriting([&](vg::PathWriter &writer) { writer.addRect(r.rect); })
				.setFillColor(r.color)
				.setStyle(vg::DrawFlags::Fill)
				// hard-edged axis-aligned rects; antialiasing would only blur the hairlines
				.setAntialiased(false);
	}

	setImage(sp::move(image));
}

} // namespace stappler::xenolith::ui
