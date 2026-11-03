#version 450
#extension GL_GOOGLE_include_directive : enable

// The pass's interface, the varyings and the material, Lambert3d, from basic3d.kdl
#include "xl_3d_mesh.vert.glsl"

void main() {
	// gl_VertexIndex includes the draw's vertexOffset
	Vertex3d v = vertices(gl_VertexIndex);
	DrawData3d d = draws(constants.drawIndex);

	vary.normal = (d.normal * vec4(v.normal.xyz, 0.0)).xyz;
	vary.texCoord = v.attr.xy * material().uvScale;

	vec4 world = d.model * vec4(v.pos.xyz, 1.0);
	vary.world = world.xyz;
	gl_Position = view().viewProj * world;
}
