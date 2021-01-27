#version AUTOVER
#include "cnovr.glsl"

#define PI 3.14159

out vec4 colorOut;

in vec2 uv;

uniform vec4 clockparams; //#MAPUNIFORM clockparams 19

float rand(vec2 co){
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

void main()
{
	if( clockparams.x < 0.5 )
	{
		colorOut = vec4( 0.5 );
	}
	else
	{
		colorOut = vec4( rand( uv ) );
	}
}
