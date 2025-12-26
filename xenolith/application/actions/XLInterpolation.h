/**
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

#ifndef XENOLITH_APPLICATION_ACTIONS_XLINTERPOLATION_H_
#define XENOLITH_APPLICATION_ACTIONS_XLINTERPOLATION_H_

#include "XLCommon.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::interpolation {

enum Type {
	Linear,

	EaseIn,
	EaseOut,
	EaseInOut,

	SineEaseIn,
	SineEaseOut,
	SineEaseInOut,

	QuadEaseIn,
	QuadEaseOut,
	QuadEaseInOut,

	CubicEaseIn,
	CubicEaseOut,
	CubicEaseInOut,

	QuartEaseIn,
	QuartEaseOut,
	QuartEaseInOut,

	QuintEaseIn,
	QuintEaseOut,
	QuintEaseInOut,

	ExpoEaseIn,
	ExpoEaseOut,
	ExpoEaseInOut,

	CircEaseIn,
	CircEaseOut,
	CircEaseInOut,

	ElasticEaseIn,
	ElasticEaseOut,
	ElasticEaseInOut,

	BackEaseIn,
	BackEaseOut,
	BackEaseInOut,

	BounceEaseIn,
	BounceEaseOut,
	BounceEaseInOut,

	Custom,

	Bezierat, // (x1, y1, x2, y2)

	Max
};

SP_PUBLIC float interpolateTo(float time, Type type, SpanView<float> = SpanView<float>());

SP_PUBLIC float linear(float time);

SP_PUBLIC float easeIn(float time, float rate);
SP_PUBLIC float easeOut(float time, float rate);
SP_PUBLIC float easeInOut(float time, float rate);

SP_PUBLIC float bezieratFunction(float t, float a, float b, float c, float d);

SP_PUBLIC float quadraticIn(float time);
SP_PUBLIC float quadraticOut(float time);
SP_PUBLIC float quadraticInOut(float time);

SP_PUBLIC float sineEaseIn(float time);
SP_PUBLIC float sineEaseOut(float time);
SP_PUBLIC float sineEaseInOut(float time);

SP_PUBLIC float quadEaseIn(float time);
SP_PUBLIC float quadEaseOut(float time);
SP_PUBLIC float quadEaseInOut(float time);

SP_PUBLIC float cubicEaseIn(float time);
SP_PUBLIC float cubicEaseOut(float time);
SP_PUBLIC float cubicEaseInOut(float time);

SP_PUBLIC float quartEaseIn(float time);
SP_PUBLIC float quartEaseOut(float time);
SP_PUBLIC float quartEaseInOut(float time);

SP_PUBLIC float quintEaseIn(float time);
SP_PUBLIC float quintEaseOut(float time);
SP_PUBLIC float quintEaseInOut(float time);

SP_PUBLIC float expoEaseIn(float time);
SP_PUBLIC float expoEaseOut(float time);
SP_PUBLIC float expoEaseInOut(float time);

SP_PUBLIC float circEaseIn(float time);
SP_PUBLIC float circEaseOut(float time);
SP_PUBLIC float circEaseInOut(float time);

SP_PUBLIC float elasticEaseIn(float time, float period);
SP_PUBLIC float elasticEaseOut(float time, float period);
SP_PUBLIC float elasticEaseInOut(float time, float period);

SP_PUBLIC float backEaseIn(float time);
SP_PUBLIC float backEaseOut(float time);
SP_PUBLIC float backEaseInOut(float time);

SP_PUBLIC float bounceEaseIn(float time);
SP_PUBLIC float bounceEaseOut(float time);
SP_PUBLIC float bounceEaseInOut(float time);

SP_PUBLIC float customEase(float time, const SpanView<float> &);

} // namespace stappler::xenolith::interpolation

#endif /* XENOLITH_APPLICATION_ACTIONS_XLINTERPOLATION_H_ */
