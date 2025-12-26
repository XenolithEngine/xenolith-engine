// Наносит объекты на SDF-карту, записывает только значения SDF

#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "XL2dGlslVertexData.h"

#define PSEUDOSDF_MODE_SOLID 0
#define PSEUDOSDF_MODE_SDF 1
#define PSEUDOSDF_MODE_BACKREAD 2

layout (constant_id = 0) const int PSEUDOSDF_MODE = PSEUDOSDF_MODE_SOLID;

layout (location = 0) in vec4 fragColor;
layout (location = 0) out vec4 outColor;

layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput inputSdf;

void main() {
	if (PSEUDOSDF_MODE != PSEUDOSDF_MODE_BACKREAD) {
		outColor = fragColor;
	} else {
		const float value = subpassLoad(inputSdf).r;
		if (abs(value - fragColor.r) < 0.1) {
			outColor = fragColor;
		} else {
			discard;
		}
	}
}
