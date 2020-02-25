#version AUTOVER
#include "cnovr.glsl"

out vec4 colorOut;

varying vec3 localpos;

void main()
{
	colorOut = vec4( abs(localpos)*1.0, 1.0);
}
