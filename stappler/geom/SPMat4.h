/**
Copyright 2013 BlackBerry Inc.
Copyright (c) 2014-2015 Chukong Technologies
Copyright (c) 2017-2019 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023-2024 Stappler LLC <admin@stappler.dev>

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

Original file from GamePlay3D: http://gameplay3d.org

This file was modified to fit the cocos2d-x project
This file was modified for stappler project
*/

#ifndef CORE_GEOM_SPMAT4_H_
#define CORE_GEOM_SPMAT4_H_

#include "SPVec2.h"
#include "SPVec3.h"
#include "SPVec4.h"

namespace STAPPLER_VERSIONIZED stappler::geom {

/**
 * Defines a 4 x 4 floating point matrix representing a 3D transformation.
 *
 * Vectors are treated as columns, resulting in a matrix that is represented as follows,
 * where x, y and z are the translation components of the matrix:
 *
 *     1  0  0  x
 *     0  1  0  y
 *     0  0  1  z
 *     0  0  0  1
 *
 * This matrix class is directly compatible with OpenGL since its elements are
 * laid out in memory exactly as they are expected by OpenGL.
 * The matrix uses column-major format such that array indices increase down column first.
 * Since matrix multiplication is not commutative, multiplication must be done in the
 * correct order when combining transformations. Suppose we have a translation
 * matrix T and a rotation matrix R. To first rotate an object around the origin
 * and then translate it, you would multiply the two matrices as TR.
 *
 * Likewise, to first translate the object and then rotate it, you would do RT.
 * So generally, matrices must be multiplied in the reverse order in which you
 * want the transformations to take place (this also applies to
 * the scale, rotate, and translate methods below; these methods are convenience
 * methods for post-multiplying by a matrix representing a scale, rotation, or translation).
 *
 * In the case of repeated local transformations (i.e. rotate around the Z-axis by 0.76 radians,
 * then translate by 2.1 along the X-axis, then ...), it is better to use the Transform class
 * (which is optimized for that kind of usage).
 *
 * @see Transform
 */
class SP_PUBLIC alignas(16) Mat4 {
public:
	static void createLookAt(const Vec3 &eyePosition, const Vec3 &targetPosition, const Vec3 &up,
			Mat4 *dst);

	static void createLookAt(float eyePositionX, float eyePositionY, float eyePositionZ,
			float targetCenterX, float targetCenterY, float targetCenterZ, float upX, float upY,
			float upZ, Mat4 *dst);

	/** Builds a perspective projection matrix based on a field of view and returns by value.
	 *
	 * Projection space refers to the space after applying projection transformation from view space.
	 * After the projection transformation, visible content has x- and y-coordinates ranging from -1 to 1,
	 * and a z-coordinate ranging from 0 to 1. To obtain the viewable area (in world space) of a scene,
	 * create a BoundingFrustum and pass the combined view and projection matrix to the constructor. */
	static void createPerspective(float fieldOfView, float aspectRatio, float zNearPlane,
			float zFarPlane, Mat4 *dst);

	/** Creates an orthographic projection matrix. */
	static void createOrthographic(float width, float height, float zNearPlane, float zFarPlane,
			Mat4 *dst);

	/** Creates an orthographic projection matrix.
	 *
	 * Projection space refers to the space after applying
	 * projection transformation from view space. After the
	 * projection transformation, visible content has
	 * x and y coordinates ranging from -1 to 1, and z coordinates
	 * ranging from 0 to 1.
	 *
	 * Unlike perspective projection, in orthographic projection
	 * there is no perspective foreshortening.
	 *
	 * The viewable area of this orthographic projection extends
	 * from left to right on the x-axis, bottom to top on the y-axis,
	 * and zNearPlane to zFarPlane on the z-axis. These values are
	 * relative to the position and x, y, and z-axes of the view.
	 * To obtain the viewable area (in world space) of a scene,
	 * create a BoundingFrustum and pass the combined view and
	 * projection matrix to the constructor. */
	static void createOrthographicOffCenter(float left, float right, float bottom, float top,
			float zNearPlane, float zFarPlane, Mat4 *dst);

	/** Creates a spherical billboard that rotates around a specified object position.
	 *
	 * This method computes the facing direction of the billboard from the object position
	 * and camera position. When the object and camera positions are too close, the matrix
	 * will not be accurate. To avoid this problem, this method defaults to the identity
	 * rotation if the positions are too close. (See the other overload of createBillboard
	 * for an alternative approach). */
	static void createBillboard(const Vec3 &objectPosition, const Vec3 &cameraPosition,
			const Vec3 &cameraUpVector, Mat4 *dst);

	/** Creates a spherical billboard that rotates around a specified object position with
	 * provision for a safe default orientation.
	 *
	 * This method computes the facing direction of the billboard from the object position
	 * and camera position. When the object and camera positions are too close, the matrix
	 * will not be accurate. To avoid this problem, this method uses the specified camera
	 * forward vector if the positions are too close. (See the other overload of createBillboard
	 * for an alternative approach). */
	static void createBillboard(const Vec3 &objectPosition, const Vec3 &cameraPosition,
			const Vec3 &cameraUpVector, const Vec3 &cameraForwardVector, Mat4 *dst);

	static void createScale(const Vec3 &scale, Mat4 *dst);
	static void createScale(float xScale, float yScale, float zScale, Mat4 *dst);
	static void createRotation(const Quaternion &quat, Mat4 *dst);
	static void createRotation(const Vec3 &axis, float angle, Mat4 *dst);
	static void createRotationX(float angle, Mat4 *dst);
	static void createRotationY(float angle, Mat4 *dst);
	static void createRotationZ(float angle, Mat4 *dst);
	static void createTranslation(const Vec3 &translation, Mat4 *dst);
	static void createTranslation(float xTranslation, float yTranslation, float zTranslation,
			Mat4 *dst);

	static void add(const Mat4 &m1, const Mat4 &m2, Mat4 *dst) {
		simd::addMat4(m1.m, m2.m, dst->m);
	}

	static void multiply(const Mat4 &mat, float scalar, Mat4 *dst) {
		simd::multiplyMat4Scalar(mat.m, scalar, dst->m);
	}

	static void multiply(const Mat4 &m1, const Mat4 &m2, Mat4 *dst) {
		simd::multiplyMat4(m1.m, m2.m, dst->m);
	}

	static void subtract(const Mat4 &m1, const Mat4 &m2, Mat4 *dst) {
		simd::subtractMat4(m1.m, m2.m, dst->m);
	}

	float m[16];

	constexpr Mat4() { *this = IDENTITY; }

	constexpr Mat4(float m11, float m12, float m13, float m14, float m21, float m22, float m23,
			float m24, float m31, float m32, float m33, float m34, float m41, float m42, float m43,
			float m44)
	: m{m11, m21, m31, m41, m12, m22, m32, m42, m13, m23, m33, m43, m14, m24, m34, m44} { }

	constexpr Mat4(float a, float b, float c, float d, float e, float f)
	: m{a, b, 0, 0, c, d, 0, 0, 0, 0, 1, 0, e, f, 0, 1} { }

	constexpr Mat4(const Mat4 &copy) = default;

	void add(float scalar) { add(scalar, this); }
	void add(float scalar, Mat4 *dst) { simd::addMat4Scalar(m, scalar, dst->m); }
	void add(const Mat4 &mat) { add(*this, mat, this); }

	bool decompose(Vec3 *scale, Quaternion *rotation, Vec3 *translation) const;
	bool decompose(float *scale, float *rotation, float *translation) const;

	float determinant() const;

	void getScale(Vec3 *scale) const;
	bool getRotation(Quaternion *rotation) const;
	void getTranslation(Vec3 *translation) const;

	void getScale(float *scale) const;
	bool getRotation(float *rotation) const;
	void getTranslation(float *translation) const;

	void getUpVector(Vec3 *dst) const;
	void getDownVector(Vec3 *dst) const;
	void getLeftVector(Vec3 *dst) const;
	void getRightVector(Vec3 *dst) const;
	void getForwardVector(Vec3 *dst) const;
	void getBackVector(Vec3 *dst) const;

	bool inverse();
	void negate() { simd::negateMat4(m, m); }
	void transpose() { simd::transposeMat4(m, m); }

	Mat4 getInversed() const {
		Mat4 mat(*this);
		mat.inverse();
		return mat;
	}
	Mat4 getNegated() const {
		Mat4 mat(*this);
		mat.negate();
		return mat;
	}
	Mat4 getTransposed() const {
		Mat4 mat(*this);
		mat.transpose();
		return mat;
	}

	bool isIdentity() const { return *this == IDENTITY; }

	void multiply(float scalar) { multiply(scalar, this); }
	void multiply(float scalar, Mat4 *dst) const { multiply(*this, scalar, dst); }
	void multiply(const Mat4 &mat) { multiply(*this, mat, this); }

	void rotate(const Quaternion &q);
	void rotate(const Quaternion &q, Mat4 *dst) const;
	void rotate(const Vec3 &axis, float angle);
	void rotate(const Vec3 &axis, float angle, Mat4 *dst) const;
	void rotateX(float angle);
	void rotateX(float angle, Mat4 *dst) const;
	void rotateY(float angle);
	void rotateY(float angle, Mat4 *dst) const;
	void rotateZ(float angle);
	void rotateZ(float angle, Mat4 *dst) const;

	void scale(float value);
	void scale(float value, Mat4 *dst) const;
	void scale(float xScale, float yScale, float zScale);
	void scale(float xScale, float yScale, float zScale, Mat4 *dst) const;
	void scale(const Vec3 &s);
	void scale(const Vec3 &s, Mat4 *dst) const;
	void subtract(const Mat4 &mat) { subtract(*this, mat, this); }

	Vec2 transformPoint(const Vec2 &point) const { // TODO: implement simd version of 2D-transform
		Vec4 ret;
		transformVector(point.x, point.y, 1.0f, 1.0f, &ret);
		return Vec2(ret.x, ret.y);
	}
	void transformPoint(Vec2 *point) const { // TODO: implement simd version of 2D-transform
		Vec4 ret;
		transformVector(point->x, point->y, 1.0f, 1.0f, &ret);
		*point = Vec2(ret.x, ret.y);
	}
	void transformVector(Vec4 *vector) const { simd::transformVec4(m, &vector->x, &vector->x); }
	void transformVector(float x, float y, float z, float w, Vec4 *dst) const {
		simd::transformVec4Components(m, x, y, z, w, &dst->x);
	}
	void transformVector(float x, float y, float z, float w, float *dst) const {
		simd::transformVec4Components(m, x, y, z, w, dst);
	}
	void transformVector(const Vec4 &vector, Vec4 *dst) const {
		simd::transformVec4(m, &vector.x, &dst->x);
	}
	void transformVector(const Vec4 &vector, Vec3 *dst) const {
		simd::transformVec4(m, &vector.x, &dst->x);
	}

	void translate(float x, float y, float z);
	void translate(float x, float y, float z, Mat4 *dst) const;
	void translate(const Vec3 &t);

	void translate(const Vec3 &t, Mat4 *dst) const;

	const Mat4 operator+(const Mat4 &mat) const {
		Mat4 result(*this);
		result.add(mat);
		return result;
	}

	Mat4 &operator+=(const Mat4 &mat) {
		add(mat);
		return *this;
	}

	const Mat4 operator-(const Mat4 &mat) const {
		Mat4 result(*this);
		result.subtract(mat);
		return result;
	}

	Mat4 &operator-=(const Mat4 &mat) {
		subtract(mat);
		return *this;
	}

	const Mat4 operator-() const {
		Mat4 mat(*this);
		mat.negate();
		return mat;
	}

	const Mat4 operator*(const Mat4 &mat) const {
		Mat4 result(*this);
		result.multiply(mat);
		return result;
	}

	Mat4 &operator*=(const Mat4 &mat) {
		multiply(mat);
		return *this;
	}

	inline bool operator==(const Mat4 &) const = default;
	inline bool operator!=(const Mat4 &) const = default;

	static const Mat4 ZERO;
	static const Mat4 IDENTITY;
	static const Mat4 INVALID;

	static const Mat4 ROTATION_Z_90;
	static const Mat4 ROTATION_Z_180;
	static const Mat4 ROTATION_Z_270;
};

constexpr const Mat4 Mat4::IDENTITY = Mat4(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);

constexpr const Mat4 Mat4::ZERO = Mat4(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);

constexpr const Mat4 Mat4::INVALID = Mat4(stappler::nan(), stappler::nan(), stappler::nan(),
		stappler::nan(), stappler::nan(), stappler::nan(), stappler::nan(), stappler::nan(),
		stappler::nan(), stappler::nan(), stappler::nan(), stappler::nan(), stappler::nan(),
		stappler::nan(), stappler::nan(), stappler::nan());

constexpr const Mat4 Mat4::ROTATION_Z_90 = Mat4(0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);

constexpr const Mat4 Mat4::ROTATION_Z_180 = Mat4(-1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);

constexpr const Mat4 Mat4::ROTATION_Z_270 = Mat4(0.0f, 1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);

inline Vec4 &operator*=(Vec4 &v, const Mat4 &m) {
	m.transformVector(&v);
	return v;
}

inline const Vec4 operator*(const Mat4 &m, const Vec4 &v) {
	Vec4 x;
	m.transformVector(v, &x);
	return x;
}

inline const Vec3 operator*(const Mat4 &m, const Vec3 &v) {
	struct alignas(16) {
		Vec3 vec;
		float w;
	} s;
	m.transformVector(v.x, v.y, v.z, 1.0f, &s.vec.x);
	return s.vec;
}

inline const Vec2 operator*(const Mat4 &m, const Vec2 &v) {
	struct alignas(16) {
		Vec2 vec;
		float z;
		float w;
	} s;
	m.transformVector(v.x, v.y, 1.0f, 1.0f, &s.vec.x);
	return s.vec;
}

inline std::basic_ostream<char> &operator<<(std::basic_ostream<char> &os, const Mat4 &m) {
	os << "{\n\t( " << m.m[0] << ", " << m.m[4] << ", " << m.m[8] << ", " << m.m[12] << ")\n"
	   << "\t( " << m.m[1] << ", " << m.m[5] << ", " << m.m[9] << ", " << m.m[13] << ")\n"
	   << "\t( " << m.m[2] << ", " << m.m[6] << ", " << m.m[10] << ", " << m.m[14] << ")\n"
	   << "\t( " << m.m[3] << ", " << m.m[7] << ", " << m.m[11] << ", " << m.m[15] << ")\n"
	   << "}";
	return os;
}

} // namespace stappler::geom

#endif /* CORE_GEOM_SPMAT4_H_ */
