/**
 Copyright (c) 2023-2024 Stappler LLC <admin@stappler.dev>
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

#include "XL2dSprite.h"

#include "XLResourceCache.h"
#include "XLTemporaryResource.h"
#include "XLTexture.h"
#include "XLDirector.h"
#include "XLFrameContext.h"
#include "XL2dFrameContext.h"
#include "XL2dCommandList.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

Sprite::Sprite() {
	_blendInfo = core::BlendInfo(core::BlendFactor::SrcAlpha, core::BlendFactor::OneMinusSrcAlpha,
			core::BlendOp::Add, core::BlendFactor::Zero, core::BlendFactor::One,
			core::BlendOp::Add);
	_materialInfo.setBlendInfo(_blendInfo);
	_materialInfo.setDepthInfo(core::DepthInfo(false, true, core::CompareOp::Less));
}

Sprite::~Sprite() { }

bool Sprite::init() { return Sprite::init(core::SolidTextureName); }

bool Sprite::init(StringView textureName) {
	if (!Node::init()) {
		return false;
	}

	_textureName = textureName.str<Interface>();
	initVertexes();
	return true;
}

bool Sprite::init(Rc<Texture> &&texture) {
	if (!Node::init()) {
		return false;
	}

	if (texture) {
		_texture = move(texture);
		_isTextureLoaded = _texture->isLoaded();
	}

	initVertexes();
	return true;
}

void Sprite::setTexture(StringView textureName) {
	if (!_running) {
		if (_texture) {
			_texture = nullptr;
			_materialDirty = true;
		}
		_textureName = textureName.str<Interface>();
	} else {
		if (textureName.empty()) {
			if (_texture) {
				if (_running) {
					_texture->handleExit(_frameContext);
				}
				_texture = nullptr;
				_materialDirty = true;
			}
		} else if (!_texture || _texture->getName() != textureName) {
			if (auto cache = _director->getResourceCache()) {
				if (auto tex = cache->acquireTexture(textureName)) {
					setTexture(move(tex));
				}
			}
		}
	}
}

void Sprite::setTexture(Rc<Texture> &&tex) {
	if (_texture) {
		if (!tex) {
			if (_running) {
				_texture->handleExit(_frameContext);
			}
			_texture = nullptr;
			_textureName.clear();
			_materialDirty = true;
			_isTextureLoaded = false;
		} else if (_texture->getName() != tex->getName()) {
			if (_running) {
				_texture->handleExit(_frameContext);
			}
			_texture = move(tex);
			if (_running) {
				_texture->handleEnter(_frameContext);
			}
			_isTextureLoaded = _texture->isLoaded();
			if (_isTextureLoaded && _textureLoadedCallback) {
				_textureLoadedCallback();
			}
			_textureName = _texture->getName().str<Interface>();
			updateBlendAndDepth();
			_materialDirty = true;
		}
	} else {
		if (tex) {
			_texture = move(tex);
			if (_running) {
				_texture->handleEnter(_frameContext);
			}
			_isTextureLoaded = _texture->isLoaded();
			if (_isTextureLoaded && _textureLoadedCallback) {
				_textureLoadedCallback();
			}
			_textureName = _texture->getName().str<Interface>();
			updateBlendAndDepth();
			_materialDirty = true;
		}
	}
}

void Sprite::setTexture(Texture *tex) { setTexture(Rc<Texture>(tex)); }

void Sprite::scheduleTextureUpdate(StringView textureName) {
	if (!_running || textureName.empty() || textureName.empty()) {
		setTexture(move(textureName));
	} else if (!_texture || _texture->getName() != textureName) {
		if (auto cache = _director->getResourceCache()) {
			if (auto tex = cache->acquireTexture(textureName)) {
				if (tex->isLoaded()) {
					setTexture(move(tex));
				} else {
					doScheduleTextureUpdate(move(tex));
				}
			}
		}
	}
}

void Sprite::scheduleTextureUpdate(Rc<Texture> &&tex) {
	if (tex->isLoaded() || !_texture || !tex) {
		setTexture(move(tex));
	} else if (_texture->getName() != tex->getName()) {
		doScheduleTextureUpdate(move(tex));
	}
}

const Rc<Texture> &Sprite::getTexture() const { return _texture; }

void Sprite::setLinearGradient(Rc<LinearGradient> &&g) { _linearGradient = move(g); }

const Rc<LinearGradient> &Sprite::getLinearGradient() const { return _linearGradient; }

void Sprite::setTextureRect(const Rect &rect) {
	if (!_texturePlacement.textureRect.equals(rect)) {
		_texturePlacement.textureRect = rect;
		_vertexesDirty = true;
	}
}

bool Sprite::visitDraw(FrameInfo &frame, NodeVisitFlags parentFlags) {
	if (_texture) {
		auto loaded = _texture->isLoaded();
		if (loaded != _isTextureLoaded && loaded) {
			handleTextureLoaded();
			_isTextureLoaded = loaded;
		}
	}
	return Node::visitDraw(frame, parentFlags);
}

void Sprite::draw(FrameInfo &frame, NodeVisitFlags flags) {
	bool hasExtraState = false;

	if (!_texture || !_texture->isLoaded()) {
		return;
	}

	if (_texturePlacement.autofit != Autofit::None) {
		auto size = _texture->getExtent();
		if (_targetTextureSize != size) {
			_targetTextureSize = size;
			_vertexesDirty = true;
		}
	}

	if (checkVertexDirty()) {
		updateVertexes(frame);
		_vertexesDirty = false;
	}

	if (_vertexColorDirty) {
		updateVertexesColor();
		_vertexColorDirty = false;
	}

	if (_materialDirty) {
		updateBlendAndDepth();

		auto info = getMaterialInfo();
		_materialId = frame.currentContext->context->getMaterial(info);
		if (_materialId == 0) {
			_materialId = frame.currentContext->context->acquireMaterial(_pipelineFamily, info,
					getMaterialImages(), nullptr, isMaterialRevokable());
			if (_materialId == 0) {
				log::source().warn("Sprite", "Material for sprite with texture '",
						_texture->getName(), "' not found");
			}
		}
		_materialDirty = false;
	}

	for (auto &it : _pendingDependencies) {
		emplace_ordered(frame.currentContext->waitDependencies, move(it));
	}

	if (_linearGradient || _outlineOffset > 0.0f) {
		auto context = frame.contextStack.back();

		DrawStateValues state;
		// copy current state
		auto stateId = context->getCurrentState();
		if (stateId != StateIdNone) {
			state = *context->getState(stateId);
		}

		auto newData = Rc<StateData>::create(
				dynamic_cast<StateData *>(state.data ? state.data.get() : nullptr));
		auto transform = frame.modelTransformStack.back();
		transform.scale(_contentSize.width, _contentSize.height, 1.0f);

		newData->transform = transform;
		if (_linearGradient) {
			newData->gradient = _linearGradient->pop();
		}

		if (_outlineOffset > 0.0f) {
			newData->outlineOffset = _outlineOffset * _inputDensity * _textureScale;
			newData->outlineColor = _outlineColor;
		}

		state.data = newData;

		auto newStateId = context->addState(state);

		context->stateStack.emplace_back(newStateId, nullptr);
		hasExtraState = true;
	}

	pushCommands(frame, flags);

	if (hasExtraState) {
		frame.contextStack.back()->stateStack.pop_back();
	}

	_pendingDependencies.clear();
}

void Sprite::handleEnter(Scene *scene) {
	Node::handleEnter(scene);

	if (!_textureName.empty()) {
		if (!_texture || _texture->getName() != _textureName) {
			if (auto cache = _director->getResourceCache()) {
				_texture = cache->acquireTexture(_textureName);
				if (_texture) {
					updateBlendAndDepth();
				}
				_materialDirty = true;
			}
		}
	}

	if (_texture) {
		_texture->handleEnter(_frameContext);
	}
}

void Sprite::handleExit() {
	if (_texture) {
		_texture->handleExit(_frameContext);
	}
	Node::handleExit();
}

void Sprite::handleContentSizeDirty() {
	_vertexesDirty = true;
	Node::handleContentSizeDirty();
}

void Sprite::handleTextureLoaded() {
	if (_textureLoadedCallback) {
		_textureLoadedCallback();
	}
}

void Sprite::setColorMode(const core::ColorMode &mode) {
	if (_colorMode != mode) {
		_colorMode = mode;
		_materialDirty = true;
	}
}

void Sprite::setBlendInfo(const core::BlendInfo &info) {
	if (_blendInfo != info) {
		_blendInfo = info;
		_materialInfo.setBlendInfo(info);
		_materialDirty = true;
	}
}

void Sprite::setTextureLayer(float value) {
	if (_textureLayer != value) {
		_textureLayer = value;
		_vertexesDirty = true;
	}
}

void Sprite::setLineWidth(float value) {
	if (_materialInfo.getLineWidth() != value) {
		_materialInfo.setLineWidth(value);
		_materialDirty = true;
	}
}

void Sprite::setRenderingLevel(RenderingLevel level) {
	if (_renderingLevel != level) {
		_renderingLevel = level;
		if (_running) {
			updateBlendAndDepth();
		}
	}
}

void Sprite::setNormalized(bool value) {
	if (_normalized != value) {
		_normalized = value;
	}
}

void Sprite::setTextureAutofit(Autofit autofit) {
	if (_texturePlacement.autofit != autofit) {
		_texturePlacement.autofit = autofit;
		_vertexesDirty = true;
	}
}

void Sprite::setTextureAutofitPosition(const Vec2 &vec) {
	if (_texturePlacement.autofitPos != vec) {
		_texturePlacement.autofitPos = vec;
		if (_texturePlacement.autofit != Autofit::None) {
			_vertexesDirty = true;
		}
	}
}

void Sprite::setSamplerIndex(SamplerIndex idx) {
	if (_samplerIdx != idx) {
		_samplerIdx = idx;
		_materialDirty = true;
	}
}

void Sprite::setTextureLoadedCallback(Function<void()> &&cb) {
	_textureLoadedCallback = sp::move(cb);
}

void Sprite::setOutlineOffset(float val) { _outlineOffset = val; }

void Sprite::setOutlineColor(const Color4F &color) { _outlineColor = color; }

void Sprite::pushCommands(FrameInfo &frame, NodeVisitFlags flags) {
	auto data = _vertexes.pop();
	Mat4 newMV;
	if (_normalized) {
		auto &modelTransform = frame.modelTransformStack.back();
		newMV.m[12] = floorf(modelTransform.m[12]);
		newMV.m[13] = floorf(modelTransform.m[13]);
		newMV.m[14] = floorf(modelTransform.m[14]);
	} else {
		newMV = frame.modelTransformStack.back();
	}

	FrameContextHandle2d *handle = static_cast<FrameContextHandle2d *>(frame.currentContext);

	handle->commands->pushVertexArray(data.get(), frame.viewProjectionStack.back() * newMV,
			buildCmdInfo(frame), _commandFlags);
}

MaterialInfo Sprite::getMaterialInfo() const {
	MaterialInfo ret;
	ret.images[0] = _texture->getIndex();
	ret.samplers[0] = _samplerIdx.get();
	ret.colorModes[0] = _colorMode;
	ret.pipeline = _materialInfo;
	return ret;
}

Vector<core::MaterialImage> Sprite::getMaterialImages() const {
	Vector<core::MaterialImage> ret;
	ret.emplace_back(_texture->getMaterialImage());
	return ret;
}

bool Sprite::isMaterialRevokable() const { return _texture && _texture->getTemporary(); }

void Sprite::updateColor() {
	if (_tmpColor != _displayedColor) {
		_vertexColorDirty = true;
		if (_tmpColor.a != _displayedColor.a) {
			if (_displayedColor.a == 1.0f || _tmpColor.a == 1.0f) {
				updateBlendAndDepth();
			}
		}
		_tmpColor = _displayedColor;
	}
}

void Sprite::updateVertexesColor() { _vertexes.updateColor(_displayedColor); }

void Sprite::initVertexes() {
	_vertexes.init(4, 6);
	_vertexesDirty = true;
}

void Sprite::updateVertexes(FrameInfo &frame) {
	_vertexes.clear();

	auto placementResult = _texturePlacement.resolve(_contentSize,
			Size2(_texture->getExtent().width, _texture->getExtent().height));

	_vertexes.addQuad()
			.setGeometry(Vec4(placementResult.viewRect.origin.x, placementResult.viewRect.origin.y,
								 _textureLayer, 1.0f),
					placementResult.viewRect.size)
			.setTextureRect(placementResult.textureRect, 1.0f, 1.0f, _flippedX, _flippedY, _rotated)
			.setColor(_displayedColor);
	_textureScale = placementResult.scale;
	_vertexColorDirty = false;
}

void Sprite::updateBlendAndDepth() {
	bool shouldBlendColors = false;
	bool shouldWriteDepth = false;

	_realRenderingLevel = getRealRenderingLevel();
	switch (_realRenderingLevel) {
	case RenderingLevel::Default: break;
	case RenderingLevel::Solid:
		shouldWriteDepth = true;
		shouldBlendColors = false;
		break;
	case RenderingLevel::Surface:
		shouldBlendColors = true;
		shouldWriteDepth = false;
		break;
	case RenderingLevel::Transparent:
		shouldBlendColors = true;
		shouldWriteDepth = false;
		break;
	}

	if (shouldBlendColors) {
		if (!_blendInfo.enabled) {
			_blendInfo.enabled = 1;
			_materialDirty = true;
		}
	} else {
		if (_blendInfo.enabled) {
			_blendInfo.enabled = 0;
			_materialDirty = true;
		}
	}

	_materialInfo.setBlendInfo(_blendInfo);

	auto depth = _materialInfo.getDepthInfo();
	if (shouldWriteDepth) {
		if (!depth.writeEnabled) {
			depth.writeEnabled = 1;
			_materialDirty = true;
		}
	} else {
		if (depth.writeEnabled) {
			depth.writeEnabled = 0;
			_materialDirty = true;
		}
	}
	if (_realRenderingLevel == RenderingLevel::Surface
			|| _realRenderingLevel == RenderingLevel::Transparent) {
		if (depth.compare != toInt(core::CompareOp::LessOrEqual)) {
			depth.compare = toInt(core::CompareOp::LessOrEqual);
			_materialDirty = true;
		}
	} else {
		if (depth.compare != toInt(core::CompareOp::Less)) {
			depth.compare = toInt(core::CompareOp::Less);
			_materialDirty = true;
		}
	}
	_materialInfo.setDepthInfo(depth);

	if (_texture) {
		auto info = _texture->getImageInfo();
		auto viewType = core::getImageViewType(info.imageType, info.arrayLayers);
		if (_materialInfo.getImageViewType() != viewType) {
			_materialInfo.setImageViewType(viewType);
			_materialDirty = true;
		}
	}
}

RenderingLevel Sprite::getRealRenderingLevel() const {
	auto level = _renderingLevel;
	if (level == RenderingLevel::Default) {
		RenderingLevel parentLevel = RenderingLevel::Default;
		auto p = _parent;
		while (p) {
			if (auto s = dynamic_cast<Sprite *>(p)) {
				if (s->getRenderingLevel() != RenderingLevel::Default) {
					parentLevel = std::max(s->getRenderingLevel(), parentLevel);
				}
			}
			p = p->getParent();
		}
		if (_displayedColor.a < 1.0f || !_texture || _materialInfo.getLineWidth() != 0.0f) {
			level = RenderingLevel::Transparent;
		} else if (_colorMode.getMode() == core::ColorMode::Solid) {
			if (_texture->hasAlpha()) {
				level = RenderingLevel::Transparent;
			} else {
				level = RenderingLevel::Solid;
			}
		} else {
			auto alphaMapping = _colorMode.getA();
			switch (alphaMapping) {
			case core::ComponentMapping::Identity:
				if (_texture->hasAlpha()) {
					level = RenderingLevel::Transparent;
				} else {
					level = RenderingLevel::Solid;
				}
				break;
			case core::ComponentMapping::Zero: level = RenderingLevel::Transparent; break;
			case core::ComponentMapping::One: level = RenderingLevel::Solid; break;
			default: level = RenderingLevel::Transparent; break;
			}
		}
		level = std::max(level, parentLevel);
	}
	return level;
}

bool Sprite::checkVertexDirty() const { return _vertexesDirty; }

CmdInfo Sprite::buildCmdInfo(const FrameInfo &frame) const {
	auto handle = static_cast<const FrameContextHandle2d *>(frame.currentContext);
	return CmdInfo{frame.zPath, _materialId, handle->getCurrentState(), _realRenderingLevel,
		_displayedColor.a > 0.0f ? frame.depthStack.back() : 0.0f};
}

void Sprite::doScheduleTextureUpdate(Rc<Texture> &&tex) {
	auto comp = Rc<CallbackSystem>::create();
	comp->setUserdata(tex);

	comp->setEnterCallback([](CallbackSystem *comp, Scene *) {
		auto tex = static_cast<Texture *>(comp->getUserdata());

		auto tmp = tex->getTemporary();

		tmp->load(comp, [](Ref *c, bool) {
			auto comp = static_cast<CallbackSystem *>(c);
			if (auto owner = comp->getOwner()) {
				auto tex = static_cast<Texture *>(comp->getUserdata());
				auto sprite = static_cast<Sprite *>(owner);

				sprite->setTexture(tex);
			}
		});
	});

	if (_textureUpdateComponent) {
		removeSystem(_textureUpdateComponent);
	}

	_textureUpdateComponent = addSystem(comp);
}

} // namespace stappler::xenolith::basic2d
