#version 120

uniform int modificationType;
uniform int multiplyAlpha;
uniform int blurFactor;
uniform int dimension;
uniform vec4 darkenHue;
uniform vec4 greyscaleHue;
uniform vec4 replaceSrcColor;
uniform vec4 replaceDstColor;

uniform sampler2D tex;

varying /* PRAGMA: ONS_RU highprecision */ vec2 texCoord;

void main(void) {
	vec4 color = vec4(0.0);
	float blurSize;
	float grey;
	if (modificationType == 0) {
		color = texture2D(tex,texCoord.st);
	} else if (modificationType == 1) { // Sepia
		color = texture2D(tex,texCoord.st);
		grey = dot(color.rgb, vec3(0.299, 0.587, 0.114));
		if (color.a != 0.0)
			color = vec4(grey * vec3(1.2, 1.0, 0.8), color.a);
	} else if (modificationType == 2) { // Horizontal blur shader
		blurSize = 1.0/float(dimension);
		color += texture2D(tex, vec2(texCoord.s - 4.0*blurSize, texCoord.t)) * 0.05;
		color += texture2D(tex, vec2(texCoord.s - 3.0*blurSize, texCoord.t)) * 0.09;
		color += texture2D(tex, vec2(texCoord.s - 2.0*blurSize, texCoord.t)) * 0.12;
		color += texture2D(tex, vec2(texCoord.s - blurSize, texCoord.t)) * 0.15;
		color += texture2D(tex, vec2(texCoord.s, texCoord.t)) * 0.16;
		color += texture2D(tex, vec2(texCoord.s + blurSize, texCoord.t)) * 0.15;
		color += texture2D(tex, vec2(texCoord.s + 2.0*blurSize, texCoord.t)) * 0.12;
		color += texture2D(tex, vec2(texCoord.s + 3.0*blurSize, texCoord.t)) * 0.09;
		color += texture2D(tex, vec2(texCoord.s + 4.0*blurSize, texCoord.t)) * 0.05;
	} else if (modificationType == 3) { // Vertical blur shader
		blurSize = 1.0/float(dimension);
		color += texture2D(tex, vec2(texCoord.s, texCoord.t - 4.0*blurSize)) * 0.05;
		color += texture2D(tex, vec2(texCoord.s, texCoord.t - 3.0*blurSize)) * 0.09;
		color += texture2D(tex, vec2(texCoord.s, texCoord.t - 2.0*blurSize)) * 0.12;
		color += texture2D(tex, vec2(texCoord.s, texCoord.t - blurSize)) * 0.15;
		color += texture2D(tex, vec2(texCoord.s, texCoord.t)) * 0.16;
		color += texture2D(tex, vec2(texCoord.s, texCoord.t + blurSize)) * 0.15;
		color += texture2D(tex, vec2(texCoord.s, texCoord.t + 2.0*blurSize)) * 0.12;
		color += texture2D(tex, vec2(texCoord.s, texCoord.t + 3.0*blurSize)) * 0.09;
		color += texture2D(tex, vec2(texCoord.s, texCoord.t + 4.0*blurSize)) * 0.05;
	} else if (modificationType == 4) { // Greyscale
		color = texture2D(tex,texCoord.st);
		grey = dot(color.rgb, vec3(0.299, 0.587, 0.114));
		if (color.a != 0.0)
			color = vec4(grey, grey, grey, color.a);
	} else if (modificationType == 5) { // Nega
		color = texture2D(tex,texCoord.st);
		if (color.a != 0.0)
			color = vec4(1.0 - color.r, 1.0 - color.g, 1.0 - color.b, color.a);
	} else if (modificationType == 6) { // Darken
		color = texture2D(tex,texCoord.st);
		if (color.a != 0.0)
			color = vec4(color.r*darkenHue.r, color.g*darkenHue.g, color.b*darkenHue.b, color.a);
	} else if (modificationType == 7) { // Colour replacement
		color = texture2D(tex,texCoord.st);
		if (color.r == replaceSrcColor.r && color.g == replaceSrcColor.g && color.b == replaceSrcColor.b) {
			color.r = replaceDstColor.r;
			color.g = replaceDstColor.g;
			color.b = replaceDstColor.b;
		}
		
	}

	// Now 'premultiply'
	if (modificationType == 0 || multiplyAlpha == 1) {
		color.r *= color.a;
		color.g *= color.a;
		color.b *= color.a;
	}

	gl_FragColor = color;
}
