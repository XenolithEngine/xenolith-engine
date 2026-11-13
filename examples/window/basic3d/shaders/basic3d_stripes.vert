#version 450
#extension GL_GOOGLE_include_directive : enable

// An app pipeline of the example: the pass's interface, its varyings and its vertex format,
// Compact3d, from basic3d-example.kdl
#include "basic3d_stripes.vert.glsl"

void main() {
	Compact3dVertex v = vertex(gl_VertexIndex);
	DrawData3d d = draws(constants.drawIndex);

	vec4 world = d.model * vec4(v.pos, 1.0);
	vary.normal = (d.normal * vec4(v.normal, 0.0)).xyz;
	vary.world = world.xyz;

	gl_Position = view().viewProj * world;
}
