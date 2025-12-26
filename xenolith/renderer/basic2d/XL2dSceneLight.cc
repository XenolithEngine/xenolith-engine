/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

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

#include "XL2dSceneLight.h"
#include "XL2dScene.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

bool SceneLight::init(SceneLightType type, const Vec4 &normal, const Color4F &color, const Vec4 &data) {
	_type = type;
	_normal = normal;
	_color = color;
	_data = data;
	return true;
}

bool SceneLight::init(SceneLightType type, const Vec2 &normal, float k, const Color4F &color, const Vec4 &data) {
	_type = type;
	_enable2dNormal = true;
	_2dNormal = normal;
	_k = k;
	_color = color;
	_data = data;
	return true;
}

void SceneLight::onEnter(Scene *scene) {
	if (_enable2dNormal) {
		_normal = Vec4(Vec3(_2dNormal.x, _2dNormal.y, 1.0f).normalize(), _k);
	}

	_running = true;
}

void SceneLight::onExit() {
	_running = false;
}

void SceneLight::setNormal(const Vec4 &vec) {
	_normal = vec;
}

void SceneLight::setColor(const Color4F &color) {
	_color = color;
}

void SceneLight::setData(const Vec4 &data) {
	_data = data;
}

void SceneLight::setName(StringView str) {
	_name = str.str<Interface>();
}

void SceneLight::setTag(uint64_t tag) {
	_tag = tag;
}

}
