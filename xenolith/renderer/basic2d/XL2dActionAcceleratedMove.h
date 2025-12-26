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

#ifndef XENOLITH_RENDERER_BASIC2D_XL2DACTIONACCELERATEDMOVE_H_
#define XENOLITH_RENDERER_BASIC2D_XL2DACTIONACCELERATEDMOVE_H_

#include "XLAction.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

class SP_PUBLIC ActionAcceleratedMove : public ActionInterval {
public:
	/** Движение от начальной точки до указанной, действие завершается по достижении конечной точки */
	static Rc<ActionInterval> createBounce(float acceleration, Vec2 from, Vec2 to, Vec2 velocity = Vec2::ZERO,
			float bounceAcceleration = 0, Function<void(Node *)> &&callback = nullptr);

	/** Движение от начальной точки до указанной, действие завершается по достижении конечной точки */
	static Rc<ActionInterval> createBounce(float acceleration, Vec2 from, Vec2 to, float velocity,
			float bounceAcceleration = 0, Function<void(Node *)> &&callback = nullptr);

	/** Движение от начальной точки до указанной, действие завершается при сбросе скорости до 0 */
	static Rc<ActionInterval> createFreeBounce(float acceleration, Vec2 from, Vec2 to, Vec2 velocity = Vec2::ZERO,
			float bounceAcceleration = 0, Function<void(Node *)> &&callback = nullptr);

	/** Движение от начальной точки в направлении вектора скорости, действие завершается при границ */
	static Rc<ActionInterval> createWithBounds(float acceleration, Vec2 from, Vec2 velocity,
			const Rect &bounds, Function<void(Node *)> &&callback = nullptr);

	/** отрезок пути до полной остановки (скорость и ускорение направлены в разные стороны) */
	static Rc<ActionAcceleratedMove> createDecceleration(Vec2 normal, Vec2 startPoint, float startVelocity,
			float acceleration, Function<void(Node *)> &&callback = nullptr);

	/** отрезок пути до полной остановки (скорость и ускорение направлены в разные стороны) */
	static Rc<ActionAcceleratedMove> createDecceleration(Vec2 startPoint, Vec2 endPoint,
			float acceleration, Function<void(Node *)> &&callback = nullptr);

	/** ускорение до достижения конечной скорости */
	static Rc<ActionAcceleratedMove> createAccelerationTo(Vec2 normal, Vec2 startPoint, float startVelocity, float endVelocity,
			float acceleration, Function<void(Node *)> &&callback = nullptr);

	/** ускорение до достижения конечной точки */
	static Rc<ActionAcceleratedMove> createAccelerationTo(Vec2 startPoint, Vec2 endPoint, float startVelocity,
			float acceleration, Function<void(Node *)> &&callback = nullptr);

	/** ускоренное движение по времени */
	static Rc<ActionAcceleratedMove> createWithDuration(float duration, Vec2 normal, Vec2 startPoint, float startVelocity,
			float acceleration, Function<void(Node *)> &&callback = nullptr);

	virtual ~ActionAcceleratedMove() { }

	virtual bool initDecceleration(Vec2 normal, Vec2 startPoint, float startVelocity,
			float acceleration, Function<void(Node *)> &&callback = nullptr);
	virtual bool initDecceleration(Vec2 startPoint, Vec2 endPoint,
			float acceleration, Function<void(Node *)> &&callback = nullptr);
	virtual bool initAccelerationTo(Vec2 normal, Vec2 startPoint, float startVelocity, float endVelocity,
			float acceleration, Function<void(Node *)> &&callback = nullptr);
	virtual bool initAccelerationTo(Vec2 startPoint, Vec2 endPoint, float startVelocity,
			float acceleration, Function<void(Node *)> &&callback = nullptr);
	virtual bool initWithDuration(float duration, Vec2 normal, Vec2 startPoint, float startVelocity,
			float acceleration, Function<void(Node *)> &&callback = nullptr);

	float getDuration() const;

	Vec2 getPosition(float timePercent) const;

	const Vec2 &getStartPosition() const { return _startPoint; }
	const Vec2 &getEndPosition() const { return _endPoint; }
	const Vec2 &getNormal() const { return _normalPoint; }

	float getStartVelocity() const { return _startVelocity; }
	float getEndVelocity() const { return _endVelocity; }
	float getCurrentVelocity() const;

	virtual void startWithTarget(Node *target) override;
	virtual void update(float time) override;

	virtual void setCallback(Function<void(Node *)> &&);

protected:
	float _accDuration;
	float _acceleration;

	float _startVelocity;
	float _endVelocity;

	Vec2 _normalPoint;
	Vec2 _startPoint;
	Vec2 _endPoint;

	Vec2 computeEndPoint();
	Vec2 computeNormalPoint();
	float computeEndVelocity();

	Function<void(Node *)> _callback;
};

}

#endif /* XENOLITH_RENDERER_BASIC2D_XL2DACTIONACCELERATEDMOVE_H_ */
