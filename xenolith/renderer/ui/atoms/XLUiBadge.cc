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

#include "XLUiBadge.h"
#include "XLUiStyleResolver.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

// Reuse the Panel style appliers under the "badge" type too: background-color / border-radius route
// to the inherited Panel::setStyleValue (Badge is-a Panel).
static void ensureBadgeStyleAppliers() {
	using document::ParameterName;
	static bool once = [] {
		StyleResolver::registerTypeApplier("badge",
				[](StyleResolver &, Node *node, const ResolvedStyle &s, document::ParameterName name,
						const document::StyleValue &val) {
					if (auto p = dynamic_cast<Panel *>(node)) {
						return p->setStyleValue(s, name, val);
					}
					return false;
				},
				StyleResolver::makeParameterMask({
						ParameterName::CssBackgroundColor,
						ParameterName::CssBorderTopLeftRadius,
						ParameterName::CssBorderTopRightRadius,
						ParameterName::CssBorderBottomRightRadius,
						ParameterName::CssBorderBottomLeftRadius,
						ParameterName::CmdReset,
				}));
		return true;
	}();
	(void)once;
}

Badge::~Badge() { }

bool Badge::init() {
	if (!Panel::init()) {
		return false;
	}
	setType("badge");
	removeStyleClass("xl-ui-panel");
	addStyleClass("xl-ui-badge");
	ensureBadgeStyleAppliers();

	_label = addChild(Rc<basic2d::Label>::create(), ZOrder(1));
	_label->setType("label");
	_label->addStyleClass("xl-ui-badge-label");
	_label->setAlignment(font::TextAlign::Center);
	_label->setColor(Color::White);
	return true;
}

void Badge::setText(StringView s) {
	_label->setString(s);
	_label->setVisible(!s.empty());
}

StringView Badge::getText() const {
	return _label->getString8();
}

void Badge::setVariant(StringView cls) {
	if (!_variant.empty()) {
		removeStyleClass(_variant);
	}
	_variant = cls.str<memory::StandartInterface>();
	if (!_variant.empty()) {
		addStyleClass(_variant);
	}
}

} // namespace stappler::xenolith::ui
