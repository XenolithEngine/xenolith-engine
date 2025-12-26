// Наносит объекты на SDF-карту, записывает только значения SDF

#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference_uvec2 : require

#include "XL2dGlslVertexData.h"
#include "XL2dGlslShadowData.h"

#define PSEUDOSDF_MODE_SOLID 0
#define PSEUDOSDF_MODE_SDF 1
#define PSEUDOSDF_MODE_BACKREAD 2

layout (constant_id = 0) const int PSEUDOSDF_MODE = PSEUDOSDF_MODE_SOLID;

layout(buffer_reference) readonly buffer VertexBuffer;
layout(buffer_reference) readonly buffer TransformBuffer;
layout(buffer_reference) readonly buffer ShadowDataBuffer;

layout(std430, buffer_reference, buffer_reference_align = 8) readonly buffer VertexBuffer {
	Vertex vertices[];
};

layout(std430, buffer_reference, buffer_reference_align = 8) readonly buffer TransformBuffer {
	TransformData transforms[];
};

layout(std430, buffer_reference, buffer_reference_align = 8) readonly buffer ShadowDataBuffer {
	ShadowData data;
};

layout (push_constant) uniform pcb {
	PSDFConstantData pushConstants;
};

layout (location = 0) out vec4 fragColor;

void main() {
	VertexBuffer vertexBuffer = VertexBuffer(pushConstants.vertexPointer);
	TransformBuffer transformBuffer = TransformBuffer(pushConstants.transformPointer);
	ShadowDataBuffer shadowBuffer = ShadowDataBuffer(pushConstants.shadowDataPointer);

	const Vertex vertex = vertexBuffer.vertices[gl_VertexIndex];
	const uint transformIdx = vertex.material >> 16;
	const TransformData transform = transformBuffer.transforms[transformIdx];
	const TransformData instance = transformBuffer.transforms[gl_InstanceIndex];

	const vec4 pos = vertex.pos;
	const float shadowValue = max(instance.shadowValue, transform.shadowValue);

	gl_Position = transform.transform * instance.transform * pos
		* makeMask(transform.flags) * makeMask(transform.flags)
		+ transform.offset + instance.offset;
	gl_Position.z = 1.0 - shadowValue / shadowBuffer.data.maxValue;

	const float pseudoSdfInset = vertex.tex.x;
	const float pseudoSdfOffset = vertex.tex.y;

	if (PSEUDOSDF_MODE != PSEUDOSDF_MODE_SOLID) {
		const float sdfValue = (1.0 - vertex.color.a);
		const float pseudoSdfScale = (pseudoSdfInset + pseudoSdfOffset) / shadowBuffer.data.density;
		const float height = shadowBuffer.data.maxValue - shadowValue;

		float preudoSdfValue = sdfValue * pseudoSdfScale;
		if (preudoSdfValue > 0.0) {
			preudoSdfValue = preudoSdfValue;
			//preudoSdfValue = sqrt(preudoSdfValue * preudoSdfValue + height * height);
		} else {
			preudoSdfValue = preudoSdfValue;
			//preudoSdfValue = preudoSdfValue + height;
		}

		fragColor = vec4(preudoSdfValue, vertex.color.g, vertex.color.b, shadowValue);
	} else {
		fragColor = vec4(pushConstants.pseudoSdfMax, 0.0, 0.0, 1.0);
	}
}
