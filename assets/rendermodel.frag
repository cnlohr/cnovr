#version 430
#include "cnovr.glsl"

out vec4 colorOut;

in vec2 texcoords;
in vec3 position;
in vec3 normal;

void main()
{
	colorOut = vec4( texture( textures[0], texcoords.xy ).xyz, 1.0);
}
