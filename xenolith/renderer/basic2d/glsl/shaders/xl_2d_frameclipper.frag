#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_buffer_reference : require

layout (location = 0) in vec2 fragTexCoord;
layout (location = 0) out vec4 outColor;

#include "SPGlslInit.h"
#include "XL2dGlslVertexData.h"

layout (std430, push_constant) uniform pcb {
	FrameClipperData data;
};

void main() {
	vec2 normCoord = fragTexCoord - 0.5;
	vec2 s = sign(normCoord);
	vec2 pos = data.frameSize * abs(normCoord);

	/*
	Top = 1 << 0,
	Left = 1 << 1,
	Bottom = 1 << 2,
	Right = 1 << 3,
	Transparent = 1 << 4,
	*/

	if (s.x > 0.0 && s.y > 0.0) {
		if ((data.constraints & (1 << 0)) != 0 || (data.constraints & (1 << 3)) != 0) {
			outColor = 1.0.xxxx;
			return;
		}
	} else if (s.x < 0.0 && s.y > 0.0) {
		if ((data.constraints & (1 << 0)) != 0 || (data.constraints & (1 << 1)) != 0) {
			outColor = 1.0.xxxx;
			return;
		}
	} else if (s.x > 0.0 && s.y < 0.0) {
		if ((data.constraints & (1 << 2)) != 0 || (data.constraints & (1 << 3)) != 0) {
			outColor = 1.0.xxxx;
			return;
		}
	} else if (s.x < 0.0 && s.y < 0.0) {
		if ((data.constraints & (1 << 2)) != 0 || (data.constraints & (1 << 1)) != 0) {
			outColor = 1.0.xxxx;
			return;
		}
	}

	vec2 d = (data.frameSize / 2.0 - pos);
	if (d.x < data.radius && d.y < data.radius) {
		float dist = length(data.radius.xx - d);
		if (dist > data.radius) {
			if ((data.constraints & (1 << 4)) == 0) {
				outColor = 0.0.xxxx;
			} else {
				dist = length(data.radius.xx - d - data.offset * s);
				// correct subpixel position by 0.5
				// WS operates in pixels, but Vulkan operates with pixel midpoints
				dist = dist - data.radius - 0.5;
				outColor = vec4(0.0, 0.0, 0.0, exp((dist * dist) * data.sigma) * data.value);
			}
		} else {
			outColor = 1.0.xxxx;
		}
	} else {
		outColor = 1.0.xxxx;
	}
}
