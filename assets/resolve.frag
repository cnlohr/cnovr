#version 430
#include "cnovr.glsl"


out vec4 colorOut;

in vec2 texcoords;
in vec3 localpos;

#inject

uniform sampler2DMS tex; //#MAPUNIFORM tex 18

#ifdef MULTISAMPLES

vec4 textureMultisample(sampler2DMS sampler, ivec2 coord)
{
    vec4 color = vec4(0.0);

    for (int i = 0; i < MULTISAMPLES; i++)
        color += texelFetch(sampler, coord, i);

    color /= float(MULTISAMPLES);
    return color;
}
#endif


void main()
{
	vec4 oc;

#ifdef MULTISAMPLES
	oc = textureMultisample( tex, ivec2( gl_FragCoord.xy ) );
#else
	oc = vec4( texture( textures[0], texcoords.xy ).xyzw );
#endif
	colorOut = oc;
}
