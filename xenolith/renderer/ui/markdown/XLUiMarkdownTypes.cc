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

#include "XLUiMarkdownTypes.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

ComponentId MarkdownRunMap::Id;

StringView getMarkdownInlineName(MarkdownInline value) {
	switch (value) {
	case MarkdownInline::Text: return StringView("text");
	case MarkdownInline::Strong: return StringView("strong");
	case MarkdownInline::Emphasis: return StringView("em");
	case MarkdownInline::Code: return StringView("code");
	case MarkdownInline::Link: return StringView("a");
	case MarkdownInline::Strikethrough: return StringView("del");
	case MarkdownInline::Insert: return StringView("ins");
	case MarkdownInline::Mark: return StringView("mark");
	case MarkdownInline::Subscript: return StringView("sub");
	case MarkdownInline::Superscript: return StringView("sup");
	case MarkdownInline::Max: break;
	}
	return StringView();
}

MarkdownInline getMarkdownInlineForTag(StringView tag) {
	if (tag == "strong" || tag == "b") {
		return MarkdownInline::Strong;
	} else if (tag == "em" || tag == "i") {
		return MarkdownInline::Emphasis;
	} else if (tag == "code") {
		return MarkdownInline::Code;
	} else if (tag == "a") {
		return MarkdownInline::Link;
	} else if (tag == "del" || tag == "s") {
		return MarkdownInline::Strikethrough;
	} else if (tag == "ins") {
		return MarkdownInline::Insert;
	} else if (tag == "mark") {
		return MarkdownInline::Mark;
	} else if (tag == "sub") {
		return MarkdownInline::Subscript;
	} else if (tag == "sup") {
		return MarkdownInline::Superscript;
	}
	return MarkdownInline::Max;
}

basic2d::Label::Style MarkdownInlineStyles::get(MarkdownInline value) const {
	using Style = basic2d::Label::Style;

	switch (value) {
	case MarkdownInline::Text: return Style();
	case MarkdownInline::Strong: return Style(font::FontWeight::Bold);
	case MarkdownInline::Emphasis: return Style(font::FontStyle::Italic);
	case MarkdownInline::Code:
		// The family is a delta: code in a heading keeps the heading's size. An unresolved family
		// is left alone.
		if (monospaceFamily == maxOf<uint32_t>()) {
			return Style(codeColor);
		}
		return Style{basic2d::Label::FontFamily(monospaceFamily), codeColor};
	case MarkdownInline::Link:
		if (underlineLinks) {
			return Style{linkColor, font::TextDecoration::Underline};
		}
		return Style(linkColor);
	case MarkdownInline::Strikethrough: return Style(font::TextDecoration::LineThrough);
	case MarkdownInline::Insert: return Style(insertColor);
	case MarkdownInline::Mark: return Style(markColor);
	case MarkdownInline::Subscript: return Style(font::VerticalAlign::Sub);
	case MarkdownInline::Superscript: return Style(font::VerticalAlign::Super);
	case MarkdownInline::Max: break;
	}
	return Style();
}

auto MarkdownRunMap::findRun(uint32_t charIndex) const -> const Run * {
	// Runs are ascending and disjoint but not gapless: builder-inserted characters have no run.
	for (auto &it : runs) {
		if (charIndex < it.charStart) {
			return nullptr;
		}
		if (charIndex < it.charStart + it.charCount) {
			return &it;
		}
	}
	return nullptr;
}

StringView MarkdownRunMap::findObjectText(uint32_t charIndex) const {
	for (auto &it : objects) {
		if (it.first == charIndex) {
			return it.second;
		}
	}
	return StringView();
}

auto MarkdownRunMap::findLink(uint32_t charIndex) const -> const Link * {
	for (auto &it : links) {
		if (charIndex >= it.charStart && charIndex < it.charStart + it.charCount) {
			return &it;
		}
	}
	return nullptr;
}

bool MarkdownTextSystem::init() {
	if (!System::init()) {
		return false;
	}

	_systemFlags = SystemFlags::HandleMeasure;
	setSystemPriority(MarkdownTextPriority);
	return true;
}

bool MarkdownTextSystem::handleMeasure(const MeasureConstraints &c, Size2 &result) {
	auto label = dynamic_cast<basic2d::Label *>(_owner);
	if (!label) {
		return false;
	}

	// Zero or unbounded width means no width is known yet; the label answers itself.
	if (c.maxWidth == maxOf<float>() || c.maxWidth <= 0.0f) {
		return false;
	}

	// Ask for the height at this width instead of the unwrapped max-content size.
	// `Label::measureContent` shapes into a separate layout, leaving the label's own undisturbed.
	MeasureConstraints mc = c;
	mc.mode = MeasureMode::Normal;
	result = label->measureContent(mc);

	_wrapWidth = c.maxWidth;
	_wrapHeight = result.height;
	return true;
}

} // namespace stappler::xenolith::ui
