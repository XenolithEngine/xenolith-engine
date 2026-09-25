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

#include "render/LabelFillLayout.h"
#include "XLAppThread.h"
#include "XLDirector.h"
#include "XLFontController.h"
#include "XLCoreDynamicImage.h"
#include "XL2dVertexArray.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// A label that lets the bench write its quads again from the layout it already has.
class LabelFillLabel : public basic2d::Label {
public:
	TextLayout *getFormat() const { return _format; }
};

static constexpr StringView s_labelFillWords[] = {
	"damage", "region", "atlas", "glyph", "layout", "frame", "vertex", "bounds", "label", "quad",
	"повреждение", "область", "атлас", "глиф", "раскладка", "кадр", "вершина", "граница", "метка",
	"λόγος", "κόσμος", "γράμμα", "σελίδα", "χρόνος", "φως", "πόλη", "θάλασσα", "ήλιος", "νύχτα",
	"Xenolith", "Stappler", "0123", "4567", "89.,", "—", "«»", "()", "[]", "!?",
};

// Deterministic text of about `length` characters; `seed` shifts where the words start.
static String LabelFill_makeText(uint32_t seed, uint32_t length) {
	constexpr size_t wordCount = sizeof(s_labelFillWords) / sizeof(s_labelFillWords[0]);
	StringStream out;
	size_t chars = 0;
	size_t i = seed * 7;
	while (chars < length) {
		auto word = s_labelFillWords[i % wordCount];
		if (chars > 0) {
			out << ' ';
			++chars;
		}
		out << word;
		chars += sprt::unicode::getUtf16Length(word);
		i += 3;
	}
	return out.str();
}

bool LabelFillLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	fill(_count, _length);
	return true;
}

void LabelFillLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();

	if (_labels.empty()) {
		return;
	}

	constexpr uint32_t columns = 4;
	const auto work = getWorkSize();
	const float cellWidth = (work.width - 40.0f) / float(columns);
	const uint32_t rows = uint32_t((_labels.size() + columns - 1) / columns);
	const float cellHeight = (work.height - 20.0f) / float(rows);

	for (size_t i = 0; i < _labels.size(); ++i) {
		const auto column = uint32_t(i % columns);
		const auto row = uint32_t(i / columns);
		_labels[i]->setWidth(cellWidth - 12.0f);
		_labels[i]->setPosition(
				Vec2(20.0f + cellWidth * float(column), getWorkTop() - 10.0f - cellHeight * row));
	}
}

void LabelFillLayout::fill(uint32_t count, uint32_t length) {
	for (auto it : _labels) { it->removeFromParent(); }
	_labels.clear();

	_count = count;
	_length = length;
	for (uint32_t i = 0; i < count; ++i) {
		auto label = addChild(Rc<LabelFillLabel>::create(), ZOrder(1));
		label->setFontSize(14);
		label->setColor(Color::Grey_900);
		label->setAnchorPoint(Anchor::TopLeft);
		label->setString(LabelFill_makeText(i, length));
		_labels.emplace_back(label);
	}
	_contentSizeDirty = true;
}

Value LabelFillLayout::bench(uint32_t iterations) {
	Value ret;

	Rc<core::DynamicImageInstance> instance;
	if (auto controller = _director
					? _director->getApplication()->getExtension<font::FontController>()
					: nullptr) {
		if (auto &image = controller->getImage()) {
			instance = image->getInstance();
		}
	}
	const core::DataAtlas *atlas = instance ? instance->data.atlas.get() : nullptr;
	if (!atlas) {
		ret.setString("no font atlas", "error");
		return ret;
	}

	Vector<font::TextLayout *> formats;
	uint64_t glyphs = 0;
	uint64_t vertexes = 0;
	for (auto it : _labels) {
		if (auto format = it->getFormat()) {
			formats.emplace_back(format);
			glyphs += format->getData()->chars.size();
		}
	}

	const bool savedMode = basic2d::Label::isBoundsFromLayout();

	// One label written once, in the current mode: the fill, then the box as damage tracking reads
	// it. A fresh VertexData misses the bounds cache, as a label does after it changed or after
	// the atlas was rebuilt.
	struct Written {
		Rc<basic2d::VertexData> data;
		basic2d::VertexData::Bounds bounds;
	};
	auto writeOne = [&](font::TextLayout *format, uint64_t &fillNs, uint64_t &boundsNs) {
		Vector<ColorMask> colorMap;
		basic2d::VertexArray array;
		const auto chars = uint32_t(format->getData()->chars.size());
		const auto t0 = sp::platform::nanoclock(ClockType::Monotonic);
		array.init(chars * 4, chars * 6);
		basic2d::Label::writeQuads(array, format->getData(), colorMap, 0.0f);
		auto data = array.pop();
		const auto t1 = sp::platform::nanoclock(ClockType::Monotonic);
		auto bounds = data->getBounds(atlas);
		const auto t2 = sp::platform::nanoclock(ClockType::Monotonic);
		fillNs += t1 - t0;
		boundsNs += t2 - t1;
		return Written{sp::move(data), bounds};
	};

	// The boxes each mode gives, and whether the layout box holds the glyphs.
	double atlasArea = 0.0;
	double layoutArea = 0.0;
	uint32_t unresolved = 0;
	uint32_t notContained = 0;
	for (auto format : formats) {
		uint64_t unused = 0;
		basic2d::Label::setBoundsFromLayout(false);
		auto exact = writeOne(format, unused, unused);
		basic2d::Label::setBoundsFromLayout(true);
		auto layout = writeOne(format, unused, unused);

		vertexes += exact.data->data.size();
		if (exact.bounds.unresolved != 0) {
			++unresolved;
		}
		atlasArea += double(exact.bounds.box.size.width) * double(exact.bounds.box.size.height);
		layoutArea += double(layout.bounds.box.size.width) * double(layout.bounds.box.size.height);

		const auto &a = exact.bounds.box;
		const auto &l = layout.bounds.box;
		constexpr float eps = 0.5f;
		if (a.getMinX() < l.getMinX() - eps || a.getMinY() < l.getMinY() - eps
				|| a.getMaxX() > l.getMaxX() + eps || a.getMaxY() > l.getMaxY() + eps) {
			++notContained;
		}
	}

	struct Spent {
		uint64_t fillNs = 0;
		uint64_t boundsNs = 0;
	};
	auto runMode = [&](bool layoutMode, Spent &spent) {
		basic2d::Label::setBoundsFromLayout(layoutMode);
		for (uint32_t i = 0; i < iterations; ++i) {
			for (auto format : formats) { writeOne(format, spent.fillNs, spent.boundsNs); }
		}
	};

	// Interleaved, so that a slower stretch of the machine does not land on one mode only.
	Spent atlasSpent, layoutSpent;
	for (uint32_t round = 0; round < 4; ++round) {
		runMode(false, atlasSpent);
		runMode(true, layoutSpent);
	}

	basic2d::Label::setBoundsFromLayout(savedMode);

	ret.setInteger(int64_t(formats.size()), "labels");
	ret.setInteger(int64_t(glyphs), "chars");
	ret.setInteger(int64_t(vertexes), "vertexes");
	ret.setInteger(int64_t(iterations) * 4, "iterations");
	ret.setInteger(int64_t(unresolved), "unresolved");
	ret.setInteger(int64_t(notContained), "notContained");
	ret.setDouble(atlasArea, "atlasArea");
	ret.setDouble(layoutArea, "layoutArea");
	ret.setBool(savedMode, "boundsFromLayout");

	auto &atlasMode = ret.emplace("atlas");
	atlasMode.setInteger(int64_t(atlasSpent.fillNs), "fillNs");
	atlasMode.setInteger(int64_t(atlasSpent.boundsNs), "boundsNs");
	auto &layoutMode = ret.emplace("layout");
	layoutMode.setInteger(int64_t(layoutSpent.fillNs), "fillNs");
	layoutMode.setInteger(int64_t(layoutSpent.boundsNs), "boundsNs");
	return ret;
}

void LabelFillLayout::registerCommands() {
	addCommand("fill", "Rebuild the grid: { count, length } - labels, characters each",
			[this](Value &&args) {
		const Value &req = args;
		fill(uint32_t(req.getInteger("count", DefaultCount)),
				uint32_t(req.getInteger("length", DefaultLength)));
		Value ret;
		ret.setInteger(int64_t(_count), "count");
		ret.setInteger(int64_t(_length), "length");
		return ret;
	});

	addCommand("bench",
			"Write every label's quads and read its box, in both bounds modes: { iterations }",
			[this](Value &&args) {
		const Value &req = args;
		return bench(uint32_t(sprt::max(req.getInteger("iterations", 20), int64_t(1))));
	});

	addCommand("mode", "Where labels written from now on take their box from: { layout }",
			[](Value &&args) {
		const Value &req = args;
		if (req.hasValue("layout")) {
			basic2d::Label::setBoundsFromLayout(req.getBool("layout"));
		}
		Value ret;
		ret.setBool(basic2d::Label::isBoundsFromLayout(), "layout");
		return ret;
	});
}

} // namespace stappler::xenolith::app
