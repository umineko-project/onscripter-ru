#version 120
uniform sampler2D tex;

varying /* PRAGMA: ONS_RU highprecision */ vec2 texCoord;

void main(void) {
	vec4 color = texture2D(tex,texCoord.st);
	
	color.r *= color.a;
	color.g *= color.a;
	color.b *= color.a;
	gl_FragColor = color;
}
