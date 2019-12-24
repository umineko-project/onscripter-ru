#version 120

#define NTEXTURES 8 /* Maximum amount of textures */
#define IMG_W 2048.0
#define IMG_H 256.0

uniform int ntextures; /* Number of sub textures we're drawing */
uniform sampler2D subTex; /* Subtitle textures, only alpha for each pixel */
uniform vec2 subDims[NTEXTURES]; /* Dimensions of sub textures */
uniform vec2 subCoords[NTEXTURES]; /* Absolute coords of sub textures */
uniform vec4 subColors[NTEXTURES]; /* RGB colors of sub textures */
uniform vec2 dstDims; /* Absolute dimensions of target image */

varying /* PRAGMA: ONS_RU highprecision */ vec2 texCoord;

void main() {
	vec4 res = vec4(0.0);
	vec2 texCoordAbs = dstDims * texCoord.st;
	
	for (int i = 0; i < NTEXTURES; i++) {
		// We should not perform loops with non-constant limit in GLES, this fails in at least ANGLE 47!
		if (i == ntextures) break;
		if (all(greaterThanEqual(texCoordAbs, subCoords[i])) &&
			all(lessThanEqual(texCoordAbs, subCoords[i] + subDims[i]))) {
			float alpha = texture2D(subTex, (texCoordAbs - subCoords[i] + vec2(0.0, float(i)*IMG_H)) / vec2(IMG_W, IMG_H*float(NTEXTURES))).r * subColors[i].a;
			vec4 col = vec4(alpha * subColors[i].rgb, alpha);
			res = col + res * (1.0 - col.a);
		}
	}
	
	gl_FragColor = res;
}
