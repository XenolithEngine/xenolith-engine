#version 450
#extension GL_GOOGLE_include_directive : enable

// An app pipeline of the example: the pass's interface and its varyings, from basic3d-example.kdl
#include "basic3d_stripes.vert.glsl"

void main() {
	Vertex3d v = vertices(gl_VertexIndex);
	DrawData3d d = draws(constants.drawIndex);

	vec4 world = d.model * vec4(v.pos.xyz, 1.0);
	vary.normal = (d.normal * vec4(v.normal.xyz, 0.0)).xyz;
	vary.world = world.xyz;

	gl_Position = view().viewProj * world;
}
