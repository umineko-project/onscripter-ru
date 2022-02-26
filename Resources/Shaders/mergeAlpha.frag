#version 120
uniform sampler2D tex;
uniform sampler2D tex1;

varying /* PRAGMA: ONS_RU highprecision */ vec2 texCoord;

void main(void) {
	vec4 color = texture2D(tex,texCoord.st);
	vec4 colorSrc = texture2D(tex1,texCoord.st);
	
	color.a = colorSrc.r;
	color.r *= colorSrc.r;
	color.g *= colorSrc.r;
	color.b *= colorSrc.r;
	gl_FragColor = color;
}
