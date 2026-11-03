#version 450
#extension GL_GOOGLE_include_directive : enable

// A translucent pipeline: a tinted glass, as clear as the depth behind it is near. The opaque draws'
// depth comes as an input attachment; the camera is orthographic, so its difference times the
// material's depthRange is how far the view goes through the glass. Its colour is the material's,
// its alpha 1 - exp(-density * that distance).
#include "basic3d_glass.frag.glsl"

void main() {
	Glass3d m = material();

	float through = max(subpassLoad(sceneDepth).r - gl_FragCoord.z, 0.0) * m.depthRange;
	outColor = vec4(m.color.rgb, 1.0 - exp(-m.density * through));
}
