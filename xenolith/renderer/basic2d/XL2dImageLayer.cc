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

#include "XLInputListener.h"
#include "XL2dActionAcceleratedMove.h"
#include "XL2dImageLayer.h"
#include "XLTexture.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

Rect ImageLayer::getCorrectRect(Size2 containerSize) {
	Size2 parentSize = getContentSize();
	Rect ret = Rect(parentSize.width - containerSize.width,
			parentSize.height - containerSize.height, containerSize.width - parentSize.width,
			containerSize.height - parentSize.height);

	if (containerSize.width <= parentSize.width) {
		ret.origin.x = (parentSize.width - containerSize.width) / 2;
		ret.size.width = 0;
	}

	if (containerSize.height <= parentSize.height) {
		ret.origin.y = (parentSize.height - containerSize.height) / 2;
		ret.size.height = 0;
	}

	if (isnan(containerSize.width) || isnan(containerSize.height)) {
		log::format(log::LogType::Error, "ImageLayer", SP_LOCATION,
				"rect %f %f %f %f : %f %f %f %f", parentSize.width, parentSize.height,
				containerSize.width, containerSize.height, ret.origin.x, ret.origin.y,
				ret.size.width, ret.size.height);
	}

	return ret;
}

Vec2 ImageLayer::getCorrectPosition(Size2 containerSize, Vec2 point) {
	Vec2 ret = point;
	Rect bounds = getCorrectRect(containerSize);

	if (ret.x < bounds.origin.x) {
		ret.x = bounds.origin.x;
	} else if (ret.x > bounds.getMaxX()) {
		ret.x = bounds.getMaxX();
	}

	if (ret.y < bounds.origin.y) {
		ret.y = bounds.origin.y;
	} else if (ret.y > bounds.getMaxY()) {
		ret.y = bounds.getMaxY();
	}

	if (isnan(ret.x) || isnan(ret.y)) {
		log::format(log::LogType::Error, "ImageLayer", SP_LOCATION,
				"pos %f %f %f %f : %f %f : %f %f", bounds.origin.x, bounds.origin.y,
				bounds.size.width, bounds.size.height, point.x, point.y, ret.x, ret.y);
	}
	return ret;
}

Size2 ImageLayer::getContainerSize() const {
	return Size2(_root->getContentSize().width * _root->getScale().x,
			_root->getContentSize().height * _root->getScale().y);
}

Size2 ImageLayer::getContainerSizeForScale(float value) const {
	return Size2(_root->getContentSize().width * value, _root->getContentSize().height * value);
}

ImageLayer::~ImageLayer() { }

bool ImageLayer::init() {
	if (!Node::init()) {
		return false;
	}

	setOpacity(1.0f);
	_gestureListener = addSystem(Rc<InputListener>::create());
	_gestureListener->setTouchFilter(
			[](const InputEvent &ev, const InputListener::DefaultEventFilter &f) { return f(ev); });
	_gestureListener->addTapRecognizer([this](const GestureTap &tap) {
		if (_actionCallback) {
			_actionCallback();
		}
		return handleTap(tap.input->currentLocation, tap.count);
	});
	_gestureListener->addSwipeRecognizer([this](const GestureSwipe &s) {
		if (s.event == GestureEvent::Began) {
			if (_actionCallback) {
				_actionCallback();
			}
			return handleSwipeBegin(s.input->currentLocation);
		} else if (s.event == GestureEvent::Activated) {
			return handleSwipe(Vec2(s.delta.x / _globalScale.x, s.delta.y / _globalScale.y));
		} else if (s.event == GestureEvent::Ended) {
			return handleSwipeEnd(
					Vec2(s.velocity.x / _globalScale.x, s.velocity.y / _globalScale.y));
		}
		return true;
	});
	_gestureListener->addPinchRecognizer([this](const GesturePinch &p) {
		if (p.event == GestureEvent::Began) {
			if (_actionCallback) {
				_actionCallback();
			}
			_hasPinch = true;
		} else if (p.event == GestureEvent::Activated) {
			return handlePinch(p.center, p.scale, p.velocity, false);
		} else if (p.event == GestureEvent::Ended || p.event == GestureEvent::Cancelled) {
			_hasPinch = false;
			return handlePinch(p.center, p.scale, p.velocity, true);
		}
		return true;
	});

	_root = addChild(Rc<Node>::create());
	_root->setCascadeOpacityEnabled(true);
	_root->setScale(1.0f);

	_image = _root->addChild(Rc<Sprite>::create(core::EmptyTextureName));

	_scaleSource = 0;

	return true;
}

void ImageLayer::handleContentSizeDirty() {
	Node::handleContentSizeDirty();

	auto imageSize = _image->getTexture()->getExtent();
	if (_imageSizePredefined) {
		imageSize = Extent3(_imageSize.width, _imageSize.height, 1);
	}
	_root->setContentSize(Size2(imageSize.width, imageSize.height));

	if (!_scaleDisabled) {
		_minScale = std::min(_contentSize.width / _image->getContentSize().width,
				_contentSize.height / _image->getContentSize().height);

		_maxScale = std::max(_image->getContentSize().width * GetMaxScaleFactor()
						/ _contentSize.width,
				_image->getContentSize().height * GetMaxScaleFactor() / _contentSize.height);
	} else {
		_minScale = _maxScale = 1.0f;
	}

	if (_textureDirty) {
		_textureDirty = false;
		_root->setScale(_minScale);
	}

	Vec2 prevCenter = Vec2(_prevContentSize.width / 2.0f, _prevContentSize.height / 2.0f);
	Vec2 center = Vec2(_contentSize.width / 2.0f, _contentSize.height / 2.0f);
	Vec2 offset = center - prevCenter;

	_root->setPosition(getCorrectPosition(getContainerSize(), _root->getPosition().xy() + offset));

	auto currentScale = _root->getScale().x;
	if (_maxScale != 0 && _minScale != 0
			&& (currentScale > _maxScale || currentScale < _minScale)) {
		float newScale = currentScale;
		if (_minScale > _maxScale) {
			newScale = _minScale;
		} else if (currentScale < _minScale) {
			newScale = _minScale;
		} else if (currentScale > _maxScale) {
			newScale = _maxScale;
		}
		Vec2 pos = _root->getPosition().xy();
		Vec2 locInParent = Vec2(_contentSize.width / 2.0f, _contentSize.height / 2.0f);

		Vec2 normal = (pos - locInParent) / currentScale * newScale;

		_root->setScale(newScale);
		_root->setPosition(getCorrectPosition(getContainerSize(), locInParent + normal));
	}

	_prevContentSize = _contentSize;
	_root->setPosition(getCorrectPosition(getContainerSize(), _root->getPosition().xy()));
}

void ImageLayer::handleTransformDirty(const Mat4 &parentTransform) {
	Node::handleTransformDirty(parentTransform);
	Vec3 scale;
	getNodeToWorldTransform().getScale(&scale);
	_globalScale = Vec2(scale.x, scale.y);
}

void ImageLayer::setTexture(Rc<Texture> &&tex) {
	_imageSizePredefined = false;

	auto extent = tex->getExtent();
	_image->setTexture(move(tex));
	_image->setContentSize(Size2(extent.width, extent.height));

	if (_contentSize.width == 0.0f || _contentSize.height == 0.0f) {
		_minScale = 1.0f;
		_maxScale = 1.0f;
		_root->setScale(1.0f);
		_contentSizeDirty = true;
		_textureDirty = true;
		return;
	}

	if (_running) {
		if (!_scaleDisabled) {
			_minScale = std::min(_contentSize.width / _image->getContentSize().width,
					_contentSize.height / _image->getContentSize().height);

			_maxScale = std::max(_image->getContentSize().width * GetMaxScaleFactor()
							/ _contentSize.width,
					_image->getContentSize().height * GetMaxScaleFactor() / _contentSize.height);

			_root->setScale(_minScale);
		} else {
			_minScale = _maxScale = 1.0f;
			Size2 imageSize = _image->getBoundingBox().size;
			_root->setContentSize(imageSize);
			_root->setScale(1.0f);
			_root->setPosition(getCorrectPosition(getContainerSize(),
					Vec2((_contentSize.width - imageSize.width) / 2.0f,
							_contentSize.height - imageSize.height)));
		}

		_contentSizeDirty = true;
	} else {
		_textureDirty = true;
	}
}

const Rc<Texture> &ImageLayer::getTexture() const { return _image->getTexture(); }

void ImageLayer::setActionCallback(Function<void()> &&cb) { _actionCallback = sp::move(cb); }

Vec2 ImageLayer::getTexturePosition() const {
	auto csize = getContainerSize();
	auto bsize = getContentSize();
	auto vec = _root->getPosition();
	bsize = Size2(csize.width - bsize.width, csize.height - bsize.height);
	Vec2 result;
	if (bsize.width <= 0) {
		bsize.width = 0;
		result.x = nan();
	} else {
		result.x = fabs(-vec.x / bsize.width);
	}
	if (bsize.height <= 0) {
		bsize.height = 0;
		result.y = nan();
	} else {
		result.y = fabs(-vec.y / bsize.height);
	}

	return result;
}

void ImageLayer::setScaleDisabled(bool value) {
	if (_scaleDisabled != value) {
		_scaleDisabled = value;
		_contentSizeDirty = true;
	}
}

void ImageLayer::setImageSize(Size2 size) {
	_imageSize = size;
	_imageSizePredefined = true;
	_contentSizeDirty = true;
	_textureDirty = true;
}

bool ImageLayer::handleTap(Vec2 point, int count) {
	if (count == 2 && !_scaleDisabled) {
		if (_root->getScale().x > _minScale) {
			Vec2 location = convertToNodeSpace(point);

			float newScale = _minScale;
			float origScale = _root->getScale().x;
			Vec2 pos = _root->getPosition().xy();
			Vec2 locInParent = convertToNodeSpace(location);

			Vec2 normal = (pos - locInParent) / origScale * newScale;
			Vec2 newPos =
					getCorrectPosition(getContainerSizeForScale(newScale), locInParent + normal);

			_root->runAction(Rc<Spawn>::create(Rc<ScaleTo>::create(0.35, newScale),
					Rc<MoveTo>::create(0.35, newPos)));
		} else {
			Vec2 location = convertToNodeSpace(point);

			float newScale = _minScale * 2.0f * _inputDensity;
			float origScale = _root->getScale().x;

			if (_minScale > _maxScale) {
				newScale = _minScale;
			} else if (newScale < _minScale) {
				newScale = _minScale;
			} else if (newScale > _maxScale) {
				newScale = _maxScale;
			}

			Vec2 pos = _root->getPosition().xy();
			Vec2 locInParent = convertToNodeSpace(location);

			Vec2 normal = (pos - locInParent) * (newScale / origScale) * _inputDensity;
			Vec2 newPos =
					getCorrectPosition(getContainerSizeForScale(newScale), locInParent + normal);

			_root->runAction(Rc<Spawn>::create(Rc<ScaleTo>::create(0.35, newScale),
					Rc<MoveTo>::create(0.35, newPos)));
		}
	}
	return true;
}

bool ImageLayer::handleSwipeBegin(Vec2 point) { return true; }

bool ImageLayer::handleSwipe(Vec2 delta) {
	Vec2 containerPosition = _root->getPosition().xy();
	_root->stopAllActions();
	_root->setPosition(getCorrectPosition(getContainerSize(), containerPosition + delta));
	return true;
}

bool ImageLayer::handleSwipeEnd(Vec2 velocity) {
	_root->stopAllActions();
	auto a = ActionAcceleratedMove::createWithBounds(5'000, _root->getPosition().xy(), velocity,
			getCorrectRect(_root->getBoundingBox().size));
	if (a) {
		_root->runAction(Rc<Sequence>::create(move(a), [this] { _contentSizeDirty = true; }));
	}
	return true;
}

bool ImageLayer::handlePinch(Vec2 location, float scale, float velocity, bool isEnded) {
	if (isEnded) {
		_contentSizeDirty = true;
		_scaleSource = 0;
		return true;
	} else if (_scaleSource == 0) {
		_scaleSource = _root->getScale().x;
	}

	if (_maxScale < _minScale) {
		return true;
	}

	float newScale = _scaleSource * scale;
	if (newScale < _minScale) {
		newScale = _minScale;
	}
	if (newScale > _maxScale && _maxScale > _minScale) {
		newScale = _maxScale;
	}

	float origScale = _root->getScale().x;
	Vec2 pos = _root->getPosition().xy();
	Vec2 locInParent = convertToNodeSpace(location);

	Vec2 normal = (pos - locInParent) / origScale * newScale;

	_root->setScale(newScale);
	_root->setPosition(getCorrectPosition(getContainerSize(), locInParent + normal));

	return true;
}

} // namespace stappler::xenolith::basic2d
