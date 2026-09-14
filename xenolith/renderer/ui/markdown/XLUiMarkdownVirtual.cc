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

#include "XLUiMarkdownVirtual.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

// A spacer: no selector-reachable type, nothing to draw, and a fixed size for the layout.
static Rc<Node> MarkdownVirtual_makeSpacer() {
	auto node = Rc<Node>::create();
	node->setAnchorPoint(Anchor::BottomLeft);
	node->setComponent<MeasureComponent>(MeasureComponent{Size2(0.0f, 0.0f)});
	return node;
}

bool MarkdownVirtualizer::init(NotNull<Node> content, uint32_t threshold) {
	clear();

	_content = content.get();

	for (auto &child : _content->getChildren()) {
		_blocks.emplace_back(Block{child, 0.0f, 0.0f, false, true});
	}

	if (_blocks.size() < threshold) {
		_blocks.clear();
		_content = nullptr;
		return false;
	}

	// Blocks took z-order from their build position, so the spacers sort before and after them.
	_spacerTop = _content->addChild(MarkdownVirtual_makeSpacer(), ZOrder(-1));
	_spacerBottom = _content->addChild(MarkdownVirtual_makeSpacer(),
			ZOrder(int16_t(sprt::min(size_t(maxOf<int16_t>() - 1), _blocks.size() + 1))));

	// Everything starts hidden, so the first frame costs one chunk.
	for (auto &it : _blocks) { collapse(it); }

	_enabled = true;
	return true;
}

void MarkdownVirtualizer::clear() {
	for (auto &it : _blocks) {
		if (it.node) {
			it.node->setVisible(true);
		}
	}
	_blocks.clear();

	if (_spacerTop) {
		_spacerTop->removeFromParent();
		_spacerTop = nullptr;
	}
	if (_spacerBottom) {
		_spacerBottom->removeFromParent();
		_spacerBottom = nullptr;
	}

	_content = nullptr;
	_measured = 0;
	_chunkBegin = _chunkEnd = maxOf<uint32_t>();
	_chunkAge = 0;
	_knownHeight = 0.0f;
	_enabled = false;
}

void MarkdownVirtualizer::setSpacer(Node *spacer, float height) {
	if (!spacer) {
		return;
	}

	height = sprt::max(height, 0.0f);
	if (sprt::fabs(spacer->getContentSize().height - height) < 0.5f) {
		return;
	}

	// The flex pass measures the component; the content size is read if nothing is measured.
	spacer->setComponent<MeasureComponent>(MeasureComponent{Size2(0.0f, height)});
	spacer->setContentSize(Size2(0.0f, height));
}

void MarkdownVirtualizer::collapse(Block &block) {
	if (!block.visible) {
		return;
	}
	block.visible = false;
	block.node->setVisible(false);
}

void MarkdownVirtualizer::materialize(Block &block) {
	if (block.visible) {
		return;
	}
	block.visible = true;
	block.node->setVisible(true);
}

uint32_t MarkdownVirtualizer::getVisibleCount() const {
	uint32_t count = 0;
	for (auto &it : _blocks) {
		if (it.visible) {
			++count;
		}
	}
	return count;
}

float MarkdownVirtualizer::getBlockTop(uint32_t index) const {
	if (index >= _blocks.size() || !_blocks[index].measured) {
		return maxOf<float>();
	}
	return _blocks[index].top;
}

uint32_t MarkdownVirtualizer::findBlock(const Node *node) const {
	for (; node; node = node->getParent()) {
		for (uint32_t i = 0; i < _blocks.size(); ++i) {
			if (_blocks[i].node == node) {
				return i;
			}
		}
	}
	return maxOf<uint32_t>();
}

void MarkdownVirtualizer::commitChunk() {
	if (_chunkBegin == maxOf<uint32_t>()) {
		return;
	}

	// Give the chunk a frame to settle before believing its heights; see _chunkAge.
	if (_chunkAge < 1) {
		++_chunkAge;
		return;
	}

	// The advance is the distance to the next block's top, so flex gaps and margins are counted
	// and a virtual document is as tall as a real one.
	auto top = [](const Node *node) {
		return node->getPosition().y + node->getContentSize().height;
	};

	// The chunk's last block stays unmeasured unless it ends the document: without a visible
	// successor its gap is unknown. The next chunk starts with it.
	auto last = sprt::min(_chunkEnd, uint32_t(_blocks.size()));
	if (last < _blocks.size() && last > _chunkBegin + 1) {
		--last;
	}

	for (auto i = _chunkBegin; i < last; ++i) {
		auto &block = _blocks[i];
		auto height = block.node->getContentSize().height;

		if (i + 1 < _blocks.size() && _blocks[i + 1].visible) {
			auto delta = top(block.node) - top(_blocks[i + 1].node);
			if (delta > 0.0f) {
				height = delta;
			}
		}

		if (!block.measured) {
			++_measured;
		}
		block.measured = true;
		block.advance = height;
	}

	// Prefix sums over measured blocks; unmeasured ones contribute nothing, so the range grows.
	float offset = 0.0f;
	for (auto &it : _blocks) {
		it.top = offset;
		offset += it.advance;
	}
	_knownHeight = offset;

	_chunkBegin = _chunkEnd = maxOf<uint32_t>();
	_chunkAge = 0;
}

bool MarkdownVirtualizer::ensureMeasuredTo(uint32_t index) {
	if (!_enabled || index >= _blocks.size()) {
		return false;
	}
	if (_blocks[index].measured) {
		return true;
	}

	// Measure the rest of the document in one pass for a caller that needs a position now.
	for (auto &it : _blocks) {
		if (!it.measured) {
			materialize(it);
		}
	}
	_chunkBegin = 0;
	_chunkEnd = uint32_t(_blocks.size());
	_chunkAge = 0;
	return false;
}

bool MarkdownVirtualizer::update(float viewportHeight, float scrollY) {
	if (!_enabled || _blocks.empty()) {
		return false;
	}

	commitChunk();

	const auto margin = viewportHeight * kOverscan;
	const auto windowTop = sprt::max(0.0f, scrollY - margin);
	const auto windowBottom = scrollY + viewportHeight + margin;

	// What the reader is looking at, by the measured prefix sums.
	float before = 0.0f;
	float after = 0.0f;

	for (auto &block : _blocks) {
		if (!block.measured) {
			continue;
		}

		const auto top = block.top;
		const auto bottom = top + block.advance;

		if (bottom <= windowTop) {
			collapse(block);
			before += block.advance;
		} else if (top >= windowBottom) {
			collapse(block);
			after += block.advance;
		} else {
			materialize(block);
		}
	}

	// The next chunk goes right after what is measured, so its spacing is real.
	auto pending = false;
	if (_chunkBegin != maxOf<uint32_t>()) {
		// The previous frame's chunk is still settling.
		pending = true;
	} else if (_measured < _blocks.size()) {
		uint32_t first = maxOf<uint32_t>();
		uint32_t count = 0;
		for (uint32_t i = 0; i < _blocks.size() && count < kMeasureChunk; ++i) {
			if (_blocks[i].measured) {
				continue;
			}
			if (first == maxOf<uint32_t>()) {
				first = i;
			}
			materialize(_blocks[i]);
			++count;
		}

		if (count > 0) {
			_chunkBegin = first;
			_chunkEnd = first + count;
			_chunkAge = 0;

			// The chunk sits below everything measured, so nothing below it is hidden.
			after = 0.0f;
			pending = true;
		}
	}

	setSpacer(_spacerTop, before);
	setSpacer(_spacerBottom, after);

	return pending;
}

} // namespace stappler::xenolith::ui
