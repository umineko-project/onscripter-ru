#version 120
// We use this notation instead of normal #pragma because ANGLE 43 and 44 boil with an error on them :/
//PRAGMA: ONS_RU highprecision
uniform sampler2D tex;
uniform float animationClock; // in seconds
uniform float amplitude; // used to be called "num"
uniform float wavelength;
uniform float speed; // used to be called "angle"
uniform float cx; // texture_w / w
uniform float cy; // texture_h / h

varying /* PRAGMA: ONS_RU highprecision */ vec2 texCoord;

const float pi = 3.14159265358;

void main(void) {
	vec2 uv = texCoord.xy; // 0~1
	//uv.y = 1.0 - uv.y;
	
	uv.x *= cx;
	uv.y *= cy;
	
	float x = +((uv.x * 2.0) - 1.0); // -1 ~ +1
	float y = +((uv.y * 2.0) - 1.0); // -1 ~ +1

	float r = sqrt(x*x + y*y);
	float phi = atan(y,x); // -pi ~ pi
	
	float cyclePerSec = animationClock*pi*2.0;
	
	phi += (amplitude*pi/1000.0)*sin((cyclePerSec * speed * 0.06) + (1000.0*1920.0*pi*r)/(1080.0*wavelength));
	
	vec2 newUv = uv;
	newUv.x = (r * cos(phi) + 1.0)/2.0;
	newUv.y = (r * sin(phi) + 1.0)/2.0;
	
	newUv.x = clamp(newUv.x, 0.0, 1.0) / cx;
	newUv.y = clamp(newUv.y, 0.0, 1.0) / cy;
	
	gl_FragColor = texture2D(tex, newUv);
}
