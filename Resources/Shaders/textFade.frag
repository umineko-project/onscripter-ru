#version 120
uniform int partial;
uniform int full;
uniform int width;
uniform sampler2D tex;
varying /* PRAGMA: ONS_RU highprecision */ vec2 texCoord;

void main(void) {
	int current = int(texCoord.s * float(width));
	gl_FragColor = texture2D(tex,texCoord.st);
	// not fully opaque
	if (current > full) {
		if (current >= full + partial) {
			// fully transparent
			gl_FragColor = vec4(0.0);
		} else {
			// Somewhere in the middle
			gl_FragColor *= (1.0 - (float(current - full) / float(partial)));
		}
	}
}
