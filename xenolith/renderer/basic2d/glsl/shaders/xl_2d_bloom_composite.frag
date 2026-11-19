#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_samplerless_texture_functions : enable

// The composite's own fragment, Vfx-2: the world's floating-point frame read same-pixel
// through an input attachment, and the bright pass's own output walked at the three fixed
// scales - eight taps a ring, the radii and weights the shared header names - each tap the
// already thresholded content the bright pass lifted, so the halo keeps the source's hue
// whatever the scale. The walk is the distortion shift's own reading shape: an
// earlier-settled attachment fetched by the coordinate's own offsets. The sum is weighted by
// the document's strength, tonemapped back into the framebuffer's own range, and handed to
// the blend units as a premultiplied pair - One, OneMinusSrcAlpha - so a pixel with no world
// in it is a no-op and the chrome the scene subpass drew stays exactly what it was

#include "sprt_glsl.h"
#include "XL2dGlslBloom.h"

layout (input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput hdrColor;
layout (set = 0, binding = 1) uniform texture2D bloomColor;

layout (location = 0) out vec4 outColor;

layout (std430, push_constant) uniform pcb {
	vec4 words; // x: the bloom strength
};

// the scales' own radii and weights, the shared header's numbers
const int radii[XL_PARTICLE_BLOOM_LEVELS] = int[XL_PARTICLE_BLOOM_LEVELS](
	XL_PARTICLE_BLOOM_RADII
);
const float levelWeights[XL_PARTICLE_BLOOM_LEVELS] = float[XL_PARTICLE_BLOOM_LEVELS](
	XL_PARTICLE_BLOOM_WEIGHTS
);

void main() {
	ivec2 coord = ivec2(gl_FragCoord.xy);
	vec4 hdr = subpassLoad(hdrColor);

	vec3 bloom = vec3(0.0);
	float alpha = 0.0;
	for (int level = 0; level < XL_PARTICLE_BLOOM_LEVELS; ++level) {
		float r = float(radii[level]);
		float weight = levelWeights[level] / float(XL_PARTICLE_BLOOM_TAPS);
		for (int k = 0; k < XL_PARTICLE_BLOOM_TAPS; ++k) {
			float angle = 0.7853981634 * float(k); // eight taps, forty-five degrees apart
			ivec2 tap = coord + ivec2(int(round(cos(angle) * r)),
					int(round(sin(angle) * r)));
			vec4 c = texelFetch(bloomColor, tap, 0);
			bloom += c.rgb * weight;
			alpha += c.a * weight;
		}
	}

	vec3 color = hdr.rgb + bloom * words.x;
	float outAlpha = min(1.0, hdr.a + alpha * words.x);
	outColor = vec4(bloomTonemap(color), outAlpha);
}
