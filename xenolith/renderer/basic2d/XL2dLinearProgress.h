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

#ifndef XENOLITH_RENDERER_BASIC2D_XL2DLINEARPROGRESS_H_
#define XENOLITH_RENDERER_BASIC2D_XL2DLINEARPROGRESS_H_

#include "XL2d.h"
#include "XL2dLayer.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

class SP_PUBLIC LinearProgress : public Node {
public:
	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

	virtual void handleEnter(Scene *) override;
	virtual void handleExit() override;

	virtual void setAnimated(bool value);
	virtual bool isAnimated() const;

	virtual void setProgress(float value);
	virtual float getProgress() const;

	virtual void setLineColor(const Color &);
	virtual void setLineOpacity(float);

	virtual void setBarColor(const Color &);
	virtual void setBarOpacity(float);

protected:
	virtual void layoutSubviews();
	virtual void updateAnimations();

	bool _animated = false;
	float _progress = 0.0f;

	Layer *_line = nullptr;
	Layer *_bar = nullptr;
};

}

#endif /* XENOLITH_RENDERER_BASIC2D_XL2DLINEARPROGRESS_H_ */
