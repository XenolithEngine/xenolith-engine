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

// The tonemap the composite returns the frame with, per channel; the white core may clip,
// the dim end stays linear
SP_GLSL_INLINE vec3 bloomTonemap(vec3 c) {
	float w = XL_PARTICLE_BLOOM_WHITE;
	return c * (1.0 + c / (w * w)) / (1.0 + c);
}

#ifndef SP_GLSL
}
#endif

#endif

