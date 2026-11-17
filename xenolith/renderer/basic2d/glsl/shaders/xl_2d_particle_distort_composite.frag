#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

// The distortion draw's own fragment, the second half of the pair: the same pixel of the
// intermediate the first half wrote is read back through an input attachment - the SDF
// backread's own shape - and handed to the blend units, whose factors the emitter's blend
// mode named when the pipeline was picked. The alpha baked into the intermediate is the
// particle's coverage, so a zero-coverage pixel is a no-op under every registered pair.

layout (input_attachment_index = 0, set = 0, binding = 1) uniform subpassInput distortTarget;

layout (location = 0) out vec4 outColor;

void main() {
	outColor = subpassLoad(distortTarget);
}
