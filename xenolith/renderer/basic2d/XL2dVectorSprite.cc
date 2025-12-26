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

#include "XL2dVectorSprite.h"
#include "XLAppThread.h"
#include "XLTexture.h"
#include "XLDirector.h"
#include "XLDynamicStateSystem.h"
#include "XLCoreFrameRequest.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

static Rc<VectorCanvasDeferredResult> VectorSprite_runDeferredVectorCavas(event::Looper *queue,
		Rc<VectorImageData> &&image, const VectorCanvasConfig &config, bool waitOnReady) {
	auto result = new std::promise<Rc<VectorCanvasResult>>;
	Rc<VectorCanvasDeferredResult> ret =
			Rc<VectorCanvasDeferredResult>::create(result->get_future(), waitOnReady);
	queue->performAsync([queue, image = move(image), config, ret, result]() mutable {
		auto canvas = VectorCanvas::getInstance();
		auto res = canvas->draw(config, move(image));
		result->set_value(res);

		queue->performOnThread([ret = move(ret), res = move(res), result]() mutable {
			ret->handleReady(move(res));
			delete result;
		}, nullptr);
	}, ret);
	return ret;
}

static float VectorSprite_getPseudoSdfOffset(float value) {
	const float prefixSdf = 15.0f;
	const float prefixDepth = 3.0f;

	if (value <= 0.0) {
		return 0.0f;
	} else if (value < prefixDepth) {
		return prefixSdf;
	} else {
		return prefixSdf
				+ (std::floor(value) - prefixDepth) / (40.0f - prefixDepth)
				* (config::VGPseudoSdfOffset - prefixSdf);
	}
}

VectorSprite::VectorSprite() { }

bool VectorSprite::init(Rc<VectorImage> &&img) {
	XL_ASSERT(img, "Image should not be nullptr");

	if (!Sprite::init() || !img) {
		return false;
	}

	_image = img;
	if (_image) {
		_contentSize = _image->getImageSize();
	}

	_imageScissorComponent = addSystem(Rc<DynamicStateSystem>::create());

	return true;
}

bool VectorSprite::init(Size2 size, StringView data) {
	if (auto image = Rc<VectorImage>::create(size, data)) {
		return init(move(image));
	}
	return false;
}

bool VectorSprite::init(Size2 size, VectorPath &&path) {
	if (auto image = Rc<VectorImage>::create(size, move(path))) {
		return init(move(image));
	}
	return false;
}

bool VectorSprite::init(Size2 size) {
	if (auto image = Rc<VectorImage>::create(size)) {
		return init(move(image));
	}
	return false;
}

bool VectorSprite::init(StringView data) {
	if (auto image = Rc<VectorImage>::create(data)) {
		return init(move(image));
	}
	return false;
}

bool VectorSprite::init(BytesView data) {
	if (auto image = Rc<VectorImage>::create(data)) {
		return init(move(image));
	}
	return false;
}

bool VectorSprite::init(const FileInfo &path) {
	if (auto image = Rc<VectorImage>::create(path)) {
		return init(move(image));
	}
	return false;
}

Rc<VectorPathRef> VectorSprite::addPath(StringView id, StringView cache, Mat4 pos) {
	return _image ? _image->addPath(id, cache, pos) : nullptr;
}

Rc<VectorPathRef> VectorSprite::addPath(const VectorPath &path, StringView id, StringView cache,
		Mat4 pos) {
	return _image ? _image->addPath(path, id, cache, pos) : nullptr;
}

Rc<VectorPathRef> VectorSprite::addPath(VectorPath &&path, StringView id, StringView cache,
		Mat4 pos) {
	return _image ? _image->addPath(move(path), id, cache, pos) : nullptr;
}

Rc<VectorPathRef> VectorSprite::getPath(StringView id) {
	return _image ? _image->getPath(id) : nullptr;
}

void VectorSprite::removePath(const Rc<VectorPathRef> &path) {
	if (_image) {
		_image->removePath(path);
	}
}

void VectorSprite::removePath(StringView id) {
	if (_image) {
		_image->removePath(id);
	}
}

void VectorSprite::clear() {
	if (_image) {
		_image->clear();
	}
}

void VectorSprite::setImage(Rc<VectorImage> &&img) {
	if (_image != img) {
		_image = move(img);
		if (_image) {
			_image->setDirty();
		}
	}
}

const Rc<VectorImage> &VectorSprite::getImage() const { return _image; }

void VectorSprite::setQuality(float val) {
	if (_quality != val) {
		_quality = val;
		if (_image) {
			_image->setDirty();
		}
	}
}

void VectorSprite::handleTransformDirty(const Mat4 &parent) {
	_vertexesDirty = true;
	Sprite::handleTransformDirty(parent);
}

bool VectorSprite::visitDraw(FrameInfo &frame, NodeVisitFlags parentFlags) {
	if (_image && _image->isDirty()) {
		_vertexesDirty = true;
	}
	return Sprite::visitDraw(frame, parentFlags);
}

uint32_t VectorSprite::getTrianglesCount() const {
	uint32_t ret = 0;
	if (_deferredResult) {
		if (_deferredResult->isReady()) {
			for (auto &it : _deferredResult->getResult()->data) {
				ret += it.data->indexes.size() / 3;
			}
		}
	} else if (_result) {
		for (auto &it : _result->data) { ret += it.data->indexes.size() / 3; }
	}
	return ret;
}

uint32_t VectorSprite::getVertexesCount() const {
	uint32_t ret = 0;
	if (_deferredResult) {
		if (_deferredResult->isReady()) {
			for (auto &it : _deferredResult->getResult()->data) { ret += it.data->data.size(); }
		}
	} else if (_result) {
		for (auto &it : _result->data) { ret += it.data->data.size(); }
	}
	return ret;
}

void VectorSprite::setDeferred(bool val) {
	if (val != _deferred) {
		_deferred = val;
		_vertexesDirty = true;
	}
}

void VectorSprite::setRespectEmptyDrawOrder(bool val) {
	if (val != _respectEmptyDrawOrder) {
		_respectEmptyDrawOrder = val;
		_vertexesDirty = true;
	}
}

void VectorSprite::setImageAutofit(Autofit autofit) {
	if (_imagePlacement.autofit != autofit) {
		_imagePlacement.autofit = autofit;
		_vertexesDirty = true;
	}
}

void VectorSprite::setImageAutofitPosition(const Vec2 &vec) {
	if (_imagePlacement.autofitPos != vec) {
		_imagePlacement.autofitPos = vec;
		if (_imagePlacement.autofit != Autofit::None) {
			_vertexesDirty = true;
		}
	}
}

Vec2 VectorSprite::convertToImageFromWorld(const Vec2 &worldLocation) const {
	auto imageSize = _image->getImageSize();

	Mat4 t = Mat4::IDENTITY;
	t.scale(_imageTargetSize.width / imageSize.width, _imageTargetSize.height / imageSize.height,
			1.0f);

	Mat4 tmp = (_modelViewTransform * _imageTargetTransform * t * _image->getViewBoxTransform())
					   .getInversed();
	return tmp.transformPoint(worldLocation);
}

Vec2 VectorSprite::convertToImageFromNode(const Vec2 &worldLocation) const {
	auto imageSize = _image->getImageSize();

	Mat4 t = Mat4::IDENTITY;
	t.scale(_imageTargetSize.width / imageSize.width, _imageTargetSize.height / imageSize.height,
			1.0f);

	Mat4 tmp = (_imageTargetTransform * t * _image->getViewBoxTransform()).getInversed();
	return tmp.transformPoint(worldLocation);
}

Vec2 VectorSprite::convertFromImageToNode(const Vec2 &imageLocation) const {
	auto imageSize = _image->getImageSize();

	Mat4 t = Mat4::IDENTITY;
	t.scale(_imageTargetSize.width / imageSize.width, _imageTargetSize.height / imageSize.height,
			1.0f);

	Mat4 tmp = _imageTargetTransform * t * _image->getViewBoxTransform();
	return tmp.transformPoint(imageLocation);
}

Vec2 VectorSprite::convertFromImageToWorld(const Vec2 &imageLocation) const {
	auto imageSize = _image->getImageSize();

	Mat4 t = Mat4::IDENTITY;
	t.scale(_imageTargetSize.width / imageSize.width, _imageTargetSize.height / imageSize.height,
			1.0f);

	Mat4 tmp = _modelViewTransform * _imageTargetTransform * t * _image->getViewBoxTransform();
	return tmp.transformPoint(imageLocation);
}

void VectorSprite::pushCommands(FrameInfo &frame, NodeVisitFlags flags) {
	if (!_image) {
		return;
	}

	if (!_deferredResult && (!_result || _result->data.empty())) {
		return;
	}

	FrameContextHandle2d *handle = static_cast<FrameContextHandle2d *>(frame.currentContext);

	if (_result) {
		handle->commands->pushVertexArray([&](memory::pool_t *pool) {
			auto &targetData = _result->mut;
			auto reqMemSize = sizeof(InstanceVertexData) * targetData.size();

			// pool memory is 16-bytes aligned, no problems with Mat4
			auto tmpData = new (memory::pool::palloc(frame.pool->getPool(), reqMemSize))
					InstanceVertexData[targetData.size()];
			auto target = tmpData;
			if (_normalized) {
				auto transform = frame.modelTransformStack.back() * _imageTargetTransform;
				for (auto &it : targetData) {
					target->instances = it.instances.pdup(pool);
					for (auto &inst : target->instances) {
						auto modelTransform = transform * inst.transform;

						Mat4 newMV;
						newMV.m[12] = floorf(modelTransform.m[12]);
						newMV.m[13] = floorf(modelTransform.m[13]);
						newMV.m[14] = floorf(modelTransform.m[14]);

						const_cast<TransformData &>(inst).transform =
								frame.viewProjectionStack.back() * newMV;
					}
					target->data = it.data;
					++target;
				}
			} else {
				auto transform = frame.viewProjectionStack.back() * frame.modelTransformStack.back()
						* _imageTargetTransform;
				for (auto &it : targetData) {
					target->instances = it.instances.pdup(pool);
					for (auto &inst : target->instances) {
						const_cast<TransformData &>(inst).transform = transform * inst.transform;
					}
					target->data = it.data;
					++target;
				}
			}

			return makeSpanView(tmpData, targetData.size());
		}, buildCmdInfo(frame), _commandFlags);
	} else if (_deferredResult) {
		if (_deferredResult->isReady() && _deferredResult->getResult()->data.empty()) {
			return;
		}

		handle->commands->pushDeferredVertexResult(_deferredResult,
				frame.viewProjectionStack.back(),
				frame.modelTransformStack.back() * _imageTargetTransform, _normalized,
				buildCmdInfo(frame), _commandFlags);
	}
}

void VectorSprite::initVertexes() {
	// prevent to do anything
}

void VectorSprite::updateVertexes(FrameInfo &frame) {
	if (!_image || !_director) {
		return;
	}

	Vec3 viewScale;
	_modelViewTransform.decompose(&viewScale, nullptr, nullptr);

	Size2 imageSize = _image->getImageSize();
	Size2 targetViewSpaceSize =
			Size2(_contentSize.width * viewScale.x, _contentSize.height * viewScale.y);
	Size2 contentSpaceSize = _contentSize;

	float targetOffsetX = -_imagePlacement.textureRect.origin.x * imageSize.width;
	float targetOffsetY = -_imagePlacement.textureRect.origin.y * imageSize.height;

	auto placementResult = _imagePlacement.resolve(_contentSize, imageSize);

	if (_imagePlacement.autofit != Autofit::None) {
		auto imageSizeInView = Size2(imageSize.width / placementResult.scale,
				imageSize.height / placementResult.scale);
		targetOffsetX = targetOffsetX
				+ (_contentSize.width - imageSizeInView.width) * _imagePlacement.autofitPos.x;
		targetOffsetY = targetOffsetY
				+ (_contentSize.height - imageSizeInView.height) * _imagePlacement.autofitPos.y;

		targetViewSpaceSize =
				Size2(imageSizeInView.width * viewScale.x, imageSizeInView.height * viewScale.y);

		if (imageSizeInView.width > _contentSize.width
				|| imageSizeInView.height > _contentSize.height) {
			_imageScissorComponent->setStateApplyMode(DynamicStateApplyMode::ApplyForSelf);
			_imageScissorComponent->enableScissor(0.0f);
		} else {
			_imageScissorComponent->setStateApplyMode(DynamicStateApplyMode::DoNotApply);
			_imageScissorComponent->disableScissor();
		}

		contentSpaceSize = placementResult.viewRect.size;
	} else {
		_imageScissorComponent->setStateApplyMode(DynamicStateApplyMode::DoNotApply);
		_imageScissorComponent->disableScissor();
	}

	Mat4 targetTransform(1.0f, 0.0f, 0.0f, targetOffsetX, 0.0f, 1.0f, 0.0f, targetOffsetY, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);

	bool isDirty = false;

	if (_imageTargetSize != targetViewSpaceSize) {
		isDirty = true;
		_imageTargetSize = targetViewSpaceSize;
	}

	auto targetDepth = VectorSprite_getPseudoSdfOffset(_depthIndex);
	if (_savedSdfValue != targetDepth) {
		isDirty = true;
		_savedSdfValue = targetDepth;
	}

	_imageTargetTransform = targetTransform;

	if (isDirty || _image->isDirty()) {
		_image->clearDirty();
		_result = nullptr;
		_deferredResult = nullptr;
		_vertexColorDirty = false; // color will be already applied

		auto imageData = _image->popData();

		if (imageData->getDrawOrder().empty() && _respectEmptyDrawOrder) {
			return;
		}

		VectorCanvasConfig config;
		config.color = _displayedColor;
		config.quality = _quality;
		config.targetSize = _imageTargetSize;
		config.textureFlippedX = _flippedX;
		config.textureFlippedY = _flippedY;

		auto texPlacementResult = _texturePlacement.resolve(contentSpaceSize,
				Size2(_texture->getExtent().width, _texture->getExtent().height));
		_textureScale = texPlacementResult.scale;

		// Canvas uses pixel-wide extents, but for sdf we need dp-wide
		config.sdfBoundaryInset *= frame.request->getFrameConstraints().density;
		config.sdfBoundaryOffset = _savedSdfValue * frame.request->getFrameConstraints().density;

		if (_depthIndex > 0.0f) {
			config.forcePseudoSdf = true;
		}

		if (_deferred) {
			_deferredResult =
					VectorSprite_runDeferredVectorCavas(_director->getApplication()->getLooper(),
							move(imageData), config, _waitDeferred);
			_result = nullptr;
		} else {
			auto canvas = VectorCanvas::getInstance();
			_result = canvas->draw(config, move(imageData));
		}
	}

	Mat4 scaleTransform;
	scaleTransform.scale(viewScale);
	scaleTransform.inverse();

	_imageTargetTransform *= scaleTransform;

	auto isSolidImage = [&, this] {
		for (auto &it : _image->getPaths()) {
			if (it.second->isAntialiased()) {
				return false;
			}

			auto s = it.second->getStyle();
			switch (s) {
			case vg::DrawFlags::Fill:
				if (it.second->getFillOpacity() != 255) {
					return false;
				}
				break;
			case vg::DrawFlags::FillAndStroke:
				if (it.second->getFillOpacity() != 255) {
					return false;
				}
				if (it.second->getStrokeOpacity() != 255) {
					return false;
				}
				break;
			case vg::DrawFlags::Stroke:
				if (it.second->getStrokeOpacity() != 255) {
					return false;
				}
				break;
			default: break;
			}
		}
		return true;
	};

	auto isSolid = isSolidImage();
	if (isSolid != _imageIsSolid) {
		_materialDirty = true;
		_imageIsSolid = isSolid;
	}
}

void VectorSprite::updateVertexesColor() {
	if (_deferred) {
		if (_deferredResult) {
			_deferredResult->updateColor(_displayedColor);
		}
	} else {
		if (_result) {
			_result->updateColor(_displayedColor);
		}
	}
}

RenderingLevel VectorSprite::getRealRenderingLevel() const {
	auto level = _renderingLevel;
	if (level == RenderingLevel::Default) {
		if (_displayedColor.a < 1.0f || !_texture || _materialInfo.getLineWidth() != 0.0f) {
			level = RenderingLevel::Transparent;
		} else if (_colorMode.getMode() == core::ColorMode::Solid) {
			if (_texture->hasAlpha()) {
				level = RenderingLevel::Transparent;
			} else {
				level = _imageIsSolid ? RenderingLevel::Solid : RenderingLevel::Transparent;
			}
		} else {
			auto alphaMapping = _colorMode.getA();
			switch (alphaMapping) {
			case core::ComponentMapping::Identity:
				if (_texture->hasAlpha()) {
					level = RenderingLevel::Transparent;
				} else {
					level = _imageIsSolid ? RenderingLevel::Solid : RenderingLevel::Transparent;
				}
				break;
			case core::ComponentMapping::Zero: level = RenderingLevel::Transparent; break;
			case core::ComponentMapping::One:
				level = _imageIsSolid ? RenderingLevel::Solid : RenderingLevel::Transparent;
				break;
			default: level = RenderingLevel::Transparent; break;
			}
		}
	}
	return level;
}

bool VectorSprite::checkVertexDirty() const {
	auto targetDepth = VectorSprite_getPseudoSdfOffset(_depthIndex);
	return Sprite::checkVertexDirty() || _savedSdfValue != targetDepth;
}

} // namespace stappler::xenolith::basic2d
