#version 450

// Three boxes built from gl_VertexIndex and gl_InstanceIndex, no vertex buffers: a tall box, a
// spinning cube that cuts through it, and a ground slab. The ground is drawn last, so without the
// depth test it would cover both boxes.

layout (push_constant) uniform SceneConstants {
	mat4 viewProj;
	vec4 params; // x - seconds
} pc;

layout (location = 0) out vec3 fragColor;

const vec3 corners[8] = vec3[8](vec3(-1.0, -1.0, -1.0), vec3(1.0, -1.0, -1.0),
		vec3(1.0, 1.0, -1.0), vec3(-1.0, 1.0, -1.0), vec3(-1.0, -1.0, 1.0), vec3(1.0, -1.0, 1.0),
		vec3(1.0, 1.0, 1.0), vec3(-1.0, 1.0, 1.0));

// six faces, two triangles each: -z, +z, -y, +y, -x, +x
const int indices[36] = int[36](0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7, 0, 1, 5, 0, 5, 4, 3, 7, 6, 3,
		6, 2, 0, 4, 7, 0, 7, 3, 1, 2, 6, 1, 6, 5);

const vec3 normals[6] = vec3[6](vec3(0.0, 0.0, -1.0), vec3(0.0, 0.0, 1.0), vec3(0.0, -1.0, 0.0),
		vec3(0.0, 1.0, 0.0), vec3(-1.0, 0.0, 0.0), vec3(1.0, 0.0, 0.0));

const vec3 scales[3] = vec3[3](vec3(0.5, 0.5, 1.8), vec3(1.0, 1.0, 1.0), vec3(6.0, 6.0, 0.05));
const vec3 offsets[3] = vec3[3](vec3(0.9, 0.5, 1.8), vec3(0.0, 0.0, 1.2), vec3(0.0, 0.0, -0.05));
const vec3 colors[3] = vec3[3](vec3(0.3, 0.5, 0.85), vec3(0.85, 0.42, 0.3), vec3(0.42, 0.55, 0.33));

void main() {
	int inst = gl_InstanceIndex;
	vec3 p = corners[indices[gl_VertexIndex]] * scales[inst];
	vec3 n = normals[gl_VertexIndex / 6];

	if (inst == 1) {
		float a = pc.params.x * 0.8;
		mat3 rz = mat3(cos(a), sin(a), 0.0, -sin(a), cos(a), 0.0, 0.0, 0.0, 1.0);
		float b = 0.5;
		mat3 rx = mat3(1.0, 0.0, 0.0, 0.0, cos(b), sin(b), 0.0, -sin(b), cos(b));
		p = rz * rx * p;
		n = rz * rx * n;
	}

	p += offsets[inst];

	vec3 sun = normalize(vec3(-0.4, 0.3, 0.85));
	float light = 0.35 + 0.65 * max(dot(n, sun), 0.0);
	fragColor = colors[inst] * light;

	gl_Position = pc.viewProj * vec4(p, 1.0);
}
