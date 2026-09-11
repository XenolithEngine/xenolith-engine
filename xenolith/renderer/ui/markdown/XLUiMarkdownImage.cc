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

#include "XLUiMarkdownImage.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

bool MarkdownImageSystem::init() {
	if (!System::init()) {
		return false;
	}

	_systemFlags = SystemFlags::HandleVisitSelf | SystemFlags::HandleNodeEvents;
	return true;
}

void MarkdownImageSystem::addImage(NotNull<Node> node, uint32_t objectIndex) {
	_images.emplace_back(Entry{node.get(), objectIndex, Rect::ZERO});
}

Rect MarkdownImageSystem::getImageRect(uint32_t i) const {
	return i < _images.size() ? _images[i].rect : Rect::ZERO;
}

void MarkdownImageSystem::handleVisitSelf(FrameInfo &info, Node *node, NodeVisitFlags flags) {
	System::handleVisitSelf(info, node, flags);
	reposition();
}

void MarkdownImageSystem::handleContentSizeDirty() {
	System::handleContentSizeDirty();
	reposition();
}

void MarkdownImageSystem::reposition() {
	auto label = dynamic_cast<basic2d::Label *>(_owner);
	if (!label) {
		return;
	}

	for (auto &it : _images) {
		auto rect = label->getInlineObjectRect(it.objectIndex);
		if (rect.equals(it.rect)) {
			continue;
		}
		it.rect = rect;

		// A box with no extent is a picture the layout has not placed yet - or one that fell
		// outside a line limit. Either way there is nothing to draw over.
		if (rect.size.width <= 0.0f || rect.size.height <= 0.0f) {
			it.node->setVisible(false);
			continue;
		}

		it.node->setVisible(true);
		it.node->setContentSize(rect.size);
		it.node->setPosition(rect.origin);
	}
}

} // namespace stappler::xenolith::ui
