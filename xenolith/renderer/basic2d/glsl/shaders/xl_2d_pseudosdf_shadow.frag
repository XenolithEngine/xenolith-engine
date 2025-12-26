// Наносит затенение на основе SDF-карты

#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_buffer_reference : require

#include "XL2dGlslVertexData.h"
#include "XL2dGlslShadowData.h"
#include "XL2dGlslSdfData.h"

layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput inputDepth;
layout(input_attachment_index = 1, set = 0, binding = 1) uniform subpassInput inputSdf;

layout (location = 0) in vec2 fragTexCoord;
layout (location = 0) out vec4 outColor;

layout(std430, buffer_reference, buffer_reference_align = 8) readonly buffer ShadowDataBuffer {
	ShadowData data;
};

layout (push_constant) uniform pcb {
	ShadowDataBuffer shadowData;
};

#define M_1_PI 0.318309886183790671538
#define GAUSSIAN_CONST -6.2383246250

vec2 project(in vec2 target, in vec2 norm) {
	return norm * (dot(target, norm) / dot(norm, norm));
}

void main() {
	const vec4 sdfSource = subpassLoad(inputSdf);
	const float depth = subpassLoad(inputDepth).r;

	outColor = vec4(1.0, 0.0, 1.0, 1.0);
	
	if (sdfSource.x > 200.0) {
		outColor = shadowData.data.discardColor;
	}

	if (sdfSource.w <= depth) {
		outColor = shadowData.data.discardColor;
		//outColor = vec4(1.0, 0.0, 1.0, 1.0);
		return;
	}

	// Направление нормали в SDF
	const vec2 norm = sdfSource.yz;

	vec4 textureColor = shadowData.data.globalColor;
	for (uint i = 0; i < shadowData.data.ambientLightCount; ++ i) {
		// Расстояние по высоте от текущего фрагмента до источника SDF
		const float dh = sdfSource.w - depth;

		const float offset = dot(shadowData.data.ambientLights[i].normal.xy * dh, norm);

		// Значение SDF
		const float v = sdfSource.x;

		// Значение SDF в плоскости объекта
		const float d = v * mix( 0.8, 1.0, clamp( dh / 40.0f, 0.0, 1.0))
			* mix( 0.2, 1.0, clamp( dh / 4.0f, 0.0, 1.0))
			+ 0.25
			// + 0.4 * clamp(1.0 - dh / 3.0f, 0.0, 0.5)
			+ offset;

		const float k = shadowData.data.ambientLights[i].normal.w;

		// Радиус конуса выбора цвета для плоскости объекта
		const float R = dh * k;

		if (d < R) {
			// Полуугол незасвеченного сектора
			const float alpha = 2.0 * acos(d / R);

			// Отноление площади незасвеченного сектора к площади сечения конуса
			// Для реальных данных коэффициент будет 0.5, параметры распределения подобраны эмпирически
			const float S = 1.0 - (alpha - sin(alpha)) * M_1_PI * mix(0.85, 0.7, clamp(k / 2.0, 0.0, 1.0));

			textureColor += shadowData.data.ambientLights[i].color
				* (shadowData.data.ambientLights[i].color.a
				//* S
				* (1.0 - exp(S * S * S * GAUSSIAN_CONST))
				* shadowData.data.luminosity);
		} else {
			textureColor += shadowData.data.ambientLights[i].color
				* (shadowData.data.ambientLights[0].color.a * shadowData.data.luminosity);
		}
	}

	outColor = vec4(textureColor.xyz, 1.0);
}
