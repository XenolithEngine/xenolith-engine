// Наносит затенение на основе SDF-карты

#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "XL2dGlslVertexData.h"

layout (location = 0) out vec2 fragTexCoord;

const vec4 positions[4] = {
	vec4(-1.0, -1.0, 0.0, 1.0),
	vec4(-1.0, 1.0, 0.0, 1.0),
	vec4(1.0, 1.0, 0.0, 1.0),
	vec4(1.0, -1.0, 0.0, 1.0),
};

const vec2 texs[4] = {
	vec2(0.0f, 0.0),
	vec2(0.0, 1.0),
	vec2(1.0, 1.0),
	vec2(1.0, 0.0),
};

void main() {
	gl_Position = positions[gl_VertexIndex % 4];
	fragTexCoord = texs[gl_VertexIndex % 4];
}
