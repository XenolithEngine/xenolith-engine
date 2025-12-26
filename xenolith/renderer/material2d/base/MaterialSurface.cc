/**
 Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>
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

#include "MaterialSurface.h"
#include "MaterialEasing.h"
#include "MaterialSurfaceInterior.h"
#include "MaterialStyleContainer.h"
#include "XLFrameContext.h"
#include "XL2dFrameContext.h"
#include "XL2dCommandList.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::material2d {

bool Surface::init(const SurfaceStyle &style) {
	if (!VectorSprite::init(Size2(8.0f, 8.0f))) {
		return false;
	}

	_interior = addSystem(Rc<SurfaceInterior>::create());

	_styleOrigin = _styleTarget = style;
	_styleDirty = true;

	setQuality(QualityHigh);

	return true;
}

void Surface::setStyle(const SurfaceStyle &style) {
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

void Surface::setStyle(const SurfaceStyle &style, float duration) {
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

void Surface::setColorRole(ColorRole value) {
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

void Surface::setElevation(Elevation value) {
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

void Surface::setShapeFamily(ShapeFamily value) {
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

void Surface::setShapeStyle(ShapeStyle value) {
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

void Surface::setNodeStyle(NodeStyle value) {
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

void Surface::setActivityState(ActivityState value) {
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

void Surface::setStyleDirtyCallback(Function<void(const SurfaceStyleData &)> &&cb) {
	_styleDirtyCallback = sp::move(cb);
	_styleDirty = true;
}

bool Surface::visitDraw(FrameInfo &frame, NodeVisitFlags parentFlags) {
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

	if (_styleDirty || _contentSizeDirty || (_image && _contentSize != _image->getImageSize())) {
		if (_styleProgress > 0.0f) {
			_styleDataCurrent = progress(_styleDataOrigin, _styleDataTarget, _styleProgress);
		} else {
			_styleDataCurrent = _styleDataOrigin;
		}
		applyStyle(style, _styleDataCurrent);
		_interior->setStyle(SurfaceStyleData(_styleDataCurrent));
	}

	return VectorSprite::visitDraw(frame, parentFlags);
}

Pair<float, float> Surface::getHeightLimits(bool flex) const {
	return pair(_minHeight, _maxHeight);
}

void Surface::setHeightLimits(float min, float max) {
	_minHeight = min;
	_maxHeight = max;
}

void Surface::applyStyle(StyleContainer *, const SurfaceStyleData &style) {
	if (style.colorElevation.a == 0.0f && style.outlineValue == 0.0f) {
		setImage(nullptr);
		setColor(style.colorElevation, false);
		setDepthIndex(style.shadowValue);
		_styleDirty = false;
		return;
	}

	auto radius = std::min(std::min(_contentSize.width / 2.0f, _contentSize.height / 2.0f),
			style.cornerRadius);

	if (radius != _realCornerRadius || (_image && _contentSize != _image->getImageSize())
			|| _outlineValue != style.outlineValue || _fillValue != style.colorElevation.a
			|| style.shapeFamily != _realShapeFamily) {
		auto img = Rc<VectorImage>::create(_contentSize);

		updateBackgroundImage(img.get(), style, radius);

		_realShapeFamily = style.shapeFamily;
		_realCornerRadius = radius;
		_outlineValue = style.outlineValue;
		_fillValue = style.colorElevation.a;

		setImage(move(img));
	}

	if (_styleDirtyCallback) {
		_styleDirtyCallback(style);
	}

	setColor(style.colorElevation, false);
	setDepthIndex(style.shadowValue);
	_styleDirty = false;
}

void Surface::updateBackgroundImage(VectorImage *img, const SurfaceStyleData &style, float radius) {
	auto path = img->addPath();
	if (radius > 0.0f) {
		switch (style.shapeFamily) {
		case ShapeFamily::RoundedCorners:
			path->openForWriting([&](vg::PathWriter &writer) {
				writer.moveTo(0.0f, radius)
						.arcTo(radius, radius, 0.0f, false, true, radius, 0.0f)
						.lineTo(_contentSize.width - radius, 0.0f)
						.arcTo(radius, radius, 0.0f, false, true, _contentSize.width, radius)
						.lineTo(_contentSize.width, _contentSize.height - radius)
						.arcTo(radius, radius, 0.0f, false, true, _contentSize.width - radius,
								_contentSize.height)
						.lineTo(radius, _contentSize.height)
						.arcTo(radius, radius, 0.0f, false, true, 0.0f,
								_contentSize.height - radius)
						.closePath();
			});
			break;
		case ShapeFamily::CutCorners:
			path->openForWriting([&](vg::PathWriter &writer) {
				writer.moveTo(0.0f, radius)
						.lineTo(radius, 0.0f)
						.lineTo(_contentSize.width - radius, 0.0f)
						.lineTo(_contentSize.width, radius)
						.lineTo(_contentSize.width, _contentSize.height - radius)
						.lineTo(_contentSize.width - radius, _contentSize.height)
						.lineTo(radius, _contentSize.height)
						.lineTo(0.0f, _contentSize.height - radius)
						.closePath();
			});
			break;
		}
	} else {
		path->openForWriting([&](vg::PathWriter &writer) {
			writer.moveTo(0.0f, 0.0f)
					.lineTo(_contentSize.width, 0.0f)
					.lineTo(_contentSize.width, _contentSize.height)
					.lineTo(0.0f, _contentSize.height)
					.closePath();
		});
	}

	path->setAntialiased(false)
			.setFillColor(Color::White)
			.setFillOpacity(uint8_t(style.colorElevation.a * 255.0f))
			.setStyle(vg::DrawFlags::None);

	if (style.colorElevation.a > 0.0f) {
		path->setStyle(path->getStyle() | vg::DrawFlags::Fill);
	}

	if (style.outlineValue > 0.0f) {
		path->setStrokeWidth(1.0f)
				.setStyle(path->getStyle() | vg::DrawFlags::Stroke)
				.setStrokeColor(Color::White)
				.setStrokeOpacity(uint8_t(style.outlineValue * 255.0f))
				.setAntialiased(true);
	}
}

StyleContainer *Surface::getStyleContainerForFrame(FrameInfo &frame) const {
	return frame.getSystem<StyleContainer>(StyleContainer::SystemFrameTag);
}

SurfaceInterior *Surface::getSurfaceInteriorForFrame(FrameInfo &frame) const {
	return frame.getSystem<SurfaceInterior>(SurfaceInterior::SystemFrameTag);
}

RenderingLevel Surface::getRealRenderingLevel() const {
	auto l = VectorSprite::getRealRenderingLevel();
	if (l == RenderingLevel::Transparent) {
		l = RenderingLevel::Surface;
	}
	return l;
}

bool BackgroundSurface::init() { return init(material2d::SurfaceStyle::Background); }

bool BackgroundSurface::init(const SurfaceStyle &style) {
	if (!Surface::init(style)) {
		return false;
	}

	_styleContainer = addSystem(Rc<StyleContainer>::create());

	return true;
}

StyleContainer *BackgroundSurface::getStyleContainerForFrame(FrameInfo &frame) const {
	return _styleContainer;
}

} // namespace stappler::xenolith::material2d
