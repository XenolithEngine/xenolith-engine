/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
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

#ifndef XENOLITH_RENDERER_BASIC2D_GLSL_INCLUDE_XL2DGLSLDATA_H_
#define XENOLITH_RENDERER_BASIC2D_GLSL_INCLUDE_XL2DGLSLDATA_H_

#include "SPGlsl.h"

#ifndef SP_GLSL
namespace STAPPLER_VERSIONIZED stappler::glsl {
#endif

#define XL_GLSL_MATERIAL_FLAG_HAS_ATLAS_INDEX 1
#define XL_GLSL_MATERIAL_FLAG_HAS_ATLAS_DATA 2
#define XL_GLSL_MATERIAL_FLAG_HAS_ATLAS 3 // (XL_GLSL_MATERIAL_FLAG_HAS_ATLAS_INDEX | XL_GLSL_MATERIAL_FLAG_HAS_ATLAS_DATA)
#define XL_GLSL_MATERIAL_FLAG_ATLAS_IS_BDA 4
#define XL_GLSL_MATERIAL_FLAG_ATLAS_POW2_INDEX_BIT_OFFSET 24

#define XL_GLSL_FLAG_POSITION_MASK_X (1 << 0)
#define XL_GLSL_FLAG_POSITION_MASK_Y (1 << 1)
#define XL_GLSL_FLAG_POSITION_MASK_Z (1 << 2)
#define XL_GLSL_FLAG_POSITION_MASK_W (1 << 3)

#define XL_GLSL_FLAG_DEFAULT (XL_GLSL_FLAG_POSITION_MASK_X | XL_GLSL_FLAG_POSITION_MASK_Y | XL_GLSL_FLAG_POSITION_MASK_W)

struct VertexConstantData {
	uvec2 vertexPointer; // 0-8
	uvec2 transformPointer; // 8-16
	uvec2 materialPointer; // 16-24
	uvec2 atlasPointer; // 24-32
	uint imageIdx; // 32-36
	uint samplerIdx; // 36-40
	uint gradientOffset; // 40-44
	uint gradientCount; // 44-48
};

struct PSDFConstantData {
	uvec2 vertexPointer; // 0-8
	uvec2 transformPointer; // 8-16
	uvec2 shadowDataPointer; // 16-24

	float pseudoSdfInset;
	float pseudoSdfOffset;
	float pseudoSdfMax;
};

struct Vertex {
	vec4 pos;
	vec4 color;
	vec2 tex;
	uint material;
	uint object;
};

struct MaterialData {
	uint samplerImageIdx;
	uint setIdx;
	uint atlasIdx;
	uint flags;
};

struct TransformData {
	mat4 transform;
	vec4 offset;
	vec4 instanceColor;
	vec4 outlineColor;
	float shadowValue;
	float padding0;
	float outlineOffset;
	uint flags;

#ifndef SP_GLSL
	TransformData()
	: transform(mat4::IDENTITY)
	, offset(vec4(0.0f, 0.0f, 0.0f, 0.0f))
	, instanceColor(vec4::ONE)
	, outlineColor(vec4::ONE)
	, shadowValue(0.0f)
	, padding0(0.0f)
	, outlineOffset(0.0f)
	, flags(XL_GLSL_FLAG_DEFAULT) { }

	TransformData(const mat4 &m)
	: transform(m)
	, offset(vec4(0.0f, 0.0f, 0.0f, 0.0f))
	, instanceColor(vec4::ONE)
	, outlineColor(vec4::ONE)
	, shadowValue(0.0f)
	, padding0(0.0f)
	, outlineOffset(0.0f)
	, flags(XL_GLSL_FLAG_DEFAULT) { }
#endif
};

struct DataAtlasIndex {
	uint key;
	uint value;
	vec2 pos;
	vec2 tex;
};

SP_GLSL_INLINE vec4 makeMask(uint value) {
	return vec4(((value & XL_GLSL_FLAG_POSITION_MASK_X) != 0) ? 1.0 : 0.0,
			((value & XL_GLSL_FLAG_POSITION_MASK_Y) != 0) ? 1.0 : 0.0,
			((value & XL_GLSL_FLAG_POSITION_MASK_Z) != 0) ? 1.0 : 0.0,
			((value & XL_GLSL_FLAG_POSITION_MASK_W) != 0) ? 1.0 : 0.0);
}

struct FrameClipperData {
	vec2 frameSize;
	float radius;
	float shadow;
	float sigma;
	float value;
	vec2 offset;
	uint constraints;
};

#ifndef SP_GLSL
}
#endif

#endif /* XENOLITH_RENDERER_BASIC2D_GLSL_INCLUDE_XL2DGLSLDATA_H_ */
