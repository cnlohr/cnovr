#version AUTOVER
#include "cnovr.glsl"

#define PI 3.14159

out vec4 colorOut;

in vec2 uv;

uniform vec4 clockparams; //#MAPUNIFORM clockparams 19

void main()
{
	vec2 uvk = uv*2.-1.;
	
	float polar = atan( uvk.x, uvk.y ) / (PI*2);
	float hand = polar + clockparams.x/2.;
	float r = length( uvk );

	vec4 col;

	if( fract( hand ) < .05 || ((fract(hand)>0.5 &&fract(hand)<0.55)) )
		col = vec4( 1. );
	else
		col = vec4( 0. );
		
	if( fract( polar ) < .05 || ((fract(polar)>0.5 &&fract(polar)<0.55)) )
		col.r += .3;
		
	colorOut = col;
}
