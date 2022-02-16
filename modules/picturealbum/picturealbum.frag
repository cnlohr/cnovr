#version AUTOVER
#include "cnovr.glsl"

out vec4 colorOut;

//layout(location = 8) uniform sampler2D textures[8];

in vec2 texcoords;
in vec3 position;
in vec3 normal;

void main()
{
//	float seg = texcoords.x * 255.0;
//	if( mod( texcoords.y, .1 ) < .1/8.0 )
//	{
//		colorOut = ( mod( seg, 1.0 ) < 0.5 ) ? vec4(1.0): vec4(0.0,0.0,0.0,1.0);
//	}
//	else
//	{
//		colorOut = vec4(pow(texcoords.xxx,vec3(1.0)),1.0);
//	}
	colorOut = vec4( texture( textures[0], texcoords.xy ).xyz, 1.0);
}
