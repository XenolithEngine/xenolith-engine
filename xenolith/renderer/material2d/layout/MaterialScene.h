/**
 Copyright (c) 2024 Stappler LLC <admin@stappler.dev>

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

#ifndef XENOLITH_RENDERER_MATERIAL2D_LAYOUT_MATERIALSCENE_H_
#define XENOLITH_RENDERER_MATERIAL2D_LAYOUT_MATERIALSCENE_H_

#include "MaterialSurfaceInterior.h"
#include "MaterialStyleContainer.h"
#include "XL2dScene.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::material2d {

class SP_PUBLIC Scene : public Scene2d {
public:
	virtual ~Scene() = default;

	material2d::StyleContainer *getStyleContainer() const { return _styleContainer; }
	material2d::SurfaceInterior *getSurfaceInterior() const { return _surfaceInterior; }

protected:
	virtual void addContentNodes(xenolith::SceneContent *) override;

	material2d::StyleContainer *_styleContainer = nullptr;
	material2d::SurfaceInterior *_surfaceInterior = nullptr;
};

}

#endif /* XENOLITH_RENDERER_MATERIAL2D_LAYOUT_MATERIALSCENE_H_ */
