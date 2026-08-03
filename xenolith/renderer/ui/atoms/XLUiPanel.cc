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

#include "XLUiPanel.h"
#include "XLUiStyleResolver.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

// Register the per-attribute style appliers for nodes of type "panel" once. Mirrors the Button
// pattern: a recursive StyleResolver on the panel routes background-color / border-radius here
// instead of the generic Layer defaults.
static void ensurePanelStyleAppliers() {
	using document::ParameterName;
	static bool once = [] {
		StyleResolver::registerTypeApplier("panel",
				[](StyleResolver &, Node *node, const ResolvedStyle &s,
						document::ParameterName name, const document::StyleValue &val) {
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

Panel::~Panel() { }

bool Panel::init() {
	if (!LayerRounded::init(Color::White, 0.0f)) {
		return false;
	}
	setType("panel");
	addStyleClass("xl-ui-panel");
	setRenderingLevel(RenderingLevel::Surface);
	ensurePanelStyleAppliers();
	return true;
}

bool Panel::setStyleValue(const ResolvedStyle &, document::ParameterName name,
		const document::StyleValue &value) {
	using document::ParameterName;
	// CmdReset arrives before the parameters of every style pass, so that a declaration which
	// stopped matching is undone rather than left applied. Panel keeps its paint in LayerRounded
	// rather than in a component, so instead of dropping a component it restores the values
	// init() set - the pass then re-applies whatever it still declares.
	if (name == ParameterName::CmdReset) {
		setPathColor(Color::White, true);
		setBorderRadius(0.0f);
		return true;
	}
	const float px = value.sizeValue.value;
	switch (name) {
	case ParameterName::CssBackgroundColor: setPathColor(value.color4, true); return true;
	case ParameterName::CssBorderTopLeftRadius:
	case ParameterName::CssBorderTopRightRadius:
	case ParameterName::CssBorderBottomRightRadius:
	case ParameterName::CssBorderBottomLeftRadius:
		// LayerRounded keeps a single radius; the four CSS corners collapse to one (cards/panels
		// in the installer design use the same radius on every corner).
		setBorderRadius(px);
		return true;
	default: return false;
	}
}

} // namespace stappler::xenolith::ui
