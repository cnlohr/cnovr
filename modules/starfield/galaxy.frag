#version 430
#include "cnovr.glsl"

out vec4 colorOut;

layout(location = 8) uniform sampler2D textures[];
layout(location = 19) uniform vec4 tparam;

in vec2 texcoords;
in vec3 position;
in vec3 normal;

void main()
{
	colorOut = vec4( texture( textures[0], texcoords.xy ).xyz, tparam.x);
}
