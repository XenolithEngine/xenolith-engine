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

#include "XLInterpolation.h"

#ifndef M_PI_X_2
#define M_PI_X_2 ((float)M_PI * 2.0f)
#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith::interpolation {

float interpolateTo(float time, Type type, SpanView<float> params) {
	float delta = 0;

	switch (type) {
	case Linear: delta = linear(time); break;

	case EaseIn: delta = easeIn(time, params.size() > 0 ? params[0] : 0.5f); break;
	case EaseOut: delta = easeOut(time, params.size() > 0 ? params[0] : 0.5f); break;
	case EaseInOut: delta = easeInOut(time, params.size() > 0 ? params[0] : 0.5f); break;

	case SineEaseIn: delta = sineEaseIn(time); break;
	case SineEaseOut: delta = sineEaseOut(time); break;
	case SineEaseInOut: delta = sineEaseInOut(time); break;
	case QuadEaseIn: delta = quadEaseIn(time); break;
	case QuadEaseOut: delta = quadEaseOut(time); break;
	case QuadEaseInOut: delta = quadEaseInOut(time); break;

	case CubicEaseIn: delta = cubicEaseIn(time); break;
	case CubicEaseOut: delta = cubicEaseOut(time); break;
	case CubicEaseInOut: delta = cubicEaseInOut(time); break;

	case QuartEaseIn: delta = quartEaseIn(time); break;
	case QuartEaseOut: delta = quartEaseOut(time); break;
	case QuartEaseInOut: delta = quartEaseInOut(time); break;

	case QuintEaseIn: delta = quintEaseIn(time); break;
	case QuintEaseOut: delta = quintEaseOut(time); break;
	case QuintEaseInOut: delta = quintEaseInOut(time); break;

	case ExpoEaseIn: delta = expoEaseIn(time); break;
	case ExpoEaseOut: delta = expoEaseOut(time); break;
	case ExpoEaseInOut: delta = expoEaseInOut(time); break;

	case CircEaseIn: delta = circEaseIn(time); break;
	case CircEaseOut: delta = circEaseOut(time); break;
	case CircEaseInOut: delta = circEaseInOut(time); break;

	case ElasticEaseIn: delta = elasticEaseIn(time, params.size() > 0 ? params[0] : 0.3f); break;
	case ElasticEaseOut: delta = elasticEaseOut(time, params.size() > 0 ? params[0] : 0.3f); break;
	case ElasticEaseInOut:
		delta = elasticEaseInOut(time, params.size() > 0 ? params[0] : 0.3f);
		break;

	case BackEaseIn: delta = backEaseIn(time); break;
	case BackEaseOut: delta = backEaseOut(time); break;
	case BackEaseInOut: delta = backEaseInOut(time); break;

	case BounceEaseIn: delta = bounceEaseIn(time); break;
	case BounceEaseOut: delta = bounceEaseOut(time); break;
	case BounceEaseInOut: delta = bounceEaseInOut(time); break;
	case Custom: delta = customEase(time, params); break;
	case Bezierat:
		if (params.size() > 3) {
			delta = bezieratFunction(time, params[0], params[1], params[2], params[3]);
		} else {
			return time;
		}
		break;
	default: delta = sineEaseInOut(time); break;
	}

	return delta;
}

// Linear
float linear(float time) { return time; }

// Sine Ease
float sineEaseIn(float time) { return -1 * cosf(time * (float)M_PI_2) + 1; }

float sineEaseOut(float time) { return sinf(time * (float)M_PI_2); }

float sineEaseInOut(float time) { return -0.5f * (cosf((float)M_PI * time) - 1); }

// Quad Ease
float quadEaseIn(float time) { return time * time; }

float quadEaseOut(float time) { return -1 * time * (time - 2); }

float quadEaseInOut(float time) {
	time = time * 2;
	if (time < 1) {
		return 0.5f * time * time;
	}
	--time;
	return -0.5f * (time * (time - 2) - 1);
}

// Cubic Ease
float cubicEaseIn(float time) { return time * time * time; }
float cubicEaseOut(float time) {
	time -= 1;
	return (time * time * time + 1);
}
float cubicEaseInOut(float time) {
	time = time * 2;
	if (time < 1) {
		return 0.5f * time * time * time;
	}
	time -= 2;
	return 0.5f * (time * time * time + 2);
}

// Quart Ease
float quartEaseIn(float time) { return time * time * time * time; }

float quartEaseOut(float time) {
	time -= 1;
	return -(time * time * time * time - 1);
}

float quartEaseInOut(float time) {
	time = time * 2;
	if (time < 1) {
		return 0.5f * time * time * time * time;
	}
	time -= 2;
	return -0.5f * (time * time * time * time - 2);
}

// Quint Ease
float quintEaseIn(float time) { return time * time * time * time * time; }

float quintEaseOut(float time) {
	time -= 1;
	return (time * time * time * time * time + 1);
}

float quintEaseInOut(float time) {
	time = time * 2;
	if (time < 1) {
		return 0.5f * time * time * time * time * time;
	}
	time -= 2;
	return 0.5f * (time * time * time * time * time + 2);
}

// Expo Ease
float expoEaseIn(float time) { return time == 0 ? 0 : powf(2, 10 * (time / 1 - 1)) - 1 * 0.001f; }
float expoEaseOut(float time) { return time == 1 ? 1 : (-powf(2, -10 * time / 1) + 1); }
float expoEaseInOut(float time) {
	time /= 0.5f;
	if (time < 1) {
		time = 0.5f * powf(2, 10 * (time - 1));
	} else {
		time = 0.5f * (-powf(2, -10 * (time - 1)) + 2);
	}

	return time;
}

// Circ Ease
float circEaseIn(float time) { return -1 * (sqrt(1 - time * time) - 1); }
float circEaseOut(float time) {
	time = time - 1;
	return sqrt(1 - time * time);
}
float circEaseInOut(float time) {
	time = time * 2;
	if (time < 1) {
		return -0.5f * (sqrt(1 - time * time) - 1);
	}
	time -= 2;
	return 0.5f * (sqrt(1 - time * time) + 1);
}

// Elastic Ease
float elasticEaseIn(float time, float period) {

	float newT = 0;
	if (time == 0 || time == 1) {
		newT = time;
	} else {
		float s = period / 4;
		time = time - 1;
		newT = -powf(2, 10 * time) * sinf((time - s) * M_PI_X_2 / period);
	}

	return newT;
}
float elasticEaseOut(float time, float period) {

	float newT = 0;
	if (time == 0 || time == 1) {
		newT = time;
	} else {
		float s = period / 4;
		newT = powf(2, -10 * time) * sinf((time - s) * M_PI_X_2 / period) + 1;
	}

	return newT;
}
float elasticEaseInOut(float time, float period) {

	float newT = 0;
	if (time == 0 || time == 1) {
		newT = time;
	} else {
		time = time * 2;
		if (!period) {
			period = 0.3f * 1.5f;
		}

		float s = period / 4;

		time = time - 1;
		if (time < 0) {
			newT = -0.5f * powf(2, 10 * time) * sinf((time - s) * M_PI_X_2 / period);
		} else {
			newT = powf(2, -10 * time) * sinf((time - s) * M_PI_X_2 / period) * 0.5f + 1;
		}
	}
	return newT;
}

// Back Ease
float backEaseIn(float time) {
	float overshoot = 1.70158f;
	return time * time * ((overshoot + 1) * time - overshoot);
}
float backEaseOut(float time) {
	float overshoot = 1.70158f;

	time = time - 1;
	return time * time * ((overshoot + 1) * time + overshoot) + 1;
}
float backEaseInOut(float time) {
	float overshoot = 1.70158f * 1.525f;

	time = time * 2;
	if (time < 1) {
		return (time * time * ((overshoot + 1) * time - overshoot)) / 2;
	} else {
		time = time - 2;
		return (time * time * ((overshoot + 1) * time + overshoot)) / 2 + 1;
	}
}

// Bounce Ease
float bounceTime(float time) {
	if (time < 1 / 2.75) {
		return 7.5625f * time * time;
	} else if (time < 2 / 2.75) {
		time -= 1.5f / 2.75f;
		return 7.5625f * time * time + 0.75f;
	} else if (time < 2.5 / 2.75) {
		time -= 2.25f / 2.75f;
		return 7.5625f * time * time + 0.9375f;
	}

	time -= 2.625f / 2.75f;
	return 7.5625f * time * time + 0.984375f;
}
float bounceEaseIn(float time) { return 1 - bounceTime(1 - time); }

float bounceEaseOut(float time) { return bounceTime(time); }

float bounceEaseInOut(float time) {
	float newT = 0;
	if (time < 0.5f) {
		time = time * 2;
		newT = (1 - bounceTime(1 - time)) * 0.5f;
	} else {
		newT = bounceTime(time * 2 - 1) * 0.5f + 0.5f;
	}

	return newT;
}

// Custom Ease
float customEase(float time, const SpanView<float> &params) {
	if (params.size() > 7) {
		float tt = 1 - time;
		return params[1] * tt * tt * tt + 3 * params[3] * time * tt * tt
				+ 3 * params[5] * time * time * tt + params[7] * time * time * time;
	}
	return time;
}

float easeIn(float time, float rate) { return powf(time, rate); }

float easeOut(float time, float rate) { return powf(time, 1 / rate); }

float easeInOut(float time, float rate) {
	time *= 2;
	if (time < 1) {
		return 0.5f * powf(time, rate);
	} else {
		return (1.0f - 0.5f * powf(2 - time, rate));
	}
}

float quadraticIn(float time) { return powf(time, 2); }

float quadraticOut(float time) { return -time * (time - 2); }

float quadraticInOut(float time) {

	float resultTime = time;
	time = time * 2;
	if (time < 1) {
		resultTime = time * time * 0.5f;
	} else {
		--time;
		resultTime = -0.5f * (time * (time - 2) - 1);
	}
	return resultTime;
}

static float evaluateCubic(float t, float p1, float p2) {
	return
			// std::pow(1.0f - t, 3.0f) * p0 // - p0 = 0.0f
			3.0f * std::pow(1.0f - t, 2) * t * p1 + 3.0f * (1.0f - t) * std::pow(t, 2.0f) * p2
			+ std::pow(t, 3.0f); // p3 = 1.0f
}

static constexpr float BezieratErrorBound = 0.001f;

static float truncateBorders(float t) {
	if (std::fabs(t) < BezieratErrorBound) {
		return 0.0f;
	} else if (std::fabs(t - 1.0f) < BezieratErrorBound) {
		return 1.0f;
	}
	return t;
}

float bezieratFunction(float t, float x1, float y1, float x2, float y2) {
	float start = 0.0f;
	float end = 1.0f;

	while (true) {
		const float midpoint = (start + end) / 2;
		const float estimate = evaluateCubic(midpoint, x1, x2);
		if (std::abs(t - estimate) < BezieratErrorBound) {
			return truncateBorders(evaluateCubic(midpoint, y1, y2));
		}
		if (estimate < t) {
			start = midpoint;
		} else {
			end = midpoint;
		}
	}
	return nan();
}

} // namespace stappler::xenolith::interpolation
