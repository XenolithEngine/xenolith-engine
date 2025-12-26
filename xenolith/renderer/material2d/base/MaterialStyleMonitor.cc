/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#include "MaterialStyleMonitor.h"
#include "MaterialSurfaceInterior.h"
#include "MaterialStyleContainer.h"
#include "XLFrameContext.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::material2d {

StyleMonitor::~StyleMonitor() { }

bool StyleMonitor::init(StyleCallback &&cb) {
	if (!System::init()) {
		return false;
	}

	_styleCallback = sp::move(cb);

	return true;
}

void StyleMonitor::setStyleCallback(StyleCallback &&cb) { _styleCallback = sp::move(cb); }

const StyleMonitor::StyleCallback &StyleMonitor::getStyleCallback() const { return _styleCallback; }

void StyleMonitor::setDirty(bool value) { _dirty = value; }

void StyleMonitor::handleVisitSelf(FrameInfo &frame, Node *node, NodeVisitFlags parentFlags) {
	auto container = frame.getSystem<StyleContainer>(StyleContainer::SystemFrameTag);
	auto style = frame.getSystem<SurfaceInterior>(SurfaceInterior::SystemFrameTag);
	if (style && (_dirty || style->getStyle() != _interiorData)) {
		_interiorData = style->getStyle();

		auto scheme = container ? container->getScheme(_interiorData.schemeTag) : nullptr;
		_styleCallback(scheme, _interiorData);
		_dirty = false;
	}

	System::handleVisitSelf(frame, node, parentFlags);
}

} // namespace stappler::xenolith::material2d
