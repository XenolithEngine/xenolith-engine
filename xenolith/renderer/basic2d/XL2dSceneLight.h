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

#ifndef XENOLITH_RENDERER_BASIC2D_XL2DSCENELIGHT_H_
#define XENOLITH_RENDERER_BASIC2D_XL2DSCENELIGHT_H_

#include "XL2d.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class Scene;

}

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

enum class SceneLightType {
	Ambient,
	Direct
};

class SP_PUBLIC SceneLight : public Ref {
public:
	virtual ~SceneLight() { }

	virtual bool init(SceneLightType, const Vec4 &normal, const Color4F &color = Color4F::WHITE, const Vec4 & = Vec4());
	virtual bool init(SceneLightType, const Vec2 &normal, float k, const Color4F &color = Color4F::WHITE, const Vec4 & = Vec4());

	virtual void onEnter(Scene *);
	virtual void onExit();

	SceneLightType getType() const { return _type; }

	virtual void setNormal(const Vec4 &);
	const Vec4 &getNormal() const { return _normal; }

	virtual void setColor(const Color4F &);
	const Color4F &getColor() const { return _color; }

	virtual void setData(const Vec4 &);
	const Vec4 &getData() const { return _data; }

	StringView getName() const { return _name; }
	uint64_t getTag() const { return _tag; }

	Scene *getScene() const { return _scene; }
	bool isRunning() const { return _running; }

	void setSoftShadow(bool value) { _softShadow = value; }
	bool isSoftShadow() const { return _softShadow; }

	virtual void setName(StringView str);
	virtual void setTag(uint64_t);

protected:
	friend class SceneContent2d;

	SceneLightType _type = SceneLightType::Ambient;
	Vec4 _normal;
	Color4F _color;
	Vec4 _data;
	bool _softShadow = true;

	bool _enable2dNormal = false;
	Vec2 _2dNormal;
	float _k = 1.0f;

	String _name;
	uint64_t _tag = InvalidTag;

	bool _running = false;
	Scene *_scene = nullptr;
};

}

#endif /* XENOLITH_RENDERER_BASIC2D_XL2DSCENELIGHT_H_ */
