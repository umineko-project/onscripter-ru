#version 120
uniform float tilesX; // number of tiles that fit into tex horizontally
uniform float tilesY; // number of tiles that fit into tex vertically
uniform int breakupCellforms; // number of cellforms in tex1
uniform sampler2D tex;  // texture containing the surface to break up
uniform sampler2D tex1; // texture containing a horizontal row of cellforms of increasing radius
uniform sampler2D tex2; // grid texture containing 1 pixel for each tile (tilesX * tilesY in size) with color representing radius (0 = no radius, 1.0 = full radius)

varying /* PRAGMA: ONS_RU highprecision */ vec2 texCoord;

void main(void) {
	float belongsToTileX = floor(texCoord.s * tilesX) / tilesX;
	float belongsToTileY = floor(texCoord.t * tilesY) / tilesY;
	float gridReportedRadius = texture2D(tex2,vec2(belongsToTileX,belongsToTileY)).r;
	if (gridReportedRadius >= 1.0) {
		gl_FragColor = texture2D(tex,texCoord.st);
	} else {
		int thisRadius = int(floor(gridReportedRadius * float(breakupCellforms)));
		float xPercentageThroughTile = mod(texCoord.s, 1.0/tilesX) * tilesX;
		float yPercentageThroughTile = mod(texCoord.t, 1.0/tilesY) * tilesY;
		float breakupCellformsInterval = 1.0/float(breakupCellforms);
		float breakupCellformsStartX = float(thisRadius-1) * breakupCellformsInterval;
		float x = breakupCellformsStartX + (breakupCellformsInterval * xPercentageThroughTile);
		float y = yPercentageThroughTile;
		gl_FragColor = texture2D(tex1,vec2(x,y)).r * texture2D(tex,texCoord.st);
	}
}
