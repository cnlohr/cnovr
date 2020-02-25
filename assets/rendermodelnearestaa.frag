#version AUTOVER
#include "cnovr.glsl"

out vec4 colorOut;

in vec2 texcoords;
in vec3 position;
in vec3 normal;

#define AAAmount 2

void main()
{
#if AAAmount == 0
	colorOut = vec4( texture( textures[0], texcoords.xy ).xyz, 1.0);
#else
	vec2 dtx = dFdx(texcoords.xy)/(float(AAAmount)/1.);
	vec2 dty = dFdy(texcoords.xy)/(float(AAAmount)/1.);
	vec2 ofs = vec2( 0.0 );//vec2( dtx + dty ) / ((float(AAAmount))*2);
	int x, y;
	vec4 outv = vec4( 0. );
	for( y = 0; y < AAAmount; y++ )
	for( x = 0; x < AAAmount; x++ )
		outv += vec4( texture( textures[0], 
			texcoords.xy + dtx*float(x) + dty*float(y) + ofs ).xyz, 1.0);
	colorOut=outv*(1./(float(AAAmount)*float(AAAmount)));
#endif

}
