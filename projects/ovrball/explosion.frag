#version AUTOVER
#include "cnovr.glsl"

out vec4 colorOut;

in vec4 localcolor;


void main()
{
	if( localcolor.a < 0.01 ) discard;
	colorOut = localcolor;
//	colorOut = vec4( 1. );
}
