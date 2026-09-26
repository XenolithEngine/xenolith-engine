#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

// The background: a premultiplied colour.

#include "XLCompositorGlslData.h"

layout (std430, push_constant) uniform pcb {
	CompositorPlaneData data;
};

layout (location = 0) out vec4 outColor;

void main() {
	outColor = data.color;
}
