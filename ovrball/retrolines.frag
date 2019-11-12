#version 430
#include "cnovr.glsl"

out vec4 colorOut;

layout(location = 9) uniform vec4 rprops;

varying vec3 localpos;

void main()
{
	colorOut = vec4( 1.-abs(localpos)*1.0, rprops.x);
}
