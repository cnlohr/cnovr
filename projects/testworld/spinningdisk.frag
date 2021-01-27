#version AUTOVER
#include "cnovr.glsl"

#define PI 3.14159

out vec4 colorOut;

in vec2 uv;

uniform vec4 clockparams; //#MAPUNIFORM clockparams 19

void main()
{
	vec2 uvk = uv*2.-1.;
	
	float angle = ((clockparams.z>0.5)?(clockparams.y/20.):(clockparams.x*20)) * clockparams.w;
	
	float polar = atan( uvk.x, uvk.y ) / (PI*2);
	float hand = polar + angle;
	float r = length( uvk );

	if( fract( hand ) < .1 )
		colorOut = vec4( 1. );
	else
		colorOut = vec4( 0. );
}
