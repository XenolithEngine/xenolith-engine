#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

// The bright pass's own fragment, Vfx-2: the world's floating-point frame read same-pixel
// through an input attachment, the share above the threshold kept and scaled onto every
// channel alike so the halo keeps the source's hue. The target it writes is premultiplied,
// the alpha scaled by the same factor the color is

#include "sprt_glsl.h"
#include "XL2dGlslBloom.h"

layout (input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput hdrColor;

layout (location = 0) out vec4 outColor;

layout (std430, push_constant) uniform pcb {
	float threshold;
};

void main() {
	vec4 c = subpassLoad(hdrColor);
	float k = bloomBrightFactor(c.rgb, threshold);
	outColor = vec4(c.rgb * k, c.a * k);
}
