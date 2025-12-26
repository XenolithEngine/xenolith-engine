#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference_uvec2 : require

#ifndef SP_GLSL
#define SP_GLSL
#endif

#include "SPGlslInit.h"
#include "XL2dGlslVertexData.h"

layout (constant_id = 0) const int BUFFERS_ARRAY_SIZE = 8;

layout(buffer_reference) readonly buffer VertexBuffer;
layout(buffer_reference) readonly buffer TransformBuffer;
layout(buffer_reference) readonly buffer MaterialDataBuffer;
layout(buffer_reference) readonly buffer DataAtlasBuffer;

layout(std430, buffer_reference, buffer_reference_align = 8) readonly buffer VertexBuffer {
	Vertex vertices[];
};

layout(std430, buffer_reference, buffer_reference_align = 8) readonly buffer TransformBuffer {
	TransformData transforms[];
};

layout(std430, buffer_reference, buffer_reference_align = 8) readonly buffer MaterialDataBuffer {
	MaterialData m;
};

layout(std430, buffer_reference, buffer_reference_align = 8) readonly buffer DataAtlasBuffer {
	DataAtlasIndex indexes[];
};

layout (std430, push_constant) uniform pcb {
	VertexConstantData pushConstants;
};

layout (location = 0) out vec4 fragColor;
layout (location = 1) out vec4 fragTexCoord;
layout (location = 2) out vec4 shadowColor;
layout (location = 3) out vec4 outlineColor;
layout (location = 4) out float outlineOffset;
layout (location = 5) out vec2 fragPosition;
layout (location = 6) out flat uvec4 texOut;

uint hash(uint k, uint capacity) {
	k ^= k >> 16;
	k *= 0x85ebca6b;
	k ^= k >> 13;
	k *= 0xc2b2ae35;
	k ^= k >> 16;
	return k & (capacity - 1);
}

void main() {
	VertexBuffer vertexBuffer = VertexBuffer(pushConstants.vertexPointer);
	TransformBuffer transformBuffer = TransformBuffer(pushConstants.transformPointer);
	MaterialDataBuffer materialBuffer = MaterialDataBuffer(pushConstants.materialPointer);

	const Vertex vertex = vertexBuffer.vertices[gl_VertexIndex];
	const TransformData transform = transformBuffer.transforms[vertex.material >> 16];
	const TransformData instance = transformBuffer.transforms[gl_InstanceIndex];

	vec4 pos = vertex.pos;
	vec4 color = vertex.color;
	vec2 tex = vertex.tex;

	if (vertex.object != 0 && (materialBuffer.m.flags & XL_GLSL_MATERIAL_FLAG_HAS_ATLAS) != 0) {
		uint size = 1 << (materialBuffer.m.flags >> XL_GLSL_MATERIAL_FLAG_ATLAS_POW2_INDEX_BIT_OFFSET);
		uint slot = hash(vertex.object, size);
		uint counter = 0;

		DataAtlasBuffer bufferPointer = DataAtlasBuffer(pushConstants.atlasPointer);
		DataAtlasIndex prev;

		while (counter < size) {
			prev = bufferPointer.indexes[slot];
			if (prev.key == vertex.object) {
				pos += vec4(prev.pos, 0, 0);
				tex = prev.tex;
				break;
			} else if (prev.key == uint(0xffffffff)) {
				color = vec4(1, 0, 0, 1);
				break;
			}
			slot = (slot + 1) & (size - 1);
			++ counter;
		}

		if (counter == size) {
			color = vec4(0, 1, 0, 1);
		}
	}

	float layer = pos.z;

	gl_Position = (transform.transform * instance.transform
			* pos * makeMask(transform.flags) * makeMask(transform.flags))
		+ transform.offset + instance.offset;
	fragPosition = gl_Position.xy;
	fragColor = color * transform.instanceColor * instance.instanceColor;
	fragTexCoord = vec4(tex, layer, 0.0);
	shadowColor = vec4(transform.shadowValue.xxx, transform.shadowValue > 0.0 ? 1.0 : 0.0);
	outlineColor = transform.outlineColor * instance.outlineColor;
	outlineOffset = max(transform.outlineOffset, instance.outlineOffset);

	texOut = uvec4(materialBuffer.m.samplerImageIdx & 0xFFFF, (materialBuffer.m.samplerImageIdx >> 16) & 0xFFFF, 0, 0);
}
