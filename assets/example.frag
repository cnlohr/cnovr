#version 430
#include "cnovr.glsl"

out vec4 colorOut;

in vec2 texcoords;
in vec3 localpos;
in vec3 localnorm;
in vec4 extradata;

layout(location = 8) uniform sampler2D textures[];

void main()
{
	vec4 colorOutTC = vec4( abs(texcoords.xyy)*1.0, 1.0);
	vec4 colorOutTEX = vec4( texture( textures[0], texcoords.xy ).xyz, 1.0);
	colorOutTEX.rgb *= extradata.rgb;
	colorOut = mix( colorOutTC, colorOutTEX, .6 );
}
