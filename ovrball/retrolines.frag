#version AUTOVER
#include "cnovr.glsl"

uniform vec4 rprops; //#MAPUNIFORM rprops 19

in vec3 localpos;
out vec4 colorOut;

void main()
{
	colorOut = vec4( 1.-abs(localpos)*1.0, rprops.x);
}
