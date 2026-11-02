/**
 Copyright (c) 2026 Stappler Team <admin@stappler.org>

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


#include "XLCommon.h"

#include "render/FontPersistentLayout.h"
#include "XLDirector.h"
#include "XLFontController.h"
#include "XLCoreDynamicImage.h"
#include "XL2dVertexArray.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

class FontPersistentLabel : public basic2d::Label {
public:
	TextLayout *getFormat() const { return _format; }
};

// Latin, Latin-1, Latin Extended-A, Greek and Cyrillic letters and the digits
static Vector<char32_t> FontPersistent_makeChars() {
	Vector<char32_t> ret;
	auto add = [&](char32_t from, char32_t to) {
		for (auto c = from; c <= to; ++c) { ret.emplace_back(c); }
	};
	add(U'A', U'Z');
	add(U'a', U'z');
	add(U'0', U'9');
	add(0x00C0, 0x00FF);
	add(0x0100, 0x017F);
	add(0x0391, 0x03A1);
	add(0x03A3, 0x03C9);
	add(0x0410, 0x044F);
	return ret;
}

static String FontPersistent_makeText(uint32_t index) {
	static const Vector<char32_t> s_chars = FontPersistent_makeChars();
	String text;
	for (uint32_t i = 0; i < FontPersistentLayout::CharsPerLabel; ++i) {
		auto c = s_chars[(index * FontPersistentLayout::CharsPerLabel + i) % s_chars.size()];
		sprt::unicode::utf8EncodeCb([&](char ch) { text.push_back(ch); }, c);
	}
	return text;
}

bool FontPersistentLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	for (uint32_t i = 0; i < MaxLabels; ++i) {
		auto label = addChild(Rc<FontPersistentLabel>::create(), ZOrder(1));
		label->setPersistentGlyphData(true);
		label->setFontSize(uint16_t(16 + (i % 6) * 6));
		label->setColor(Color::Grey_900);
		label->setAnchorPoint(Anchor::TopLeft);
		label->setString(FontPersistent_makeText(i));
		label->setVisible(false);
		_labels.emplace_back(label);
	}
	show(1);
	return true;
}

void FontPersistentLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();

	constexpr uint32_t columns = 4;
	const auto work = getWorkSize();
	const float cellWidth = (work.width - 40.0f) / float(columns);
	const float cellHeight = (work.height - 20.0f) / float(MaxLabels / columns);
	for (size_t i = 0; i < _labels.size(); ++i) {
		_labels[i]->setWidth(cellWidth - 12.0f);
		_labels[i]->setPosition(Vec2(20.0f + cellWidth * float(i % columns),
				getWorkTop() - 10.0f - cellHeight * float(i / columns)));
	}
}

void FontPersistentLayout::show(uint32_t count) {
	_shown = sprt::min(count, MaxLabels);
	for (uint32_t i = 0; i < _labels.size(); ++i) { _labels[i]->setVisible(i < _shown); }
}

Value FontPersistentLayout::state() const {
	Value ret;
	const core::DataAtlas *atlas = nullptr;
	if (auto controller = _director
					? _director->getApplication()->getExtension<font::FontController>()
					: nullptr) {
		if (auto &image = controller->getImage()) {
			if (auto instance = image->getInstance()) {
				atlas = instance->data.atlas.get();
			}
		}
	}

	uint64_t chars = 0;
	uint64_t missing = 0;
	uint32_t laidOut = 0;
	for (uint32_t i = 0; i < _shown; ++i) {
		auto format = _labels[i]->getFormat();
		if (!format) {
			continue;
		}
		++laidOut;
		chars += format->getData()->chars.size();
		if (!atlas) {
			missing += format->getData()->chars.size();
			continue;
		}
		Vector<ColorMask> colorMap;
		basic2d::VertexArray array;
		array.init(uint32_t(format->getData()->chars.size() * 4),
				uint32_t(format->getData()->chars.size() * 6));
		basic2d::Label::writeQuads(array, format->getData(), colorMap, 0.0f);
		missing += array.pop()->getBounds(atlas).unresolved;
	}
	ret.setInteger(int64_t(_shown), "shown");
	ret.setInteger(int64_t(laidOut), "laidOut");
	ret.setInteger(int64_t(chars), "chars");
	ret.setInteger(int64_t(missing), "missing");
	return ret;
}

void FontPersistentLayout::registerCommands() {
	addCommand("show", "Show the first { count } labels", [this](Value &&args) {
		const Value &req = args;
		show(uint32_t(req.getInteger("count", 1)));
		return state();
	});

	addCommand("state", "The shown labels, their glyphs and how many the atlas lacks",
			[this](Value &&) { return state(); });
}

} // namespace stappler::xenolith::app
