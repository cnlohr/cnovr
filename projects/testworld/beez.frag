#version AUTOVER
#include "cnovr.glsl"

#define PI 3.14159

out vec4 colorOut;

in vec2 uv;

uniform vec4 clockparams; //#MAPUNIFORM clockparams 19

float rand(vec2 co){
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

vec3 QuickRanbow( float r )
{
	return vec3( rand( vec2( r, 0.0 ) ), rand( vec2( r, 1.0 ) ), rand( vec2( r, 2.0 ) ) );
	/*
		sin( r )+.5, sin( r + 2.094 )+.5, sin( r+4.189 )+.5 );
		*/
}

void main()
{
	if( clockparams.x < 0.5 )
	{
		colorOut = vec4( 0.5 );
	}
	else
	{
		float dx, dy;
		
		#define BEEZ 10
		#define speed 8.
		float bestZ = 10000;
		for( dx = -8; dx < 8; dx++ )
		for( dy = -8; dy < 8; dy++ )
		{
			vec2 dl = vec2( dx, dy );
			float phase = rand( dl )*10. + clockparams.y*speed;
			vec2 iuv = uv * 2. - 1. + dl*.2;
			vec2 vv = vec2( sin(phase), cos(phase) );
			//colorOut = vec4( iuv, 0., 1. );
			float linedist = (abs(dot( vv, iuv ))<.003)?1:0;
			float l = length( iuv );
			if( linedist > 0.)
			{
				bestZ = l;
				colorOut = vec4( linedist*QuickRanbow(phase),1. );
			}
		}
	}
}
