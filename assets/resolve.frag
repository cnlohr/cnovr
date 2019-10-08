#version 430
#include "cnovr.glsl"

out vec4 colorOut;

in vec2 texcoords;
in vec3 localpos;

layout(location = 8) uniform sampler2D textures[];
layout(location = 18) uniform sampler2DMS tex;

#define multisample 4
//#layout(location = 17) uniform int multisample;

vec4 textureMultisample(sampler2DMS sampler, ivec2 coord)
{
    vec4 color = vec4(0.0);

    for (int i = 0; i < multisample; i++)
        color += texelFetch(sampler, coord, i);

    color /= float(multisample);

    return color;
}

void main()
{
	vec4 oc;
	if( multisample > 0 )
	{
		oc = textureMultisample( tex, ivec2( gl_FragCoord.xy ) );
	}
	else
	{
		oc = vec4( texture( textures[0], texcoords.xy ).xyz, 1.0);
	}
	colorOut = oc;
}
