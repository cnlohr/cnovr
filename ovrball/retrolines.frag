#version 430
#include "cnovr.glsl"

layout(location = 19) uniform vec4 rprops;
layout(location = 0) in vec3 localpos;

out vec4 colorOut;

void main()
{
	colorOut = vec4( 1.-abs(localpos)*1.0, rprops.x);
}
