#version 120
uniform int mask_value;
uniform bool constant_mask;
uniform bool crossfade;
uniform sampler2D tex;
uniform sampler2D tex1;
uniform sampler2D tex2;

varying /* PRAGMA: ONS_RU highprecision */ vec2 texCoord;

void main(void) {
	vec4 img1 = texture2D(tex,texCoord.st);
	vec4 img2 = texture2D(tex1,texCoord.st);
	
	vec4 mask;
	if (constant_mask) {
		mask = vec4(0, 0, 0, 0);
	} else {
		mask = texture2D(tex2,texCoord.st);
	}

	if (crossfade) {
		float left = clamp((float(256.0+(mask.r*256.0)-float(mask_value))/256.0),0.0,1.0);
		float right = 1.0 - left; //clamp((float((mask_value-(mask.r*256)))/256),0.0,1.0);
		gl_FragColor = clamp((img1*left) + (img2*right), 0.0, 1.0);
	} else {
		bool top = (mask.r*256.0 >= float(mask_value));
		gl_FragColor = top ? img1 : img2;
	}
}
