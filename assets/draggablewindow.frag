#version 430
#include "cnovr.glsl"

out vec4 colorOut;

in vec2 texcoords;
in vec3 position;
in vec3 normal;
in vec4 extradataout;

uniform vec4 paramdata[16*4]; //#MAPUNIFORM paramdata 19

void main()
{
	vec4 pd = paramdata[ int( extradataout.x+0.5 )+0];
	float aspect = pd.x / pd.y;
	vec2 cursorpos = pd.zw;
	vec2 dcp = (texcoords - cursorpos) * vec2( aspect, 1.0 );

	float dcpl = 100.*length( dcp );
	dcpl = (cos(dcpl*10.0)/dcpl + 0.4) + 1.0;
	dcpl = (clamp( dcpl, 0., 1. )*2.0-1.0);	//Mouse cursor	
	dcpl = clamp( dcpl, -1., 1. );
	colorOut = vec4( ((texture( textures[0], texcoords.xy ).bgr*2.0-1.0) * dcpl )*0.5+0.5, 1.0 );
}
