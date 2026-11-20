#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_samplerless_texture_functions : enable

// The distortion draw's own fragment, the first half of the pair: the frame the earlier
// subpasses settled is fetched at a coordinate the particle's own color names, and the
// fetched value lands in the intermediate with the particle's coverage as its alpha. The
// composite half reads the intermediate back same-pixel and blends it into the frame, so
// the displaced fetches never read what this subpass writes. The shift is frame pixels,
// mid-gray the no-shift seat; the mask math is the particle fragment's own
// (xl_2d_particle.frag), carried here because the distorted draw does not run that fragment.

#include "sprt_glsl.h"
#include "XL2dGlslVertexData.h"

layout (set = 0, binding = 0) uniform texture2D frameColor;

layout (location = 0) in vec4 fragColor;
layout (location = 1) in vec4 fragTexCoord;

layout (location = 0) out vec4 outColor;

layout (std430, push_constant) uniform pcb {
	ParticleDrawConstantData pushConstants;
};

// the mask's alpha, the pixel math xl_2d_particle.frag carries: the shape sits in mask.x,
// the ring's thickness and softness in mask.y and mask.z
float particleMaskAlpha(vec2 uv, vec4 maskWords) {
	// the mask's own coordinates are [-1, 1] around the quad's center
	vec2 p = uv * 2.0 - 1.0;
	float d = length(p);

	const uint shape = uint(maskWords.x);
	if (shape == 1u) {
		float t = clamp(1.0 - d, 0.0, 1.0);
		return t * t * (3.0 - 2.0 * t);
	} else if (shape == 2u) {
		float inner = 1.0 - maskWords.y;
		float softness = max(maskWords.z, 1.0e-4);
		return clamp((d - inner) / softness, 0.0, 1.0)
				* clamp((1.0 - d) / softness, 0.0, 1.0);
	} else if (shape == 3u) {
		float t = clamp(1.0 - d, 0.0, 1.0);
		return t * t;
	}
	return 1.0;
}

void main() {
	// the frame's own pixels, half the color's range aside the middle: the shift the color
	// names, zero at mid-gray. The pixel reach is the emitter's own scale word riding the
	// mask push's free tail, Horizon-2's H3 - sixteen the fresh emitter's default, the reach
	// every distortion document drew before the word existed
	float distortPixels = pushConstants.mask.w;
	ivec2 shift = ivec2(round((fragColor.rg - vec2(0.5)) * 2.0 * distortPixels));
	ivec2 coord = ivec2(gl_FragCoord.xy) + shift;
	vec3 displaced = texelFetch(frameColor, coord, 0).rgb;

	outColor = vec4(displaced, fragColor.a * particleMaskAlpha(fragTexCoord.xy, pushConstants.mask));
}
