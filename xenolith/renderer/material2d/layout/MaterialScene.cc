/**
 Copyright (c) 2024 Stappler LLC <admin@stappler.dev>
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

#include "MaterialScene.h"
#include "XL2dSceneContent.h"
#include "XL2dSceneLight.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::material2d {

void Scene::addContentNodes(SceneContent *content) {
	Scene2d::addContentNodes(content);

	_styleContainer = content->addSystem(Rc<material2d::StyleContainer>::create());
	_styleContainer->setPrimaryScheme(material2d::ThemeType::LightTheme,
			Color::Teal_500.asColor4F(), false);

	_surfaceInterior = content->addSystem(Rc<material2d::SurfaceInterior>::create(
			material2d::SurfaceStyle(material2d::ColorRole::Background,
					material2d::NodeStyle::SurfaceTonal, material2d::Elevation::Level0)));

	if (auto c2d = dynamic_cast<SceneContent2d *>(content)) {
		// standart material light model

		auto color = Color4F::WHITE;
		color.a = 0.5f;

		auto light = Rc<basic2d::SceneLight>::create(basic2d::SceneLightType::Ambient,
				Vec2(0.0f, 0.3f), 1.5f, color);
		auto ambient = Rc<basic2d::SceneLight>::create(basic2d::SceneLightType::Ambient,
				Vec2(0.0f, 0.0f), 1.5f, color);

		c2d->setGlobalLight(Color4F::WHITE);
		c2d->removeAllLights();
		c2d->addLight(move(light));
		c2d->addLight(move(ambient));
	}
}

} // namespace stappler::xenolith::material2d
