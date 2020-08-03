#version AUTOVER
#include "cnovr.glsl"

in vec3 coord;
uniform sampler2D MarkTex; //#MAPUNIFORM MarkTex 15

void main()
{
	gl_FragData[0] = texture( textures[0], coord.xy*.5 + .5 );
//	gl_FragData[0] = vec4( coord.xy, 0., 1. );
}

