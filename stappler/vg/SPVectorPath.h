/**
Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023-2024 Stappler LLC <admin@stappler.dev>

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

#ifndef CORE_VG_SPVECTORPATH_H_
#define CORE_VG_SPVECTORPATH_H_

#include "SPRef.h"
#include "SPVectorPathData.h"

namespace STAPPLER_VERSIONIZED stappler::vg {

using Interface = memory::StandartInterface;

struct PathXRef {
	Interface::StringType id;
	Interface::StringType cacheId;
	Mat4 mat;
	Color4F color = Color4F::WHITE;
};

class SP_PUBLIC VectorPath : public Ref {
public:
	using DrawStyle = geom::DrawFlags;
	using Winding = geom::Winding;
	using LineCup = geom::LineCup;
	using LineJoin = geom::LineJoin;

	VectorPath();
	VectorPath(size_t);

	VectorPath(const VectorPath &);
	VectorPath &operator=(const VectorPath &);

	VectorPath(VectorPath &&);
	VectorPath &operator=(VectorPath &&);

	bool init();
	bool init(StringView);
	bool init(const FileInfo &);
	bool init(BytesView);

	bool init(const PathData<memory::StandartInterface> &);
	bool init(const PathData<memory::PoolInterface> &);

	VectorPath & addPath(const VectorPath &);
	VectorPath & addPath(BytesView);
	VectorPath & addPath(StringView);

	size_t count() const;

	VectorPath & openForWriting(const Callback<void(PathWriter &)> &);

	VectorPath & setFillColor(const Color4B &color);
	VectorPath & setFillColor(const Color3B &color, bool preserveOpacity = false);
	VectorPath & setFillColor(const Color &color, bool preserveOpacity = false);
	const Color4B &getFillColor() const;

	VectorPath & setStrokeColor(const Color4B &color);
	VectorPath & setStrokeColor(const Color3B &color, bool preserveOpacity = false);
	VectorPath & setStrokeColor(const Color &color, bool preserveOpacity = false);
	const Color4B &getStrokeColor() const;

	VectorPath & setFillOpacity(uint8_t value);
	uint8_t getFillOpacity() const;

	VectorPath & setStrokeOpacity(uint8_t value);
	uint8_t getStrokeOpacity() const;

	VectorPath & setStrokeWidth(float width);
	float getStrokeWidth() const;

	VectorPath &setWindingRule(Winding);
	Winding getWindingRule() const;

	VectorPath &setLineCup(LineCup);
	LineCup getLineCup() const;

	VectorPath &setLineJoin(LineJoin);
	LineJoin getLineJoin() const;

	VectorPath &setMiterLimit(float);
	float getMiterLimit() const;

	VectorPath & setStyle(DrawStyle s);
	DrawStyle getStyle() const;

	VectorPath &setAntialiased(bool);
	bool isAntialiased() const;

	// transform should be applied in reverse order
	VectorPath & setTransform(const Mat4 &);
	VectorPath & applyTransform(const Mat4 &);
	const Mat4 &getTransform() const;

	VectorPath & clear();

	VectorPath & setParams(const PathParams &);
	PathParams getParams() const;

	bool empty() const;

	void reserve(size_t);

	const Interface::VectorType<Command> &getCommands() const;
	const Interface::VectorType<CommandData> &getPoints() const;

	explicit operator bool() const { return !empty(); }

	size_t commandsCount() const;
	size_t dataCount() const;

	Interface::BytesType encode() const;
	Interface::StringType toString(bool newline) const;

protected:
	friend class Image;
	friend struct SvgTag;

	PathWriter getWriter();

	PathData<Interface> _data;
};

}

#endif /* STAPPLER_VG_SPVECTORPATH_H_ */
