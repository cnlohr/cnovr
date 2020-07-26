#version AUTOVER
#include "cnovr.glsl"

out vec4 colorOut;

varying vec3 localpos;

void main()
{
	float intensity = mod(length(localpos)*20.0,1.0)+0.5;
	//float intensity = 0.5;
	colorOut = vec4( abs(localpos), intensity);
}
