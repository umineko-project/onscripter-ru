#version 120
uniform sampler2D tex;
uniform sampler2D tex1;

varying /* PRAGMA: ONS_RU highprecision */ vec2 texCoord;

void main(void) {
	vec4 img = texture2D(tex,texCoord.st);

	float x = texCoord.s;
	float y = texCoord.t;
	const float d = 1.0/512.0;
	const float n = 1.0/9.0;

	// do a bit of blurring to fix crappy masks
	// ehhh, it's not bad. a bit rough and d,d is actually wrong for the corners (should be euclidean distance of d) but generally ok?
	vec4 mask =	(texture2D(tex1,vec2(x-d,y-d))*n) + (texture2D(tex1,vec2(x,y-d))*n)   + (texture2D(tex1,vec2(x+d,y-d))*n) +
			(texture2D(tex1,vec2(x-d,y))*n)   + (texture2D(tex1,vec2(x,y))*n)   + (texture2D(tex1,vec2(x+d,y))*n) +
			(texture2D(tex1,vec2(x-d,y+d))*n) + (texture2D(tex1,vec2(x,y+d))*n) + (texture2D(tex1,vec2(x+d,y+d))*n);

	gl_FragColor = img*(mask.r);
}
