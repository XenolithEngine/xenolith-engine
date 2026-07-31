#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference_uvec2 : require

#ifndef SP_GLSL
#define SP_GLSL
#endif

// Lightweight counterpart of xl_2d_material.vert for the flat render queue:
// no shadow output, no outline, and a single transform lookup (see below).

// clang-format off

#include "sprt_glsl.h"
#include "XL2dGlslVertexData.h"

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

	// xl_2d_material.vert reads two transforms and multiplies them, but exactly one of
	// them is always transforms[0] (the identity nullTransform written by pushInitial):
	//   packed draws    - the real index is baked into (vertex.material >> 16), and
	//                     instanceCount == 1 / firstInstance == 0, so gl_InstanceIndex == 0
	//   instanced draws - (vertex.material >> 16) == 0, and gl_InstanceIndex carries the index
	// So adding the two indexes selects the right transform in both cases with a single load.
	const TransformData transform =
		transformBuffer.transforms[(vertex.material >> 16) + gl_InstanceIndex];

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

	gl_Position = (transform.transform * pos * makeMask(transform.flags)) + transform.offset;
	fragColor = color * transform.instanceColor;
	fragTexCoord = vec4(tex, layer, 0.0);
}
