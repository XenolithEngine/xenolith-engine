/**
 Copyright (c) 2024 Stappler LLC <admin@stappler.dev>
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 **/

#ifndef XENOLITH_RENDERER_BASIC2D_GLSL_INCLUDE_XL2DGLSLGRADIENT_H_
#define XENOLITH_RENDERER_BASIC2D_GLSL_INCLUDE_XL2DGLSLGRADIENT_H_

vec4 applyGradient(in vec4 outColor, in uint offset, in uint count) {

#define GRAD_COLORS(n) vertexBuffer[0].vertices[offset + 2 + n].color
#define GRAD_STEPS(n) vertexBuffer[0].vertices[offset + 2 + n].pos.z
#define GRAD_FACTOR(n) vertexBuffer[0].vertices[offset + 2 + n].pos.w

	/*Vertex start = vertexBuffer[0].vertices[offset];
	Vertex end = vertexBuffer[0].vertices[offset + 1];

	float grad_loc;
	if (end.pos.x == start.pos.x) {
		grad_loc = (gl_FragCoord.y - start.pos.y) / (end.pos.y - start.pos.y);
	} else if (end.pos.y == start.pos.y) {
		grad_loc = (gl_FragCoord.x - start.pos.x) / (end.pos.x - start.pos.x);
	} else {
		float nx = (gl_FragCoord.x - start.pos.x) / (end.pos.x - start.pos.x);
		float ny = (gl_FragCoord.y - start.pos.y) / (end.pos.y - start.pos.y);

		grad_loc = nx + (ny - nx) * start.tex.y;
	}

	float gradFactor = mix(
		GRAD_FACTOR(0),
		GRAD_FACTOR(1),
		smoothstep(
			GRAD_STEPS(0),
			GRAD_STEPS(1), grad_loc ) );

	vec4 gradColor = mix(
		GRAD_COLORS(0),
		GRAD_COLORS(1),
		smoothstep(
			GRAD_STEPS(0),
			GRAD_STEPS(1), grad_loc ) );
    for ( int i = 2; i < count; ++i ) {
        gradColor = mix(
        	gradColor,
        	GRAD_COLORS(i),
        	smoothstep(
        		GRAD_STEPS(i-1),
        		GRAD_STEPS(i), grad_loc ) );
        gradFactor = mix(
        	gradFactor,
			GRAD_FACTOR(i),
        	smoothstep(
        		GRAD_STEPS(i-1),
        		GRAD_STEPS(i), grad_loc ) );
    }

    float a1 = outColor.a;
    float a2 = gradColor.a;
    outColor = outColor * gradColor * (1.0 - gradFactor) + gradFactor * (outColor * (1.0 - a2) + gradColor * a2);
    outColor.a = (1.0 - gradFactor) * a1 * a2 + gradFactor * a1;*/

#undef GRAD_COLORS
#undef GRAD_STEPS
#undef GRAD_FACTOR

	return outColor;
}

#endif /* XENOLITH_RENDERER_BASIC2D_GLSL_INCLUDE_XL2DGLSLGRADIENT_H_ */
