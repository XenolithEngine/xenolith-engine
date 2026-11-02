#version 450
#extension GL_GOOGLE_include_directive : enable

// Stripes across the world's x, half a unit wide, in the material's two params colours; with
// flag 1 the first colour alone. A second texture, when the material has one, takes the place of
// the second colour, laid over the world's xy; with flag 2 the material's texture does, a layered
// one, its layer params[2].x
#include "basic3d_stripes.frag.glsl"

void main() {
	DrawData3d d = draws(constants.drawIndex);
	ViewData3d v = view();

	bool second = (d.flags & 1u) == 0u && fract(vary.world.x) >= 0.5;
	vec4 base = second ? d.params[1] : d.params[0];
	if (second && d.textureIdx2 != XL_GLSL_3D_NO_TEXTURE) {
		base = texture(textures[nonuniformEXT(d.textureIdx2)], vary.world.xy * 0.5);
	}
	if (!second && d.textureIdx3 != XL_GLSL_3D_NO_TEXTURE) {
		base = texture(textures[nonuniformEXT(d.textureIdx3)], vary.world.xy * 0.5);
	}
	if (second && (d.flags & 2u) != 0u && d.textureIdx != XL_GLSL_3D_NO_TEXTURE) {
		base = texture(textureArrays[nonuniformEXT(d.textureIdx)],
				vec3(vary.world.xy * 0.5, d.params[2].x));
	}

	vec3 n = normalize(vary.normal);
	float lambert = max(dot(n, v.sunDirection.xyz), 0.0);
	vec3 light = v.ambient.rgb + v.sunColor.rgb * lambert;

	outColor = vec4(base.rgb * light, base.a);
}
