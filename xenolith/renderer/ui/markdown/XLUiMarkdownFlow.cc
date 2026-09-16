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

#include "XLUiMarkdownFlow.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

uint32_t MarkdownFlow::emplace(Node *node, MarkdownFlowKind kind, document::SourceSpan span,
		uint32_t textLength, const document::Node *source) {
	auto index = uint32_t(_entries.size());
	_entries.emplace_back(MarkdownFlowEntry{node, _next, textLength, span, kind, source});

	// One position past the entry's end: the block boundary, so a range can include the break.
	_next += textLength + 1;

	auto textBegin = _entries.back().textBegin;
	// Created when missing: markers and rules have no runs but still need to find their entry.
	node->setOrUpdateComponent<MarkdownRunMap>([&](NotNull<MarkdownRunMap> map) {
		map->flowIndex = index;
		map->textBegin = textBegin;
		return true;
	});
	return index;
}

uint32_t MarkdownFlow::getLength() const {
	if (_entries.empty()) {
		return 0;
	}
	auto &back = _entries.back();
	return back.textBegin + back.textLength;
}

auto MarkdownFlow::findByPosition(uint32_t position) const -> const MarkdownFlowEntry * {
	if (_entries.empty()) {
		return nullptr;
	}

	// The last entry that starts at or before the position; entries are contiguous, so that is
	// also the one containing it.
	size_t low = 0;
	size_t high = _entries.size();
	while (low + 1 < high) {
		auto mid = (low + high) / 2;
		if (_entries[mid].textBegin <= position) {
			low = mid;
		} else {
			high = mid;
		}
	}

	auto &entry = _entries[low];
	if (position < entry.textBegin) {
		return nullptr;
	}
	return &entry;
}

auto MarkdownFlow::findByNode(const Node *node) const -> const MarkdownFlowEntry * {
	if (!node) {
		return nullptr;
	}
	if (auto map = node->getComponent<MarkdownRunMap>()) {
		if (map->flowIndex < _entries.size() && _entries[map->flowIndex].node == node) {
			return &_entries[map->flowIndex];
		}
	}
	for (auto &it : _entries) {
		if (it.node == node) {
			return &it;
		}
	}
	return nullptr;
}

uint32_t MarkdownFlow::mapEdge(const MarkdownFlowEntry &entry, uint32_t position,
		bool leftEdge) const {
	auto charIndex = (position > entry.textBegin) ? position - entry.textBegin : 0;
	if (charIndex > entry.textLength) {
		charIndex = entry.textLength;
	}

	auto map = entry.node ? entry.node->getComponent<MarkdownRunMap>() : nullptr;
	if (!map || map->runs.empty()) {
		// A marker, rule or checkbox is not a slice of the source; answer with its block.
		return leftEdge ? entry.span.offset : entry.span.end();
	}

	auto &runs = map->runs;
	const MarkdownRunMap::Run *previous = nullptr;
	for (auto &run : runs) {
		if (charIndex >= run.charStart + run.charCount) {
			previous = &run;
			continue;
		}

		if (charIndex < run.charStart) {
			// Between two runs: a builder-inserted character. The edge takes the boundary on the
			// side it came from.
			if (leftEdge || !previous) {
				return run.srcOffset;
			}
			return previous->srcOffset + previous->srcLength;
		}

		if (!run.verbatim) {
			// Not verbatim, so there is no inner offset; the run is taken whole.
			return leftEdge ? run.srcOffset : run.srcOffset + run.srcLength;
		}

		// Walk characters to bytes: multibyte UTF-8 and surrogate pairs rule out arithmetic.
		auto fragment = _source.sub(run.srcOffset, run.srcLength);
		auto wanted = charIndex - run.charStart;
		uint32_t bytes = 0;
		uint32_t chars = 0;
		while (bytes < fragment.size() && chars < wanted) {
			auto len = uint32_t(sprt::unicode::utf8_length_data[uint8_t(fragment[bytes])]);
			if (len == 0) {
				break;
			}
			chars += uint32_t(sprt::unicode::getUtf16Length(fragment.sub(bytes, len)));
			bytes += len;
		}
		return run.srcOffset + bytes;
	}

	// past the last run
	auto &last = runs.back();
	return last.srcOffset + last.srcLength;
}

Pair<uint32_t, uint32_t> MarkdownFlow::getSourceRange(uint32_t begin, uint32_t end) const {
	if (_entries.empty() || begin >= end) {
		return pair(uint32_t(0), uint32_t(0));
	}

	auto first = findByPosition(begin);
	auto last = findByPosition(end);
	if (!first) {
		first = &_entries.front();
	}
	if (!last) {
		last = &_entries.back();
	}

	auto srcBegin = mapEdge(*first, begin, true);
	auto srcEnd = mapEdge(*last, end, false);
	if (srcEnd < srcBegin) {
		srcEnd = srcBegin;
	}
	return pair(srcBegin, srcEnd);
}

// Two entries of the same item or cell are lines of one thing; a marker and the text it marks are
// one line; anything else is a paragraph break.
static StringView MarkdownFlow_separator(const MarkdownFlowEntry &from,
		const MarkdownFlowEntry &to) {
	if (from.kind == MarkdownFlowKind::Marker) {
		return StringView(" ");
	}

	auto owner = [](const Node *node) -> const Node * {
		while (node) {
			auto type = node->getType();
			if (type == "li" || type == "td" || type == "th" || type == "pre") {
				return node;
			}
			node = node->getParent();
		}
		return nullptr;
	};

	auto a = owner(from.node);
	if (a && a == owner(to.node)) {
		return StringView("\n");
	}
	return StringView("\n\n");
}

void MarkdownFlow::writeText(const Callback<void(StringView)> &out, uint32_t begin,
		uint32_t end) const {
	if (begin >= end) {
		return;
	}

	const MarkdownFlowEntry *previous = nullptr;
	for (auto &it : _entries) {
		auto entryEnd = it.textBegin + it.textLength;
		if (entryEnd <= begin || it.textBegin >= end) {
			continue;
		}

		if (previous) {
			out(MarkdownFlow_separator(*previous, it));
		}
		previous = &it;

		if (it.textLength == 0) {
			continue;
		}

		auto label = dynamic_cast<basic2d::Label *>(it.node.get());
		if (!label) {
			continue;
		}

		auto string = label->getString();
		auto from = (begin > it.textBegin) ? begin - it.textBegin : 0;
		auto to = sprt::min(end, entryEnd) - it.textBegin;
		if (to > string.size()) {
			to = uint32_t(string.size());
		}
		if (from >= to) {
			continue;
		}

		/* Object characters are replaced by the image's alt text; text between objects is copied
		whole. The markup copy needs none of this: the object's run covers its `![alt](src)`. */
		auto map = label->getComponent<MarkdownRunMap>();
		auto pos = from;
		for (auto i = from; i < to; ++i) {
			if (string[i] != MarkdownObjectChar) {
				continue;
			}

			if (i > pos) {
				out(string::toUtf8<Interface>(string.sub(pos, i - pos)));
			}
			if (map) {
				out(map->findObjectText(i));
			}
			pos = i + 1;
		}

		if (pos < to) {
			out(string::toUtf8<Interface>(string.sub(pos, to - pos)));
		}
	}
}

// --- geometry ----------------------------------------------------------------------------------

basic2d::Label *MarkdownFlow::labelOf(const MarkdownFlowEntry &entry) {
	return dynamic_cast<basic2d::Label *>(entry.node.get());
}

uint32_t MarkdownFlow::getPositionForPoint(Vec2 world) const {
	const MarkdownFlowEntry *nearest = nullptr;
	auto nearestDistance = maxOf<float>();
	auto nearestAbove = false;

	for (auto &it : _entries) {
		auto label = labelOf(it);
		if (!label || it.textLength == 0) {
			continue;
		}

		// Hit-test the frame as drawn: a scroll may have moved the tree since.
		if (label->isTouchedAsDrawn(world)) {
			auto local = label->convertToNodeSpace(world);
			auto index = label->getCharIndex(local, font::CharSelectMode::Best);
			if (index.first != maxOf<uint32_t>()) {
				// a Suffix hit means the caret belongs AFTER the glyph
				auto charIndex = index.first + (index.second ? 1 : 0);
				return it.textBegin + sprt::min(charIndex, it.textLength);
			}

			// Inside the box but past the end of a line.
			return it.textBegin + (local.x <= 0.0f ? 0 : it.textLength);
		}

		auto box = label->getWorldBoundingBox();
		auto above = false;
		auto distance = 0.0f;
		if (world.y > box.getMaxY()) {
			distance = world.y - box.getMaxY();
			above = true; // the engine is Y-up, so a larger y is higher up the document
		} else if (world.y < box.getMinY()) {
			distance = box.getMinY() - world.y;
		} else {
			// level with the block but beside it: the nearer horizontal edge decides
			above = world.x < box.getMidX();
		}

		if (distance < nearestDistance) {
			nearestDistance = distance;
			nearest = &it;
			nearestAbove = above;
		}
	}

	if (!nearest) {
		return 0;
	}
	return nearestAbove ? nearest->textBegin : nearest->textBegin + nearest->textLength;
}

Pair<Vec2, float> MarkdownFlow::getPointForPosition(uint32_t position) const {
	auto entry = findByPosition(position);
	if (!entry) {
		return pair(Vec2::INVALID, 0.0f);
	}

	auto label = labelOf(*entry);
	if (!label) {
		return pair(Vec2::INVALID, 0.0f);
	}

	auto charIndex = sprt::min(position - entry->textBegin, entry->textLength);

	// The baseline of the position's line in the label's space, as `getCursorPosition` answers.
	auto local = label->empty() ? label->getCursorOrigin() : label->getCursorPosition(charIndex);
	auto height = float(label->getFontHeight());

	// Converted as two points so the height comes back in world units under a scaled parent.
	auto base = label->convertToWorldSpace(local);
	auto top = label->convertToWorldSpace(local + Vec2(0.0f, height));
	return pair(base, top.y - base.y);
}

Pair<uint32_t, uint32_t> MarkdownFlow::getWordRange(uint32_t position) const {
	auto entry = findByPosition(position);
	if (!entry || entry->textLength == 0) {
		return pair(position, position);
	}

	auto label = labelOf(*entry);
	if (!label) {
		return pair(position, position);
	}

	auto charIndex = sprt::min(position - entry->textBegin, entry->textLength);
	if (charIndex >= entry->textLength && charIndex > 0) {
		--charIndex; // the caret past the last character still belongs to the last word
	}

	auto word = label->selectWord(charIndex);
	if (word == core::TextCursor::InvalidCursor || word.length == 0) {
		return pair(position, position);
	}
	return pair(entry->textBegin + word.start, entry->textBegin + word.start + word.length);
}

Pair<uint32_t, uint32_t> MarkdownFlow::getBlockRange(uint32_t position) const {
	auto entry = findByPosition(position);
	if (!entry) {
		return pair(position, position);
	}
	return pair(entry->textBegin, entry->textBegin + entry->textLength);
}

/* Slices the selection into the labels, each painting its own highlight (`Label::Selection`);
the rest get the invalid cursor. The equality guard matters: setSelectionCursor rebuilds its quads
unconditionally, and this runs on every pointer move of a drag. */
void MarkdownFlow::applySelection(uint32_t begin, uint32_t end) const {
	for (auto &it : _entries) {
		auto label = labelOf(it);
		if (!label) {
			continue;
		}

		auto cursor = core::TextCursor::InvalidCursor;
		if (end > begin && it.textLength > 0) {
			auto from = sprt::max(begin, it.textBegin);
			auto to = sprt::min(end, it.textBegin + it.textLength);
			if (to > from) {
				cursor = core::TextCursor(from - it.textBegin, to - from);
			}
		}

		if (label->getSelectionCursor() != cursor) {
			label->setSelectionCursor(cursor);
		}
	}
}

void MarkdownFlow::setSelectionColor(const Color4F &color) const {
	for (auto &it : _entries) {
		if (auto label = labelOf(it)) {
			label->setSelectionColor(color);
		}
	}
}

auto MarkdownFlow::findLink(uint32_t position) const -> const MarkdownRunMap::Link * {
	auto entry = findByPosition(position);
	if (!entry || !entry->node) {
		return nullptr;
	}

	auto map = entry->node->getComponent<MarkdownRunMap>();
	if (!map) {
		return nullptr;
	}
	return map->findLink(position - entry->textBegin);
}

} // namespace stappler::xenolith::ui
