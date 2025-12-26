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

#ifndef XENOLITH_RENDERER_BASIC2D_XL2DVECTORCANVAS_H_
#define XENOLITH_RENDERER_BASIC2D_XL2DVECTORCANVAS_H_

#include "XL2dVectorResult.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

using VectorPath = stappler::vg::VectorPath;

class SP_PUBLIC VectorCanvas : public Ref {
public:
	static Rc<VectorCanvas> getInstance(bool deferred = false);

	virtual ~VectorCanvas();

	bool init(bool deferred);

	void setConfig(const VectorCanvasConfig &config);
	const VectorCanvasConfig &getConfig() const;

	Rc<VectorCanvasResult> draw(const VectorCanvasConfig &config, Rc<VectorImageData> &&);
	Rc<VectorCanvasResult> draw(Rc<VectorImageData> &&);

protected:
	struct Data;

	Data *_data;
};

}

#endif /* XENOLITH_RENDERER_BASIC2D_XL2DVECTORCANVAS_H_ */
