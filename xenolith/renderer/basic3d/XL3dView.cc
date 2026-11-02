/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
 SPDX-License-Identifier: MIT
**/

#include "XL3dView.h"
#include "XL3dMesh.h"
#include "XLFrameContext.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic3d {

bool View3d::init() {
	if (!Node::init()) {
		return false;
	}

	_sunDirection.normalize();
	return true;
}

void View3d::handleEnter(xenolith::Scene *scene) {
	Node::handleEnter(scene);
	for (auto &it : _draws) {
		retainTexture(it.material.texture);
		retainTexture(it.material.texture2);
		retainTexture(it.material.texture3);
	}
	for (auto &it : _drawPacks) { retainPack(it); }
}

void View3d::handleExit() {
	for (auto &it : _draws) {
		releaseTexture(it.material.texture);
		releaseTexture(it.material.texture2);
		releaseTexture(it.material.texture3);
	}
	for (auto &it : _drawPacks) { releasePack(it); }
	Node::handleExit();
}

void View3d::setCamera(const Camera &camera) {
	_camera = camera;
	markSceneChanged();
}

void View3d::setAmbient(const Color4F &color) {
	_ambient = color;
	markSceneChanged();
}

void View3d::setSun(const Vec3 &towardSun, const Color4F &color) {
	_sunDirection = towardSun;
	_sunDirection.normalize();
	_sunColor = color;
	markSceneChanged();
}

uint32_t View3d::addDraw(Draw3d &&d) {
	retainTexture(d.material.texture);
	retainTexture(d.material.texture2);
	retainTexture(d.material.texture3);
	auto pack = d.mesh ? d.mesh->getPack() : nullptr;
	retainPack(pack);
	_drawPacks.emplace_back(pack);
	_draws.emplace_back(sp::move(d));
	markSceneChanged();
	return uint32_t(_draws.size() - 1);
}

void View3d::setDrawTransform(uint32_t idx, const Mat4 &transform) {
	if (idx < _draws.size()) {
		_draws[idx].transform = transform;
		markSceneChanged();
	}
}

void View3d::setDrawMaterial(uint32_t idx, Material3d &&material) {
	if (idx < _draws.size()) {
		retainTexture(material.texture);
		retainTexture(material.texture2);
		retainTexture(material.texture3);
		releaseTexture(_draws[idx].material.texture);
		releaseTexture(_draws[idx].material.texture2);
		releaseTexture(_draws[idx].material.texture3);
		_draws[idx].material = sp::move(material);
		markSceneChanged();
	}
}

void View3d::clearDraws() {
	for (auto &it : _draws) {
		releaseTexture(it.material.texture);
		releaseTexture(it.material.texture2);
		releaseTexture(it.material.texture3);
	}
	for (auto &it : _drawPacks) { releasePack(it); }
	_drawPacks.clear();
	_draws.clear();
	markSceneChanged();
}

void View3d::draw(FrameInfo &frame, NodeVisitFlags flags) {
	Node::draw(frame, flags);

	auto handle = dynamic_cast<FrameContextHandle3d *>(frame.currentContext);
	if (!handle) {
		if (!_contextWarned) {
			log::source().error("basic3d::View3d",
					"not in a SceneContent3d: the frame has no 3d views to add to");
			_contextWarned = true;
		}
		return;
	}

	// the rect in the scene's pixels, y up; the camera measures its scale in points
	_camera.setViewport(getWorldBoundingBox(), sprt::max(getInputDensity(), 0.01f));

	auto &view = handle->views.emplace_back();
	view.viewport = _camera.getViewport();
	view.data.viewProj = _camera.getViewProjection();
	view.data.sunDirection = Vec4(_sunDirection.x, _sunDirection.y, _sunDirection.z, 0.0f);
	view.data.sunColor = Vec4(_sunColor.r, _sunColor.g, _sunColor.b, 1.0f);
	view.data.ambient = Vec4(_ambient.r, _ambient.g, _ambient.b, 1.0f);
	view.data.screen =
			Vec4(view.viewport.size.width, view.viewport.size.height, _camera.getDensity(), 0.0f);
	collectDraws(view);

	// where the meshes of loaded packs are, taken here on the app thread for the pass's worker
	if (!isPackingDisabled()) {
		for (auto &d : view.draws) {
			auto pack = d.mesh ? d.mesh->getPack() : nullptr;
			if (pack && pack->isLoaded()) {
				auto &range = d.mesh->getPackRange();
				d.gpu = DrawGpu3d{pack->getVertexBuffer(), pack->getIndexBuffer(), range.firstIndex,
					range.vertexOffset};
			}
		}
	}
}

void View3d::retainPack(MeshPack3d *pack) {
	if (pack && isRunning() && _frameContext) {
		pack->addUser(_frameContext);
	}
}

void View3d::releasePack(MeshPack3d *pack) {
	if (pack && isRunning() && _frameContext) {
		pack->removeUser(_frameContext);
	}
}

void View3d::addLight(const Light3d &light) {
	_lights.emplace_back(light);
	markSceneChanged();
}

void View3d::clearLights() {
	_lights.clear();
	markSceneChanged();
}

void View3d::collectDraws(ViewInput3d &view) {
	view.draws = _draws;
	view.lights = _lights;
}

void View3d::retainTexture(const Rc<Texture> &texture) {
	if (texture && isRunning() && _frameContext) {
		texture->handleEnter(_frameContext);
	}
}

void View3d::releaseTexture(const Rc<Texture> &texture) {
	if (texture && isRunning() && _frameContext) {
		texture->handleExit(_frameContext);
	}
}

} // namespace stappler::xenolith::basic3d
