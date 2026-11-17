#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

// The particle draw's own fragment: the quad's alpha comes from its UV instead of a sampled
// texture, so a procedural mask stays sharp at any zoom. The math is the pixel math of the
// window example's baked rasters (examples/window/particles ParticleTextures.cpp) made
// continuous: disc - smoothstep over the free distance to the inscribed circle, streak - the
// same distance squared (the quad's own aspect carries the elongation), ring - a band of
// `thickness` under the inscribed circle with both edges eased by `softness`.

// clang-format off

#include "sprt_glsl.h"
#include "XL2dGlslVertexData.h"

layout (location = 0) in vec4 fragColor;
layout (location = 1) in vec4 fragTexCoord;

layout (location = 0) out vec4 outColor;
layout (location = 1) out vec4 outShadow;

layout (std430, push_constant) uniform pcb {
	ParticleDrawConstantData pushConstants;
};

void main() {
	// the mask's own coordinates are [-1, 1] around the quad's center
	vec2 p = fragTexCoord.xy * 2.0 - 1.0;
	float d = length(p);

	float alpha = 1.0;
	const uint shape = uint(pushConstants.mask.x);
	if (shape == 1u) {
		float t = clamp(1.0 - d, 0.0, 1.0);
		alpha = t * t * (3.0 - 2.0 * t);
	} else if (shape == 2u) {
		float inner = 1.0 - pushConstants.mask.y;
		float softness = max(pushConstants.mask.z, 1.0e-4);
		alpha = clamp((d - inner) / softness, 0.0, 1.0)
				* clamp((1.0 - d) / softness, 0.0, 1.0);
	} else if (shape == 3u) {
		float t = clamp(1.0 - d, 0.0, 1.0);
		alpha = t * t;
	}

	outColor = vec4(fragColor.rgb, fragColor.a * alpha);
	outShadow = vec4(0.0);
}
