#version 450
#extension GL_GOOGLE_include_directive : enable

// A translucent pipeline: a tinted glass, as clear as the depth behind it is near. The opaque draws'
// depth comes as an input attachment; the camera is orthographic, so its difference times the
// depth range (params[0].x) is how far the view goes through the glass. Its colour is params[1],
// its alpha 1 - exp(-params[0].y * that distance).
#include "basic3d_glass.frag.glsl"

void main() {
	DrawData3d d = draws(constants.drawIndex);

	float through = max(subpassLoad(sceneDepth).r - gl_FragCoord.z, 0.0) * d.params[0].x;
	outColor = vec4(d.params[1].rgb, 1.0 - exp(-d.params[0].y * through));
}
