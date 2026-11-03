#version 450
#extension GL_GOOGLE_include_directive : enable

// The pass's interface, the varyings and the material, Lambert3d, from basic3d.kdl; every slot of
// the texture arrays is written, the unused ones with an empty image
#include "xl_3d_mesh.frag.glsl"

void main() {
	Lambert3d m = material();
	ViewData3d v = view();

	vec4 base = m.color;
	if (m.texture != XL_GLSL_3D_NO_TEXTURE) {
		base *= texture(textures[nonuniformEXT(m.texture)], vary.texCoord);
	}

	vec3 n = normalize(vary.normal);
	float lambert = max(dot(n, v.sunDirection.xyz), 0.0);
	vec3 light = v.ambient.rgb + v.sunColor.rgb * lambert;

	// the point lights, each falling off as (1 - (d / r)^2)^2 to zero at its radius
	for (uint i = 0; i < v.lightCount; ++i) {
		LightData3d l = lights(v.firstLight + i);
		vec3 toLight = l.position.xyz - vary.world;
		float dist = length(toLight);
		float k = clamp(1.0 - (dist * dist) / (l.position.w * l.position.w), 0.0, 1.0);
		light += l.color.rgb * (k * k) * max(dot(n, toLight / max(dist, 1.0e-4)), 0.0);
	}

	outColor = vec4(base.rgb * light, base.a);
}
