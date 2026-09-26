#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

// A plane copied 1:1: texelFetch, no filtering. The output is premultiplied, as in blendPlanes.

#include "XLCompositorGlslData.h"

layout (set = 0, binding = 0) uniform sampler2D planes[8];

layout (std430, push_constant) uniform pcb {
	CompositorPlaneData data;
};

layout (location = 0) out vec4 outColor;

void main() {
	ivec2 p = ivec2(gl_FragCoord.xy) - ivec2(data.dst.xy) + ivec2(data.srcOrigin);

	// Constant indexes: dynamic indexing of a sampler array is an optional device feature.
	vec4 c;
	switch (data.index) {
	case 0: c = texelFetch(planes[0], p, 0); break;
	case 1: c = texelFetch(planes[1], p, 0); break;
	case 2: c = texelFetch(planes[2], p, 0); break;
	case 3: c = texelFetch(planes[3], p, 0); break;
	case 4: c = texelFetch(planes[4], p, 0); break;
	case 5: c = texelFetch(planes[5], p, 0); break;
	case 6: c = texelFetch(planes[6], p, 0); break;
	default: c = texelFetch(planes[7], p, 0); break;
	}

	if (data.opaque != 0) {
		c.a = 1.0;
	}
	outColor = c * data.alpha;
}
