#version 450
#extension GL_GOOGLE_include_directive : enable

// Stripes across the world's x, half a unit wide, in the material's two colours; with
// STRIPES_SINGLE the first colour alone. The second texture, when the material has one, takes the
// place of the second colour, laid over the world's xy, and the first texture that of the first;
// with STRIPES_LAYERED the layered one does, its layer the material's
#include "basic3d_stripes.frag.glsl"

void main() {
	Stripes3d m = material();
	ViewData3d v = view();

	bool second = (m.flags & BASIC3D_EXAMPLE_STRIPES_SINGLE) == 0u && fract(vary.world.x) >= 0.5;
	vec4 base = second ? m.second : m.first;
	if (second && m.secondTexture != XL_GLSL_3D_NO_TEXTURE) {
		base = texture(textures[nonuniformEXT(m.secondTexture)], vary.world.xy * 0.5);
	}
	if (!second && m.firstTexture != XL_GLSL_3D_NO_TEXTURE) {
		base = texture(textures[nonuniformEXT(m.firstTexture)], vary.world.xy * 0.5);
	}
	if (second && (m.flags & BASIC3D_EXAMPLE_STRIPES_LAYERED) != 0u
			&& m.layers != XL_GLSL_3D_NO_TEXTURE) {
		base = texture(textureArrays[nonuniformEXT(m.layers)], vec3(vary.world.xy * 0.5, m.layer));
	}

	vec3 n = normalize(vary.normal);
	float lambert = max(dot(n, v.sunDirection.xyz), 0.0);
	vec3 light = v.ambient.rgb + v.sunColor.rgb * lambert;

	outColor = vec4(base.rgb * light, base.a);
}
