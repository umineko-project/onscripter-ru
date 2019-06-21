#version 120
uniform int script_width;
uniform int script_height;
uniform int effect_counter;
uniform int duration;
uniform sampler2D tex;

varying /* PRAGMA: ONS_RU highprecision */ vec2 texCoord;

const float PI = 3.14159265358979323846264;
const float TRVSWAVE_AMPLITUDE = 18.0;
const float TRVSWAVE_WVLEN_END  = 64.0;
const float TRVSWAVE_WVLEN_START = 512.0;

void main(void) {
	vec4 img = texture2D(tex,texCoord.st);
	
	int w = script_width;
	int h = script_height;
	
	float t = float(effect_counter) * PI / float(duration * 2);
	// t = 0..π/2　　as  effect_counter = 0..duration
	
	float ampl, wvlen;
	
	if (effect_counter * 2 < duration) {
		ampl = TRVSWAVE_AMPLITUDE * float(2 * effect_counter) / float(duration);
		wvlen = (1.0/(((1.0/TRVSWAVE_WVLEN_END - 1.0/TRVSWAVE_WVLEN_START) * float(2 * effect_counter)
					   / float(duration)) + (1.0/TRVSWAVE_WVLEN_START)));
	} else {
		ampl = TRVSWAVE_AMPLITUDE * float(2 * (duration - effect_counter)) / float(duration);
		wvlen = (1.0/(((1.0/TRVSWAVE_WVLEN_END - 1.0/TRVSWAVE_WVLEN_START) * float(2 * (duration - effect_counter))
					   / float(duration)) + (1.0/TRVSWAVE_WVLEN_START)));
	}
		
	int i = int(texCoord.s*float(w)); //x
	int j = int(texCoord.t*float(h)); //y

	int ii = i + int((ampl * sin(PI * 2.0 * float(j) / wvlen)));
	
	if (ii < 0) ii = 0;
	if (ii >= w) ii = w-1;
	
	vec2 newcoords = vec2(float(ii)/float(w), float(j)/float(h));
	
	gl_FragColor = texture2D(tex,newcoords);
}
