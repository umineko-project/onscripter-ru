#version 120
uniform vec4 color;
uniform int maxy;
uniform int faceAscender;
uniform int height;
uniform sampler2D tex;

varying /* PRAGMA: ONS_RU highprecision */ vec2 texCoord;

void main(void) {
	gl_FragColor = texture2D(tex, texCoord.st);
	
	float current_y = float(height) * (1.0 - texCoord.t);
	float current_y_above_baseline = current_y - (float(height) - float(maxy));
	float percent_above_baseline = max(0.0, current_y_above_baseline / float(faceAscender));
	
	bool isWhite = vec3(1.0) == color.rgb;
	float lightening_factor = 0.5 * (float(isWhite) * (percent_above_baseline - 0.6) + float(!isWhite) * (0.65 - percent_above_baseline));
	
	gl_FragColor = vec4(color.r+lightening_factor,
						color.g+lightening_factor,
						color.b+lightening_factor,
						gl_FragColor.a);
	
	gl_FragColor = vec4(gl_FragColor.r*gl_FragColor.a, gl_FragColor.g*gl_FragColor.a, gl_FragColor.b*gl_FragColor.a, gl_FragColor.a);
}
