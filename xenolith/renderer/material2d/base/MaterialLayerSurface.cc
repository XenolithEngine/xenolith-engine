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

#include "MaterialEasing.h"
#include "MaterialLayerSurface.h"
#include "MaterialSurfaceInterior.h"
#include "MaterialStyleContainer.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::material2d {

bool LayerSurface::init(const SurfaceStyle &style) {
	if (!Layer::init(Color::White)) {
		return false;
	}

	_interior = addSystem(Rc<SurfaceInterior>::create());

	_styleOrigin = _styleTarget = style;
	_styleDirty = true;

	return true;
}

void LayerSurface::setStyle(const SurfaceStyle &style) {
	if (_inTransition) {
		_styleDirty = true;
		stopAllActionsByTag(TransitionActionTag);
		_inTransition = false;
		_styleProgress = 0.0f;
	}

	if (_styleOrigin != style) {
		_styleOrigin = _styleTarget = move(style);
		_styleDirty = true;
	}
}

void LayerSurface::setStyle(const SurfaceStyle &style, float duration) {
	if (duration <= 0.0f || !_running) {
		setStyle(style);
		return;
	}

	if (_inTransition || getActionByTag(TransitionActionTag)) {
		_styleDirty = true;
		stopAllActionsByTag(TransitionActionTag);
		_inTransition = false;
		_styleProgress = 0.0f;
	}

	if (_styleOrigin != style) {
		_styleTarget = move(style);
		runAction(makeEasing(Rc<ActionProgress>::create(duration,
						  [this](float progress) {
			_styleProgress = progress;
			_styleDirty = true;
		}, [this] { _inTransition = true; },
						  [this] {
			_styleOrigin = _styleTarget;
			_styleDirty = true;
			_inTransition = false;
			_styleProgress = 0.0f;
		})),
				TransitionActionTag);
		_styleDirty = true;
	}
}

void LayerSurface::setColorRole(ColorRole value) {
	if (_styleTarget.colorRole != value) {
		if (_styleOrigin == _styleTarget) {
			_styleTarget.colorRole = _styleOrigin.colorRole = value;
			_styleDirty = true;
		} else {
			_styleTarget.colorRole = value;
			_styleDirty = true;
		}
	}
}

void LayerSurface::setElevation(Elevation value) {
	if (_styleTarget.elevation != value) {
		if (_styleOrigin == _styleTarget) {
			_styleTarget.elevation = _styleOrigin.elevation = value;
			_styleDirty = true;
		} else {
			_styleTarget.elevation = value;
			_styleDirty = true;
		}
	}
}

void LayerSurface::setShapeFamily(ShapeFamily value) {
	if (_styleTarget.shapeFamily != value) {
		if (_styleOrigin == _styleTarget) {
			_styleTarget.shapeFamily = _styleOrigin.shapeFamily = value;
			_styleDirty = true;
		} else {
			_styleTarget.shapeFamily = value;
			_styleDirty = true;
		}
	}
}

void LayerSurface::setShapeStyle(ShapeStyle value) {
	if (_styleTarget.shapeStyle != value) {
		if (_styleOrigin == _styleTarget) {
			_styleTarget.shapeStyle = _styleOrigin.shapeStyle = value;
			_styleDirty = true;
		} else {
			_styleTarget.shapeStyle = value;
			_styleDirty = true;
		}
	}
}

void LayerSurface::setNodeStyle(NodeStyle value) {
	if (_styleTarget.nodeStyle != value) {
		if (_styleOrigin == _styleTarget) {
			_styleTarget.nodeStyle = _styleOrigin.nodeStyle = value;
			_styleDirty = true;
		} else {
			_styleTarget.nodeStyle = value;
			_styleDirty = true;
		}
	}
}

void LayerSurface::setActivityState(ActivityState value) {
	if (_styleTarget.activityState != value) {
		if (_styleOrigin == _styleTarget) {
			_styleTarget.activityState = _styleOrigin.activityState = value;
			_styleDirty = true;
		} else {
			_styleTarget.activityState = value;
			_styleDirty = true;
		}
	}
}

void LayerSurface::setStyleDirtyCallback(Function<void(const SurfaceStyleData &)> &&cb) {
	_styleDirtyCallback = sp::move(cb);
	_styleDirty = true;
}

bool LayerSurface::visitDraw(FrameInfo &frame, NodeVisitFlags parentFlags) {
	if (!_visible) {
		return false;
	}

	auto style = getStyleContainerForFrame(frame);
	if (!style) {
		return false;
	}

	if (style) {
		if (_styleTarget.apply(_styleDataTarget, _contentSize, style,
					getSurfaceInteriorForFrame(frame))) {
			_styleDirty = true;
		}
		if (_styleOrigin.apply(_styleDataOrigin, _contentSize, style,
					getSurfaceInteriorForFrame(frame))) {
			_styleDirty = true;
		}
	}

	if (_styleDirty || _contentSizeDirty) {
		if (_styleProgress > 0.0f) {
			_styleDataCurrent = progress(_styleDataOrigin, _styleDataTarget, _styleProgress);
		} else {
			_styleDataCurrent = _styleDataOrigin;
		}
		applyStyle(_styleDataCurrent);
		_interior->setStyle(SurfaceStyleData(_styleDataCurrent));
	}

	return Layer::visitDraw(frame, parentFlags);
}

void LayerSurface::applyStyle(const SurfaceStyleData &style) {
	if (_styleDirtyCallback) {
		_styleDirtyCallback(style);
	}

	setColor(style.colorElevation, false);
	setDepthIndex(style.shadowValue);
	_styleDirty = false;
}

StyleContainer *LayerSurface::getStyleContainerForFrame(FrameInfo &frame) const {
	return frame.getSystem<StyleContainer>(StyleContainer::SystemFrameTag);
}

SurfaceInterior *LayerSurface::getSurfaceInteriorForFrame(FrameInfo &frame) const {
	return frame.getSystem<SurfaceInterior>(SurfaceInterior::SystemFrameTag);
}

RenderingLevel LayerSurface::getRealRenderingLevel() const {
	auto l = Layer::getRealRenderingLevel();
	if (l == RenderingLevel::Transparent) {
		l = RenderingLevel::Surface;
	}
	return l;
}

} // namespace stappler::xenolith::material2d
