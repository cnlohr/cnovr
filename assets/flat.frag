#version AUTOVER
#include "cnovr.glsl"

out vec4 colorOut;

in vec2 texcoords;
in vec3 localpos;
in vec3 localnorm;
in vec4 extradata;
void main()
{
	vec4 colorOutTEX = vec4( texture( textures[0], texcoords.xy ).xyz, 1.0);
	colorOut = colorOutTEX;
}
