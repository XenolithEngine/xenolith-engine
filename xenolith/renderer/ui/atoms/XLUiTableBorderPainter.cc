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
	// Transparent pass, so the hairlines are not depth-rejected behind the opaque cells.
	setRenderingLevel(RenderingLevel::Transparent);

	// The painter is a child of the table; out of flow keeps it from being laid out as a row.
	setComponent<OutOfFlowComponent>();

	// handleComponentsDirty does not fire for the parent's components, so the generation is
	// checked on each visit instead.
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
	// Match the owner exactly: the rects are in its content-box space.
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

	// One path per rect: the tessellator fills across closed subpaths of a shared path. The list is
	// short (collapseTableBorders merges each grid line into runs).
	for (auto &r : borders->rects) {
		// No y mirroring: a VectorImage built for the content size shares the node's axes.
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
