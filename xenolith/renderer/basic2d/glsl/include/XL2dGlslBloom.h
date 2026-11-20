#ifndef XENOLITH_RENDERER_BASIC2D_GLSL_INCLUDE_XL2DGLSLBLOOM_H_
#define XENOLITH_RENDERER_BASIC2D_GLSL_INCLUDE_XL2DGLSLBLOOM_H_

/* The HDR post chain's own numbers, compiled both by the bloom fragments and by C++
(tests/particles), so the CPU reference runs the same text.

Every constant here is fixed and deterministic - no time, no grain, no per-frame variation: the
document names only the threshold and the strength, the chain's own shape is the model's. */

#include "sprt_glsl.h"

#ifndef SP_GLSL
namespace STAPPLER_VERSIONIZED stappler::glsl {

using namespace sprt::glsl;

#endif

// The tonemap's white point, the value the extended Reinhard curve maps to one. The curve the
// composite returns the frame with: c * (1 + c / W^2) / (1 + c) per channel - linear at the
// dim end, compressing the HDR range above one into the framebuffer's own [0, 1]
#define XL_PARTICLE_BLOOM_WHITE 4.0

// The chain's own shape: three scales, the composite's kernel walking eight taps of the
// bright pass's own output per scale at these radii (in texels), the levels' weights fixed
// and summing to one - so the kernel is normalized whatever the source. The constants are
// the shader's and the stand's both, one definition
#define XL_PARTICLE_BLOOM_LEVELS 3
#define XL_PARTICLE_BLOOM_TAPS 8
#define XL_PARTICLE_BLOOM_RADII 3, 9, 27
#define XL_PARTICLE_BLOOM_WEIGHTS 0.45, 0.33, 0.22

// The luma weights the threshold reads, Rec. 709
SP_GLSL_INLINE float bloomLuma(vec3 c) {
	return dot(c, vec3(0.2126, 0.7152, 0.0722));
}

// The bright pass's own scale: the share of the pixel's color that sits above the threshold,
// applied to every channel alike so the halo keeps the source's hue - a red source keeps its
// red ratio whatever the threshold trims. A pixel at or below the threshold answers zero
SP_GLSL_INLINE float bloomBrightFactor(vec3 c, float threshold) {
	float luma = bloomLuma(c);
	if (luma <= threshold || luma <= 0.0) {
		return 0.0;
	}
	return (luma - threshold) / luma;
}

// The tonemap the composite returns the frame with, Horizon-2's hue-preserving shape: the
// extended Reinhard curve rides the pixel's LUMA and its answer rescales every channel by the
// same ratio, so a mixed color - a ribbon's gold, a dusk gradient - keeps its channel ratios
// whatever the curve compresses. The per-channel curve this replaces answered a mid-luminance
// blue (0.35, 0.7, 1.0) with ratios crushed toward gray, each channel compressed by its own
// factor; the luma curve answers the same blue with its own ratios intact. Gray inputs read
// the same numbers the per-channel curve always did - a gray pixel's luma is the pixel itself -
// and the white point, the dim-end linearity and the monotonic climb stand as they were; a
// pure hue stays pure the same way, all its energy one luma seat
SP_GLSL_INLINE vec3 bloomTonemap(vec3 c) {
	float w = XL_PARTICLE_BLOOM_WHITE;
	float luma = bloomLuma(c);
	if (luma <= 0.0) {
		return vec3(0.0, 0.0, 0.0);
	}
	float mapped = luma * (1.0 + luma / (w * w)) / (1.0 + luma);
	return c * (mapped / luma);
}

#ifndef SP_GLSL
}
#endif

#endif

