#version 120
uniform sampler2D tex;
uniform int width;
uniform int height;
uniform int factor;

varying /* PRAGMA: ONS_RU highprecision */ vec2 texCoord;

void main(void) {
	vec2 uv = texCoord.xy;
	
	float px_w = 1.0/float(width);
	float px_h = 1.0/float(height);
	
	float cell_w = float(factor+1)*px_w;
	float cell_h = float(factor+1)*px_h;

	vec2 coord = vec2(cell_w*floor(uv.x/cell_w + 0.5), cell_h*floor(uv.y/cell_h + 0.5));
	gl_FragColor = vec4(texture2D(tex, coord).rgb, 1.0);
}
