#version 430
#include "cnovr.glsl"

out vec4 colorOut;

layout(location = 8) uniform sampler2D textures[];

in vec2 texcoords;
in vec3 position;
in vec3 normal;

void main()
{
	vec3 texbg = texture( textures[0], texcoords.xy ).xyz*vec3(1.,0.,1.); 
	vec3 texfg = texture( textures[1], texcoords.xy ).xyz*vec3(0.,1.,0.); 
	colorOut = vec4( texfg + texbg, 1.0);
}
