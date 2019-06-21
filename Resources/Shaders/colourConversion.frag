#version 120

uniform int conversionType;
uniform int maskHeight; // must be power of two
varying /* PRAGMA: ONS_RU highprecision */ vec2 texCoord;
uniform sampler2D tex, tex1, tex2;

// https://github.com/freecores/color_converter/blob/master/rtl/vhdl/ccfactors_pkg.vhd#L195
vec3 YUVToRGB(vec3 yuv) {
	float y = (yuv.r - 16.0/255.0) * 1.16438;
	float cb = yuv.g - 128.0/255.0;
	float cr = yuv.b - 128.0/255.0;
	vec3 rgb;
	rgb.r = y + cr * 1.79274;
	rgb.g = y - 0.532910 * cr - 0.213250 * cb;
	rgb.b = y + cb * 2.11240;
	
	return rgb;
}

vec3 grabYUV(vec2 coord) {
	vec3 yuv;
	
	if (conversionType == 1) {
		yuv.x = texture2D(tex, coord).r;
		yuv.y = texture2D(tex1, coord).r;
		yuv.z = texture2D(tex2, coord).r;
	} else {
		yuv.x = texture2D(tex, coord).r;
		yuv.yz = texture2D(tex1, coord).ra;
	}

	return yuv;
}

void main() {
	if (maskHeight == 0 || texCoord.y <= 0.5) {
		vec4 rgba = vec4(YUVToRGB(grabYUV(texCoord)), 1.0);

		if (maskHeight > 0) {
			rgba.a = YUVToRGB(grabYUV(texCoord+vec2(0.0, 0.5))).r;
			rgba.rgb *= rgba.a;
		}
		
		gl_FragColor = rgba;
	}
}
