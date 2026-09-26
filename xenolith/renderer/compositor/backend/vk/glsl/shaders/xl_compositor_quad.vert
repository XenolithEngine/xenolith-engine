#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

// A rectangle of the output from push constants: two triangles, no vertex data.

#include "XLCompositorGlslData.h"

layout (std430, push_constant) uniform pcb {
	CompositorPlaneData data;
};

const vec2 corners[6] = {
	vec2(0.0, 0.0),
	vec2(1.0, 0.0),
	vec2(0.0, 1.0),
	vec2(1.0, 0.0),
	vec2(1.0, 1.0),
	vec2(0.0, 1.0),
};

void main() {
	vec2 pos = vec2(data.dst.xy) + corners[gl_VertexIndex % 6] * vec2(data.dst.zw);
	gl_Position = vec4(pos / vec2(data.extent) * 2.0 - 1.0, 0.0, 1.0);
}
