#version 120
uniform sampler2D tex;
uniform float alpha;

varying /* PRAGMA: ONS_RU highprecision */ vec2 texCoord;

// return 1 if v inside the box, return 0 otherwise
// http://stackoverflow.com/questions/12751080/glsl-point-inside-box-test
// uses "step" to avoid "if" branching
float insideBox(vec2 v, vec2 bottomLeft, vec2 topRight) {
    vec2 s = step(bottomLeft, v) - step(topRight, v);
    return s.x * s.y;   
}

void main(void) {
	vec4 img = texture2D(tex,texCoord.st);
	gl_FragColor = insideBox(texCoord.st, vec2(0.0,0.0), vec2(1.0,1.0)) * img * alpha;
}
