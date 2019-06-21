#version 120
// We use this notation instead of normal #pragma because ANGLE 43 and 44 boil with an error on them :/
//PRAGMA: ONS_RU highprecision
uniform int effect_counter;
uniform int duration;
uniform int direction;
uniform sampler2D tex;
uniform float render_width;
uniform float render_height;
uniform float texture_width;
uniform float texture_height;

varying /* PRAGMA: ONS_RU highprecision */ vec2 texCoord;

const float PI = 3.14159265358979323846264;
const float E =  2.71828182845904523536029;
const float OMEGA = PI/64.0;

void main(void) {
	float t = float(float(effect_counter) * PI) / float(duration * 2);
	// t = 0..π/2　　as  effect_counter = 0..duration
	
	float rad_amp = sin(2.0*t);
	float rad_base = 0.0; 	// It seems that PS3 whirl has no such constant spin (4*t). Try turning it off
	int d = -1;
	if (direction == -1 || direction == 1) {
		/* Approximation version */
		rad_base = 4.0*t;
	} else if (direction == -2 || direction == 2) {
		/* Original version */
		d = int(clamp(float(d), -1.0, 1.0));
		float one_minus_cos = 1.0 - cos(t);
		rad_amp = PI * (sin(t) - one_minus_cos); // at end this is pi * (sin(pi/2) - (1 - cos(pi/2)) = 0
		rad_base = PI * 2.0 * one_minus_cos + rad_amp;
	}

	float centre_x = render_width/2.0;
	float centre_y = render_height/2.0;

	// actual x = x + 0.5, actual y = y + 0.5, let's reverse this
	float x = texCoord.x*texture_width - centre_x;
	float y = texCoord.y*texture_height - centre_y;

	//whirl factor
	float theta = float(d) * (rad_base + rad_amp * sin(sqrt(x * x + y * y) * OMEGA));

	float i = clamp(x * cos(theta) - y * sin(theta) + centre_x, 0.0, render_width-1.0);
	float j = clamp(x * sin(theta) + y * cos(theta) + centre_y, 0.0, render_height-1.0);

	gl_FragColor = texture2D(tex, vec2(i/texture_width, j/texture_height));
}
